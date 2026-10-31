#include <components/falloutscript/furniture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace
{
    using FalloutScript::chooseFurnitureSeat;
    using FalloutScript::FurnitureDelta;
    using FalloutScript::FurnitureDeltas;
    using FalloutScript::FurnitureEntry;
    using FalloutScript::furnitureEntryEnabled;
    using FalloutScript::FurniturePlacement;
    using FalloutScript::furnitureSeat;
    using FalloutScript::Point;
    using FalloutScript::settledSeat;

    constexpr float pi = 3.14159265f;

    FurniturePlacement placedAt(float x, float y, float z, float yaw = 0, float scale = 1)
    {
        FurniturePlacement placement;
        placement.mPosition = { x, y, z };
        placement.mRotation[2] = yaw;
        placement.mScale = scale;
        return placement;
    }

    FurnitureEntry entry(float x, float y, float z, float heading = 0, std::uint8_t ref = 14)
    {
        FurnitureEntry result;
        result.mOffset = { x, y, z };
        result.mHeading = heading;
        result.mRef = ref;
        return result;
    }

    TEST(FalloutFurniture, aRecordEnablesTheEntriesOfItsBits)
    {
        // The flags of the couch and the chair of the New Vegas intro: only the third entry, and the kind in the top
        constexpr std::uint32_t flags = 0x40000004;
        EXPECT_FALSE(furnitureEntryEnabled(flags, 0));
        EXPECT_FALSE(furnitureEntryEnabled(flags, 1));
        EXPECT_TRUE(furnitureEntryEnabled(flags, 2));
        EXPECT_FALSE(furnitureEntryEnabled(flags, 30));
        EXPECT_FALSE(furnitureEntryEnabled(flags, 100));
    }

    TEST(FalloutFurniture, anEntryIsAtItsOffsetFromTheReference)
    {
        const auto seat = furnitureSeat(entry(10, 20, 5, 0.5f, 11), 3, placedAt(100, 200, 300));
        EXPECT_FLOAT_EQ(seat.mPosition.x, 110);
        EXPECT_FLOAT_EQ(seat.mPosition.y, 220);
        EXPECT_FLOAT_EQ(seat.mPosition.z, 305);
        EXPECT_FLOAT_EQ(seat.mHeading, 0.5f);
        EXPECT_EQ(seat.mRef, 11);
        EXPECT_EQ(seat.mIndex, 3u);
    }

    TEST(FalloutFurniture, aReferenceTurnsClockwiseSeenFromAbove)
    {
        // North of the furniture, turned a quarter to the right: it is east of it
        const auto seat = furnitureSeat(entry(0, 10, 0, 1.0f), 0, placedAt(100, 200, 300, pi / 2));
        EXPECT_NEAR(seat.mPosition.x, 110, 1e-3);
        EXPECT_NEAR(seat.mPosition.y, 200, 1e-3);
        EXPECT_FLOAT_EQ(seat.mPosition.z, 300);
        // The heading of the entry is added to the rotation of the reference
        EXPECT_NEAR(seat.mHeading, pi / 2 + 1.0f, 1e-5);
    }

    TEST(FalloutFurniture, aHalfTurnPutsTheEntryOnTheOtherSide)
    {
        const auto seat = furnitureSeat(entry(3, 4, 0), 0, placedAt(0, 0, 0, pi));
        EXPECT_NEAR(seat.mPosition.x, -3, 1e-4);
        EXPECT_NEAR(seat.mPosition.y, -4, 1e-4);
    }

    TEST(FalloutFurniture, theScaleOfTheReferenceScalesTheOffset)
    {
        const auto seat = furnitureSeat(entry(10, 20, 5), 0, placedAt(0, 0, 0, 0, 2));
        EXPECT_FLOAT_EQ(seat.mPosition.x, 20);
        EXPECT_FLOAT_EQ(seat.mPosition.y, 40);
        EXPECT_FLOAT_EQ(seat.mPosition.z, 10);
    }

    TEST(FalloutFurniture, aCharacterUsesTheNearestEntryThatIsEnabledAndFree)
    {
        const std::vector<FurnitureEntry> entries{ entry(0, 0, 0), entry(100, 0, 0), entry(200, 0, 0) };
        const FurniturePlacement placement = placedAt(0, 0, 0);
        // Entries 0 and 2 are enabled: from the east end the last is the nearest
        auto seat = chooseFurnitureSeat(entries, 0b101, 0, placement, { 190, 0, 0 });
        ASSERT_TRUE(seat.has_value());
        EXPECT_EQ(seat->mIndex, 2u);
        // With that one in use the other is used, however far
        seat = chooseFurnitureSeat(entries, 0b101, 0b100, placement, { 190, 0, 0 });
        ASSERT_TRUE(seat.has_value());
        EXPECT_EQ(seat->mIndex, 0u);
        // The entry between them is not enabled
        seat = chooseFurnitureSeat(entries, 0b101, 0, placement, { 100, 0, 0 });
        ASSERT_TRUE(seat.has_value());
        EXPECT_NE(seat->mIndex, 1u);
    }

    TEST(FalloutFurniture, noEntryIsUsedWhenNoneIsEnabledOrFree)
    {
        const std::vector<FurnitureEntry> entries{ entry(0, 0, 0), entry(100, 0, 0) };
        const FurniturePlacement placement = placedAt(0, 0, 0);
        EXPECT_FALSE(chooseFurnitureSeat(entries, 0, 0, placement, { 0, 0, 0 }).has_value());
        EXPECT_FALSE(chooseFurnitureSeat(entries, 0b11, 0b11, placement, { 0, 0, 0 }).has_value());
        EXPECT_FALSE(chooseFurnitureSeat({}, 0b11, 0, placement, { 0, 0, 0 }).has_value());
        // A flag for an entry the model does not have is no entry
        EXPECT_FALSE(chooseFurnitureSeat(entries, 0b100, 0, placement, { 0, 0, 0 }).has_value());
    }

    // The settings of New Vegas for the refs of the chairs, in the form the files have them
    const std::map<std::string, double> chairSettings{
        { "ffurnituremarker11deltax", 2.6068 },
        { "ffurnituremarker11deltay", 53.961 },
        { "ffurnituremarker11deltaz", -28.948 },
        { "ffurnituremarker11headingdelta", -1.5708 },
        { "ffurnituremarker12deltax", 2.6669 },
        { "ffurnituremarker12deltay", 53.6492 },
        { "ffurnituremarker12deltaz", -28.948 },
        { "ffurnituremarker12headingdelta", 1.5708 },
        { "ffurnituremarker14deltax", 2.4809 },
        { "ffurnituremarker14deltay", 57.3572 },
        { "ffurnituremarker14deltaz", -28.948 },
        { "ffurnituremarker14headingdelta", 3.1416 },
        { "ffurnituremarker15deltax", -16.3983 },
        { "ffurnituremarker15deltay", 49.5436 },
        { "ffurnituremarker15deltaz", -7.9503 },
        { "ffurnituremarker15headingdelta", 0 },
        { "ffurniturefoo", 3 },
        { "fai furniture", 5 },
    };

    TEST(FalloutFurniture, theDeltasAreReadFromTheSettingsOfEachRef)
    {
        const FurnitureDeltas deltas = FurnitureDeltas::fromGameSettings(chairSettings);
        EXPECT_EQ(deltas.size(), 4u);
        const FurnitureDelta front = deltas.of(14);
        EXPECT_FLOAT_EQ(front.mX, 2.4809f);
        EXPECT_FLOAT_EQ(front.mY, 57.3572f);
        EXPECT_FLOAT_EQ(front.mZ, -28.948f);
        EXPECT_FLOAT_EQ(front.mHeading, 3.1416f);
        EXPECT_FLOAT_EQ(deltas.of(11).mHeading, -1.5708f);
        // A ref the files have no setting for does not move the character
        const FurnitureDelta none = deltas.of(1);
        EXPECT_FLOAT_EQ(none.mX, 0);
        EXPECT_FLOAT_EQ(none.mY, 0);
        EXPECT_FLOAT_EQ(none.mHeading, 0);
        EXPECT_EQ(FurnitureDeltas::fromGameSettings({}).size(), 0u);
    }

    TEST(FalloutFurniture, aSettingThatIsNotAFiniteNumberMovesNoOne)
    {
        const double inf = std::numeric_limits<double>::infinity();
        const FurnitureDeltas deltas = FurnitureDeltas::fromGameSettings({
            { "ffurnituremarker11deltax", std::numeric_limits<double>::quiet_NaN() },
            { "ffurnituremarker11deltay", inf },
            { "ffurnituremarker11deltaz", 1e300 },
            { "ffurnituremarker11headingdelta", -inf },
            { "ffurnituremarker12deltax", 2 },
        });
        const FurnitureDelta bad = deltas.of(11);
        EXPECT_FLOAT_EQ(bad.mX, 0);
        EXPECT_FLOAT_EQ(bad.mY, 0);
        EXPECT_FLOAT_EQ(bad.mZ, 0);
        EXPECT_FLOAT_EQ(bad.mHeading, 0);
        EXPECT_FLOAT_EQ(deltas.of(12).mX, 2);
    }

    TEST(FalloutFurniture, aSettingMayNameTheHeadingOtherwise)
    {
        const FurnitureDeltas deltas = FurnitureDeltas::fromGameSettings(
            { { "ffurnituremarker13deltaangle", 0.5 }, { "ffurnituremarker13deltax", 1 } });
        EXPECT_FLOAT_EQ(deltas.of(13).mHeading, 0.5f);
        EXPECT_FLOAT_EQ(deltas.of(13).mX, 1);
    }

    TEST(FalloutFurniture, aCharacterThatHasSatDownIsOnTheSeatOfTheFurnitureWhicheverWayItIsTurned)
    {
        // The entries of the chair and the stool of the games: the left, right and front of a chair, and a stool
        const FurnitureEntry left = entry(-51.9f, -0.5f, -26.f, 1.570f, 11);
        const FurnitureEntry right = entry(51.3f, -0.5f, -26.f, 4.712f, 12);
        const FurnitureEntry front = entry(-0.3f, 54.6f, -26.f, 3.141f, 14);
        const FurnitureEntry stool = entry(20.5f, -41.f, -24.1f, 0.f, 15);
        const FurnitureDeltas deltas = FurnitureDeltas::fromGameSettings(chairSettings);
        for (const float yaw : { 0.f, 0.7f, 2.f, -1.3f, 3.5f })
        {
            const FurniturePlacement placement = placedAt(100, 200, 50, yaw);
            const std::pair<const FurnitureEntry*, std::uint8_t> uses[]
                = { { &left, 11 }, { &right, 12 }, { &front, 14 }, { &stool, 15 } };
            for (const auto& [use, ref] : uses)
            {
                const auto entrySeat = furnitureSeat(*use, 0, placement);
                const auto seat = settledSeat(entrySeat, deltas.of(ref), 1.f);
                // a few units from the origin of the furniture, which is the seat; the stool is a little off it
                EXPECT_NEAR(seat.mPosition.x, 100, 9) << int(ref) << " at " << yaw;
                EXPECT_NEAR(seat.mPosition.y, 200, 9) << int(ref) << " at " << yaw;
                // at the level of the entry: the animation of sitting lowers the pelvis
                EXPECT_FLOAT_EQ(seat.mPosition.z, entrySeat.mPosition.z);
                // and it faces the front of the furniture whichever side it came from: the heading of the reference
                const float turn = seat.mHeading - yaw;
                EXPECT_NEAR(std::sin(turn), 0.f, 0.01f) << int(ref) << " at " << yaw;
                EXPECT_NEAR(std::cos(turn), 1.f, 0.01f) << int(ref) << " at " << yaw;
                EXPECT_EQ(seat.mRef, ref);
            }
        }
    }

    TEST(FalloutFurniture, theVerticalDeltaIsScaledByTheSizeOfTheReferenceLessOne)
    {
        FurnitureDelta delta;
        delta.mZ = -10;
        const auto entrySeat = furnitureSeat(entry(0, 0, 5), 0, placedAt(0, 0, 0));
        EXPECT_FLOAT_EQ(settledSeat(entrySeat, delta, 1.f).mPosition.z, 5);
        EXPECT_FLOAT_EQ(settledSeat(entrySeat, delta, 1.5f).mPosition.z, 0);
        EXPECT_FLOAT_EQ(settledSeat(entrySeat, delta, 0.5f).mPosition.z, 10);
    }
}
