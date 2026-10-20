#include <components/esm4/loadregn.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace ESM4Test;

    constexpr std::uint32_t deletedRecord = 0x20;

    std::string points(const std::vector<float>& corners)
    {
        std::string data;
        for (const float corner : corners)
            append<float>(data, corner);
        return data;
    }

    std::string area(std::uint32_t edgeFalloff, const std::vector<float>& corners)
    {
        return valueSubRecord<std::uint32_t>("RPLI", edgeFalloff) + subRecord("RPLD", points(corners));
    }

    // A square from (0, 0) to (100, 100) and a triangle with its corners at (200, 0), (300, 0) and (200, 100)
    const std::vector<float> square = { 0, 0, 100, 0, 100, 100, 0, 100 };
    const std::vector<float> triangle = { 200, 0, 300, 0, 200, 100 };

    ESM4::Region loadRegion(const std::string& data, std::uint32_t flags = 0)
    {
        const std::vector<ESM4::Region> records = loadRecords<ESM4::Region>("REGN", record("REGN", 0x801, data, flags));
        EXPECT_EQ(records.size(), 1u);
        return records.empty() ? ESM4::Region() : records.front();
    }

    TEST(ESM4RegionTest, readsTheAreasWithTheirFalloffAndTheirPoints)
    {
        const ESM4::Region region
            = loadRegion(zString("EDID", "OFTestRegion") + valueSubRecord<std::uint32_t>("RCLR", 0x00ff00ffu)
                + valueSubRecord<std::uint32_t>("WNAM", 0x808u) + area(8, square) + area(16, triangle));

        EXPECT_EQ(region.mEditorId, "OFTestRegion");
        EXPECT_EQ(region.mWorldId.toUint32(), 0x808u);
        ASSERT_EQ(region.mAreas.size(), 2u);
        EXPECT_EQ(region.mAreas[0].mEdgeFalloff, 8u);
        EXPECT_EQ(region.mAreas[0].mPoints, square);
        EXPECT_EQ(region.mAreas[1].mEdgeFalloff, 16u);
        EXPECT_EQ(region.mAreas[1].mPoints, triangle);
    }

    TEST(ESM4RegionTest, takesPointsThatComeWithoutAFalloffAsAnAreaOfTheirOwn)
    {
        const ESM4::Region region = loadRegion(zString("EDID", "x") + subRecord("RPLD", points(square)));

        ASSERT_EQ(region.mAreas.size(), 1u);
        EXPECT_EQ(region.mAreas[0].mEdgeFalloff, 0u);
        EXPECT_EQ(region.mAreas[0].mPoints, square);
    }

    TEST(ESM4RegionTest, coversThePointsInsideOneOfItsAreas)
    {
        const ESM4::Region region = loadRegion(area(0, square) + area(0, triangle));
        const ESM::FormId world;

        EXPECT_TRUE(region.covers(world, 50, 50));
        EXPECT_TRUE(region.covers(world, 1, 99));
        EXPECT_TRUE(region.covers(world, 210, 20));
        // outside both, and in the part of the box of the triangle that is not the triangle
        EXPECT_FALSE(region.covers(world, 150, 50));
        EXPECT_FALSE(region.covers(world, 290, 90));
        EXPECT_FALSE(region.covers(world, -1, 50));
        EXPECT_FALSE(region.covers(world, 50, 101));
    }

    TEST(ESM4RegionTest, holdsNothingWithFewerThanThreeCorners)
    {
        const ESM4::Region region = loadRegion(area(0, { 0, 0, 100, 100 }));

        EXPECT_FALSE(region.covers(ESM::FormId(), 50, 50));
        EXPECT_FALSE(ESM4::Region().covers(ESM::FormId(), 0, 0));
    }

    TEST(ESM4RegionTest, coversOnlyInTheWorldspaceItNames)
    {
        const ESM4::Region named = loadRegion(valueSubRecord<std::uint32_t>("WNAM", 0x808u) + area(0, square));
        const ESM4::Region unnamed = loadRegion(area(0, square));

        EXPECT_TRUE(named.covers(ESM::FormId::fromUint32(0x808u), 50, 50));
        EXPECT_FALSE(named.covers(ESM::FormId::fromUint32(0x809u), 50, 50));
        // a region of no worldspace is in every one
        EXPECT_TRUE(unnamed.covers(ESM::FormId::fromUint32(0x809u), 50, 50));
    }

    TEST(ESM4RegionTest, readsTheAreasOfARegionThatHasSubrecordsOfNoUseToTheEngine)
    {
        // The small icon of the games, a sound list and a map name that stand where no data entry of their type is,
        // and a subrecord that no game has: none of them stops the load, and the areas are read
        const ESM4::Region region = loadRegion(zString("EDID", "x") + zString("ICON", "large.dds")
            + zString("MICO", "small.dds") + valueSubRecord<std::uint32_t>("WNAM", 0x808u) + area(0, square)
            + subRecord("RDSD", std::string(12, 'x')) + zString("RDMP", "map") + subRecord("ZZZZ", "1")
            + area(0, triangle));

        EXPECT_EQ(region.mEditorId, "x");
        ASSERT_EQ(region.mAreas.size(), 2u);
        EXPECT_EQ(region.mAreas[0].mPoints, square);
        EXPECT_EQ(region.mAreas[1].mPoints, triangle);
        EXPECT_TRUE(region.covers(ESM::FormId::fromUint32(0x808u), 210, 20));
    }

    TEST(ESM4RegionTest, readsOnFromPointsThatEndInBytesOfNoCorner)
    {
        // A payload that is not a whole number of floats: the stray bytes are skipped, and the area that follows is
        // read from where it begins
        const ESM4::Region region = loadRegion(valueSubRecord<std::uint32_t>("RPLI", 0)
            + subRecord("RPLD", points(square) + std::string(2, 'x')) + area(0, triangle));

        ASSERT_EQ(region.mAreas.size(), 2u);
        EXPECT_EQ(region.mAreas[0].mPoints, square);
        EXPECT_EQ(region.mAreas[1].mPoints, triangle);
    }

    TEST(ESM4RegionTest, aDeletedRegionCoversNothing)
    {
        const ESM4::Region region = loadRegion(area(0, square), deletedRecord);

        EXPECT_FALSE(region.covers(ESM::FormId(), 50, 50));
    }
}
