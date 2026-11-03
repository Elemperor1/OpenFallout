#include <components/falloutscript/lockpick.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScript::Lockpick;

    TEST(FalloutScriptLockpick, letsACharacterTryALockOnlyAtTheSkillOfItsClass)
    {
        EXPECT_TRUE(mayTry(0, 0));
        EXPECT_FALSE(mayTry(24, 1));
        EXPECT_TRUE(mayTry(25, 1));
        EXPECT_FALSE(mayTry(99, 4));
        EXPECT_TRUE(mayTry(100, 4));
        // A lock that only its key opens is beyond the skill scale
        EXPECT_FALSE(mayTry(100, 5));
    }

    TEST(FalloutScriptLockpick, sizesTheSweetSpotByTheClassAndNotBySkill)
    {
        const double width = 1200;
        EXPECT_EQ(sweetSpot(0, width, 0.5).mLength, 400);
        EXPECT_EQ(sweetSpot(1, width, 0.5).mLength, 200);
        EXPECT_EQ(sweetSpot(2, width, 0.5).mLength, 100);
        EXPECT_EQ(sweetSpot(3, width, 0.5).mLength, 50);
        EXPECT_EQ(sweetSpot(4, width, 0.5).mLength, 25);
        EXPECT_EQ(sweetSpot(4, width, 0.5).mConcentric, 200);
    }

    TEST(FalloutScriptLockpick, keepsTheWholeSweetSpotOnTheMeter)
    {
        const double width = 1200;
        const SweetSpot left = sweetSpot(2, width, 0.0);
        EXPECT_DOUBLE_EQ(left.mCenter, left.outer());
        const SweetSpot right = sweetSpot(2, width, 1.0);
        EXPECT_DOUBLE_EQ(right.mCenter, width - right.outer());
        EXPECT_DOUBLE_EQ(sweetSpot(2, width, 0.5).mCenter, 600);
    }

    TEST(FalloutScriptLockpick, putsTheSpotInTheMiddleOfAMeterThatIsTooNarrowForIt)
    {
        EXPECT_DOUBLE_EQ(sweetSpot(0, 700, 0.1).mCenter, 350);
    }

    TEST(FalloutScriptLockpick, turnsTheCylinderNinetyDegreesOnlyInTheInnermostBand)
    {
        const SweetSpot spot = sweetSpot(2, 1200, 0.5);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter, spot, 1200), 90);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + 50, spot, 1200), 90);
        // Just beyond the first band (half the sweet spot) is the second
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + 50.5, spot, 1200), 75);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter - spot.halfWidth(2) + 1, spot, 1200), 60);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + spot.halfWidth(3) - 1, spot, 1200), 45);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + spot.halfWidth(4) - 1, spot, 1200), 30);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + spot.outer(), spot, 1200), 15);
    }

    TEST(FalloutScriptLockpick, fallsFromFifteenToNothingBeyondTheOutermostBand)
    {
        const double width = 1200;
        const SweetSpot spot = sweetSpot(2, width, 0.5);
        // The spot is in the middle: the far edge of the meter is 600 from it, the outermost band ends at 250
        EXPECT_NEAR(maxTurn(spot.mCenter + spot.outer() + 0.001, spot, width), 15, 1e-3);
        const double middle = maxTurn(spot.mCenter + (spot.outer() + width / 2) / 2, spot, width);
        EXPECT_GT(middle, 0);
        EXPECT_LT(middle, 15);
        // It is nothing only at the end of the meter that is furthest from a spot at the other end
        const SweetSpot atTheEdge = sweetSpot(2, width, 0.0);
        EXPECT_NEAR(maxTurn(width, atTheEdge, width), 0, 1e-9);
        EXPECT_NEAR(maxTurn(width, spot, width), 7.5, 1e-9);
    }

    TEST(FalloutScriptLockpick, holdsAPickLongerForALongerSkill)
    {
        EXPECT_DOUBLE_EQ(pickHealth(0), 1.0);
        EXPECT_DOUBLE_EQ(pickHealth(15), 1.15);
        EXPECT_DOUBLE_EQ(pickHealth(100), 2.0);
    }

    TEST(FalloutScriptLockpick, roundsTheChanceToForceIntoTheScale)
    {
        EXPECT_DOUBLE_EQ(forceChance(30, 2), 0); // 30 - 50 + 10 is below nothing
        EXPECT_DOUBLE_EQ(forceChance(50, 2), 10);
        EXPECT_DOUBLE_EQ(forceChance(60, 2), 20);
        EXPECT_DOUBLE_EQ(forceChance(100, 0), 100);
        EXPECT_DOUBLE_EQ(forceChance(15, 0), 25);
    }
}
