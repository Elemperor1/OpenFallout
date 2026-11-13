#include <components/falloutscript/nearwater.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
    using namespace FalloutScript::NearWater;

    TEST(FalloutScriptNearWaterTest, theSamplePointsAreASquareOfPointsAroundThePlayer)
    {
        const std::vector<Point> points = samplePoints({ 100.f, -50.f });
        ASSERT_EQ(points.size(), 64u);
        // the first corner and the last, with the points 2000 / 7 apart
        EXPECT_FLOAT_EQ(points.front().mX, -900.f);
        EXPECT_FLOAT_EQ(points.front().mY, -1050.f);
        EXPECT_FLOAT_EQ(points.back().mX, 1100.f);
        EXPECT_FLOAT_EQ(points.back().mY, 950.f);
        // x changes the slowest
        EXPECT_FLOAT_EQ(points[1].mX, -900.f);
        EXPECT_NEAR(points[1].mY, -1050.f + 2000.f / 7.f, 0.01f);
        EXPECT_NEAR(points[8].mX, -900.f + 2000.f / 7.f, 0.01f);
    }

    TEST(FalloutScriptNearWaterTest, oneOrNoPointsIsOnePointOnThePlayerSideOrNone)
    {
        Settings single;
        single.mPoints = 1;
        const std::vector<Point> one = samplePoints({ 0.f, 0.f }, single);
        ASSERT_EQ(one.size(), 1u);
        EXPECT_FLOAT_EQ(one[0].mX, -1000.f);
        Settings none;
        none.mPoints = 0;
        EXPECT_TRUE(samplePoints({ 0.f, 0.f }, none).empty());
    }

    TEST(FalloutScriptNearWaterTest, indoorsTheWaterIsHeardWithinHalfAThousandUnitsOfItsSurface)
    {
        EXPECT_FLOAT_EQ(interiorStrength(100.f, 100.f, false), 1.f);
        EXPECT_FLOAT_EQ(interiorStrength(100.f, 356.f, false), 0.5f);
        EXPECT_FLOAT_EQ(interiorStrength(100.f, -156.f, false), 0.5f);
        EXPECT_FLOAT_EQ(interiorStrength(100.f, 612.f, false), 0.f);
        EXPECT_FLOAT_EQ(interiorStrength(100.f, 900.f, false), 0.f);
    }

    TEST(FalloutScriptNearWaterTest, underTheWaterTheSamplerHearsNothing)
    {
        EXPECT_FLOAT_EQ(interiorStrength(100.f, 100.f, true), 0.f);
        Wet wet;
        wet.mCount = 64;
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 0.f, true), 0.f);
    }

    TEST(FalloutScriptNearWaterTest, outOfDoorsHalfTheWetPointsAreAllOfTheStrength)
    {
        Wet wet;
        wet.mCount = 32;
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 0.f, false), 1.f);
        wet.mCount = 64;
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 0.f, false), 1.f);
        wet.mCount = 16;
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 0.f, false), 0.5f);
        wet.mCount = 0;
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 0.f, false), 0.f);
    }

    TEST(FalloutScriptNearWaterTest, outOfDoorsTheHeightAboveTheWaterTakesAwayFromTheStrength)
    {
        Wet wet;
        wet.mCount = 32;
        // 512 above the water of a tolerance of 1024 leaves half of it
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 512.f, false), 0.5f);
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, -512.f, false), 0.5f);
        // as far as the tolerance, or farther, nothing
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 1024.f, false), 0.f);
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 4000.f, false), 0.f);
    }

    TEST(FalloutScriptNearWaterTest, theSettingsChangeTheNumbers)
    {
        Settings settings;
        settings.mPoints = 4;
        settings.mOutdoorTolerance = 200.f;
        settings.mIndoorTolerance = 100.f;
        Wet wet;
        wet.mCount = 4;
        // four of sixteen is a quarter, twice is half; 100 of 200 above the water halves it again
        EXPECT_FLOAT_EQ(exteriorStrength(wet, 0.f, 100.f, false, settings), 0.25f);
        EXPECT_FLOAT_EQ(interiorStrength(0.f, 50.f, false, settings), 0.5f);
        EXPECT_FLOAT_EQ(interiorStrength(0.f, 100.f, false, settings), 0.f);
    }

    TEST(FalloutScriptNearWaterTest, theSoundComesFromTheWetPointsAtADistanceThatShrinksWithTheStrength)
    {
        Wet wet;
        wet.add({ 2000.f, 0.f });
        wet.add({ 2000.f, 100.f });
        const Point player{ 1000.f, 50.f };
        // the middle of the wet points is to the east of the player: with nothing near the sound is a radius away
        Point at = soundPosition(player, wet, 0.f, false);
        EXPECT_NEAR(at.mX, 2000.f, 0.01f);
        EXPECT_NEAR(at.mY, 50.f, 0.01f);
        // half near: half a radius
        at = soundPosition(player, wet, 0.5f, false);
        EXPECT_NEAR(at.mX, 1500.f, 0.01f);
        // all near: on the player
        at = soundPosition(player, wet, 1.f, false);
        EXPECT_NEAR(at.mX, 1000.f, 0.01f);
        EXPECT_NEAR(at.mY, 50.f, 0.01f);
    }

    TEST(FalloutScriptNearWaterTest, theDirectionPointsAtTheMiddleOfTheWetPoints)
    {
        Wet wet;
        wet.add({ 0.f, 500.f });
        wet.add({ 0.f, 1500.f });
        // the middle is (0, 1000), straight north of the player at the origin
        const Point at = soundPosition({ 0.f, 0.f }, wet, 0.f, false);
        EXPECT_NEAR(at.mX, 0.f, 0.01f);
        EXPECT_NEAR(at.mY, 1000.f, 0.01f);
    }

    TEST(FalloutScriptNearWaterTest, indoorsAndWithNoWetPointOrNoDirectionTheSoundIsOnThePlayer)
    {
        Wet wet;
        wet.add({ 2000.f, 2000.f });
        const Point player{ 5.f, 6.f };
        Point at = soundPosition(player, wet, 0.1f, true);
        EXPECT_FLOAT_EQ(at.mX, 5.f);
        EXPECT_FLOAT_EQ(at.mY, 6.f);
        at = soundPosition(player, Wet(), 0.1f, false);
        EXPECT_FLOAT_EQ(at.mX, 5.f);
        EXPECT_FLOAT_EQ(at.mY, 6.f);
        // the middle of the wet points is the player: no direction to go in
        Wet under;
        under.add({ 4.f, 5.f });
        under.add({ 6.f, 7.f });
        at = soundPosition(player, under, 0.f, false);
        EXPECT_FLOAT_EQ(at.mX, 5.f);
        EXPECT_FLOAT_EQ(at.mY, 6.f);
    }
}
