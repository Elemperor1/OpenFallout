#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadwrld.hpp>

#include "apps/openfallout/mwworld/cell.hpp"

namespace OFWorld
{
    namespace
    {
        // The values the files of New Vegas hold for a cell without water of its own
        constexpr float sLargestFloat = std::numeric_limits<float>::max();
        constexpr float sSmallestInteger = -2147483648.f;

        ESM4::Cell makeCell(std::uint16_t flags, float height)
        {
            ESM4::Cell cell;
            cell.mCellFlags = flags;
            cell.mWaterHeight = height;
            return cell;
        }

        ESM4::Cell exterior(bool hasWater, float height)
        {
            return makeCell(hasWater ? ESM4::CELL_HasWater : 0, height);
        }

        ESM4::Cell interior(bool hasWater, float height)
        {
            return makeCell(ESM4::CELL_Interior | (hasWater ? ESM4::CELL_HasWater : 0), height);
        }

        ESM4::World makeWorld(float waterLevel)
        {
            ESM4::World world;
            world.mWaterLevel = waterLevel;
            return world;
        }

        TEST(ESM4CellWaterHeightTest, aHeightOfWaterIsAHeight)
        {
            EXPECT_TRUE(ESM4::Cell::isWaterHeight(2600.f));
            EXPECT_TRUE(ESM4::Cell::isWaterHeight(-4200.f));
            EXPECT_TRUE(ESM4::Cell::isWaterHeight(0.f));
        }

        TEST(ESM4CellWaterHeightTest, aHeightBeyondTheSentinelOfAnUnsetCellIsAHeight)
        {
            // only the values the files use for no height are left out, not the range around them
            EXPECT_TRUE(ESM4::Cell::isWaterHeight(250000.f));
            EXPECT_TRUE(ESM4::Cell::isWaterHeight(-250000.f));
            EXPECT_TRUE(ESM4::Cell::isWaterHeight(2.0e9f));
        }

        TEST(ESM4CellWaterHeightTest, theValuesOfACellWithoutWaterAreNoHeights)
        {
            EXPECT_FALSE(ESM4::Cell::isWaterHeight(sLargestFloat));
            EXPECT_FALSE(ESM4::Cell::isWaterHeight(sSmallestInteger));
            EXPECT_FALSE(ESM4::Cell::isWaterHeight(ESM4::Cell::sInvalidWaterLevel));
            EXPECT_FALSE(ESM4::Cell::isWaterHeight(std::numeric_limits<float>::infinity()));
            EXPECT_FALSE(ESM4::Cell::isWaterHeight(std::numeric_limits<float>::quiet_NaN()));
        }

        TEST(ESM4CellWaterHeightTest, aCellWithoutTheSubRecordHasNoHeight)
        {
            EXPECT_FALSE(ESM4::Cell().hasWaterHeight());
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellWithAHeightOfItsOwnHasWaterAtThatHeight)
        {
            const ESM4::World world = makeWorld(-2300.f);
            const CellWater water = resolveCellWater(exterior(true, 2600.f), &world);
            EXPECT_TRUE(water.mHasWater);
            EXPECT_EQ(water.mHeight, 2600.f);
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellWithoutAHeightHasTheDefaultHeightOfItsWorldspace)
        {
            const ESM4::World world = makeWorld(-2300.f);
            const CellWater water = resolveCellWater(exterior(true, sLargestFloat), &world);
            EXPECT_TRUE(water.mHasWater);
            EXPECT_EQ(water.mHeight, -2300.f);
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellWithoutTheFlagHasNoWater)
        {
            const ESM4::World world = makeWorld(-2300.f);
            EXPECT_FALSE(resolveCellWater(exterior(false, 2600.f), &world).mHasWater);
            EXPECT_FALSE(resolveCellWater(exterior(false, sLargestFloat), &world).mHasWater);
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellOfAGameThatDoesNotFlagItsWaterHasTheWaterOfItsWorldspace)
        {
            // Oblivion and Skyrim use the flag for interior cells only
            const ESM4::World world = makeWorld(-2300.f);
            ESM4::Cell cell = exterior(false, sLargestFloat);
            cell.mExteriorWaterIsFlagged = false;
            const CellWater water = resolveCellWater(cell, &world);
            EXPECT_TRUE(water.mHasWater);
            EXPECT_EQ(water.mHeight, -2300.f);

            ESM4::Cell inside = interior(false, 120.f);
            inside.mExteriorWaterIsFlagged = false;
            EXPECT_FALSE(resolveCellWater(inside, &world).mHasWater);
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellWithoutAHeightInAWorldspaceWithoutOneHasNoWater)
        {
            const ESM4::World world = makeWorld(sLargestFloat);
            EXPECT_FALSE(resolveCellWater(exterior(true, sLargestFloat), &world).mHasWater);
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellWithoutAWorldspaceKeepsItsOwnHeight)
        {
            EXPECT_TRUE(resolveCellWater(exterior(true, 2600.f), nullptr).mHasWater);
            EXPECT_FALSE(resolveCellWater(exterior(true, sLargestFloat), nullptr).mHasWater);
        }

        TEST(OFWorldResolveCellWaterTest, anInteriorCellWithAHeightHasWaterAtThatHeight)
        {
            const ESM4::World world = makeWorld(-2300.f);
            const CellWater water = resolveCellWater(interior(true, 120.f), &world);
            EXPECT_TRUE(water.mHasWater);
            EXPECT_EQ(water.mHeight, 120.f);
        }

        TEST(OFWorldResolveCellWaterTest, anInteriorCellDoesNotTakeTheHeightOfAWorldspace)
        {
            const ESM4::World world = makeWorld(-2300.f);
            EXPECT_FALSE(resolveCellWater(interior(true, sLargestFloat), &world).mHasWater);
            EXPECT_FALSE(resolveCellWater(interior(true, sSmallestInteger), &world).mHasWater);
        }

        TEST(OFWorldResolveCellWaterTest, anInteriorCellFlaggedForWaterWithoutAHeightHasNoWater)
        {
            EXPECT_FALSE(resolveCellWater(interior(true, ESM4::Cell::sInvalidWaterLevel), nullptr).mHasWater);
        }

        TEST(OFWorldResolveCellWaterTest, anInteriorCellWithoutTheFlagHasNoWater)
        {
            EXPECT_FALSE(resolveCellWater(interior(false, 120.f), nullptr).mHasWater);
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellTakesTheKindOfWaterOfItsWorldspaceUnlessItNamesOne)
        {
            ESM4::World world = makeWorld(-2300.f);
            world.mWater = ESM::FormId::fromUint32(0x00030009);

            ESM4::Cell cell = exterior(true, 2600.f);
            EXPECT_EQ(resolveCellWater(cell, &world).mType, world.mWater);

            cell.mWater = ESM::FormId::fromUint32(0x001009CA);
            EXPECT_EQ(resolveCellWater(cell, &world).mType, cell.mWater);
        }

        TEST(OFWorldResolveCellWaterTest, anInteriorCellHasNoKindOfWaterUnlessItNamesOne)
        {
            ESM4::World world = makeWorld(-2300.f);
            world.mWater = ESM::FormId::fromUint32(0x00030009);

            ESM4::Cell cell = interior(true, 120.f);
            EXPECT_TRUE(resolveCellWater(cell, &world).mType.isZeroOrUnset());

            cell.mWater = ESM::FormId::fromUint32(0x001009CA);
            EXPECT_EQ(resolveCellWater(cell, &world).mType, cell.mWater);
        }

        TEST(OFWorldResolveCellWaterTest, anExteriorCellWithoutAWorldspaceHasNoKindOfWaterUnlessItNamesOne)
        {
            ESM4::Cell cell = exterior(true, 2600.f);
            EXPECT_TRUE(resolveCellWater(cell, nullptr).mType.isZeroOrUnset());

            cell.mWater = ESM::FormId::fromUint32(0x001009CA);
            EXPECT_EQ(resolveCellWater(cell, nullptr).mType, cell.mWater);
        }
    }
}
