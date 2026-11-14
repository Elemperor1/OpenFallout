#include <components/falloutscript/respawn.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/spawnrules.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScript;

    TEST(FalloutScriptSpawnRulesTest, aZoneGivesItsMinimumToAPlayerBelowIt)
    {
        EXPECT_EQ(zoneLevel(3, 10, 0, 1.0f), 10);
        // unless it matches the player below its minimum
        EXPECT_EQ(zoneLevel(3, 10, ZoneFlag::matchPlayerBelowMinimum, 1.0f), 3);
        // the never resets flag has nothing to do with it
        EXPECT_EQ(zoneLevel(3, 10, ZoneFlag::neverResets, 1.0f), 10);
    }

    TEST(FalloutScriptSpawnRulesTest, aZoneAddsTheScaledPartOfTheLevelAboveItsMinimum)
    {
        EXPECT_EQ(zoneLevel(10, 10, 0, 1.0f), 10);
        EXPECT_EQ(zoneLevel(20, 10, 0, 1.0f), 20);
        EXPECT_EQ(zoneLevel(20, 10, 0, 0.5f), 15);
        EXPECT_EQ(zoneLevel(15, 10, 0, 0.5f), 12); // 12.5, the fraction cut off
        EXPECT_EQ(zoneLevel(20, 0, 0, 1.0f), 20);
    }

    TEST(FalloutScriptSpawnRulesTest, theLevelModifierMultipliesTheLevelAndNoneChangesNothing)
    {
        const SpawnSettings settings;
        EXPECT_FLOAT_EQ(levelModMultiplier(0, settings), 0.5f);
        EXPECT_FLOAT_EQ(levelModMultiplier(1, settings), 0.75f);
        EXPECT_FLOAT_EQ(levelModMultiplier(2, settings), 1.25f);
        EXPECT_FLOAT_EQ(levelModMultiplier(3, settings), 2.0f);
        EXPECT_FLOAT_EQ(levelModMultiplier(noLevelMod, settings), 1.0f);
        EXPECT_FLOAT_EQ(levelModMultiplier(-1, settings), 1.0f);
        EXPECT_FLOAT_EQ(levelModMultiplier(9, settings), 1.0f);

        EXPECT_EQ(scaledSpawnLevel(10, 0, settings), 5);
        EXPECT_EQ(scaledSpawnLevel(10, 1, settings), 7); // 7.5
        EXPECT_EQ(scaledSpawnLevel(10, 2, settings), 12); // 12.5
        EXPECT_EQ(scaledSpawnLevel(10, 3, settings), 20);
        EXPECT_EQ(scaledSpawnLevel(10, noLevelMod, settings), 10);
        EXPECT_EQ(scaledSpawnLevel(1, 0, settings), 1); // never below 1
    }

    TEST(FalloutScriptSpawnRulesTest, theLevelsTheListsAreToldAreTheModifierOfThePlacementOrZero)
    {
        EXPECT_EQ(storedLevelMod(0), 0);
        EXPECT_EQ(storedLevelMod(1), 1);
        EXPECT_EQ(storedLevelMod(3), 3);
        EXPECT_EQ(storedLevelMod(noLevelMod), 0);
        EXPECT_EQ(storedLevelMod(-2), 0);
    }

    TEST(FalloutScriptSpawnRulesTest, placementsGoOneHigherOneLowerAndAtTheLevelInTurn)
    {
        SpawnLevels levels;
        EXPECT_EQ(levels.placementLevel(10, true), 11);
        EXPECT_EQ(levels.placementLevel(10, true), 9);
        EXPECT_EQ(levels.placementLevel(10, true), 10);
        EXPECT_EQ(levels.placementLevel(10, true), 11);
        // without the rotation the counter stands still
        const int counter = levels.counter();
        EXPECT_EQ(levels.placementLevel(10, false), 10);
        EXPECT_EQ(levels.counter(), counter);
        // never below 1
        levels.clear();
        levels.placementLevel(1, true);
        EXPECT_EQ(levels.placementLevel(1, true), 1);
    }

    TEST(FalloutScriptSpawnRulesTest, theRotationFollowsTheSettings)
    {
        SpawnSettings settings;
        settings.mAdjustUp = 2;
        settings.mAdjustDown = 3;
        SpawnLevels levels(settings);
        std::vector<int> seen;
        for (int i = 0; i < 8; ++i)
            seen.push_back(levels.placementLevel(10, true));
        // the counter goes 1, 2, then below zero at -3 and up again
        EXPECT_THAT(seen, ElementsAre(11, 12, 7, 8, 9, 10, 11, 12));
    }

    TEST(FalloutScriptSpawnRulesTest, theLevelAReferenceWasMadeAtIsKeptAndForgotten)
    {
        SpawnLevels levels;
        int level = 0, mod = 0;
        EXPECT_FALSE(levels.find(0x0100, level, mod));
        levels.set(0xFF000001, 14, 2);
        ASSERT_TRUE(levels.find(0xFF000001, level, mod));
        EXPECT_EQ(level, 14);
        EXPECT_EQ(mod, 2);
        levels.set(0xFF000002, 0); // below 1 is 1
        levels.find(0xFF000002, level, mod);
        EXPECT_EQ(level, 1);
        EXPECT_EQ(mod, noLevelMod);
        levels.forget(0xFF000001);
        EXPECT_FALSE(levels.find(0xFF000001, level, mod));
        levels.set(0, 5);
        EXPECT_EQ(levels.size(), 1u);
    }

    TEST(FalloutScriptSpawnRulesTest, theLevelsAndTheCounterSurviveASave)
    {
        SpawnLevels levels;
        levels.placementLevel(10, true);
        levels.set(0xFF000001, 14, 2);
        levels.lockZoneLevel(0x0100AB12, 9);
        SaveWriter out;
        levels.write(out);

        SpawnLevels loaded;
        SaveReader in(out.bytes());
        loaded.read(in, 2);
        EXPECT_TRUE(in.atEnd());
        int level = 0, mod = 0;
        ASSERT_TRUE(loaded.find(0xFF000001, level, mod));
        EXPECT_EQ(level, 14);
        EXPECT_EQ(mod, 2);
        EXPECT_EQ(loaded.counter(), levels.counter());
        EXPECT_EQ(loaded.lockedZoneLevel(0x0100AB12), 9);
    }

    TEST(FalloutScriptSpawnRulesTest, aSaveOfTheFirstVersionHasNoZoneLevels)
    {
        // The stream of the section before the zones kept their levels: the counter and the placements only
        SaveWriter out;
        out.i32(3);
        out.count(1);
        out.form(0xFF000001);
        out.i32(14);
        out.i32(2);
        SpawnLevels loaded;
        loaded.lockZoneLevel(0x0100AB12, 9);
        SaveReader in(out.bytes());
        loaded.read(in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.counter(), 3);
        EXPECT_EQ(loaded.lockedZoneLevel(0x0100AB12), 0);
    }

    // The zone works its level out once and keeps it (Fallout3.exe 0x4BFBA0, New Vegas 0x526270): a zone that has a level
    // does not follow the player any more
    TEST(FalloutScriptSpawnRulesTest, aZoneKeepsTheFirstLevelItWasGiven)
    {
        SpawnLevels levels;
        EXPECT_EQ(levels.lockedZoneLevel(0x0100AB12), 0);
        levels.lockZoneLevel(0x0100AB12, 7);
        levels.lockZoneLevel(0x0100AB12, 12);
        EXPECT_EQ(levels.lockedZoneLevel(0x0100AB12), 7);
        EXPECT_EQ(levels.lockedZoneLevel(0x0100AB13), 0);
        // 0 is how the executable says "none": neither a zone 0 nor a level below 1 is kept
        levels.lockZoneLevel(0, 5);
        levels.lockZoneLevel(0x0100AB13, 0);
        EXPECT_EQ(levels.lockedZoneLevel(0), 0);
        EXPECT_EQ(levels.lockedZoneLevel(0x0100AB13), 0);
        levels.clear();
        EXPECT_EQ(levels.lockedZoneLevel(0x0100AB12), 0);
    }

    TEST(FalloutScriptSpawnRulesTest, theZoneOfAPlacementIsItsOwnThenTheCellsThenTheWorldspaces)
    {
        EXPECT_EQ(placementZone(0x100, 0x200, 0x300), 0x100u);
        EXPECT_EQ(placementZone(0, 0x200, 0x300), 0x200u);
        EXPECT_EQ(placementZone(0, 0, 0x300), 0x300u);
        EXPECT_EQ(placementZone(0, 0, 0), 0u);
    }

    TEST(FalloutScriptSpawnRulesTest, anActorWithALeveledTemplateIsRolledOnceForTheLevelOfItsZoneInTurn)
    {
        SpawnLevels levels;
        int level = 0, mod = 0;
        // The counter goes 1, then back to -1, 0, 1: one higher, one lower, as it is
        ASSERT_TRUE(levels.placedLevel(0x01000001, 10, -1, true, level, mod));
        EXPECT_EQ(level, 11);
        EXPECT_EQ(mod, noLevelMod);
        ASSERT_TRUE(levels.placedLevel(0x01000002, 10, -1, true, level, mod));
        EXPECT_EQ(level, 9);
        ASSERT_TRUE(levels.placedLevel(0x01000003, 10, -1, true, level, mod));
        EXPECT_EQ(level, 10);
        // The same reference gets the level it has, whatever the zone or the player says now, and the counter stays
        const int counter = levels.counter();
        EXPECT_FALSE(levels.placedLevel(0x01000001, 40, 3, true, level, mod));
        EXPECT_EQ(level, 11);
        EXPECT_EQ(mod, noLevelMod);
        EXPECT_EQ(levels.counter(), counter);
    }

    TEST(FalloutScriptSpawnRulesTest, theModifierOfTheReferenceMultipliesTheLevelOfTheTemplateRoll)
    {
        SpawnLevels levels;
        int level = 0, mod = 0;
        // 20 plus the first step of the counter is 21; the multipliers are 0.5, 0.75, 1.25 and 2 (21 * 1.25 = 26.25)
        ASSERT_TRUE(levels.placedLevel(0x01000001, 20, 2, true, level, mod));
        EXPECT_EQ(level, 26);
        EXPECT_EQ(mod, 2);
        // The next is 19 in the turn of the counter, times the boss multiplier: 38
        ASSERT_TRUE(levels.placedLevel(0x01000002, 20, 3, true, level, mod));
        EXPECT_EQ(level, 38);
        EXPECT_EQ(mod, 3);
        // A modifier outside 0 to 3 is none: the level stays as the counter made it (20)
        ASSERT_TRUE(levels.placedLevel(0x01000003, 20, 7, true, level, mod));
        EXPECT_EQ(level, 20);
        EXPECT_EQ(mod, noLevelMod);
        // Never below 1
        ASSERT_TRUE(levels.placedLevel(0x01000004, 1, 0, true, level, mod));
        EXPECT_EQ(level, 1);
    }

    TEST(FalloutScriptSpawnRulesTest, anActorWithoutALeveledTemplateKeepsThePlayersLevelAndMovesNoCounter)
    {
        SpawnLevels levels;
        int level = 0, mod = 0;
        ASSERT_TRUE(levels.placedLevel(0x01000001, 14, 3, false, level, mod));
        EXPECT_EQ(level, 14);
        EXPECT_EQ(mod, noLevelMod);
        EXPECT_EQ(levels.counter(), 0);
        ASSERT_TRUE(levels.placedLevel(0x01000002, 0, -1, false, level, mod));
        EXPECT_EQ(level, 1);
    }

    TEST(FalloutScriptSpawnRulesTest, aPlacedLevelIsForgottenForTheNextRollAndSurvivesASave)
    {
        SpawnLevels levels;
        int level = 0, mod = 0;
        ASSERT_TRUE(levels.placedLevel(0x01000001, 30, 1, true, level, mod));
        const int chosen = level;
        SaveWriter out;
        levels.write(out);
        SpawnLevels loaded;
        SaveReader in(out.bytes());
        loaded.read(in, 1);
        EXPECT_FALSE(loaded.placedLevel(0x01000001, 99, 3, true, level, mod));
        EXPECT_EQ(level, chosen);
        EXPECT_EQ(mod, 1);
        // The cell resets: the next roll is a new one, for the level of the player then
        loaded.forget(0x01000001);
        EXPECT_TRUE(loaded.placedLevel(0x01000001, 50, -1, true, level, mod));
        EXPECT_GE(level, 49);
        EXPECT_LE(level, 51);
    }

    TEST(FalloutScriptGameHoursTest, wholeHoursOfTheDaysThatPassed)
    {
        EXPECT_EQ(gameHoursOf(0.0), 0u);
        EXPECT_EQ(gameHoursOf(1.0), 24u);
        EXPECT_EQ(gameHoursOf(2.5), 60u);
        EXPECT_EQ(gameHoursOf(0.99), 23u);
        EXPECT_EQ(gameHoursOf(-3.0), 0u);
        EXPECT_EQ(gameHoursOf(1e12), 0xFFFFFFFFu);
    }

    constexpr FormId cell = 0x01000A00;
    constexpr FormId actor = 0x01000B00;

    TEST(FalloutScriptRespawnTest, aCellThatWasNeverLeftDoesNotReset)
    {
        RespawnState state;
        EXPECT_FALSE(state.cellResets(cell, 1000, false));
        EXPECT_FALSE(state.attachCell(cell, 1000, false));
    }

    TEST(FalloutScriptRespawnTest, aCellResetsWhenMoreThan72HoursPassedSinceItWasLeft)
    {
        RespawnState state;
        state.cellDetached(cell, 100);
        EXPECT_FALSE(state.cellResets(cell, 100, false));
        EXPECT_FALSE(state.cellResets(cell, 172, false)); // exactly 72 is not more
        EXPECT_TRUE(state.cellResets(cell, 173, false));
        // loading asks the question and forgets that the cell was left
        EXPECT_TRUE(state.attachCell(cell, 173, false));
        EXPECT_FALSE(state.wasDetached(cell));
        EXPECT_FALSE(state.attachCell(cell, 500, false));
    }

    TEST(FalloutScriptRespawnTest, aZoneThatNeverResetsKeepsTheCellsInIt)
    {
        RespawnState state;
        state.cellDetached(cell, 0);
        EXPECT_FALSE(state.cellResets(cell, 1000, true));
        EXPECT_TRUE(state.cellResets(cell, 1000, false));
    }

    TEST(FalloutScriptRespawnTest, aResetInteriorMakesTheCellResetAtOnceAndInAnyZone)
    {
        RespawnState state;
        state.forceReset(cell);
        EXPECT_TRUE(state.cellResets(cell, 0, true));
        // leaving the cell again does not take that back
        state.cellDetached(cell, 5);
        EXPECT_TRUE(state.attachCell(cell, 6, true));
        EXPECT_FALSE(state.attachCell(cell, 7, true));
    }

    TEST(FalloutScriptRespawnTest, theHoursAreCountedAsUnsignedNumbers)
    {
        RespawnState state;
        state.cellDetached(cell, 500);
        // a clock that went back (an older save) makes the difference huge, and the cell reset
        EXPECT_TRUE(state.cellResets(cell, 400, false));
    }

    TEST(FalloutScriptRespawnTest, theSettingChangesHowLongItTakes)
    {
        RespawnSettings settings;
        settings.mHoursToRespawn = 10;
        RespawnState state(settings);
        state.cellDetached(cell, 0);
        EXPECT_FALSE(state.cellResets(cell, 10, false));
        EXPECT_TRUE(state.cellResets(cell, 11, false));
    }

    TEST(FalloutScriptRespawnTest, aMerchantGetsNewStockOnTheTwoDaysOfTheWeekTheSettingsNameOncePerDay)
    {
        RespawnState state;
        // Days 0 and 3 of the week are the defaults: the days 7, 10 and 14 are such days, the others are not
        EXPECT_TRUE(state.merchantRestocks(0x01000100, 7));
        EXPECT_FALSE(state.merchantRestocks(0x01000100, 7));
        EXPECT_FALSE(state.merchantRestocks(0x01000100, 8));
        EXPECT_FALSE(state.merchantRestocks(0x01000100, 9));
        EXPECT_TRUE(state.merchantRestocks(0x01000100, 10));
        EXPECT_FALSE(state.merchantRestocks(0x01000100, 13));
        EXPECT_TRUE(state.merchantRestocks(0x01000100, 14));
        // Every merchant has its own day, and a day before the game began is none
        EXPECT_TRUE(state.merchantRestocks(0x01000200, 7));
        EXPECT_FALSE(state.merchantRestocks(0x01000300, -3));
        // The settings change the days
        RespawnSettings settings;
        settings.mMerchantDay1 = 2;
        settings.mMerchantDay2 = 6;
        RespawnState other(settings);
        EXPECT_FALSE(other.merchantRestocks(0x01000100, 7));
        EXPECT_TRUE(other.merchantRestocks(0x01000100, 9));
        EXPECT_TRUE(other.merchantRestocks(0x01000100, 13));
    }

    TEST(FalloutScriptRespawnTest, aLivingActorIsAlwaysDueAndADeadOneAfter72Hours)
    {
        RespawnState state;
        EXPECT_TRUE(state.actorDue(actor, 10, true));
        state.died(actor, 100);
        EXPECT_FALSE(state.actorDue(actor, 100, false));
        EXPECT_FALSE(state.actorDue(actor, 172, false));
        EXPECT_TRUE(state.actorDue(actor, 173, false));
        EXPECT_FALSE(state.actorDue(actor, 173, true)); // its zone never resets
        EXPECT_FALSE(state.actorDue(actor, 50, false)); // the clock is behind the death
        std::uint32_t when = 0;
        EXPECT_TRUE(state.deathTime(actor, when));
        EXPECT_EQ(when, 100u);
        state.revived(actor);
        EXPECT_TRUE(state.actorDue(actor, 100, true));
        EXPECT_FALSE(state.deathTime(actor, when));
    }

    TEST(FalloutScriptRespawnTest, aZoneScriptsChangedFollowsThemAndOtherwiseItsFlags)
    {
        RespawnState state;
        constexpr FormId zone = 0x01000C00;
        EXPECT_FALSE(state.zoneNeverResets(zone, 0));
        EXPECT_TRUE(state.zoneNeverResets(zone, ZoneFlag::neverResets));
        state.setZoneNeverResets(zone, false);
        EXPECT_FALSE(state.zoneNeverResets(zone, ZoneFlag::neverResets));
        state.setZoneNeverResets(zone, true);
        EXPECT_TRUE(state.zoneNeverResets(zone, 0));
    }

    TEST(FalloutScriptRespawnTest, everyResetOfAnInventoryIsANewGeneration)
    {
        RespawnState state;
        EXPECT_EQ(state.inventoryGeneration(actor), 0);
        state.resetInventory(actor);
        state.resetInventory(actor);
        EXPECT_EQ(state.inventoryGeneration(actor), 2);
        EXPECT_EQ(state.inventoryGeneration(cell), 0);
    }

    TEST(FalloutScriptRespawnTest, theClocksAndFlagsSurviveASave)
    {
        RespawnState state;
        state.cellDetached(cell, 100);
        state.forceReset(0x01000A01);
        state.died(actor, 80);
        state.setZoneNeverResets(0x01000C00, true);
        state.resetInventory(actor);
        SaveWriter out;
        state.write(out);

        RespawnState loaded;
        SaveReader in(out.bytes());
        loaded.read(in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_TRUE(loaded.cellResets(cell, 173, false));
        EXPECT_FALSE(loaded.cellResets(cell, 172, false));
        EXPECT_TRUE(loaded.cellResets(0x01000A01, 0, true));
        EXPECT_TRUE(loaded.actorDue(actor, 153, false));
        EXPECT_FALSE(loaded.actorDue(actor, 152, false));
        EXPECT_TRUE(loaded.zoneNeverResets(0x01000C00, 0));
        EXPECT_EQ(loaded.inventoryGeneration(actor), 1);
        loaded.clear();
        EXPECT_FALSE(loaded.wasDetached(cell));
    }

    TEST(FalloutScriptRespawnTest, aSaveThatIsCutShortIsAnError)
    {
        RespawnState state;
        state.cellDetached(cell, 100);
        SaveWriter out;
        state.write(out);
        const std::string cut = out.bytes().substr(0, 6);
        RespawnState loaded;
        SaveReader in(cut);
        EXPECT_THROW(loaded.read(in, 1), SaveError);
    }
}
