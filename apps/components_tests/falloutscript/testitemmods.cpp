#include <components/falloutscript/itemmods.hpp>

#include <map>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScript::ItemMods;

    constexpr FalloutScript::FormId scope = 0x100;
    constexpr FalloutScript::FormId frame = 0x200;
    constexpr FalloutScript::FormId barrel = 0x300;

    // A weapon with a scope in the first slot (damage), a frame in the second (200 health and 100 more) and the same
    // frame in the third (a weapon that takes one twice)
    Slots makeSlots()
    {
        Slots slots;
        slots.mMod = { scope, frame, frame };
        slots.mEffect = { Damage, MaxCondition, MaxCondition };
        slots.mValue1 = { 3.0, 100.0, 100.0 };
        slots.mValue2 = { 0.0, 0.0, 0.0 };
        return slots;
    }

    TEST(FalloutScriptItemModsTest, aModGoesToTheFirstFreeSlotThatTakesIt)
    {
        const Slots slots = makeSlots();
        EXPECT_EQ(chooseSlot(0, slots, scope), 0);
        EXPECT_EQ(chooseSlot(0, slots, frame), 1);
        // With the second slot taken the same mod goes to the third
        EXPECT_EQ(chooseSlot(2, slots, frame), 2);
        EXPECT_EQ(chooseSlot(6, slots, frame), -1);
        EXPECT_EQ(chooseSlot(1, slots, scope), -1);
    }

    TEST(FalloutScriptItemModsTest, aModTheWeaponDoesNotNameGoesNowhere)
    {
        const Slots slots = makeSlots();
        EXPECT_EQ(chooseSlot(0, slots, barrel), -1);
        EXPECT_EQ(chooseSlot(0, slots, 0), -1);
    }

    TEST(FalloutScriptItemModsTest, theInstalledSlotsAreTheBitsOfTheMask)
    {
        EXPECT_EQ(installedSlots(0), std::vector<int>());
        EXPECT_EQ(installedSlots(5), (std::vector<int>{ 0, 2 }));
        EXPECT_EQ(installedSlots(7), (std::vector<int>{ 0, 1, 2 }));
    }

    TEST(FalloutScriptItemModsTest, anEffectIsReadFromTheFirstInstalledSlotThatHasIt)
    {
        const Slots slots = makeSlots();
        EXPECT_FALSE(installedValue(0, slots, Damage));
        EXPECT_EQ(installedValue(1, slots, Damage), 3.0);
        // A slot that is not installed counts for nothing
        EXPECT_FALSE(installedValue(2, slots, Damage));
        EXPECT_EQ(installedValue(2, slots, MaxCondition), 100.0);
        EXPECT_FALSE(installedValue(7, slots, Spread));
    }

    TEST(FalloutScriptItemModsTest, theListHasTheInstalledModsFirstAndOneLineForEachUnitInThePack)
    {
        const Slots slots = makeSlots();
        const std::map<FalloutScript::FormId, int> held{ { scope, 1 }, { frame, 3 }, { barrel, 5 } };
        const std::vector<Row> rows = listRows(0, slots, held);
        ASSERT_EQ(rows.size(), 4u);
        EXPECT_EQ(rows[0].mMod, scope);
        EXPECT_FALSE(rows[0].mInstalled);
        // Three copies of one mod are three lines; a mod the weapon does not take is not listed
        EXPECT_EQ(rows[1].mMod, frame);
        EXPECT_EQ(rows[2].mMod, frame);
        EXPECT_EQ(rows[3].mMod, frame);
    }

    TEST(FalloutScriptItemModsTest, anInstalledModIsListedFirstEvenWithNoneInThePack)
    {
        const Slots slots = makeSlots();
        const std::vector<Row> rows = listRows(1, slots, {});
        ASSERT_EQ(rows.size(), 1u);
        EXPECT_EQ(rows[0].mMod, scope);
        EXPECT_TRUE(rows[0].mInstalled);
    }

    TEST(FalloutScriptItemModsTest, aModThatIsInstalledIsNotOfferedAgainFromThePack)
    {
        const Slots slots = makeSlots();
        const std::map<FalloutScript::FormId, int> held{ { scope, 2 }, { frame, 1 } };
        const std::vector<Row> rows = listRows(1, slots, held);
        ASSERT_EQ(rows.size(), 2u);
        EXPECT_TRUE(rows[0].mInstalled);
        EXPECT_EQ(rows[0].mMod, scope);
        EXPECT_FALSE(rows[1].mInstalled);
        EXPECT_EQ(rows[1].mMod, frame);
    }

    TEST(FalloutScriptItemModsTest, aWeaponWithNoSlotsListsNothing)
    {
        EXPECT_TRUE(listRows(0, Slots(), { { scope, 2 } }).empty());
    }

    TEST(FalloutScriptItemModsTest, theMostHealthIsTheRecordsAndTheWholePartOfTheMod)
    {
        Slots slots = makeSlots();
        EXPECT_EQ(mostHealth(200.0, 0, slots), 200.0);
        EXPECT_EQ(mostHealth(200.0, 2, slots), 300.0);
        slots.mValue1[1] = 100.9;
        EXPECT_EQ(mostHealth(200.0, 2, slots), 300.0);
    }

    TEST(FalloutScriptItemModsTest, aModOfAnotherEffectLeavesTheHealthAsItWas)
    {
        const Slots slots = makeSlots();
        EXPECT_EQ(conditionAfterInstalling(200.0, 750, 0, slots, 0), 750);
        EXPECT_EQ(conditionAfterInstalling(200.0, 1000, 0, slots, 0), 1000);
    }

    TEST(FalloutScriptItemModsTest, aMaxConditionModAddsItsValueToTheHealthInPoints)
    {
        const Slots slots = makeSlots();
        // 150 of 200 points, 100 more of at most 300: 250 of 300
        EXPECT_EQ(conditionAfterInstalling(200.0, 750, 0, slots, 1), 833);
    }

    TEST(FalloutScriptItemModsTest, aCopyThatHasLostNothingEndsAPointShortOfTheValue)
    {
        const Slots slots = makeSlots();
        // The executable adds the value to "no health" (-1): 99 of 300
        EXPECT_EQ(conditionAfterInstalling(200.0, 1000, 0, slots, 1), 330);
    }

    TEST(FalloutScriptItemModsTest, aSecondMaxConditionModCountsTheFirstOneInTheMost)
    {
        const Slots slots = makeSlots();
        // 300 at most with the first one (a copy at 600 has 180 points), 100 more of 300 + ... the first installed
        // slot with the effect gives the most, so the most stays 300
        EXPECT_EQ(conditionAfterInstalling(200.0, 600, 2, slots, 2), 933);
    }

    TEST(FalloutScriptItemModsTest, aSlotThatDoesNotExistChangesNothing)
    {
        const Slots slots = makeSlots();
        EXPECT_EQ(conditionAfterInstalling(200.0, 500, 0, slots, -1), 500);
        EXPECT_EQ(conditionAfterInstalling(200.0, 500, 0, slots, 3), 500);
        EXPECT_EQ(conditionAfterInstalling(0.0, 500, 0, slots, 1), 500);
    }
}
