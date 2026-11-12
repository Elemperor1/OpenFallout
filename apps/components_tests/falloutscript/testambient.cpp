#include <components/falloutscript/ambient.hpp>

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace
{
    using namespace FalloutScript::Ambient;

    TEST(FalloutScriptAmbientTest, theDayIsDividedAtTheHoursOfTheSettings)
    {
        EXPECT_EQ(timeOfDay(4.9f), TimeOfDay::Night);
        EXPECT_EQ(timeOfDay(5.f), TimeOfDay::Dawn);
        EXPECT_EQ(timeOfDay(7.99f), TimeOfDay::Dawn);
        EXPECT_EQ(timeOfDay(8.f), TimeOfDay::Day);
        EXPECT_EQ(timeOfDay(17.99f), TimeOfDay::Day);
        EXPECT_EQ(timeOfDay(18.f), TimeOfDay::Dusk);
        EXPECT_EQ(timeOfDay(19.99f), TimeOfDay::Dusk);
        EXPECT_EQ(timeOfDay(20.f), TimeOfDay::Night);
        EXPECT_EQ(timeOfDay(0.f), TimeOfDay::Night);
        EXPECT_EQ(timeOfDay(23.5f), TimeOfDay::Night);
        Settings late;
        late.mDawn = 6.f;
        late.mDay = 9.f;
        EXPECT_EQ(timeOfDay(5.5f, late), TimeOfDay::Night);
        EXPECT_EQ(timeOfDay(6.5f, late), TimeOfDay::Dawn);
    }

    TEST(FalloutScriptAmbientTest, anInteriorSpacePlaysItsFirstSoundAndTheOthersOneForThePeriod)
    {
        const std::array<std::uint32_t, 5> exterior = { 11, 12, 13, 14, 0 };
        EXPECT_EQ(loopOfSpace(exterior, false, TimeOfDay::Dawn), 11u);
        EXPECT_EQ(loopOfSpace(exterior, false, TimeOfDay::Day), 12u);
        EXPECT_EQ(loopOfSpace(exterior, false, TimeOfDay::Dusk), 13u);
        EXPECT_EQ(loopOfSpace(exterior, false, TimeOfDay::Night), 14u);
        // an interior has the one: it does not change with the hour
        const std::array<std::uint32_t, 5> room = { 21, 0, 0, 0, 0 };
        EXPECT_EQ(loopOfSpace(room, true, TimeOfDay::Night), 21u);
        // a period that has none takes the first that has one
        EXPECT_EQ(loopOfSpace(room, false, TimeOfDay::Dusk), 21u);
        const std::array<std::uint32_t, 4> gap = { 0, 31, 0, 0 };
        EXPECT_EQ(loopOfSpace(gap, false, TimeOfDay::Night), 31u);
        EXPECT_EQ(loopOfSpace({}, false, TimeOfDay::Day), 0u);
        const std::array<std::uint32_t, 2> none = { 0, 0 };
        EXPECT_EQ(loopOfSpace(none, false, TimeOfDay::Day), 0u);
    }

    TEST(FalloutScriptAmbientTest, aRegionSoundIsForTheWeatherItsFlagsName)
    {
        EXPECT_TRUE(weatherAllows(15, 4));
        EXPECT_TRUE(weatherAllows(1, 1));
        EXPECT_FALSE(weatherAllows(1, 4));
        EXPECT_FALSE(weatherAllows(6, 8));
        EXPECT_TRUE(weatherAllows(8, 8));
        // a weather without a class is pleasant
        EXPECT_TRUE(weatherAllows(1, 0));
        EXPECT_FALSE(weatherAllows(2, 0));
    }

    TEST(FalloutScriptAmbientTest, theChancesOfTheDataAreMillionths)
    {
        EXPECT_FLOAT_EQ(chanceOf(0), 0.f);
        EXPECT_FLOAT_EQ(chanceOf(100000), 0.1f);
        EXPECT_FLOAT_EQ(chanceOf(900000), 0.9f);
        EXPECT_FLOAT_EQ(chanceOf(10000000), 1.f);
    }

    TEST(FalloutScriptAmbientTest, eachSoundOfARegionIsRolledOnItsOwnClockForTheWeatherAndNotForLoops)
    {
        const std::vector<RegionSound> sounds = { { 1, 15, 1000000 }, { 2, 1, 1000000 }, { 3, 15, 1000000 },
            { 4, 15, 0 }, { 5, 15, 500000 } };
        const auto isLoop = [](std::uint32_t id) { return id == 3; };
        // the clocks start at a share of the interval: 0.5 of 10 seconds. A roll of 0.5 does not pass a chance of a half
        // (the roll must be under the chance), 0.4 does
        double next = 0.5;
        const auto unit = [&next] { return next; };
        RegionRoller roller(10.f);
        EXPECT_TRUE(roller.update(0.f, sounds, 4, unit, isLoop).empty());
        EXPECT_TRUE(roller.update(4.f, sounds, 4, unit, isLoop).empty());
        // the clock of sound 1 and 5 (sound 2 is for pleasant weather only, 3 loops, 4 has no chance) runs out at 5 seconds
        next = 0.4;
        const std::vector<std::uint32_t> played = roller.update(1.5f, sounds, 4, unit, isLoop);
        ASSERT_EQ(played.size(), 2u);
        EXPECT_EQ(played[0], 1u);
        EXPECT_EQ(played[1], 5u);
        // and goes on for another interval
        EXPECT_TRUE(roller.update(9.f, sounds, 4, unit, isLoop).empty());
        const std::vector<std::uint32_t> again = roller.update(1.f, sounds, 4, unit, isLoop);
        EXPECT_EQ(again.size(), 2u);
    }

    TEST(FalloutScriptAmbientTest, aSoundThatMissesItsRollWaitsAnotherInterval)
    {
        const std::vector<RegionSound> sounds = { { 5, 15, 500000 } };
        const auto isLoop = [](std::uint32_t) { return false; };
        double next = 0.0;
        const auto unit = [&next] { return next; };
        RegionRoller roller(10.f);
        // the clock starts at zero and the roll of 0.9 misses a chance of a half
        next = 0.0;
        EXPECT_EQ(roller.update(0.f, sounds, 4, unit, isLoop), std::vector<std::uint32_t>({ 5u }));
        next = 0.9;
        EXPECT_TRUE(roller.update(10.f, sounds, 4, unit, isLoop).empty());
        EXPECT_TRUE(roller.update(9.f, sounds, 4, unit, isLoop).empty());
        next = 0.1;
        EXPECT_EQ(roller.update(1.f, sounds, 4, unit, isLoop), std::vector<std::uint32_t>({ 5u }));
    }

    TEST(FalloutScriptAmbientTest, theWeatherChangesWhichClocksRunAndAResetStartsThemAgain)
    {
        const std::vector<RegionSound> sounds = { { 1, 1, 1000000 }, { 2, 4, 1000000 } };
        const auto isLoop = [](std::uint32_t) { return false; };
        double next = 0.0;
        const auto unit = [&next] { return next; };
        RegionRoller roller(10.f);
        // clocks start at zero: a pleasant weather plays sound 1 at once, a rainy one sound 2
        EXPECT_EQ(roller.update(0.f, sounds, 1, unit, isLoop), std::vector<std::uint32_t>({ 1u }));
        EXPECT_EQ(roller.update(0.f, sounds, 4, unit, isLoop), std::vector<std::uint32_t>({ 2u }));
        EXPECT_TRUE(roller.update(1.f, sounds, 1, unit, isLoop).empty());
        roller.reset();
        EXPECT_EQ(roller.update(0.f, sounds, 1, unit, isLoop), std::vector<std::uint32_t>({ 1u }));
    }
}
