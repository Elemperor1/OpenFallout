#include <components/falloutscript/worldrules.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{
    namespace World = FalloutScript::World;
    using World::SightFacts;
    using World::SkyView;

    constexpr std::uint8_t pleasant = 0x01;
    constexpr std::uint8_t cloudy = 0x02;
    constexpr std::uint8_t rainy = 0x04;
    constexpr std::uint8_t snow = 0x08;

    SkyView::Weather weather(FalloutScript::FormId form, std::uint8_t classification, std::uint8_t begin = 0,
        std::uint8_t end = 0, std::uint8_t wind = 0)
    {
        SkyView::Weather result;
        result.mPresent = true;
        result.mForm = form;
        result.mClassification = classification;
        result.mPrecipitationBeginFadeIn = begin;
        result.mPrecipitationEndFadeOut = end;
        result.mWindSpeed = wind;
        return result;
    }

    // ---- Form types

    TEST(FalloutScriptWorldRulesTest, the_form_types_are_the_indexes_of_the_table_of_the_executables)
    {
        EXPECT_EQ(World::formTypeSignature(0), "NONE");
        EXPECT_EQ(World::formTypeSignature(0x15), "ACTI");
        EXPECT_EQ(World::formTypeSignature(0x1C), "DOOR");
        EXPECT_EQ(World::formTypeSignature(0x2A), "NPC_");
        EXPECT_EQ(World::formTypeSignature(0x28), "WEAP");
        EXPECT_EQ(World::formTypeSignature(0x78), "SLPD");
    }

    TEST(FalloutScriptWorldRulesTest, a_form_type_number_outside_the_table_has_no_signature)
    {
        EXPECT_TRUE(World::formTypeSignature(-1).empty());
        EXPECT_TRUE(World::formTypeSignature(0x79).empty());
    }

    TEST(FalloutScriptWorldRulesTest, a_signature_is_found_in_any_case)
    {
        EXPECT_EQ(World::formTypeNumber("DOOR"), 0x1C);
        EXPECT_EQ(World::formTypeNumber("door"), 0x1C);
        EXPECT_EQ(World::formTypeNumber("Npc_"), 0x2A);
        EXPECT_EQ(World::formTypeNumber("XXXX"), -1);
        EXPECT_EQ(World::formTypeNumber(""), -1);
    }

    TEST(FalloutScriptWorldRulesTest, the_signature_of_a_record_type_is_its_four_letters)
    {
        const std::uint32_t door = 'D' | ('O' << 8) | ('O' << 16) | (static_cast<std::uint32_t>('R') << 24);
        EXPECT_EQ(World::signatureOfRecordType(door), "DOOR");
    }

    TEST(FalloutScriptWorldRulesTest, the_mark_the_engine_puts_on_the_number_of_a_record_is_not_a_letter)
    {
        const std::uint32_t door = 'D' | ('O' << 8) | ('O' << 16) | (static_cast<std::uint32_t>('R') << 24);
        EXPECT_EQ(World::signatureOfRecordType(door | 0x00800000), "DOOR");
        EXPECT_EQ(World::formTypeNumber(World::signatureOfRecordType(door | 0x00800000)), 0x1C);
    }

    // ---- Cells

    TEST(FalloutScriptWorldRulesTest, a_cell_name_is_compared_by_prefix_without_regard_to_case)
    {
        EXPECT_TRUE(World::cellNameMatches("Goodsprings", "goodsprings"));
        EXPECT_TRUE(World::cellNameMatches("Goodsprings Saloon", "Goodsprings"));
        EXPECT_FALSE(World::cellNameMatches("Goodsprings", "Goodsprings Saloon"));
        EXPECT_FALSE(World::cellNameMatches("Novac", "Goodsprings"));
        EXPECT_TRUE(World::cellNameMatches("Anything", ""));
    }

    // ---- The sky

    TEST(FalloutScriptWorldRulesTest, the_weather_that_is_shown_is_the_current_one)
    {
        SkyView sky;
        sky.mCurrent = weather(0x100, pleasant);
        sky.mOutgoing = weather(0x200, rainy);
        EXPECT_TRUE(World::isCurrentWeather(sky, 0x100));
        EXPECT_FALSE(World::isCurrentWeather(sky, 0x200));
        EXPECT_FALSE(World::isCurrentWeather(sky, 0));
        EXPECT_FALSE(World::isCurrentWeather(SkyView(), 0x100));
    }

    TEST(FalloutScriptWorldRulesTest, the_percent_of_the_weather_is_one_when_no_change_goes_on_and_zero_without_a_weather)
    {
        SkyView sky;
        EXPECT_EQ(World::weatherPercent(sky), 0.0);
        sky.mCurrent = weather(1, pleasant);
        EXPECT_EQ(World::weatherPercent(sky), 1.0);
        sky.mPercent = 0.25;
        EXPECT_EQ(World::weatherPercent(sky), 0.25);
    }

    TEST(FalloutScriptWorldRulesTest, the_share_of_cloud_follows_the_change_from_one_weather_to_the_other)
    {
        SkyView sky;
        sky.mOutgoing = weather(1, pleasant);
        sky.mCurrent = weather(2, cloudy);
        sky.mPercent = 0.25;
        EXPECT_DOUBLE_EQ(World::classificationShare(sky, cloudy), 0.25);
        EXPECT_DOUBLE_EQ(World::classificationShare(sky, pleasant), 0.75);
        // A weather that has both bits counts on both sides of a change
        sky.mOutgoing = weather(1, cloudy | pleasant);
        EXPECT_DOUBLE_EQ(World::classificationShare(sky, cloudy), 1.0);
    }

    TEST(FalloutScriptWorldRulesTest, snow_falls_when_the_change_has_passed_where_the_precipitation_begins)
    {
        SkyView sky;
        sky.mOutgoing = weather(1, pleasant);
        sky.mCurrent = weather(2, snow, 128);
        sky.mPercent = 0.4;
        EXPECT_FALSE(World::isSnowing(sky));
        sky.mPercent = 0.6;
        EXPECT_TRUE(World::isSnowing(sky));
        sky.mOutgoing = SkyView::Weather();
        sky.mPercent = 1.0;
        EXPECT_TRUE(World::isSnowing(sky));
    }

    TEST(FalloutScriptWorldRulesTest, snow_of_the_old_weather_goes_on_until_the_change_gets_to_where_it_ends)
    {
        SkyView sky;
        sky.mOutgoing = weather(1, snow, 0, 128);
        sky.mCurrent = weather(2, pleasant);
        sky.mPercent = 0.3;
        EXPECT_TRUE(World::isSnowing(sky));
        sky.mPercent = 0.7;
        EXPECT_FALSE(World::isSnowing(sky));
    }

    TEST(FalloutScriptWorldRulesTest, snow_is_not_rain)
    {
        SkyView sky;
        sky.mCurrent = weather(1, rainy);
        EXPECT_FALSE(World::isSnowing(sky));
        EXPECT_FALSE(World::isSnowing(SkyView()));
    }

    TEST(FalloutScriptWorldRulesTest, the_wind_is_the_byte_of_the_record_over_255_blended_by_the_change)
    {
        SkyView sky;
        EXPECT_EQ(World::windSpeed(sky), 0.0);
        sky.mCurrent = weather(1, pleasant, 0, 0, 255);
        EXPECT_DOUBLE_EQ(World::windSpeed(sky), 1.0);
        sky.mOutgoing = weather(2, pleasant, 0, 0, 0);
        sky.mPercent = 0.5;
        EXPECT_DOUBLE_EQ(World::windSpeed(sky), 0.5);
    }

    // ---- Line of sight

    SightFacts facts()
    {
        SightFacts result;
        result.mTargetLoaded = true;
        result.mSameSpace = true;
        result.mDistance = 500;
        result.mEye = { 0, 0, 110 };
        result.mFacing = 0;
        result.mTarget = { 0, 500, 0 };
        result.mTargetHeight = 120;
        return result;
    }

    const World::SightRay clear = [](const World::Vec3&, const World::Vec3&) { return true; };
    const World::SightRay blocked = [](const World::Vec3&, const World::Vec3&) { return false; };

    TEST(FalloutScriptWorldRulesTest, a_character_sees_what_is_in_front_of_it_with_nothing_between)
    {
        EXPECT_TRUE(World::seesTarget(facts(), clear));
        EXPECT_FALSE(World::seesTarget(facts(), blocked));
    }

    TEST(FalloutScriptWorldRulesTest, a_character_does_not_see_what_is_behind_it)
    {
        SightFacts behind = facts();
        behind.mTarget = { 0, -500, 0 };
        EXPECT_FALSE(World::seesTarget(behind, clear));
        // The edge of the cone is 95 degrees to each side
        SightFacts side = facts();
        side.mTarget = { 500 * std::sin(94.0 * M_PI / 180), 500 * std::cos(94.0 * M_PI / 180), 0 };
        EXPECT_TRUE(World::seesTarget(side, clear));
        side.mTarget = { 500 * std::sin(96.0 * M_PI / 180), 500 * std::cos(96.0 * M_PI / 180), 0 };
        EXPECT_FALSE(World::seesTarget(side, clear));
    }

    TEST(FalloutScriptWorldRulesTest, the_facing_of_a_character_turns_the_cone)
    {
        SightFacts turned = facts();
        turned.mFacing = M_PI;
        EXPECT_FALSE(World::seesTarget(turned, clear));
        turned.mTarget = { 0, -500, 0 };
        EXPECT_TRUE(World::seesTarget(turned, clear));
    }

    TEST(FalloutScriptWorldRulesTest, a_dead_or_unloaded_target_is_not_seen_by_a_character)
    {
        SightFacts dead = facts();
        dead.mTargetDead = true;
        EXPECT_FALSE(World::seesTarget(dead, clear));
        SightFacts unloaded = facts();
        unloaded.mTargetLoaded = false;
        EXPECT_FALSE(World::seesTarget(unloaded, clear));
    }

    TEST(FalloutScriptWorldRulesTest, a_target_within_two_units_is_seen_whatever_lies_between)
    {
        SightFacts closeBy = facts();
        closeBy.mDistance = 2.0;
        closeBy.mTarget = { 0, -2, 0 };
        EXPECT_TRUE(World::seesTarget(closeBy, blocked));
        closeBy.mSameSpace = false;
        EXPECT_FALSE(World::seesTarget(closeBy, blocked));
    }

    TEST(FalloutScriptWorldRulesTest, a_target_in_another_space_is_not_seen)
    {
        SightFacts other = facts();
        other.mSameSpace = false;
        EXPECT_FALSE(World::seesTarget(other, clear));
    }

    TEST(FalloutScriptWorldRulesTest, with_everything_visible_the_cone_is_enough)
    {
        SightFacts all = facts();
        all.mAllVisible = true;
        EXPECT_TRUE(World::seesTarget(all, blocked));
        all.mTarget = { 0, -500, 0 };
        EXPECT_FALSE(World::seesTarget(all, blocked));
    }

    TEST(FalloutScriptWorldRulesTest, a_character_casts_one_ray_unless_the_target_is_the_player_or_its_own_target)
    {
        std::vector<double> heights;
        const World::SightRay record = [&heights](const World::Vec3&, const World::Vec3& to) {
            heights.push_back(to[2]);
            return false;
        };
        EXPECT_FALSE(World::seesTarget(facts(), record));
        ASSERT_EQ(heights.size(), 1u);
        EXPECT_DOUBLE_EQ(heights[0], 90.0);

        heights.clear();
        SightFacts player = facts();
        player.mTargetIsPlayer = true;
        EXPECT_FALSE(World::seesTarget(player, record));
        ASSERT_EQ(heights.size(), 3u);
        EXPECT_DOUBLE_EQ(heights[0], 90.0);
        EXPECT_DOUBLE_EQ(heights[1], 60.0);
        EXPECT_DOUBLE_EQ(heights[2], 30.0);

        heights.clear();
        SightFacts fought = facts();
        fought.mTargetIsViewerTarget = true;
        EXPECT_FALSE(World::seesTarget(fought, record));
        EXPECT_EQ(heights.size(), 3u);
    }

    TEST(FalloutScriptWorldRulesTest, a_later_ray_that_reaches_the_target_is_enough)
    {
        SightFacts player = facts();
        player.mTargetIsPlayer = true;
        const World::SightRay lowOnly = [](const World::Vec3&, const World::Vec3& to) { return to[2] < 40; };
        EXPECT_TRUE(World::seesTarget(player, lowOnly));
        const World::SightRay one = [](const World::Vec3&, const World::Vec3& to) { return to[2] < 40; };
        EXPECT_FALSE(World::seesTarget(facts(), one));
    }

    TEST(FalloutScriptWorldRulesTest, the_player_sees_only_what_the_camera_shows)
    {
        SightFacts player = facts();
        player.mViewerIsPlayer = true;
        EXPECT_FALSE(World::seesTarget(player, clear));
        player.mInFrustum = true;
        EXPECT_TRUE(World::seesTarget(player, clear));
        EXPECT_FALSE(World::seesTarget(player, blocked));
    }

    TEST(FalloutScriptWorldRulesTest, the_player_sees_a_near_target_in_view_whatever_lies_between)
    {
        SightFacts player = facts();
        player.mViewerIsPlayer = true;
        player.mInFrustum = true;
        player.mDistance = 1.5;
        EXPECT_TRUE(World::seesTarget(player, blocked));
        player.mInFrustum = false;
        EXPECT_FALSE(World::seesTarget(player, blocked));
    }

    // ---- The player's actions

    TEST(FalloutScriptWorldRulesTest, an_action_is_active_until_its_seconds_have_gone)
    {
        World::PlayerActions actions;
        EXPECT_EQ(actions.last(), 0);
        actions.raise(3, 2.0, 0x77);
        EXPECT_TRUE(actions.active(3));
        EXPECT_FALSE(actions.active(4));
        actions.update(1.0);
        EXPECT_TRUE(actions.active(3));
        actions.update(1.5);
        EXPECT_FALSE(actions.active(3));
        EXPECT_EQ(actions.last(), 0);
    }

    TEST(FalloutScriptWorldRulesTest, the_newest_action_is_the_last_one_and_a_raised_action_replaces_its_old_record)
    {
        World::PlayerActions actions;
        actions.raise(3, 10);
        actions.raise(4, 10);
        EXPECT_EQ(actions.last(), 4);
        actions.raise(3, 10);
        EXPECT_EQ(actions.last(), 3);
        actions.remove(3);
        EXPECT_EQ(actions.last(), 4);
        actions.clear();
        EXPECT_EQ(actions.last(), 0);
    }

    TEST(FalloutScriptWorldRulesTest, two_actions_stay_until_they_are_removed)
    {
        World::PlayerActions actions;
        actions.raise(5, 1);
        actions.raise(8, 1);
        actions.raise(6, 1);
        actions.update(100);
        EXPECT_TRUE(actions.active(5));
        EXPECT_TRUE(actions.active(8));
        EXPECT_FALSE(actions.active(6));
        EXPECT_TRUE(World::PlayerActions::persistent(5));
        EXPECT_FALSE(World::PlayerActions::persistent(6));
    }

    // ---- Splatters

    FalloutScript::World::Unit half()
    {
        return [] { return 0.5; };
    }

    TEST(FalloutScriptWorldRulesTest, a_custom_splatter_makes_a_group_of_quads_with_the_multipliers)
    {
        World::ScreenSplatter splatter;
        ASSERT_TRUE(splatter.triggerCustom(4, 2.0, 2.0, 0.5, half()));
        ASSERT_EQ(splatter.groups().size(), 1u);
        const World::ScreenSplatter::Group group = splatter.groups()[0];
        EXPECT_EQ(group.mQuads.size(), 4u);
        EXPECT_DOUBLE_EQ(group.mDuration, 2.0);
        // Half way between the smallest and the largest, times 2
        EXPECT_DOUBLE_EQ(group.mQuads[0].mHalfSize, (0.025 + 0.5 * (0.125 - 0.025)) * 2.0);
        EXPECT_DOUBLE_EQ(group.mQuads[0].mX, 0.0);
        EXPECT_EQ(splatter.quadCount(), 4);
    }

    TEST(FalloutScriptWorldRulesTest, a_splatter_of_no_count_or_with_the_setting_off_makes_nothing)
    {
        World::ScreenSplatter splatter;
        EXPECT_FALSE(splatter.triggerCustom(0, 1, 1, 1, half()));
        EXPECT_FALSE(splatter.triggerCustom(-3, 1, 1, 1, half()));
        World::SplatterSettings off;
        off.mCustomEnabled = false;
        off.mBloodEnabled = false;
        splatter.setSettings(off);
        EXPECT_FALSE(splatter.triggerCustom(2, 1, 1, 1, half()));
        EXPECT_EQ(splatter.triggerBlood(2, half()), 0);
        EXPECT_EQ(splatter.quadCount(), 0);
    }

    TEST(FalloutScriptWorldRulesTest, a_splatter_with_no_duration_lasts_as_long_as_the_setting_says)
    {
        World::ScreenSplatter splatter;
        splatter.triggerCustom(1, 0, 1, 1, half());
        EXPECT_DOUBLE_EQ(splatter.groups()[0].mDuration, 5.0);
    }

    TEST(FalloutScriptWorldRulesTest, the_blood_stays_within_the_maximum_count)
    {
        World::ScreenSplatter splatter;
        EXPECT_EQ(splatter.triggerBlood(6, half()), 6);
        EXPECT_EQ(splatter.triggerBlood(6, half()), 4);
        EXPECT_EQ(splatter.triggerBlood(6, half()), 0);
        EXPECT_EQ(splatter.quadCount(), 10);
        // The custom splatters are not counted against it
        EXPECT_TRUE(splatter.triggerCustom(3, 1, 1, 1, half()));
        EXPECT_EQ(splatter.quadCount(), 13);
    }

    TEST(FalloutScriptWorldRulesTest, a_group_goes_when_its_time_is_over)
    {
        World::ScreenSplatter splatter;
        splatter.triggerCustom(2, 1.0, 1, 1, half());
        splatter.update(0.5);
        EXPECT_EQ(splatter.groups().size(), 1u);
        splatter.update(0.6);
        EXPECT_TRUE(splatter.groups().empty());
    }

    TEST(FalloutScriptWorldRulesTest, a_group_fades_over_the_end_of_its_time)
    {
        World::ScreenSplatter splatter;
        splatter.setFade(0.5, 1);
        splatter.triggerCustom(1, 2.0, 1, 1, half());
        splatter.update(1.0);
        EXPECT_DOUBLE_EQ(splatter.groups()[0].mAlpha, 1.0);
        splatter.update(0.75);
        // 1.5 s from the start, 0.5 s from the end of 2: the fade began at 1.5
        splatter.update(0.0);
        EXPECT_NEAR(splatter.groups()[0].mAlpha, 1.0 - (1.75 - 1.5) / 0.5, 1e-9);
    }

    TEST(FalloutScriptWorldRulesTest, with_mode_zero_the_number_is_a_threshold_and_the_ramp_starts_at_the_setting)
    {
        World::ScreenSplatter splatter;
        splatter.setFade(0.5, 0);
        splatter.triggerCustom(1, 10.0, 1, 1, half());
        splatter.update(4.0);
        EXPECT_DOUBLE_EQ(splatter.groups()[0].mAlpha, 1.0);
        splatter.update(2.0);
        // 6 s of 10: past the 0.5 threshold, ramping from 0.8 of the time (8 s) to the end
        EXPECT_NEAR(splatter.groups()[0].mAlpha, 1.0 - (6.0 - 8.0) / 2.0, 1e-9);
    }

    TEST(FalloutScriptWorldRulesTest, the_textures_are_set_by_slot_and_a_clear_gives_them_back)
    {
        World::ScreenSplatter splatter;
        splatter.setDefaultTextures({ "a.dds", "b.dds", "c.dds" });
        EXPECT_TRUE(splatter.setTexture(1, "x.dds"));
        EXPECT_EQ(splatter.texture(1), "x.dds");
        EXPECT_FALSE(splatter.setTexture(3, "y.dds"));
        EXPECT_FALSE(splatter.setTexture(-1, "y.dds"));
        EXPECT_EQ(splatter.texture(3), "");
        splatter.setFade(2.0, 1);
        splatter.triggerCustom(1, 1, 1, 1, half());
        splatter.clear();
        EXPECT_EQ(splatter.texture(1), "b.dds");
        EXPECT_TRUE(splatter.groups().empty());
        EXPECT_EQ(splatter.fade(), 0.0);
        EXPECT_EQ(splatter.fadeMode(), 0);
    }

    TEST(FalloutScriptWorldRulesTest, a_reset_gives_back_the_fade_of_the_settings)
    {
        World::ScreenSplatter splatter;
        splatter.setFade(3.0, 1);
        splatter.reset();
        EXPECT_DOUBLE_EQ(splatter.fade(), 0.8);
        EXPECT_EQ(splatter.fadeMode(), 0);
        EXPECT_DOUBLE_EQ(splatter.settings().mDuration, 5.0);
    }

    // ---- The shudder

    TEST(FalloutScriptWorldRulesTest, the_shudder_swings_and_dies_away)
    {
        World::HudShudder shudder;
        EXPECT_FALSE(shudder.active());
        shudder.start(500, 0.1, 0.05);
        EXPECT_TRUE(shudder.active());
        shudder.update(10);
        EXPECT_NEAR(shudder.offset()[0], std::sin(10 * 0.05) * 0.1 * (1.0 - 10.0 / 500.0), 1e-12);
        EXPECT_EQ(shudder.offset()[1], 0.0);
        shudder.update(600);
        EXPECT_FALSE(shudder.active());
        EXPECT_EQ(shudder.offset()[0], 0.0);
    }

    // ---- Starting poses

    TEST(FalloutScriptWorldRulesTest, the_start_of_a_reference_is_the_pose_its_record_gives_it)
    {
        World::StartingPoses poses;
        World::Pose recorded;
        recorded.mPosition = { 1, 2, 3 };
        bool asked = false;
        const auto now = [&asked]() -> std::optional<World::Pose> {
            asked = true;
            return World::Pose();
        };
        const std::optional<World::Pose> pose = poses.poseOf(7, recorded, now);
        ASSERT_TRUE(pose);
        EXPECT_EQ(pose->mPosition[2], 3.0);
        EXPECT_FALSE(asked);
    }

    TEST(FalloutScriptWorldRulesTest, a_reference_with_no_record_starts_where_it_was_when_first_asked)
    {
        World::StartingPoses poses;
        World::Pose here;
        here.mPosition = { 5, 6, 7 };
        EXPECT_TRUE(poses.poseOf(8, std::nullopt, [&here] { return std::optional<World::Pose>(here); }));
        World::Pose later;
        later.mPosition = { 9, 9, 9 };
        const std::optional<World::Pose> again = poses.poseOf(8, std::nullopt, [&later] { return std::optional<World::Pose>(later); });
        ASSERT_TRUE(again);
        EXPECT_EQ(again->mPosition[0], 5.0);
        EXPECT_FALSE(poses.poseOf(9, std::nullopt, [] { return std::nullopt; }));
    }

    TEST(FalloutScriptWorldRulesTest, setting_the_start_to_now_replaces_the_old_start)
    {
        World::StartingPoses poses;
        World::Pose old;
        old.mPosition = { 1, 1, 1 };
        poses.poseOf(3, old, {});
        World::Pose now;
        now.mPosition = { 4, 4, 4 };
        poses.setToNow(3, now);
        EXPECT_EQ(poses.poseOf(3, old, {})->mPosition[0], 4.0);
        poses.clear();
        EXPECT_EQ(poses.poseOf(3, old, {})->mPosition[0], 1.0);
    }

    TEST(FalloutScriptWorldRulesTest, an_angle_is_given_in_degrees)
    {
        EXPECT_NEAR(World::degreesOf(M_PI), 180.0, 1e-4);
        EXPECT_EQ(World::degreesOf(0), 0.0);
    }

    // ---- The small answers

    TEST(FalloutScriptWorldRulesTest, the_controls_are_masked_by_the_bits_of_the_player_byte)
    {
        EXPECT_EQ(World::controlMask({ true, true, true, true, true, true, true }), 0x7F);
        EXPECT_EQ(World::controlMask({ true, false, false, false, false, false, false }), 0x01);
        EXPECT_EQ(World::controlMask({ false, true, false, false, false, false, false }), 0x04);
        EXPECT_EQ(World::controlMask({ false, false, false, false, true, false, false }), 0x02);
        EXPECT_EQ(World::controlMask({ false, false, false, false, false, false, true }), 0x40);
        EXPECT_EQ(World::controlMask({}), 0);
    }

    TEST(FalloutScriptWorldRulesTest, the_machine_is_a_pc_unless_a_pad_of_the_box_is_in_use)
    {
        EXPECT_TRUE(World::isWin32(false, false));
        EXPECT_FALSE(World::isWin32(true, false));
        EXPECT_TRUE(World::isWin32(true, true));
    }

    TEST(FalloutScriptWorldRulesTest, an_ini_setting_reads_as_the_windows_call_reads_it)
    {
        EXPECT_EQ(World::iniInteger("1"), 1);
        EXPECT_EQ(World::iniInteger("  42 ; comment"), 42);
        EXPECT_EQ(World::iniInteger("-3"), -3);
        EXPECT_EQ(World::iniInteger("+7"), 7);
        EXPECT_EQ(World::iniInteger("12abc"), 12);
        EXPECT_EQ(World::iniInteger("99999999999"), 0x7fffffff);
        EXPECT_FALSE(World::iniInteger("").has_value());
        EXPECT_FALSE(World::iniInteger("   ").has_value());
        EXPECT_FALSE(World::iniInteger("yes").has_value());
        EXPECT_FALSE(World::iniInteger("-").has_value());
    }

    TEST(FalloutScriptWorldRulesTest, gore_is_off_only_for_a_setting_of_exactly_one)
    {
        EXPECT_TRUE(World::goreDisabledBy(1));
        EXPECT_FALSE(World::goreDisabledBy(0));
        EXPECT_FALSE(World::goreDisabledBy(2));
        EXPECT_FALSE(World::goreDisabledBy(std::nullopt));
        EXPECT_TRUE(World::isWin32(false, true));
    }

    TEST(FalloutScriptWorldRulesTest, the_picture_of_a_securitron_screen_is_named_by_its_two_words)
    {
        EXPECT_EQ(World::securitronTexture("Happy", "Eyes"), "Textures\\creatures\\Securitron\\Happy_Eyes.dds");
        EXPECT_EQ(World::securitronScreenShape(), "Screen01:0");
    }

    TEST(FalloutScriptWorldRulesTest, a_package_is_over_when_its_hundredths_of_an_hour_have_gone)
    {
        EXPECT_FALSE(World::packageTimeIsUp(0, 20, 8));
        EXPECT_FALSE(World::packageTimeIsUp(150, 9.0, 8.0));
        EXPECT_TRUE(World::packageTimeIsUp(150, 9.5, 8.0));
        EXPECT_TRUE(World::packageTimeIsUp(100, 9.0, 8.0));
    }

    TEST(FalloutScriptWorldRulesTest, the_scale_after_modscale_is_kept_between_the_limits_and_rounded)
    {
        EXPECT_DOUBLE_EQ(World::modScaleResult(1.0, 0.5), 1.5);
        EXPECT_DOUBLE_EQ(World::modScaleResult(1.9, 0.5), 2.0);
        EXPECT_DOUBLE_EQ(World::modScaleResult(1.0, -3.0), 0.5);
        EXPECT_DOUBLE_EQ(World::modScaleResult(1.0, 0.123), 1.12);
    }

    TEST(FalloutScriptWorldRulesTest, the_size_of_an_actor_is_two_when_it_is_not_positive_and_at_most_five)
    {
        EXPECT_EQ(World::actorSizeResult(0), 2.0);
        EXPECT_EQ(World::actorSizeResult(-4), 2.0);
        EXPECT_EQ(World::actorSizeResult(3), 3.0);
        EXPECT_EQ(World::actorSizeResult(50), 5.0);
    }

    TEST(FalloutScriptWorldRulesTest, a_code_is_kept_in_lower_case)
    {
        EXPECT_EQ(World::codeKey("AbC-12"), "abc-12");
        EXPECT_EQ(World::codeKey(""), "");
    }

    TEST(FalloutScriptWorldRulesTest, the_bits_of_the_open_state_are_the_ones_of_the_executable)
    {
        EXPECT_EQ(World::defaultOpenBit, 0x08u);
        EXPECT_EQ(World::openNowBit, 0x04u);
    }
}
