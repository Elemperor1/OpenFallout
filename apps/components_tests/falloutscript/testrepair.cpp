#include <components/falloutscript/repair.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScript::Repair;

    TEST(FalloutScriptRepairTest, repairsToTheBetterOfTheTwoAndALittleMore)
    {
        const Settings settings;
        // 50 and 50 at skill 50: 5 + 0.5 + 0.75 + 0.25 = 6.5 tenths
        EXPECT_EQ(repairedCondition(50, 500, 500, settings), 650);
        // The skill makes the difference: nothing at 0, the most at 100
        EXPECT_EQ(repairedCondition(0, 500, 500, settings), 575);
        EXPECT_EQ(repairedCondition(100, 500, 500, settings), 725);
    }

    TEST(FalloutScriptRepairTest, neverRepairsBelowTheBetterOfTheTwoNorAboveFull)
    {
        const Settings settings;
        EXPECT_EQ(repairedCondition(0, 0, 0, settings), 50);
        EXPECT_GE(repairedCondition(0, 800, 100, settings), 800);
        EXPECT_EQ(repairedCondition(100, 1000, 1000, settings), 1000);
        EXPECT_EQ(repairedCondition(100, 900, 900, settings), 1000);
        // A condition out of range counts as the nearest in range
        EXPECT_EQ(repairedCondition(50, -20, 5000, settings), repairedCondition(50, 0, 1000, settings));
    }

    TEST(FalloutScriptRepairTest, aVendorLeavesAnItemInTheConditionItsSkillGives)
    {
        const Settings settings;
        EXPECT_EQ(serviceCondition(0, settings), 400);
        EXPECT_EQ(serviceCondition(50, settings), 650);
        EXPECT_EQ(serviceCondition(100, settings), 900);
        Settings plugin;
        plugin.mSkillMax = 10.0;
        EXPECT_EQ(serviceCondition(100, plugin), 1000);
        EXPECT_EQ(serviceCondition(200, plugin), 1000);
    }

    TEST(FalloutScriptRepairTest, aVendorAsksForWhatIsMissingAndNothingWhenThereIsNone)
    {
        const Settings settings;
        // A broken thing worth 100 at vendor skill 50: 2 * 100 * (6.5 - 0) / 10 = 130
        EXPECT_EQ(serviceCost(50, 0, 100, settings), 130);
        // At the condition the vendor leaves things in there is nothing more to do
        EXPECT_EQ(serviceCost(50, 650, 100, settings), 0);
        EXPECT_EQ(serviceCost(50, 1000, 100, settings), 0);
        // A tiny sum is not asked for, a small one is at least 1 cap
        EXPECT_EQ(serviceCost(50, 600, 1, settings), 0);
        EXPECT_EQ(serviceCost(50, 0, 1, settings), 1);
    }
}
