#include <components/falloutscript/detection.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace
{
    using FalloutScript::Detection::Input;
    using FalloutScript::Detection::Setting;
    using FalloutScript::Detection::Sighting;

    constexpr double pi = 3.14159265358979323846;

    /// No setting in the files: the defaults of the executable
    const Setting defaults = [](std::string_view, double fallback) { return fallback; };

    Input watcher()
    {
        Input input;
        input.mPerception = 5;
        input.mLight = 50;
        return input;
    }

    TEST(FalloutScriptDetectionTest, a_target_next_to_the_watcher_in_the_light_is_seen)
    {
        // skill 50, near 1: -25 + 0 + 50 * 1.4 + 0.5 * 50
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(watcher(), defaults), 70.0);
    }

    TEST(FalloutScriptDetectionTest, a_target_beyond_the_distance_a_watcher_can_tell_is_left_with_the_base_value)
    {
        Input input = watcher();
        input.mDistance = 1500;
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, defaults), -25.0);
        input.mDistance = 9000;
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, defaults), -25.0);
    }

    TEST(FalloutScriptDetectionTest, the_distance_counts_squared_and_twice_as_far_outside)
    {
        Input input = watcher();
        input.mDistance = 750;
        // near 0.25: -25 + 0.25 * 70 + 0.5 * 50 * 0.25
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, defaults), -1.25);
        input.mExterior = true;
        // near (2250 / 3000)^2 = 0.5625
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, defaults), -25 + 0.5625 * 70 + 0.5 * 50 * 0.5625);
    }

    TEST(FalloutScriptDetectionTest, a_target_that_moves_is_heard_and_seen_better)
    {
        Input input = watcher();
        input.mDistance = 750;
        input.mMoving = true;
        // sound 0.25 * 1.6 * 14, seen 0.25 * 1.01 * 70
        EXPECT_NEAR(FalloutScript::Detection::stealth(input, defaults), -25 + 5.6 + 0.25 * 1.01 * 70 + 6.25, 1e-9);
        input.mRunning = true;
        // boots 14 * 2, movement 1 + 0.01 + 0.2
        EXPECT_NEAR(FalloutScript::Detection::stealth(input, defaults), -25 + 11.2 + 0.25 * 1.21 * 70 + 6.25, 1e-9);
        // The weight of the boots adds to the noise
        input.mRunning = false;
        input.mBootWeight = 10;
        EXPECT_NEAR(
            FalloutScript::Detection::stealth(input, defaults), -25 + 0.25 * 1.6 * 24 + 0.25 * 1.01 * 70 + 6.25, 1e-9);
    }

    TEST(FalloutScriptDetectionTest, no_line_of_sight_makes_the_sound_a_fourth)
    {
        Input input = watcher();
        input.mDistance = 750;
        input.mMoving = true;
        const double seen = FalloutScript::Detection::stealth(input, defaults);
        input.mLineOfSight = false;
        EXPECT_NEAR(FalloutScript::Detection::stealth(input, defaults), seen - 5.6 + 1.4, 1e-9);
    }

    TEST(FalloutScriptDetectionTest, a_sneaking_target_is_hidden_by_its_skill_and_a_cloak_hides_its_light)
    {
        Input input = watcher();
        input.mDistance = 750;
        input.mSneaking = true;
        input.mSneakSkill = 100;
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, defaults), -25 + 17.5 + 0.5 * (12.5 - 100));
        input.mSneaking = false;
        input.mCloaked = true;
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, defaults), -25 + 6.25);
    }

    TEST(FalloutScriptDetectionTest, what_the_watcher_is_doing_changes_how_well_it_perceives)
    {
        Input input = watcher();
        input.mDistance = 750;
        const auto at = [&](bool alert, bool asleep, bool combat, int ambush) {
            input.mAlert = alert;
            input.mAsleep = asleep;
            input.mInCombat = combat;
            input.mAmbush = ambush;
            return FalloutScript::Detection::stealth(input, defaults);
        };
        const auto skillPart = [](double factor) { return -25 + 17.5 + 0.5 * 50 * factor * 0.25; };
        EXPECT_DOUBLE_EQ(at(false, false, false, 0), skillPart(1.0));
        EXPECT_DOUBLE_EQ(at(true, false, false, 0), skillPart(1.2));
        EXPECT_DOUBLE_EQ(at(false, true, false, 0), skillPart(0.8));
        EXPECT_DOUBLE_EQ(at(false, false, true, 0), skillPart(0.6));
        EXPECT_DOUBLE_EQ(at(false, false, false, 1), skillPart(3.5));
        EXPECT_DOUBLE_EQ(at(false, false, false, -1), skillPart(0.75));
    }

    TEST(FalloutScriptDetectionTest, a_perception_of_ten_perceives_nine_times_what_a_perception_of_zero_does)
    {
        Input input = watcher();
        input.mDistance = 0;
        input.mPerception = 0;
        const double none = FalloutScript::Detection::stealth(input, defaults);
        input.mPerception = 10;
        const double most = FalloutScript::Detection::stealth(input, defaults);
        EXPECT_DOUBLE_EQ(most - none, 0.5 * (90 - 10));
    }

    TEST(FalloutScriptDetectionTest, the_files_of_the_game_set_the_values)
    {
        const std::map<std::string, double> files{ { "fSneakBaseValue", -35 }, { "fSneakMaxDistance", 2500 } };
        const Setting setting = [&](std::string_view name, double fallback) {
            const auto found = files.find(std::string(name));
            return found == files.end() ? fallback : found->second;
        };
        Input input = watcher();
        input.mDistance = 2500;
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, setting), -35.0);
        input.mDistance = 0;
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::stealth(input, setting), -35 + 70 + 25);
    }

    TEST(FalloutScriptDetectionTest, a_stealth_value_is_seen_noticed_or_unseen_by_the_limits)
    {
        using FalloutScript::Detection::sighting;
        EXPECT_EQ(sighting(0.0, defaults), Sighting::Seen);
        EXPECT_EQ(sighting(50.0, defaults), Sighting::Seen);
        EXPECT_EQ(sighting(-0.5, defaults), Sighting::Noticed);
        EXPECT_EQ(sighting(-20.0, defaults), Sighting::Noticed);
        EXPECT_EQ(sighting(-20.5, defaults), Sighting::Unseen);
    }

    TEST(FalloutScriptDetectionTest, the_cone_of_view_is_the_setting_across_in_front_of_the_watcher)
    {
        using FalloutScript::Detection::inViewCone;
        // The watcher faces the y axis, and a cone of 190 degrees reaches 95 degrees to each side
        EXPECT_TRUE(inViewCone(0, 0, 100, 190));
        EXPECT_TRUE(inViewCone(0, 100, 0, 190));
        EXPECT_TRUE(inViewCone(0, -100, 0, 190));
        EXPECT_TRUE(inViewCone(0, 100, -8, 190));
        EXPECT_FALSE(inViewCone(0, 100, -10, 190));
        EXPECT_FALSE(inViewCone(0, 0, -100, 190));
        // Turned round, what was behind is in front
        EXPECT_TRUE(inViewCone(pi, 0, -100, 190));
        EXPECT_FALSE(inViewCone(pi, 0, 100, 190));
        // On the same spot it is seen
        EXPECT_TRUE(inViewCone(0, 0, 0, 190));
    }

    TEST(FalloutScriptDetectionTest, the_check_comes_every_setting_seconds)
    {
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::checkInterval(defaults), 0.3);
        EXPECT_DOUBLE_EQ(FalloutScript::Detection::checkInterval([](std::string_view, double) { return 0.0; }), 0.05);
    }
}
