#include "scriptbuilder.hpp"

#include <components/falloutscript/reputation.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::Reputation;

    constexpr FormId ncr = 0x000F43DE;
    constexpr FormId legion = 0x000F43DD;
    constexpr FormId nobody = 0x000F9999;

    constexpr std::uint16_t OpSetReputation = 0x1238;
    constexpr std::uint16_t OpAddReputation = 0x1239;
    constexpr std::uint16_t OpGetReputation = 0x123D;
    constexpr std::uint16_t OpGetReputationThreshold = 0x123F;

    class FakeRecords : public FalloutScript::ReputationRecords
    {
    public:
        std::map<FormId, double> mCaps{ { ncr, 80.0 }, { legion, 100.0 } };
        std::map<std::string, double> mSettings;

        bool reputation(FormId reputation, double& cap, std::string& name) override
        {
            const auto found = mCaps.find(reputation);
            if (found == mCaps.end())
                return false;
            cap = found->second;
            name = reputation == ncr ? "NCR" : "Legion";
            return true;
        }
        double setting(std::string_view name, double fallback) override
        {
            const auto found = mSettings.find(std::string(name));
            return found != mSettings.end() ? found->second : fallback;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeRecords mRecords;
        Reputation mReputation{ mRecords };
        std::vector<Reputation::Bump> mMessages;

        Runner()
        {
            FalloutScript::addReputationCommands(
                mInterpreter, mReputation, [this](const Reputation::Bump& bump) { mMessages.push_back(bump); });
        }

        double call(std::uint16_t opcode, std::vector<double> numbers)
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, 0, false, std::move(arguments));
        }
    };

    TEST(FalloutScriptReputationTest, a_reputation_starts_at_nothing_and_is_neutral)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetReputation, { ncr, 1 }), 0.0);
        EXPECT_EQ(run.call(OpGetReputation, { ncr, 0 }), 0.0);
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 0 }), 1.0);
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 1 }), 1.0);
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 2 }), 1.0);
    }

    TEST(FalloutScriptReputationTest, a_step_adds_the_amount_of_its_setting_to_the_side)
    {
        Runner run;
        run.call(OpAddReputation, { ncr, 1, 1 });
        run.call(OpAddReputation, { ncr, 1, 3 });
        // Very minor is 1 point and average 4
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 1 }), 5.0);
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 0 }), 0.0);
        run.call(OpAddReputation, { ncr, 0, 5 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 0 }), 12.0);
    }

    TEST(FalloutScriptReputationTest, the_amounts_of_the_steps_are_the_settings_of_the_files)
    {
        Runner run;
        run.mRecords.mSettings["fReputationBumpMajor"] = 9.5;
        run.call(OpAddReputation, { legion, 1, 4 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { legion, 1 }), 9.5);
    }

    TEST(FalloutScriptReputationTest, a_side_stops_at_the_cap_of_the_record)
    {
        Runner run;
        for (int i = 0; i < 10; ++i)
            run.call(OpAddReputation, { ncr, 1, 5 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 1 }), 80.0);
    }

    TEST(FalloutScriptReputationTest, a_record_without_a_cap_holds_no_side_back)
    {
        Runner run;
        run.mRecords.mCaps[ncr] = 0.0;
        run.call(OpAddReputation, { ncr, 1, 5 });
        run.call(OpAddReputation, { ncr, 1, 5 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 1 }), 24.0);
        EXPECT_THAT(run.mMessages, SizeIs(2));
    }

    TEST(FalloutScriptReputationTest, a_step_or_side_that_is_none_changes_nothing)
    {
        Runner run;
        run.call(OpAddReputation, { ncr, 1, 0 });
        run.call(OpAddReputation, { ncr, 1, 6 });
        run.call(OpAddReputation, { ncr, 1, 10 });
        run.call(OpAddReputation, { ncr, 2, 1 });
        run.call(OpAddReputation, { ncr, 3, 1 });
        run.call(OpAddReputation, { nobody, 1, 1 });
        EXPECT_EQ(run.call(OpGetReputation, { ncr, 1 }), 0.0);
        EXPECT_EQ(run.call(OpGetReputation, { ncr, 0 }), 0.0);
        EXPECT_THAT(run.mMessages, IsEmpty());
    }

    TEST(FalloutScriptReputationTest, setting_a_side_does_not_stop_at_the_cap_and_a_step_then_brings_it_down_to_it)
    {
        Runner run;
        run.call(OpSetReputation, { ncr, 1, 500 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 1 }), 500.0);
        run.call(OpAddReputation, { ncr, 1, 1 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 1 }), 80.0);
        // Nothing holds it from below
        run.call(OpSetReputation, { ncr, 0, -20 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 0 }), -20.0);
        run.call(OpSetReputation, { ncr, 2, 7 });
        run.call(OpSetReputation, { nobody, 1, 7 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 0 }), -20.0);
        EXPECT_EQ(run.call(OpGetReputation, { nobody, 1 }), 0.0);
    }

    TEST(FalloutScriptReputationTest, a_side_that_is_not_the_positive_one_reads_the_negative_one)
    {
        Runner run;
        run.call(OpSetReputation, { ncr, 0, 12 });
        EXPECT_DOUBLE_EQ(run.call(OpGetReputation, { ncr, 2 }), 12.0);
    }

    TEST(FalloutScriptReputationTest, the_level_of_a_side_is_how_far_it_is_to_the_cap)
    {
        const std::array<double, 3> limits{ 0.15, 0.5, 1.0 };
        EXPECT_EQ(Reputation::levelOf(0, 80, limits), 0);
        EXPECT_EQ(Reputation::levelOf(11.9, 80, limits), 0);
        EXPECT_EQ(Reputation::levelOf(12.0, 80, limits), 1);
        EXPECT_EQ(Reputation::levelOf(39.9, 80, limits), 1);
        EXPECT_EQ(Reputation::levelOf(40, 80, limits), 2);
        EXPECT_EQ(Reputation::levelOf(79.9, 80, limits), 2);
        EXPECT_EQ(Reputation::levelOf(80, 80, limits), 3);
        // A reputation without a cap is full with any points
        EXPECT_EQ(Reputation::levelOf(0, 0, limits), 0);
        EXPECT_EQ(Reputation::levelOf(1, 0, limits), 3);
    }

    TEST(FalloutScriptReputationTest, the_class_of_two_levels_is_what_the_executable_tells)
    {
        // (fame, infamy) -> class for the kinds 0, 1 and 2
        struct Row
        {
            int mFame;
            int mInfamy;
            int mClasses[3];
        };
        const Row rows[] = {
            { 0, 0, { 1, 1, 1 } },
            { 1, 0, { 0, 4, 0 } },
            { 2, 0, { 0, 5, 0 } },
            { 3, 0, { 0, 6, 0 } },
            { 0, 1, { 0, 0, 4 } },
            { 0, 2, { 0, 0, 5 } },
            { 0, 3, { 0, 0, 6 } },
            { 1, 1, { 3, 0, 0 } },
            { 2, 2, { 4, 0, 0 } },
            { 3, 3, { 5, 0, 0 } },
            { 3, 2, { 2, 0, 0 } },
            { 2, 3, { 2, 0, 0 } },
            { 2, 1, { 0, 2, 0 } },
            { 3, 1, { 0, 3, 0 } },
            { 1, 2, { 0, 0, 2 } },
            { 1, 3, { 0, 0, 3 } },
        };
        for (const Row& row : rows)
            for (int kind = 0; kind < 3; ++kind)
                EXPECT_EQ(Reputation::classOf(row.mFame, row.mInfamy, kind), row.mClasses[kind])
                    << row.mFame << "/" << row.mInfamy << " kind " << kind;
        EXPECT_EQ(Reputation::classOf(1, 0, 3), 0);
        EXPECT_EQ(Reputation::classOf(1, 0, -1), 0);
    }

    TEST(FalloutScriptReputationTest, the_threshold_follows_the_points_of_both_sides)
    {
        Runner run;
        // NCR has a cap of 80: 12 points is the first level, 40 the second
        run.call(OpSetReputation, { ncr, 1, 12 });
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 1 }), 4.0);
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 0 }), 0.0);
        run.call(OpSetReputation, { ncr, 1, 40 });
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 1 }), 5.0);
        run.call(OpSetReputation, { ncr, 0, 80 });
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 0 }), 2.0);
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 1 }), 0.0);
        // A reputation that is none is nothing
        EXPECT_EQ(run.call(OpGetReputationThreshold, { nobody, 1 }), 0.0);
    }

    TEST(FalloutScriptReputationTest, the_limits_of_the_levels_are_the_settings_of_the_files)
    {
        Runner run;
        run.mRecords.mSettings["fReputationThresholdOne"] = 0.5;
        run.call(OpSetReputation, { ncr, 1, 12 });
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 1 }), 1.0);
        run.call(OpSetReputation, { ncr, 1, 40 });
        EXPECT_EQ(run.call(OpGetReputationThreshold, { ncr, 1 }), 5.0);
    }

    TEST(FalloutScriptReputationTest, a_step_tells_the_player_and_says_whether_the_level_changed)
    {
        Runner run;
        run.call(OpAddReputation, { ncr, 1, 1 });
        ASSERT_EQ(run.mMessages.size(), 1u);
        EXPECT_EQ(run.mMessages[0].mName, "NCR");
        EXPECT_EQ(run.mMessages[0].mSide, 1);
        EXPECT_FALSE(run.mMessages[0].mLevelChanged);
        // 12 points is the first level of 80
        run.call(OpAddReputation, { ncr, 1, 5 });
        ASSERT_EQ(run.mMessages.size(), 2u);
        EXPECT_TRUE(run.mMessages[1].mLevelChanged);
        EXPECT_EQ(run.mMessages[1].mPositiveLevel, 1);
        EXPECT_EQ(run.mMessages[1].mNegativeLevel, 0);
        run.call(OpAddReputation, { legion, 0, 1 });
        ASSERT_EQ(run.mMessages.size(), 3u);
        EXPECT_EQ(run.mMessages[2].mSide, 0);
        EXPECT_EQ(run.mMessages[2].mName, "Legion");
    }

    TEST(FalloutScriptReputationTest, a_saved_game_brings_the_points_back)
    {
        Runner run;
        run.call(OpSetReputation, { ncr, 1, 33.5 });
        run.call(OpSetReputation, { legion, 0, 7 });
        FalloutScript::SaveWriter out;
        FalloutScript::writeReputation(run.mReputation, out);

        Runner other;
        other.call(OpSetReputation, { ncr, 1, 1 });
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readReputation(other.mReputation, in, FalloutScript::reputationSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_DOUBLE_EQ(other.call(OpGetReputation, { ncr, 1 }), 33.5);
        EXPECT_DOUBLE_EQ(other.call(OpGetReputation, { legion, 0 }), 7.0);
        EXPECT_EQ(other.call(OpGetReputation, { ncr, 0 }), 0.0);

        other.mReputation.clear();
        EXPECT_EQ(other.call(OpGetReputation, { ncr, 1 }), 0.0);
    }
}
