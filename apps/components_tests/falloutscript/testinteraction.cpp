#include <components/falloutscript/interaction.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace
{
    using namespace FalloutScript::Interaction;

    /// No setting in the files: the defaults of the executable
    const Setting defaults = [](std::string_view, double fallback) { return fallback; };

    TEST(FalloutScriptInteractionTest, a_lock_that_is_not_leveled_keeps_its_level)
    {
        EXPECT_EQ(lockLevel(40, false, 12, defaults), 40);
        EXPECT_EQ(lockLevel(0, false, 30, defaults), 0);
    }

    TEST(FalloutScriptInteractionTest, a_leveled_lock_grows_with_the_level_of_the_character_and_stops_at_99)
    {
        // trunc(1.5) is 1: the level of the lock plus the level of the character
        EXPECT_EQ(lockLevel(40, true, 12, defaults), 52);
        EXPECT_EQ(lockLevel(90, true, 30, defaults), 99);
        const Setting two
            = [](std::string_view name, double fallback) { return name == "fLeveledLockMult" ? 2.0 : fallback; };
        EXPECT_EQ(lockLevel(10, true, 5, two), 20);
    }

    TEST(FalloutScriptInteractionTest, a_lock_that_only_its_key_opens_stays_that_way)
    {
        EXPECT_EQ(lockLevel(keyOnlyLevel, true, 20, defaults), keyOnlyLevel);
        EXPECT_EQ(lockClass(keyOnlyLevel, defaults), LockClass::Impossible);
    }

    TEST(FalloutScriptInteractionTest, the_class_of_a_lock_follows_the_settings_of_the_levels)
    {
        EXPECT_EQ(lockClass(0, defaults), LockClass::VeryEasy);
        EXPECT_EQ(lockClass(1, defaults), LockClass::Easy);
        EXPECT_EQ(lockClass(25, defaults), LockClass::Easy);
        EXPECT_EQ(lockClass(26, defaults), LockClass::Average);
        EXPECT_EQ(lockClass(50, defaults), LockClass::Average);
        EXPECT_EQ(lockClass(75, defaults), LockClass::Hard);
        EXPECT_EQ(lockClass(76, defaults), LockClass::VeryHard);
        EXPECT_EQ(lockClass(100, defaults), LockClass::VeryHard);
        EXPECT_EQ(lockClass(101, defaults), LockClass::Impossible);
        const Setting hard
            = [](std::string_view name, double fallback) { return name == "iLockLevelMaxEasy" ? 10.0 : fallback; };
        EXPECT_EQ(lockClass(11, hard), LockClass::Average);
    }

    TEST(FalloutScriptInteractionTest, every_class_but_the_impossible_one_has_a_word_and_experience)
    {
        EXPECT_STREQ(lockClassWord(LockClass::VeryEasy), "sLockLevelNameVeryEasy");
        EXPECT_STREQ(lockClassWord(LockClass::Impossible), "sLockLevelNameImpossible");
        EXPECT_STREQ(lockClassExperience(LockClass::Average), "iXPLevelPickLockAverage");
        EXPECT_STREQ(lockClassExperience(LockClass::VeryHard), "iXPLevelPickLockVeryHard");
        EXPECT_EQ(lockClassExperience(LockClass::Impossible), nullptr);
    }

    TEST(FalloutScriptInteractionTest, a_bobby_pin_opens_an_easy_lock_more_often_than_a_hard_one)
    {
        EXPECT_GT(pickChance(50, 10, defaults), pickChance(50, 60, defaults));
        EXPECT_GT(pickChance(80, 60, defaults), pickChance(30, 60, defaults));
    }

    TEST(FalloutScriptInteractionTest, the_chance_to_pick_stays_between_five_and_ninety_five_percent)
    {
        EXPECT_DOUBLE_EQ(pickChance(100, 0, defaults), 0.95);
        EXPECT_DOUBLE_EQ(pickChance(0, 99, defaults), 0.05);
        // The skill counts from 0 to 100
        EXPECT_DOUBLE_EQ(pickChance(500, 99, defaults), pickChance(100, 99, defaults));
    }

    TEST(FalloutScriptInteractionTest, the_hacking_level_of_a_terminal_grows_with_the_level_when_it_is_leveled)
    {
        EXPECT_EQ(hackLevel(2, false, 30, defaults), 2);
        // trunc(0.25 * 8) is 2
        EXPECT_EQ(hackLevel(1, true, 8, defaults), 3);
        EXPECT_EQ(hackLevel(3, true, 40, defaults), 4);
        EXPECT_EQ(hackLevel(9, false, 1, defaults), 4);
        EXPECT_EQ(hackLevel(-3, false, 1, defaults), 0);
    }

    TEST(FalloutScriptInteractionTest, nothing_stops_a_character_that_may_rest)
    {
        EXPECT_FALSE(restRefusal({}));
        RestConditions sleeping;
        sleeping.mSleeping = true;
        EXPECT_FALSE(restRefusal(sleeping));
    }

    TEST(FalloutScriptInteractionTest, waiting_and_sleeping_are_refused_by_the_settings_of_their_own)
    {
        RestConditions waiting;
        waiting.mHostilesNear = true;
        EXPECT_EQ(restRefusal(waiting), "sNoWaitHostileActorsNear");
        RestConditions sleeping;
        sleeping.mSleeping = true;
        sleeping.mHostilesNear = true;
        EXPECT_EQ(restRefusal(sleeping), "sNoSleepHostileActorsNear");
        sleeping.mHostilesNear = false;
        sleeping.mOwnedBed = true;
        EXPECT_EQ(restRefusal(sleeping), "sNoSleepInOwnedBed");
        waiting.mHostilesNear = false;
        waiting.mUnderwater = true;
        EXPECT_EQ(restRefusal(waiting), "sNoWaitUnderWater");
        // A bed does not matter to a character that waits, nor the water to one that sleeps
        waiting.mUnderwater = false;
        waiting.mOwnedBed = true;
        EXPECT_FALSE(restRefusal(waiting));
        sleeping.mOwnedBed = false;
        sleeping.mUnderwater = true;
        EXPECT_FALSE(restRefusal(sleeping));
    }

    TEST(FalloutScriptInteractionTest, the_first_check_of_the_executable_that_fails_is_the_refusal)
    {
        RestConditions waiting;
        waiting.mTrespassing = true;
        waiting.mHostilesNear = true;
        EXPECT_EQ(restRefusal(waiting), "sNoWaitTrespass");
        RestConditions combat;
        combat.mInCombat = true;
        EXPECT_EQ(restRefusal(combat), "sNoWaitInCombat");
        combat.mSleeping = true;
        EXPECT_EQ(restRefusal(combat), "sNoSleepCombat");
    }

    TEST(FalloutScriptInteractionTest, travel_is_refused_in_the_order_of_the_executable)
    {
        EXPECT_FALSE(travelRefusal({}));
        TravelConditions travel;
        travel.mInAir = true;
        travel.mCellForbids = true;
        EXPECT_EQ(travelRefusal(travel), "sNoFastTravelInAir");
        travel.mOverencumbered = true;
        EXPECT_EQ(travelRefusal(travel), "sNoFastTravelOverencumbered");
        travel.mHostilesNear = true;
        EXPECT_EQ(travelRefusal(travel), "sNoFastTravelHostileActorsNear");
        travel.mUndiscovered = true;
        EXPECT_EQ(travelRefusal(travel), "sNoFastTravelUndiscovered");
        TravelConditions cell;
        cell.mCellForbids = true;
        EXPECT_EQ(travelRefusal(cell), "sNoFastTravelCell");
    }

    TEST(FalloutScriptInteractionTest, a_trip_takes_longer_the_farther_it_goes_and_never_less_than_half_an_hour)
    {
        EXPECT_DOUBLE_EQ(travelHours(0), 0.5);
        EXPECT_DOUBLE_EQ(travelHours(10000), 2.0);
        EXPECT_GT(travelHours(40000), travelHours(10000));
    }
}
