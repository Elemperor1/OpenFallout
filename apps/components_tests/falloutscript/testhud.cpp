#include <components/falloutscript/hud.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
    namespace Hud = FalloutScript::Hud;
    using FalloutScript::Detection::Sighting;

    TEST(FalloutScriptHudTest, a_message_without_a_time_stays_as_long_as_its_text_needs)
    {
        EXPECT_FLOAT_EQ(Hud::secondsFor("Hi"), Hud::messageSeconds);
        EXPECT_FLOAT_EQ(Hud::secondsFor(std::string(60, 'x')), 4.f);
        EXPECT_FLOAT_EQ(Hud::secondsFor(std::string(1000, 'x')), 8.f);
    }

    TEST(FalloutScriptHudTest, the_message_line_hands_over_what_was_said_in_order_once)
    {
        Hud::MessageLine line;
        line.push("first");
        line.push("second", "icon.dds", 5.f);
        const std::vector<Hud::Message> taken = line.take();
        ASSERT_EQ(taken.size(), 2u);
        EXPECT_EQ(taken[0].mText, "first");
        EXPECT_FLOAT_EQ(taken[0].mSeconds, Hud::messageSeconds);
        EXPECT_EQ(taken[1].mIcon, "icon.dds");
        EXPECT_FLOAT_EQ(taken[1].mSeconds, 5.f);
        EXPECT_TRUE(line.take().empty());
    }

    TEST(FalloutScriptHudTest, the_message_line_keeps_the_newest_and_ignores_empty_text)
    {
        Hud::MessageLine line;
        line.push("");
        EXPECT_EQ(line.size(), 0u);
        for (int i = 0; i < 20; ++i)
            line.push("m" + std::to_string(i));
        const std::vector<Hud::Message> taken = line.take();
        ASSERT_EQ(taken.size(), Hud::MessageLine::capacity);
        EXPECT_EQ(taken.front().mText, "m12");
        EXPECT_EQ(taken.back().mText, "m19");
    }

    TEST(FalloutScriptHudTest, nobody_around_leaves_the_player_hidden)
    {
        Hud::Watchers watchers;
        EXPECT_EQ(watchers.status(10), Hud::Sneak::Hidden);
        watchers.note(1, Sighting::Unseen, false, 10);
        EXPECT_EQ(watchers.status(10), Hud::Sneak::Hidden);
    }

    TEST(FalloutScriptHudTest, the_strongest_look_decides_the_word)
    {
        Hud::Watchers watchers;
        watchers.note(1, Sighting::Noticed, false, 10);
        EXPECT_EQ(watchers.status(10), Hud::Sneak::Caution);
        watchers.note(2, Sighting::Seen, false, 10);
        EXPECT_EQ(watchers.status(10), Hud::Sneak::Detected);
        watchers.note(3, Sighting::Seen, true, 10);
        EXPECT_EQ(watchers.status(10), Hud::Sneak::Danger);
        EXPECT_EQ(watchers.seen(10), 2);
        watchers.forget(3);
        EXPECT_EQ(watchers.status(10), Hud::Sneak::Detected);
    }

    TEST(FalloutScriptHudTest, an_old_look_does_not_count_and_a_newer_one_replaces_it)
    {
        Hud::Watchers watchers;
        watchers.note(1, Sighting::Seen, true, 10);
        EXPECT_EQ(watchers.status(12), Hud::Sneak::Danger);
        EXPECT_EQ(watchers.status(14), Hud::Sneak::Hidden);
        EXPECT_EQ(watchers.seen(14), 0);
        watchers.note(1, Sighting::Seen, true, 10);
        watchers.note(1, Sighting::Unseen, true, 11);
        EXPECT_EQ(watchers.status(11), Hud::Sneak::Hidden);
    }

    TEST(FalloutScriptHudTest, the_bearing_grows_clockwise_from_the_y_axis)
    {
        EXPECT_DOUBLE_EQ(Hud::bearing(0, 5), 0.0);
        EXPECT_DOUBLE_EQ(Hud::bearing(5, 0), 90.0);
        EXPECT_DOUBLE_EQ(Hud::bearing(0, -5), 180.0);
        EXPECT_DOUBLE_EQ(Hud::bearing(-5, 0), 270.0);
        EXPECT_NEAR(Hud::bearing(-1, 1), 315.0, 1e-9);
    }

    TEST(FalloutScriptHudTest, the_compass_puts_a_thing_ahead_in_the_middle_and_the_rest_to_the_sides)
    {
        EXPECT_DOUBLE_EQ(*Hud::compassShare(90, 90, 120), 0.5);
        EXPECT_DOUBLE_EQ(*Hud::compassShare(90, 120, 120), 0.75);
        EXPECT_DOUBLE_EQ(*Hud::compassShare(90, 60, 120), 0.25);
        // The edge is in the window, past it is not
        EXPECT_DOUBLE_EQ(*Hud::compassShare(0, 60, 120), 1.0);
        EXPECT_FALSE(Hud::compassShare(0, 61, 120).has_value());
        EXPECT_FALSE(Hud::compassShare(0, 180, 120).has_value());
    }

    TEST(FalloutScriptHudTest, the_compass_turns_the_short_way_round_north)
    {
        EXPECT_DOUBLE_EQ(*Hud::compassShare(350, 10, 120), 0.5 + 20.0 / 120.0);
        EXPECT_DOUBLE_EQ(*Hud::compassShare(10, 350, 120), 0.5 - 20.0 / 120.0);
        EXPECT_FALSE(Hud::compassShare(0, 0, 0).has_value());
    }

    TEST(FalloutScriptHudTest, the_experience_bar_fills_between_the_two_levels)
    {
        EXPECT_DOUBLE_EQ(Hud::experienceShare(200, 200, 550), 0.0);
        EXPECT_DOUBLE_EQ(Hud::experienceShare(375, 200, 550), 0.5);
        EXPECT_DOUBLE_EQ(Hud::experienceShare(550, 200, 550), 1.0);
        EXPECT_DOUBLE_EQ(Hud::experienceShare(100, 200, 550), 0.0);
        // At the highest level there is no next one
        EXPECT_DOUBLE_EQ(Hud::experienceShare(30000, 29000, 29000), 1.0);
    }

    TEST(FalloutScriptHudTest, every_part_of_the_body_with_an_icon_has_its_texture_name)
    {
        EXPECT_EQ(Hud::limbIcon(0), "torso");
        EXPECT_EQ(Hud::limbIcon(1), "head");
        EXPECT_EQ(Hud::limbIcon(2), "head");
        EXPECT_EQ(Hud::limbIcon(4), "left_arm");
        EXPECT_EQ(Hud::limbIcon(6), "right_arm");
        EXPECT_EQ(Hud::limbIcon(9), "left_leg");
        EXPECT_EQ(Hud::limbIcon(12), "right_leg");
        EXPECT_EQ(Hud::limbIcon(13), "face_00");
        EXPECT_FALSE(Hud::limbIcon(14).has_value());
        EXPECT_FALSE(Hud::limbIcon(-1).has_value());
    }
}
