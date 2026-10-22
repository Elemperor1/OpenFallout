#include <components/terrain/lodindex.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Terrain
{
    namespace
    {
        using Kind = LodTileName::Kind;

        std::string terrainPath(std::string_view worldspace, int level, int x, int y)
        {
            return "meshes/landscape/lod/" + std::string(worldspace) + "/" + std::string(worldspace) + ".level"
                + std::to_string(level) + ".x" + std::to_string(x) + ".y" + std::to_string(y) + ".nif";
        }

        std::string objectsPath(std::string_view worldspace, int x, int y, bool high = false)
        {
            return "meshes/landscape/lod/" + std::string(worldspace) + "/blocks/" + std::string(worldspace) + ".level4"
                + (high ? ".high" : "") + ".x" + std::to_string(x) + ".y" + std::to_string(y) + ".nif";
        }

        std::vector<std::string> sorted(std::vector<std::string> names)
        {
            std::sort(names.begin(), names.end());
            return names;
        }

        std::vector<std::string> values(const std::vector<const LodTile*>& tiles)
        {
            std::vector<std::string> result;
            for (const LodTile* tile : tiles)
                result.push_back(tile->mPath.value());
            return sorted(std::move(result));
        }

        TEST(TerrainLodTileName, shouldParseTheNameOfATerrainTile)
        {
            const auto name = parseLodTileName("wasteland.level4.x-4.y-12", "wasteland", Kind::Terrain);
            ASSERT_TRUE(name.has_value());
            EXPECT_EQ(name->mKind, Kind::Terrain);
            EXPECT_EQ(name->mLevel, 4);
            EXPECT_FALSE(name->mHigh);
            EXPECT_EQ(name->mX, -4);
            EXPECT_EQ(name->mY, -12);
        }

        TEST(TerrainLodTileName, shouldParseTheNameOfAHighBlockOfObjects)
        {
            const auto name = parseLodTileName("wastelandnv.level4.high.x24.y0", "wastelandnv", Kind::Objects);
            ASSERT_TRUE(name.has_value());
            EXPECT_EQ(name->mKind, Kind::Objects);
            EXPECT_EQ(name->mLevel, 4);
            EXPECT_TRUE(name->mHigh);
            EXPECT_EQ(name->mX, 24);
            EXPECT_EQ(name->mY, 0);
        }

        TEST(TerrainLodTileName, shouldRejectNamesThatAreNotTiles)
        {
            EXPECT_FALSE(parseLodTileName("wasteland.level4.x-4.y-4", "wastelandnv", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wastelandnv.level4.x-4.y-4", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.level5.x0.y0", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.level2.x0.y0", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.level64.x0.y0", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.level4.high.x0.y0", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.level4.x0", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.level4.x0.y0.old", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.level4.xa.y0", "wasteland", Kind::Terrain));
            EXPECT_FALSE(parseLodTileName("wasteland.postapocalypse.level4.x0.y0", "wasteland", Kind::Objects));
        }

        TEST(TerrainLodIndex, shouldTakeOnlyTheFilesOfTheWorldspace)
        {
            LodIndex index;
            EXPECT_TRUE(index.add(VFS::Path::Normalized(terrainPath("wasteland", 4, -4, -4)), "Wasteland"));
            EXPECT_TRUE(index.add(VFS::Path::Normalized(objectsPath("wasteland", 8, 8)), "Wasteland"));
            EXPECT_FALSE(index.add(VFS::Path::Normalized(terrainPath("wastelandnv", 4, 0, 0)), "Wasteland"));
            EXPECT_FALSE(
                index.add(VFS::Path::Normalized("meshes/landscape/lod/wasteland/trees/wasteland.level8.x0.y0.dtl"),
                    "Wasteland"));
            EXPECT_FALSE(index.add(
                VFS::Path::Normalized("meshes/landscape/lod/wasteland/trees/wasteland.level4.x0.y0.nif"), "Wasteland"));
            EXPECT_FALSE(index.add(VFS::Path::Normalized("meshes/landscape/lod/wasteland/treetypes.lst"), "Wasteland"));
            EXPECT_FALSE(index.add(VFS::Path::Normalized("meshes/architecture/wasteland.level4.x0.y0.nif"), "Wasteland"));

            EXPECT_FALSE(index.empty());
            EXPECT_EQ(index.getTerrainCount(), 1u);
            EXPECT_EQ(index.getObjectCount(), 1u);
            ASSERT_NE(index.find(Kind::Terrain, 4, false, -4, -4), nullptr);
            EXPECT_EQ(index.find(Kind::Terrain, 4, false, -4, -4)->mPath.value(), terrainPath("wasteland", 4, -4, -4));
            EXPECT_EQ(index.find(Kind::Terrain, 8, false, -4, -4), nullptr);
            EXPECT_EQ(index.find(Kind::Objects, 4, true, 8, 8), nullptr);
            EXPECT_NE(index.find(Kind::Objects, 4, false, 8, 8), nullptr);
        }

        TEST(TerrainLodIndex, shouldBeEmptyWithoutFiles)
        {
            const LodIndex index;
            EXPECT_TRUE(index.empty());
            EXPECT_TRUE(index.getTerrain(0, 0, 32).empty());
            EXPECT_TRUE(index.getObjects(0, 0, 8, false).empty());
        }

        TEST(TerrainLodIndex, shouldAnswerWithTheTileOfTheSizeWhenThereIsOne)
        {
            LodIndex index;
            index.add(VFS::Path::Normalized(terrainPath("w", 8, 8, -8)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 8, -8)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 12, -8)), "w");

            EXPECT_EQ(values(index.getTerrain(8, -8, 8)), std::vector<std::string>{ terrainPath("w", 8, 8, -8) });
        }

        TEST(TerrainLodIndex, shouldFillASquareWithTheTilesOfHalfTheSize)
        {
            LodIndex index;
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 8, -8)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 12, -8)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 8, -4)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 16, -8)), "w");

            const std::vector<std::string> expected{ terrainPath("w", 4, 12, -8), terrainPath("w", 4, 8, -4),
                terrainPath("w", 4, 8, -8) };
            EXPECT_EQ(values(index.getTerrain(8, -8, 8)), sorted(expected));
        }

        TEST(TerrainLodIndex, shouldLookDownTheLevelsAndStopAtTheSmallest)
        {
            LodIndex index;
            index.add(VFS::Path::Normalized(terrainPath("w", 8, 0, 0)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 16, 0)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 20, 4)), "w");

            const std::vector<std::string> expected{ terrainPath("w", 8, 0, 0), terrainPath("w", 4, 16, 0),
                terrainPath("w", 4, 20, 4) };
            EXPECT_EQ(values(index.getTerrain(0, 0, 32)), sorted(expected));
            EXPECT_TRUE(index.getTerrain(0, 0, 2).empty());
        }

        TEST(TerrainLodIndex, shouldSplitASquareAboveTheLargestLevelIntoTilesOfThatLevel)
        {
            LodIndex index;
            index.add(VFS::Path::Normalized(terrainPath("w", 32, 0, 0)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 32, 32, 32)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 32, 64, 0)), "w");

            const std::vector<std::string> expected{ terrainPath("w", 32, 0, 0), terrainPath("w", 32, 32, 32) };
            EXPECT_EQ(values(index.getTerrain(0, 0, 64)), sorted(expected));
        }

        TEST(TerrainLodIndex, shouldTellHowMuchOfASquareTheTilesFill)
        {
            using Coverage = LodIndex::Coverage;
            LodIndex index;
            EXPECT_EQ(index.getTerrainCoverage(8, -8, 8), Coverage::None);

            // three of the four quarters
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 8, -8)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 12, -8)), "w");
            index.add(VFS::Path::Normalized(terrainPath("w", 4, 8, -4)), "w");
            EXPECT_EQ(index.getTerrainCoverage(8, -8, 8), Coverage::Partial);
            EXPECT_EQ(index.getTerrainCoverage(8, -8, 4), Coverage::Full);
            EXPECT_EQ(index.getTerrainCoverage(12, -4, 4), Coverage::None);

            index.add(VFS::Path::Normalized(terrainPath("w", 4, 12, -4)), "w");
            EXPECT_EQ(index.getTerrainCoverage(8, -8, 8), Coverage::Full);
            // a square twice as large, with only one of its four quarters
            EXPECT_EQ(index.getTerrainCoverage(8, -8, 16), Coverage::Partial);
        }

        TEST(TerrainLodIndex, shouldCountATileOfTheSizeAsFullCoverage)
        {
            LodIndex index;
            index.add(VFS::Path::Normalized(terrainPath("w", 16, 0, 0)), "w");
            EXPECT_EQ(index.getTerrainCoverage(0, 0, 16), LodIndex::Coverage::Full);
            EXPECT_EQ(index.getTerrainCoverage(0, 0, 8), LodIndex::Coverage::None);
        }

        TEST(TerrainLodIndex, shouldKeepTheTwoSetsOfObjectBlocksApart)
        {
            LodIndex index;
            index.add(VFS::Path::Normalized(objectsPath("w", -12, 24)), "w");
            index.add(VFS::Path::Normalized(objectsPath("w", -12, 24, true)), "w");
            index.add(VFS::Path::Normalized(objectsPath("w", -8, 24, true)), "w");

            EXPECT_EQ(values(index.getObjects(-12, 24, 4, false)), std::vector<std::string>{ objectsPath("w", -12, 24) });
            EXPECT_EQ(
                values(index.getObjects(-12, 24, 4, true)), std::vector<std::string>{ objectsPath("w", -12, 24, true) });
            EXPECT_EQ(values(index.getObjects(-12, 24, 8, true)),
                sorted({ objectsPath("w", -12, 24, true), objectsPath("w", -8, 24, true) }));
        }
    }
}
