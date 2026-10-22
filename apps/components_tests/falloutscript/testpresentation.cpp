#include "scriptbuilder.hpp"

#include <components/falloutscript/presentation.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;

    constexpr FormId shout = 0x00100001;
    constexpr FormId flash = 0x00100002;
    constexpr FormId blur = 0x00100003;
    constexpr FormId guard = 0x00100004;
    constexpr FormId clerk = 0x00100005;

    constexpr std::uint16_t OpPlaySound = 0x1026;
    constexpr std::uint16_t OpLook = 0x105C;
    constexpr std::uint16_t OpStopLook = 0x105D;
    constexpr std::uint16_t OpPlaySound3D = 0x10B2;
    constexpr std::uint16_t OpAddAchievement = 0x114A;
    constexpr std::uint16_t OpApplyImageSpaceModifier = 0x1191;
    constexpr std::uint16_t OpRemoveImageSpaceModifier = 0x1192;
    constexpr std::uint16_t OpAbs = 0x1221;
    constexpr std::uint16_t OpIsImageSpaceActive = 0x1232;

    class FakeWorld : public FalloutScript::PresentationWorld
    {
    public:
        std::vector<std::pair<FormId, FormId>> mPlayed;
        std::set<FormId> mPlaying;
        std::map<FormId, double> mDurations{ { flash, 2.0 }, { blur, 0.0 } };
        std::vector<int> mAchievements;
        bool mMusic = false;

        void playSound(FormId sound, FormId reference) override { mPlayed.emplace_back(sound, reference); }
        bool isSoundPlaying(FormId sound, FormId) override { return mPlaying.count(sound) != 0; }
        bool isMusicPlaying() override { return mMusic; }
        double imageSpaceDuration(FormId modifier) override
        {
            const auto found = mDurations.find(modifier);
            return found == mDurations.end() ? 0.0 : found->second;
        }
        void achievement(int id) override { mAchievements.push_back(id); }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeWorld mWorld;
        FalloutScript::Presentation mPresentation;

        Runner() { FalloutScript::addPresentationCommands(mInterpreter, mPresentation, mWorld); }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    TEST(FalloutScriptPresentationTest, a_sound_plays_from_nowhere_or_from_the_reference_it_is_called_on)
    {
        Runner run;
        run.call(OpPlaySound, 0, { shout });
        run.call(OpPlaySound3D, guard, { shout });
        run.call(OpPlaySound3D, 0, { shout });
        run.call(OpPlaySound, 0, { 0 });
        EXPECT_THAT(run.mWorld.mPlayed,
            ElementsAre(
                std::make_pair(shout, FormId(0)), std::make_pair(shout, guard), std::make_pair(shout, FormId(0))));
    }

    TEST(FalloutScriptPresentationTest, an_image_space_modifier_is_active_for_its_duration)
    {
        Runner run;
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { flash }), 0.0);
        run.call(OpApplyImageSpaceModifier, 0, { flash });
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { flash }), 1.0);
        run.mPresentation.update(1.5);
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { flash }), 1.0);
        run.mPresentation.update(1.0);
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { flash }), 0.0);
    }

    TEST(FalloutScriptPresentationTest, a_modifier_without_a_duration_lasts_until_it_is_removed)
    {
        Runner run;
        run.call(OpApplyImageSpaceModifier, 0, { blur });
        run.mPresentation.update(1000);
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { blur }), 1.0);
        run.call(OpRemoveImageSpaceModifier, 0, { blur });
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { blur }), 0.0);
        // A modifier that was never applied can be removed
        run.call(OpRemoveImageSpaceModifier, 0, { flash });
    }

    TEST(FalloutScriptPresentationTest, applying_a_modifier_again_starts_it_again)
    {
        Runner run;
        run.call(OpApplyImageSpaceModifier, 0, { flash });
        run.mPresentation.update(1.9);
        run.call(OpApplyImageSpaceModifier, 0, { flash });
        run.mPresentation.update(1.9);
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { flash }), 1.0);
    }

    TEST(FalloutScriptPresentationTest, a_character_looks_at_what_it_was_told_until_told_to_stop)
    {
        Runner run;
        run.call(OpLook, guard, { clerk });
        EXPECT_EQ(run.mPresentation.lookingAt(guard), clerk);
        EXPECT_EQ(run.mPresentation.lookingAt(clerk), 0u);
        run.call(OpStopLook, guard);
        EXPECT_EQ(run.mPresentation.lookingAt(guard), 0u);
        // Without a target or a reference nothing is looked at
        run.call(OpLook, guard, {});
        run.call(OpLook, 0, { clerk });
        EXPECT_EQ(run.mPresentation.lookingAt(guard), 0u);
    }

    TEST(FalloutScriptPresentationTest, achievements_are_handed_to_the_host_and_abs_drops_the_sign)
    {
        Runner run;
        run.call(OpAddAchievement, 0, { 12 });
        EXPECT_THAT(run.mWorld.mAchievements, ElementsAre(12));
        EXPECT_EQ(run.call(OpAbs, 0, { -3.5 }), 3.5);
        EXPECT_EQ(run.call(OpAbs, 0, { 2 }), 2.0);
        EXPECT_EQ(run.call(OpAbs, 0, {}), 0.0);
    }

    TEST(FalloutScriptPresentationTest, clearing_forgets_the_modifiers_and_the_looks)
    {
        Runner run;
        run.call(OpApplyImageSpaceModifier, 0, { blur });
        run.call(OpLook, guard, { clerk });
        run.mPresentation.clear();
        EXPECT_EQ(run.call(OpIsImageSpaceActive, 0, { blur }), 0.0);
        EXPECT_EQ(run.mPresentation.lookingAt(guard), 0u);
    }
}
