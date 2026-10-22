#include "scriptbuilder.hpp"

#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/placementcommands.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    using FalloutScript::Placement;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId guard = 0x01000100;
    constexpr FormId marker = 0x01000200;
    constexpr FormId scriptOfGuards = 0x01002000;

    // The type of a variable in the record: 1 for a short or a long, 0 for a float
    constexpr std::uint32_t floatVariable = 0;
    constexpr double pi = 3.14159265358979323846;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpGetPos = 0x1006;
    constexpr std::uint16_t OpSetPos = 0x1007;
    constexpr std::uint16_t OpGetAngle = 0x1008;
    constexpr std::uint16_t OpGetHeadingAngle = 0x1063;
    constexpr std::uint16_t OpSetAngle = 0x1009;
    constexpr std::uint16_t OpGetScale = 0x1018;
    constexpr std::uint16_t OpSetScale = 0x113C;
    constexpr std::uint16_t OpMoveToMarker = 0x109E;
    constexpr std::uint16_t OpMoveToMarkerWithFade = 0x124F;

    class FakePlacements : public FalloutScript::PlacementWorld
    {
    public:
        struct Move
        {
            FormId mReference;
            FormId mTarget;
            std::array<double, 3> mOffset;
            bool operator==(const Move& other) const
            {
                return mReference == other.mReference && mTarget == other.mTarget && mOffset == other.mOffset;
            }
        };

        struct Set
        {
            FormId mReference;
            bool mAngle;
            int mAxis;
            double mValue;
        };

        std::map<FormId, Placement> mPlaces;
        std::vector<Move> mMoves;
        std::vector<Set> mSets;
        std::vector<std::pair<FormId, double>> mScales;

        bool placement(FormId reference, Placement& result) override
        {
            const auto found = mPlaces.find(reference);
            if (found == mPlaces.end())
                return false;
            result = found->second;
            return true;
        }
        void moveTo(FormId reference, FormId target, const std::array<double, 3>& offset) override
        {
            mMoves.push_back({ reference, target, offset });
        }
        void setPosition(FormId reference, int axis, double value) override
        {
            if (mPlaces.count(reference) != 0)
                mSets.push_back({ reference, false, axis, value });
        }
        void setAngle(FormId reference, int axis, double radians) override
        {
            if (mPlaces.count(reference) != 0)
                mSets.push_back({ reference, true, axis, radians });
        }
        void setScale(FormId reference, double scale) override { mScales.emplace_back(reference, scale); }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        FakePlacements mWorld;

        Runner() { FalloutScript::addPlacementCommands(mInterpreter, mWorld); }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGuards];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards);
            record.mScript = builder.definition();
            mObjects.add(guard, scriptOfGuards);
            mObjects.trigger(guard, BlockType::OnActivate);
        }

        double variable(std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(guard);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    Bytes axis(char letter)
    {
        return Bytes().u8(static_cast<std::uint8_t>(letter));
    }

    Bytes axisAndValue(char letter, double value)
    {
        return arguments(2).add(axis(letter)).add(floatArg(value));
    }

    TEST(FalloutScriptPlacementTest, a_script_moves_a_reference_to_a_marker_with_offsets)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t target = builder.form(marker);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(
                OpMoveToMarker, arguments(4).add(refArg(target)).add(floatArg(10.5)).add(floatArg(-20)).add(intArg(30)))
            + end());

        setup.run(builder);

        using Move = FakePlacements::Move;
        EXPECT_THAT(setup.mWorld.mMoves, ElementsAre(Move{ guard, marker, { 10.5, -20, 30 } }));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPlacementTest, the_offsets_of_a_move_are_optional_and_the_fade_changes_nothing)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t target = builder.form(marker);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpMoveToMarker, arguments(1).add(refArg(target)))
            + command(OpMoveToMarkerWithFade, arguments(2).add(refArg(target)).add(intArg(5))) + end());

        setup.run(builder);

        using Move = FakePlacements::Move;
        EXPECT_THAT(
            setup.mWorld.mMoves, ElementsAre(Move{ guard, marker, { 0, 0, 0 } }, Move{ guard, marker, { 5, 0, 0 } }));
    }

    TEST(FalloutScriptPlacementTest, a_script_reads_the_position_and_the_angles_in_degrees)
    {
        Runner setup;
        Placement where;
        where.mPosition = { 100.5, -200, 300 };
        where.mAngle = { pi / 2, 0, -pi / 2 };
        setup.mWorld.mPlaces[guard] = where;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetPos, arguments(1).add(axis('X'))))
            + setTo(var('f', 2), callToken(OpGetPos, arguments(1).add(axis('y'))))
            + setTo(var('f', 3), callToken(OpGetPos, arguments(1).add(axis('Z'))))
            + setTo(var('f', 4), callToken(OpGetAngle, arguments(1).add(axis('X'))))
            + setTo(var('f', 5), callToken(OpGetAngle, arguments(1).add(axis('Z')))) + end());
        for (std::uint32_t index = 1; index <= 5; ++index)
            builder.variable(index, floatVariable, "v" + std::to_string(index));

        setup.run(builder);

        EXPECT_FLOAT_EQ(setup.variable(1), 100.5f);
        EXPECT_FLOAT_EQ(setup.variable(2), -200.f);
        EXPECT_FLOAT_EQ(setup.variable(3), 300.f);
        EXPECT_NEAR(setup.variable(4), 90.0, 1e-4);
        // The heading is told from 0 to 360
        EXPECT_NEAR(setup.variable(5), 270.0, 1e-4);
    }

    double headingAngle(
        Runner& setup, const std::array<double, 3>& self, double heading, const std::array<double, 3>& target)
    {
        Placement from;
        from.mPosition = self;
        from.mAngle = { 0, 0, heading };
        Placement to;
        to.mPosition = target;
        setup.mWorld.mPlaces[guard] = from;
        setup.mWorld.mPlaces[marker] = to;
        return setup.mInterpreter.call(OpGetHeadingAngle, guard, true, { { marker, {}, false } });
    }

    TEST(FalloutScriptPlacementTest, the_heading_angle_is_positive_for_a_target_on_the_right)
    {
        Runner setup;
        // Facing the y axis, which is heading 0
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, 0, { 0, 100, 0 }), 0.0, 1e-9);
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, 0, { 100, 0, 0 }), 90.0, 1e-9);
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, 0, { -100, 0, 0 }), -90.0, 1e-9);
        EXPECT_NEAR(std::abs(headingAngle(setup, { 0, 0, 0 }, 0, { 0, -100, 0 })), 180.0, 1e-9);
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, 0, { 100, 100, 0 }), 45.0, 1e-9);
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, 0, { -100, -100, 0 }), -135.0, 1e-9);
    }

    TEST(FalloutScriptPlacementTest, the_heading_angle_counts_from_where_the_reference_faces)
    {
        Runner setup;
        // Facing the x axis
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, pi / 2, { 100, 0, 0 }), 0.0, 1e-9);
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, pi / 2, { 0, 100, 0 }), -90.0, 1e-9);
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, pi / 2, { 0, -100, 0 }), 90.0, 1e-9);
        // The place of the reference is where the target is measured from, and the heights do not count
        EXPECT_NEAR(headingAngle(setup, { 50, 50, 5000 }, 0, { 150, 50, -3000 }), 90.0, 1e-9);
        // A heading outside of a turn is the same heading
        EXPECT_NEAR(headingAngle(setup, { 0, 0, 0 }, 5 * pi / 2, { 100, 0, 0 }), 0.0, 1e-9);
    }

    TEST(FalloutScriptPlacementTest, two_references_on_one_place_are_at_270_degrees_as_in_the_games)
    {
        Runner setup;
        EXPECT_NEAR(headingAngle(setup, { 10, 20, 0 }, 0, { 10, 20, 7 }), -90.0, 1e-9);
        EXPECT_NEAR(headingAngle(setup, { 10, 20, 0 }, -pi / 2, { 10, 20, 7 }), 0.0, 1e-9);
    }

    TEST(FalloutScriptPlacementTest, the_heading_angle_of_something_not_placed_is_zero)
    {
        Runner setup;
        Placement from;
        setup.mWorld.mPlaces[guard] = from;
        EXPECT_EQ(setup.mInterpreter.call(OpGetHeadingAngle, guard, true, { { marker, {}, false } }), 0.0);
        EXPECT_EQ(setup.mInterpreter.call(OpGetHeadingAngle, guard, true, {}), 0.0);
        setup.mWorld.mPlaces[marker] = from;
        EXPECT_EQ(setup.mInterpreter.call(OpGetHeadingAngle, 0, false, { { marker, {}, false } }), 0.0);
    }

    TEST(FalloutScriptPlacementTest, the_scale_of_a_reference_is_what_the_host_says)
    {
        Runner setup;
        Placement place;
        place.mScale = 1.5;
        setup.mWorld.mPlaces[guard] = place;
        EXPECT_EQ(setup.mInterpreter.call(OpGetScale, guard, true, {}), 1.5);
        EXPECT_EQ(setup.mInterpreter.call(OpGetScale, 0, false, {}), 0.0);
        EXPECT_EQ(setup.mInterpreter.call(OpGetScale, marker, true, {}), 0.0);
    }

    TEST(FalloutScriptPlacementTest, a_scale_is_held_between_a_hundredth_and_ten)
    {
        Runner setup;
        setup.mInterpreter.call(OpSetScale, guard, true, { { 2.5, {}, false } });
        setup.mInterpreter.call(OpSetScale, guard, true, { { 0.0, {}, false } });
        setup.mInterpreter.call(OpSetScale, guard, true, { { 50.0, {}, false } });
        setup.mInterpreter.call(OpSetScale, guard, true, { { -3.0, {}, false } });
        setup.mInterpreter.call(OpSetScale, 0, false, { { 2.0, {}, false } });
        ASSERT_EQ(setup.mWorld.mScales.size(), 4u);
        EXPECT_EQ(setup.mWorld.mScales[0].second, 2.5);
        EXPECT_EQ(setup.mWorld.mScales[1].second, 0.01);
        EXPECT_EQ(setup.mWorld.mScales[2].second, 10.0);
        EXPECT_EQ(setup.mWorld.mScales[3].second, 0.01);
        EXPECT_EQ(setup.mWorld.mScales[0].first, guard);
    }

    TEST(FalloutScriptPlacementTest, a_reference_the_host_does_not_know_is_nowhere)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetPos, arguments(1).add(axis('X'))))
            + command(OpSetPos, axisAndValue('X', 5)) + end());
        builder.variable(1, floatVariable, "x");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_THAT(setup.mWorld.mSets, IsEmpty());
    }

    TEST(FalloutScriptPlacementTest, setting_one_axis_asks_for_that_axis_only)
    {
        Runner setup;
        Placement where;
        where.mPosition = { 1, 2, 3 };
        where.mAngle = { 0.25, 0.5, 0.75 };
        setup.mWorld.mPlaces[guard] = where;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpSetPos, axisAndValue('Y', 42.5))
            + command(OpSetAngle, axisAndValue('Z', 180)) + end());

        setup.run(builder);

        // The world keeps the rest as it finds it when it makes the move, so nothing else is asked for
        ASSERT_EQ(setup.mWorld.mSets.size(), 2u);
        const auto& first = setup.mWorld.mSets[0];
        EXPECT_EQ(first.mReference, guard);
        EXPECT_FALSE(first.mAngle);
        EXPECT_EQ(first.mAxis, 1);
        EXPECT_DOUBLE_EQ(first.mValue, 42.5);
        const auto& second = setup.mWorld.mSets[1];
        EXPECT_TRUE(second.mAngle);
        EXPECT_EQ(second.mAxis, 2);
        EXPECT_NEAR(second.mValue, pi, 1e-9);
    }

    TEST(FalloutScriptPlacementTest, an_axis_that_is_none_changes_nothing)
    {
        Runner setup;
        setup.mWorld.mPlaces[guard] = Placement();
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpSetPos, axisAndValue('Q', 5))
            + setTo(var('f', 1), callToken(OpGetAngle, arguments(1).add(axis('W')))) + end());
        builder.variable(1, floatVariable, "angle");

        setup.run(builder);

        EXPECT_THAT(setup.mWorld.mSets, IsEmpty());
        EXPECT_EQ(setup.variable(1), 0);
    }

    TEST(FalloutScriptPlacementTest, a_number_that_is_not_a_number_is_not_a_place)
    {
        Runner setup;
        setup.mWorld.mPlaces[guard] = Placement();
        ScriptBuilder builder;
        builder.code(
            scriptName() + begin(BlockType::OnActivate) + command(OpSetPos, axisAndValue('X', std::nan(""))) + end());

        setup.run(builder);

        ASSERT_EQ(setup.mWorld.mSets.size(), 1u);
        EXPECT_EQ(setup.mWorld.mSets[0].mValue, 0);
    }

    TEST(FalloutScriptPlacementTest, a_number_too_big_for_a_place_is_cut_to_the_largest_one)
    {
        Runner setup;
        setup.mWorld.mPlaces[guard] = Placement();
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpSetPos, axisAndValue('X', 1e300))
            + command(OpSetPos, axisAndValue('Y', -1e300)) + command(OpSetAngle, axisAndValue('Z', 1e300)) + end());

        setup.run(builder);

        ASSERT_EQ(setup.mWorld.mSets.size(), 3u);
        EXPECT_EQ(setup.mWorld.mSets[0].mValue, 1e9);
        EXPECT_EQ(setup.mWorld.mSets[1].mValue, -1e9);
        EXPECT_NEAR(setup.mWorld.mSets[2].mValue, 1e9 * pi / 180.0, 1.0);
    }
}
