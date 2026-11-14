#include <gtest/gtest.h>

#include <components/falloutscript/restrictions.hpp>

#include <string>

namespace
{
    using namespace FalloutScript::Restrictions;

    std::string say(Action action, const Facts& facts)
    {
        const char* setting = refusal(action, facts);
        return setting == nullptr ? std::string() : std::string(setting);
    }

    TEST(FalloutScriptRestrictionsTest, nothing_stops_a_player_that_has_none_of_the_facts)
    {
        const Facts none;
        EXPECT_EQ(say(Action::Wait, none), "");
        EXPECT_EQ(say(Action::Sleep, none), "");
        EXPECT_EQ(say(Action::FastTravel, none), "");
    }

    TEST(FalloutScriptRestrictionsTest, each_fact_of_waiting_has_the_setting_of_the_executable)
    {
        const auto only = [](auto member) {
            Facts facts;
            facts.*member = true;
            return say(Action::Wait, facts);
        };
        EXPECT_EQ(only(&Facts::mTrespassing), "sNoWaitTrespass");
        EXPECT_EQ(only(&Facts::mAlarmSounding), "sNoWaitWhileAlarmSounding");
        EXPECT_EQ(only(&Facts::mUnderwater), "sNoWaitUnderWater");
        // The executables spell the setting for hostile characters without its e
        EXPECT_EQ(only(&Facts::mHostilesNear), "sNoWaitHostilActorsNear");
        EXPECT_EQ(only(&Facts::mAirborne), "sNoWaitInAir");
        EXPECT_EQ(only(&Facts::mNoWaitPlace), "sNoWaitInCell");
        EXPECT_EQ(only(&Facts::mWaitSwitchedOff), "sNoWaitInCell");
        EXPECT_EQ(only(&Facts::mInRadiation), "sNoWaitInRadiation");
        EXPECT_EQ(only(&Facts::mRadiationProcess), "sNoWaitInRadiation");
        EXPECT_EQ(only(&Facts::mTakingHealthDamage), "sNoWaitTakingHealthDamage");
    }

    TEST(FalloutScriptRestrictionsTest, waiting_is_refused_by_the_first_check_of_the_executable_that_fails)
    {
        // Every check fails: trespass comes first, then the alarm, water, hostile characters, the air, the place,
        // radiation and last a harmful effect
        Facts facts;
        facts.mTrespassing = facts.mAlarmSounding = facts.mUnderwater = facts.mHostilesNear = facts.mAirborne = true;
        facts.mNoWaitPlace = facts.mInRadiation = facts.mTakingHealthDamage = true;
        const char* expected[] = { "sNoWaitTrespass", "sNoWaitWhileAlarmSounding", "sNoWaitUnderWater",
            "sNoWaitHostilActorsNear", "sNoWaitInAir", "sNoWaitInCell", "sNoWaitInRadiation",
            "sNoWaitTakingHealthDamage" };
        bool* order[] = { &facts.mTrespassing, &facts.mAlarmSounding, &facts.mUnderwater, &facts.mHostilesNear,
            &facts.mAirborne, &facts.mNoWaitPlace, &facts.mInRadiation, &facts.mTakingHealthDamage };
        for (int step = 0; step < 8; ++step)
        {
            EXPECT_EQ(say(Action::Wait, facts), expected[step]) << "step " << step;
            *order[step] = false;
        }
        EXPECT_EQ(say(Action::Wait, facts), "");
    }

    TEST(FalloutScriptRestrictionsTest, a_bed_has_the_settings_of_sleeping_and_only_some_of_the_checks)
    {
        const auto only = [](auto member) {
            Facts facts;
            facts.*member = true;
            return say(Action::Sleep, facts);
        };
        EXPECT_EQ(only(&Facts::mOwnedBed), "sNoSleepInOwnedBed");
        // The word of the place is the one of waiting: the games have no word of sleeping for it
        EXPECT_EQ(only(&Facts::mNoWaitPlace), "sNoWaitInCell");
        EXPECT_EQ(only(&Facts::mTrespassing), "sNoSleepTrespass");
        EXPECT_EQ(only(&Facts::mHostilesNear), "sNoSleepHostileActorsNear");
        EXPECT_EQ(only(&Facts::mInRadiation), "sNoSleepInRadiation");
        EXPECT_EQ(only(&Facts::mTakingHealthDamage), "sNoSleepTakingHealthDamage");
        // These matter to waiting and not to a bed
        EXPECT_EQ(only(&Facts::mAlarmSounding), "");
        EXPECT_EQ(only(&Facts::mUnderwater), "");
        EXPECT_EQ(only(&Facts::mAirborne), "");
        EXPECT_EQ(only(&Facts::mWaitSwitchedOff), "");
        EXPECT_EQ(only(&Facts::mRadiationProcess), "");
    }

    TEST(FalloutScriptRestrictionsTest, a_bed_is_refused_in_the_order_of_the_executable)
    {
        Facts facts;
        facts.mOwnedBed = facts.mNoWaitPlace = facts.mTrespassing = facts.mHostilesNear = true;
        facts.mInRadiation = facts.mTakingHealthDamage = true;
        EXPECT_EQ(say(Action::Sleep, facts), "sNoSleepInOwnedBed");
        facts.mOwnedBed = false;
        EXPECT_EQ(say(Action::Sleep, facts), "sNoWaitInCell");
        facts.mNoWaitPlace = false;
        EXPECT_EQ(say(Action::Sleep, facts), "sNoSleepTrespass");
        facts.mTrespassing = false;
        EXPECT_EQ(say(Action::Sleep, facts), "sNoSleepHostileActorsNear");
        facts.mHostilesNear = false;
        EXPECT_EQ(say(Action::Sleep, facts), "sNoSleepInRadiation");
        facts.mInRadiation = false;
        EXPECT_EQ(say(Action::Sleep, facts), "sNoSleepTakingHealthDamage");
    }

    TEST(FalloutScriptRestrictionsTest, an_owned_bed_matters_to_sleeping_only)
    {
        Facts facts;
        facts.mOwnedBed = true;
        EXPECT_EQ(say(Action::Wait, facts), "");
        EXPECT_EQ(say(Action::FastTravel, facts), "");
    }

    TEST(FalloutScriptRestrictionsTest, each_fact_of_fast_travel_has_the_setting_of_the_executable)
    {
        const auto only = [](auto member) {
            Facts facts;
            facts.*member = true;
            return say(Action::FastTravel, facts);
        };
        EXPECT_EQ(only(&Facts::mHostilesNear), "sNoFastTravelHostileActorsNear");
        EXPECT_EQ(only(&Facts::mAlarmSounding), "sNoFastTravelAlarm");
        EXPECT_EQ(only(&Facts::mTakingHealthDamage), "sFastTravelNoTravelHealthDamage");
        EXPECT_EQ(only(&Facts::mScriptBlocked), "sNoFastTravelScriptBlock");
        EXPECT_EQ(only(&Facts::mOverencumbered), "sNoFastTravelOverencumbered");
        EXPECT_EQ(only(&Facts::mSpent), "sFastTravelNoTravelHealthDamage");
        EXPECT_EQ(only(&Facts::mAirborne), "sNoFastTravelInAir");
        EXPECT_EQ(only(&Facts::mTravelForbidden), "sNoFastTravelCell");
        EXPECT_EQ(only(&Facts::mUndiscovered), "sNoFastTravelUndiscovered");
        // Waiting has no business with these
        EXPECT_EQ(only(&Facts::mNoWaitPlace), "");
        EXPECT_EQ(only(&Facts::mUnderwater), "");
        EXPECT_EQ(only(&Facts::mInRadiation), "");
    }

    TEST(FalloutScriptRestrictionsTest, fast_travel_is_refused_in_the_order_of_the_executable)
    {
        Facts facts;
        facts.mHostilesNear = facts.mAlarmSounding = facts.mTakingHealthDamage = facts.mScriptBlocked = true;
        facts.mOverencumbered = facts.mSpent = facts.mAirborne = facts.mTravelForbidden = facts.mUndiscovered = true;
        const char* expected[] = { "sNoFastTravelHostileActorsNear", "sNoFastTravelAlarm",
            "sFastTravelNoTravelHealthDamage", "sNoFastTravelScriptBlock", "sNoFastTravelOverencumbered",
            "sFastTravelNoTravelHealthDamage", "sNoFastTravelInAir", "sNoFastTravelCell",
            "sNoFastTravelUndiscovered" };
        bool* order[] = { &facts.mHostilesNear, &facts.mAlarmSounding, &facts.mTakingHealthDamage,
            &facts.mScriptBlocked, &facts.mOverencumbered, &facts.mSpent, &facts.mAirborne, &facts.mTravelForbidden,
            &facts.mUndiscovered };
        for (int step = 0; step < 9; ++step)
        {
            EXPECT_EQ(say(Action::FastTravel, facts), expected[step]) << "step " << step;
            *order[step] = false;
        }
        EXPECT_EQ(say(Action::FastTravel, facts), "");
    }

    TEST(FalloutScriptRestrictionsTest, a_perk_lets_an_overencumbered_player_travel)
    {
        Facts facts;
        facts.mOverencumbered = true;
        facts.mTravelWhileOverencumbered = true;
        EXPECT_EQ(say(Action::FastTravel, facts), "");
        facts.mTravelWhileOverencumbered = false;
        EXPECT_EQ(say(Action::FastTravel, facts), "sNoFastTravelOverencumbered");
    }

    TEST(FalloutScriptRestrictionsTest, hostile_characters_count_only_when_alive_enabled_hostile_and_in_reach)
    {
        EXPECT_FALSE(hostileNear({}, 2000));
        Candidate raider;
        raider.mHostile = true;
        raider.mDistance = 1500;
        EXPECT_TRUE(hostileNear({ raider }, 2000));
        // Within the reach is not beyond it
        raider.mDistance = 2000;
        EXPECT_TRUE(hostileNear({ raider }, 2000));
        raider.mDistance = 2000.5;
        EXPECT_FALSE(hostileNear({ raider }, 2000));
        raider.mDistance = 100;
        raider.mDead = true;
        EXPECT_FALSE(hostileNear({ raider }, 2000));
        raider.mDead = false;
        raider.mDisabled = true;
        EXPECT_FALSE(hostileNear({ raider }, 2000));
        raider.mDisabled = false;
        raider.mHostile = false;
        EXPECT_FALSE(hostileNear({ raider }, 2000));
        // One of several is enough
        Candidate far;
        far.mHostile = true;
        far.mDistance = 9000;
        Candidate near;
        near.mHostile = true;
        near.mDistance = 10;
        EXPECT_TRUE(hostileNear({ far, raider, near }, 2000));
    }

    TEST(FalloutScriptRestrictionsTest, the_reach_is_the_interior_distance_inside_and_the_exterior_one_outside)
    {
        EXPECT_DOUBLE_EQ(hostileReach(true), 2000.0);
        EXPECT_DOUBLE_EQ(hostileReach(false), 3000.0);
        EXPECT_DOUBLE_EQ(hostileReach(true, 5000, 100), 100.0);
        EXPECT_DOUBLE_EQ(hostileReach(false, 5000, 100), 5000.0);
    }

    TEST(FalloutScriptRestrictionsTest, a_trip_takes_the_whole_hours_of_its_time_at_the_time_scale)
    {
        // 108000 units at 300 a second is 360 seconds of walking; at the time scale of 30 that is 10800 seconds of
        // the game, three hours
        EXPECT_EQ(travelHours(108000, 300, 30), 3);
        // The hours are cut, not rounded: 2.99 hours is two
        EXPECT_EQ(travelHours(107000, 300, 30), 2);
        // A trip of less than an hour takes none
        EXPECT_EQ(travelHours(1000, 300, 30), 0);
        // The time scale scales the hours
        EXPECT_EQ(travelHours(108000, 300, 60), 6);
        EXPECT_EQ(travelHours(108000, 300, 15), 1);
    }

    TEST(FalloutScriptRestrictionsTest, a_trip_that_cannot_be_measured_takes_no_time)
    {
        EXPECT_EQ(travelHours(0, 300, 30), 0);
        EXPECT_EQ(travelHours(-5, 300, 30), 0);
        EXPECT_EQ(travelHours(1000, 0, 30), 0);
        EXPECT_EQ(travelHours(1000, 300, 0), 0);
    }

    TEST(FalloutScriptRestrictionsTest, the_speed_of_a_trip_is_the_base_speed_times_the_run_multiplier)
    {
        EXPECT_DOUBLE_EQ(travelSpeed(100, 0), 77.0 * 4.0);
        // The speed multiplier is a percentage
        EXPECT_DOUBLE_EQ(travelSpeed(120, 0), 77.0 * 4.0 * 1.2);
        EXPECT_DOUBLE_EQ(travelSpeed(0, 0), 0.0);
        EXPECT_DOUBLE_EQ(travelSpeed(-50, 0), 0.0);
    }

    TEST(FalloutScriptRestrictionsTest, crippled_legs_slow_a_trip)
    {
        EXPECT_DOUBLE_EQ(travelSpeed(100, 1), 77.0 * 4.0 * 0.85);
        EXPECT_DOUBLE_EQ(travelSpeed(100, 2), 77.0 * 4.0 * 0.75);
        EXPECT_DOUBLE_EQ(travelSpeed(100, 3), 77.0 * 4.0 * 0.75);
        EXPECT_DOUBLE_EQ(travelSpeed(100, 1, 100, 2, 0.5, 0.25), 100.0);
    }

    TEST(FalloutScriptRestrictionsTest, a_trip_heals_for_two_seconds_of_regeneration_each_hour)
    {
        EXPECT_DOUBLE_EQ(travelHealingSeconds, 2.0);
    }
}
