#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <map>

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

        // The worldspaces of a plugin by their form ids, as the lookup of a store gives them
        std::function<const ESM4::World*(ESM::FormId)> lookup(const std::map<ESM::FormId, ESM4::World>& worlds)
        {
            return [&worlds](ESM::FormId id) -> const ESM4::World* {
                const auto it = worlds.find(id);
                return it != worlds.end() ? &it->second : nullptr;
            };
        }

        ESM4::World makeChild(
            ESM::FormId id, ESM::FormId parent, std::uint16_t useFlags, float waterLevel, ESM::FormId water)
        {
            ESM4::World world = makeWorld(waterLevel);
            world.mId = id;
            world.mParent = parent;
            world.mParentUseFlags = useFlags;
            world.mWater = water;
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

        TEST(OFWorldResolveCellWaterTest, aWorldspaceThatUsesTheWaterOfItsParentHasThePartsOfItsParent)
        {
            const ESM::FormId parentId = ESM::FormId::fromUint32(0x00000100);
            const ESM::FormId childId = ESM::FormId::fromUint32(0x00000200);
            const ESM::FormId parentWater = ESM::FormId::fromUint32(0x00030009);
            const ESM::FormId childWater = ESM::FormId::fromUint32(0x001009CA);
            std::map<ESM::FormId, ESM4::World> worlds;
            worlds[parentId] = makeChild(parentId, ESM::FormId(), 0, -2300.f, parentWater);
            worlds[childId] = makeChild(childId, parentId, ESM4::World::UseFlag_Water, 500.f, childWater);

            const ESM4::Cell cell = exterior(true, sLargestFloat);
            const CellWater water = resolveCellWater(cell, &worlds[childId], lookup(worlds));
            EXPECT_TRUE(water.mHasWater);
            EXPECT_EQ(water.mHeight, -2300.f);
            EXPECT_EQ(water.mType, parentWater);
        }

        TEST(OFWorldResolveCellWaterTest, aCellOfTheWorldspaceThatNamesItsOwnWaterKeepsIt)
        {
            const ESM::FormId parentId = ESM::FormId::fromUint32(0x00000100);
            const ESM::FormId childId = ESM::FormId::fromUint32(0x00000200);
            std::map<ESM::FormId, ESM4::World> worlds;
            worlds[parentId] = makeChild(parentId, ESM::FormId(), 0, -2300.f, ESM::FormId::fromUint32(0x00030009));
            worlds[childId]
                = makeChild(childId, parentId, ESM4::World::UseFlag_Water, 500.f, ESM::FormId::fromUint32(0x001009CA));

            ESM4::Cell cell = exterior(true, 2600.f);
            cell.mWater = ESM::FormId::fromUint32(0x0010FFFF);
            const CellWater water = resolveCellWater(cell, &worlds[childId], lookup(worlds));
            EXPECT_EQ(water.mHeight, 2600.f);
            EXPECT_EQ(water.mType, cell.mWater);
        }

        TEST(OFWorldResolveCellWaterTest, aWorldspaceThatDoesNotUseTheWaterOfItsParentHasItsOwn)
        {
            const ESM::FormId parentId = ESM::FormId::fromUint32(0x00000100);
            const ESM::FormId childId = ESM::FormId::fromUint32(0x00000200);
            const ESM::FormId childWater = ESM::FormId::fromUint32(0x001009CA);
            std::map<ESM::FormId, ESM4::World> worlds;
            worlds[parentId] = makeChild(parentId, ESM::FormId(), 0, -2300.f, ESM::FormId::fromUint32(0x00030009));
            // the flag of the climate is not the one of the water
            worlds[childId] = makeChild(childId, parentId, ESM4::World::UseFlag_Climate, 500.f, childWater);

            const CellWater water = resolveCellWater(exterior(true, sLargestFloat), &worlds[childId], lookup(worlds));
            EXPECT_EQ(water.mHeight, 500.f);
            EXPECT_EQ(water.mType, childWater);
        }

        TEST(OFWorldResolveCellWaterTest, theWaterOfAWorldspaceComesFromTheTopOfTheChainOfParents)
        {
            const ESM::FormId topId = ESM::FormId::fromUint32(0x00000100);
            const ESM::FormId middleId = ESM::FormId::fromUint32(0x00000200);
            const ESM::FormId childId = ESM::FormId::fromUint32(0x00000300);
            const ESM::FormId topWater = ESM::FormId::fromUint32(0x00030009);
            std::map<ESM::FormId, ESM4::World> worlds;
            worlds[topId] = makeChild(topId, ESM::FormId(), 0, -2300.f, topWater);
            worlds[middleId]
                = makeChild(middleId, topId, ESM4::World::UseFlag_Water, 100.f, ESM::FormId::fromUint32(0x001009CA));
            worlds[childId]
                = makeChild(childId, middleId, ESM4::World::UseFlag_Water, 500.f, ESM::FormId::fromUint32(0x001009CB));

            const CellWater water = resolveCellWater(exterior(true, sLargestFloat), &worlds[childId], lookup(worlds));
            EXPECT_EQ(water.mHeight, -2300.f);
            EXPECT_EQ(water.mType, topWater);
        }

        TEST(OFWorldResolveCellWaterTest, aWorldspaceWhoseParentIsNotFoundHasItsOwnWater)
        {
            const ESM::FormId childId = ESM::FormId::fromUint32(0x00000200);
            const ESM::FormId childWater = ESM::FormId::fromUint32(0x001009CA);
            const std::map<ESM::FormId, ESM4::World> worlds;
            const ESM4::World child = makeChild(
                childId, ESM::FormId::fromUint32(0x00000100), ESM4::World::UseFlag_Water, 500.f, childWater);

            const CellWater water = resolveCellWater(exterior(true, sLargestFloat), &child, lookup(worlds));
            EXPECT_TRUE(water.mHasWater);
            EXPECT_EQ(water.mHeight, 500.f);
            EXPECT_EQ(water.mType, childWater);
        }

        TEST(OFWorldResolveCellWaterTest, aWorldspaceWithoutALookupHasItsOwnWater)
        {
            const ESM::FormId childId = ESM::FormId::fromUint32(0x00000200);
            const ESM4::World child = makeChild(childId, ESM::FormId::fromUint32(0x00000100),
                ESM4::World::UseFlag_Water, 500.f, ESM::FormId::fromUint32(0x001009CA));

            const CellWater water = resolveCellWater(exterior(true, sLargestFloat), &child);
            EXPECT_EQ(water.mHeight, 500.f);
            EXPECT_EQ(water.mType, child.mWater);
        }

        TEST(OFWorldResolveCellWaterTest, aLoopOfParentsEndsTheWalkWithoutHangingTheGame)
        {
            const ESM::FormId firstId = ESM::FormId::fromUint32(0x00000100);
            const ESM::FormId secondId = ESM::FormId::fromUint32(0x00000200);
            std::map<ESM::FormId, ESM4::World> worlds;
            worlds[firstId] = makeChild(
                firstId, secondId, ESM4::World::UseFlag_Water, -2300.f, ESM::FormId::fromUint32(0x00030009));
            worlds[secondId]
                = makeChild(secondId, firstId, ESM4::World::UseFlag_Water, 500.f, ESM::FormId::fromUint32(0x001009CA));

            const CellWater water = resolveCellWater(exterior(true, sLargestFloat), &worlds[firstId], lookup(worlds));
            EXPECT_TRUE(water.mHasWater);
            EXPECT_EQ(water.mHeight, 500.f);
        }

        TEST(OFWorldResolveCellWaterTest, anInteriorCellDoesNotTakeTheWaterOfTheParentsOfAWorldspace)
        {
            const ESM::FormId parentId = ESM::FormId::fromUint32(0x00000100);
            const ESM::FormId childId = ESM::FormId::fromUint32(0x00000200);
            std::map<ESM::FormId, ESM4::World> worlds;
            worlds[parentId] = makeChild(parentId, ESM::FormId(), 0, -2300.f, ESM::FormId::fromUint32(0x00030009));
            worlds[childId] = makeChild(childId, parentId, ESM4::World::UseFlag_Water, 500.f, ESM::FormId());

            const CellWater water = resolveCellWater(interior(true, 120.f), &worlds[childId], lookup(worlds));
            EXPECT_EQ(water.mHeight, 120.f);
            EXPECT_TRUE(water.mType.isZeroOrUnset());
        }
    }
}
