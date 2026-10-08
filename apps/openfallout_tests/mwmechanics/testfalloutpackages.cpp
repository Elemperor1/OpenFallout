#include <apps/openfallout/mwmechanics/falloutpackages.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <osg/Math>

namespace
{
    using namespace testing;
    using namespace OFMechanics;

    constexpr float epsilon = 0.001f;

    TEST(FalloutPackagesTest, aGoalThatRoamsGetsTheRadiusOfThePackageWithinLimits)
    {
        const auto roam = ESM4::PackageBehaviour::Roam;
        EXPECT_FLOAT_EQ(falloutGoalRadius(1000, roam), 1000.f);
        EXPECT_FLOAT_EQ(falloutGoalRadius(0, roam), falloutDefaultRoam);
        EXPECT_FLOAT_EQ(falloutGoalRadius(-5, roam), falloutDefaultRoam);
        EXPECT_FLOAT_EQ(falloutGoalRadius(10, roam), falloutSmallestRoam);
        EXPECT_FLOAT_EQ(falloutGoalRadius(1000000, roam), falloutLargestRoam);
    }

    TEST(FalloutPackagesTest, aGoalThatStaysGetsAnArrivalDistanceWithinLimits)
    {
        const auto stay = ESM4::PackageBehaviour::Stay;
        EXPECT_FLOAT_EQ(falloutGoalRadius(100, stay), 100.f);
        EXPECT_FLOAT_EQ(falloutGoalRadius(0, stay), falloutSmallestArrival);
        EXPECT_FLOAT_EQ(falloutGoalRadius(-1, stay), falloutSmallestArrival);
        EXPECT_FLOAT_EQ(falloutGoalRadius(5000, stay), falloutLargestArrival);
    }

    TEST(FalloutPackagesTest, roamPointsAreInTheCircleAndSpreadOverIt)
    {
        FalloutGoal goal;
        goal.mBehaviour = ESM4::PackageBehaviour::Roam;
        goal.mCenter = osg::Vec3f(1000.f, -500.f, 70.f);
        goal.mRadius = 400.f;

        ESM4::LevelledRandom random(7);
        constexpr int points = 2000;
        int north = 0;
        int east = 0;
        int inner = 0;
        float farthest = 0.f;
        for (int i = 0; i < points; ++i)
        {
            const osg::Vec3f point = falloutRoamPoint(goal, random);
            EXPECT_FLOAT_EQ(point.z(), 70.f);
            const float distance = (osg::Vec2f(point.x(), point.y()) - osg::Vec2f(1000.f, -500.f)).length();
            EXPECT_LE(distance, 400.f + epsilon);
            farthest = std::max(farthest, distance);
            north += point.y() > -500.f ? 1 : 0;
            east += point.x() > 1000.f ? 1 : 0;
            inner += distance < 200.f ? 1 : 0;
        }
        // Half of the points are on each side, and a quarter are in the inner half of the radius
        EXPECT_THAT(north, AllOf(Gt(points * 4 / 10), Lt(points * 6 / 10)));
        EXPECT_THAT(east, AllOf(Gt(points * 4 / 10), Lt(points * 6 / 10)));
        EXPECT_THAT(inner, AllOf(Gt(points * 2 / 10), Lt(points * 3 / 10)));
        EXPECT_GT(farthest, 380.f);
    }

    TEST(FalloutPackagesTest, theSameSeedGivesTheSamePointsAndPauses)
    {
        FalloutGoal goal;
        goal.mCenter = osg::Vec3f(0.f, 0.f, 0.f);
        goal.mRadius = 300.f;
        ESM4::LevelledRandom first(99);
        ESM4::LevelledRandom second(99);
        for (int i = 0; i < 10; ++i)
        {
            EXPECT_EQ(falloutRoamPoint(goal, first), falloutRoamPoint(goal, second));
            EXPECT_FLOAT_EQ(falloutRoamPause(first), falloutRoamPause(second));
        }
    }

    TEST(FalloutPackagesTest, aPauseLastsAFewSeconds)
    {
        ESM4::LevelledRandom random(3);
        for (int i = 0; i < 200; ++i)
        {
            const float pause = falloutRoamPause(random);
            EXPECT_GE(pause, 3.f);
            EXPECT_LT(pause, 12.f);
        }
    }

    TEST(FalloutPackagesTest, theClockBeginsOnAMonday)
    {
        EXPECT_FLOAT_EQ(falloutClock(13.5f, 0).mHour, 13.5f);
        EXPECT_EQ(falloutClock(0.f, 0).mDayOfWeek, 1);
        EXPECT_EQ(falloutClock(0.f, 5).mDayOfWeek, 6);
        EXPECT_EQ(falloutClock(0.f, 6).mDayOfWeek, 0);
        EXPECT_EQ(falloutClock(0.f, 7).mDayOfWeek, 1);
        EXPECT_EQ(falloutClock(0.f, -1).mDayOfWeek, 0);
    }

    TEST(FalloutPackagesTest, theClockStaysWithinTheDay)
    {
        EXPECT_FLOAT_EQ(falloutClock(-3.f, 0).mHour, 0.f);
        EXPECT_FLOAT_EQ(falloutClock(30.f, 0).mHour, 24.f);
    }

    TEST(FalloutPackagesTest, aTurnGoesTheShorterWayRound)
    {
        const float pi = osg::PIf;
        EXPECT_NEAR(falloutTurnToward(0.f, 1.f, 0.25f), 0.25f, epsilon);
        EXPECT_NEAR(falloutTurnToward(0.f, -1.f, 0.25f), -0.25f, epsilon);
        // Across the half turn: from 3 to -3 the short way is up through pi
        EXPECT_NEAR(falloutTurnToward(3.f, -3.f, 0.1f), 3.1f, epsilon);
        EXPECT_NEAR(falloutTurnToward(-3.f, 3.f, 0.1f), -3.1f, epsilon);
        // and a turn that reaches its target stops there
        EXPECT_NEAR(falloutTurnToward(0.5f, 0.6f, 1.f), 0.6f, epsilon);
        EXPECT_NEAR(falloutTurnToward(pi - 0.05f, -pi + 0.05f, 1.f), -pi + 0.05f, epsilon);
        EXPECT_NEAR(falloutTurnToward(0.f, 0.f, 0.5f), 0.f, epsilon);
    }

    TEST(FalloutPackagesTest, aStepGoesTowardThePlaceAndNoFarther)
    {
        const osg::Vec3f from(0.f, 0.f, 0.f);
        const osg::Vec3f to(30.f, 40.f, 0.f);
        const osg::Vec3f step = falloutStepToward(from, to, 25.f);
        EXPECT_NEAR(step.x(), 15.f, epsilon);
        EXPECT_NEAR(step.y(), 20.f, epsilon);
        EXPECT_EQ(falloutStepToward(from, to, 50.f), to);
        EXPECT_EQ(falloutStepToward(from, to, 100.f), to);
        EXPECT_EQ(falloutStepToward(to, to, 5.f), to);
        const osg::Vec3f rising = falloutStepToward(from, osg::Vec3f(0.f, 3.f, 4.f), 2.5f);
        EXPECT_NEAR(rising.y(), 1.5f, epsilon);
        EXPECT_NEAR(rising.z(), 2.f, epsilon);
    }

    TEST(FalloutPackagesTest, aFollowerKeepsTheDistanceOfThePackageWithinLimits)
    {
        EXPECT_FLOAT_EQ(falloutFollowDistance(300), 300.f);
        EXPECT_FLOAT_EQ(falloutFollowDistance(0), falloutDefaultFollow);
        EXPECT_FLOAT_EQ(falloutFollowDistance(-4), falloutDefaultFollow);
        EXPECT_FLOAT_EQ(falloutFollowDistance(10), falloutSmallestFollow);
        EXPECT_FLOAT_EQ(falloutFollowDistance(100000), falloutLargestFollow);
    }

    TEST(FalloutPackagesTest, aScriptCanNameAnyDistanceToFollowAt)
    {
        EXPECT_FLOAT_EQ(falloutScriptFollowDistance(300.5f), 300.5f);
        EXPECT_FLOAT_EQ(falloutScriptFollowDistance(0.f), falloutDefaultFollow);
        EXPECT_FLOAT_EQ(falloutScriptFollowDistance(-4.f), falloutDefaultFollow);
        EXPECT_FLOAT_EQ(falloutScriptFollowDistance(std::numeric_limits<float>::quiet_NaN()), falloutDefaultFollow);
        // A fraction is a distance that is too short, not none
        EXPECT_FLOAT_EQ(falloutScriptFollowDistance(0.5f), falloutSmallestFollow);
        EXPECT_FLOAT_EQ(falloutScriptFollowDistance(1e20f), falloutLargestFollow);
        EXPECT_FLOAT_EQ(falloutScriptFollowDistance(std::numeric_limits<float>::infinity()), falloutLargestFollow);
    }

    TEST(FalloutPackagesTest, aFollowerStartsToWalkBeyondTheSlackAndStopsWithinTheDistance)
    {
        const float distance = 200.f;
        // Standing still, it waits until the one it follows is out of the distance and the slack
        EXPECT_FALSE(falloutFollowMoves(distance, distance, false));
        EXPECT_FALSE(falloutFollowMoves(distance + falloutFollowSlack - 1.f, distance, false));
        EXPECT_TRUE(falloutFollowMoves(distance + falloutFollowSlack + 1.f, distance, false));
        // Walking, it goes on until it is within the distance
        EXPECT_TRUE(falloutFollowMoves(distance + 1.f, distance, true));
        EXPECT_FALSE(falloutFollowMoves(distance, distance, true));
        EXPECT_FALSE(falloutFollowMoves(0.f, distance, true));
    }

    TEST(FalloutPackagesTest, aFollowerRunsOnlyWhenFarBehind)
    {
        EXPECT_FALSE(falloutFollowRuns(300.f, 200.f));
        EXPECT_FALSE(falloutFollowRuns(200.f + falloutFollowRunBeyond, 200.f));
        EXPECT_TRUE(falloutFollowRuns(200.f + falloutFollowRunBeyond + 1.f, 200.f));
    }

    TEST(FalloutPackagesTest, thePlayerReferenceIsForm14OfTheFirstPlugin)
    {
        // (the scripts that the engine adds come first in the list of content files)
        const std::vector<std::string> files{ "builtin.omwscripts", "FalloutNV.ESM", "DeadMoney.esm" };
        const int first = falloutFirstPlugin(files);
        EXPECT_EQ(first, 1);
        EXPECT_TRUE(falloutIsPlayerReference(ESM::FormId::fromUint32(0x01000014), first));
        EXPECT_FALSE(falloutIsPlayerReference(ESM::FormId::fromUint32(0x02000014), first));
        EXPECT_FALSE(falloutIsPlayerReference(ESM::FormId::fromUint32(0x01000015), first));
        EXPECT_FALSE(falloutIsPlayerReference(ESM::FormId{}, first));
        EXPECT_FALSE(falloutIsPlayerReference(ESM::FormId::fromUint32(0x01000014), -1));
    }

    TEST(FalloutPackagesTest, theFirstPluginIsTheFirstFileOfAGameFormat)
    {
        EXPECT_EQ(falloutFirstPlugin({}), -1);
        EXPECT_EQ(falloutFirstPlugin({ "builtin.omwscripts", "mod.omwaddon" }), -1);
        EXPECT_EQ(falloutFirstPlugin({ "a.omwscripts", "Fallout3.esm" }), 1);
        EXPECT_EQ(falloutFirstPlugin({ "noextension", "x.ESP", "y.esm" }), 1);
        EXPECT_EQ(falloutFirstPlugin({ "x.esl" }), 0);
    }
}
