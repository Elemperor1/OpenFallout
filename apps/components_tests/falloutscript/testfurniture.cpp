#include <components/falloutscript/furniture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{
    using FalloutScript::chooseFurnitureSeat;
    using FalloutScript::FurnitureEntry;
    using FalloutScript::furnitureEntryEnabled;
    using FalloutScript::FurniturePlacement;
    using FalloutScript::furnitureSeat;
    using FalloutScript::Point;

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
}
