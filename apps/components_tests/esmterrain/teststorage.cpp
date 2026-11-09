#include <components/esmterrain/storage.hpp>

#include <components/esm/util.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadland.hpp>
#include <components/esm4/loadland.hpp>
#include <components/esm4/loadltex.hpp>
#include <components/esm4/loadtxst.hpp>
#include <components/misc/constants.hpp>
#include <components/testing/util.hpp>

#include <osg/Image>

#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ESMTerrain
{
    namespace
    {
        using namespace testing;
        using namespace TestingOpenMW;

        VFSTestFile file("");

        constexpr int sSize = ESM::Land::LAND_SIZE;
        constexpr int sSize4 = static_cast<int>(ESM4::Land::sVertsPerSide);
        constexpr int sDefaultHeight = ESM::Land::DEFAULT_HEIGHT;

        ESM::RefId esm3Space()
        {
            return ESM::Cell::sDefaultWorldspaceId;
        }

        ESM::RefId esm4Space()
        {
            return ESM::RefId::formIdRefId(ESM::FormId::fromUint32(0x3c));
        }

        // The land of the tests is in memory: the tests of the loaders have the files
        class TestStorage : public Storage
        {
        public:
            using Storage::Storage;

            std::map<std::pair<int, int>, std::shared_ptr<const LandObject>> mLands;
            std::map<std::pair<std::uint16_t, int>, VFS::Path::Normalized> mLandTextures;
            std::vector<std::pair<ESM::FormId, ESM4::LandTexture>> mEsm4LandTextures;
            std::vector<std::pair<ESM::FormId, ESM4::TextureSet>> mTextureSets;
            std::vector<std::pair<int, int>> mRequested;
            // The land of Fallout is where the record is: the object of it looks at the data of it
            std::vector<std::unique_ptr<ESM4::Land>> mOwned;

            void add(int x, int y, std::unique_ptr<ESM4::Land> land)
            {
                mLands[{ x, y }] = std::make_shared<const LandObject>(*land, 0);
                mOwned.push_back(std::move(land));
            }

            std::shared_ptr<const LandObject> getLand(ESM::ExteriorCellLocation cellLocation) override
            {
                mRequested.emplace_back(cellLocation.mX, cellLocation.mY);
                const auto it = mLands.find({ cellLocation.mX, cellLocation.mY });
                return it == mLands.end() ? nullptr : it->second;
            }

            const VFS::Path::Normalized* getLandTexture(std::uint16_t index, int plugin) override
            {
                const auto it = mLandTextures.find({ index, plugin });
                return it == mLandTextures.end() ? nullptr : &it->second;
            }

            const ESM4::LandTexture* getEsm4LandTexture(ESM::RefId ltexId) const override
            {
                for (const auto& [id, ltex] : mEsm4LandTextures)
                    if (ESM::RefId::formIdRefId(id) == ltexId)
                        return &ltex;
                return nullptr;
            }

            const ESM4::TextureSet* getEsm4TextureSet(ESM::RefId txstId) const override
            {
                for (const auto& [id, txst] : mTextureSets)
                    if (ESM::RefId::formIdRefId(id) == txstId)
                        return &txst;
                return nullptr;
            }

            void getBounds(float& minX, float& maxX, float& minY, float& maxY, ESM::RefId worldspace) override
            {
                minX = -1;
                maxX = 2;
                minY = -3;
                maxY = 4;
            }
        };

        // What OpenMW-CS does to the terrain it shows
        class AlteringStorage : public TestStorage
        {
        public:
            using TestStorage::TestStorage;

        private:
            bool useAlteration() const override { return true; }

            float getAlteredHeight(int col, int row) const override { return static_cast<float>(col * 2 + row); }

            void adjustColor(int col, int row, const ESM::LandData* heightData, osg::Vec4ub& color) const override
            {
                color.r() = heightData != nullptr ? 7 : 8;
            }
        };

        constexpr int allData
            = ESM::Land::DATA_VNML | ESM::Land::DATA_VHGT | ESM::Land::DATA_VCLR | ESM::Land::DATA_VTEX;

        ESM::Land makeEsm3Land(int x, int y, int plugin = 0)
        {
            ESM::Land land;
            land.mX = x;
            land.mY = y;
            land.blank();
            land.setPlugin(plugin);
            return land;
        }

        std::size_t vertex(int x, int y)
        {
            return static_cast<std::size_t>(y) * sSize + x;
        }

        void setHeights(ESM::Land& land, auto heightAt)
        {
            for (int y = 0; y < sSize; ++y)
                for (int x = 0; x < sSize; ++x)
                    land.mLandData->mHeights[vertex(x, y)] = heightAt(x, y);
        }

        void setFlatHeight(ESM::Land& land, float height)
        {
            setHeights(land, [&](int, int) { return height; });
        }

        void setNormal(ESM::Land& land, int x, int y, std::int8_t nx, std::int8_t ny, std::int8_t nz)
        {
            land.mLandData->mNormals[vertex(x, y) * 3 + 0] = nx;
            land.mLandData->mNormals[vertex(x, y) * 3 + 1] = ny;
            land.mLandData->mNormals[vertex(x, y) * 3 + 2] = nz;
        }

        void setColour(ESM::Land& land, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b)
        {
            land.mLandData->mColours[vertex(x, y) * 3 + 0] = r;
            land.mLandData->mColours[vertex(x, y) * 3 + 1] = g;
            land.mLandData->mColours[vertex(x, y) * 3 + 2] = b;
        }

        std::shared_ptr<const LandObject> object(const ESM::Land& land, int flags = allData)
        {
            return std::make_shared<const LandObject>(land, flags);
        }

        struct Buffers
        {
            osg::ref_ptr<osg::Vec3Array> mPositions = new osg::Vec3Array;
            osg::ref_ptr<osg::Vec3Array> mNormals = new osg::Vec3Array;
            osg::ref_ptr<osg::Vec4ubArray> mColours = new osg::Vec4ubArray;
        };

        Buffers fill(Storage& storage, int lod, float size, osg::Vec2f center, ESM::RefId worldspace)
        {
            Buffers result;
            storage.fillVertexBuffers(
                lod, size, center, worldspace, (*result.mPositions), (*result.mNormals), (*result.mColours));
            return result;
        }

        void expectNear(const osg::Vec3f& actual, const osg::Vec3f& expected)
        {
            EXPECT_NEAR(actual.x(), expected.x(), 1e-4f);
            EXPECT_NEAR(actual.y(), expected.y(), 1e-4f);
            EXPECT_NEAR(actual.z(), expected.z(), 1e-4f);
        }

        std::unique_ptr<VFS::Manager> makeVfs()
        {
            return createTestVFS({
                { VFS::Path::NormalizedView("textures/tx_sand.dds"), &file },
                { VFS::Path::NormalizedView("textures/tx_sand_nh.dds"), &file },
                { VFS::Path::NormalizedView("textures/tx_rock.dds"), &file },
                { VFS::Path::NormalizedView("textures/tx_rock_n.dds"), &file },
                { VFS::Path::NormalizedView("textures/tx_grass.dds"), &file },
                { VFS::Path::NormalizedView("textures/tx_grass_s.dds"), &file },
                { VFS::Path::NormalizedView("textures/tx_dirt.dds"), &file },
                { VFS::Path::NormalizedView("textures/landscape/dirtwasteland01.dds"), &file },
                { VFS::Path::NormalizedView("textures/landscape/dirtwasteland01_n.dds"), &file },
                { VFS::Path::NormalizedView("textures/landscape/oblivion.dds"), &file },
            });
        }

        TEST(ESMTerrainStorageTest, the_size_of_a_cell_and_its_grids_depend_on_the_worldspace)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());

            EXPECT_EQ(storage.getCellWorldSize(esm3Space()), 8192.f);
            EXPECT_EQ(storage.getCellVertices(esm3Space()), 65);
            EXPECT_EQ(storage.getTextureTileCount(1.f, esm3Space()), 16);
            EXPECT_EQ(storage.getTextureTileCount(0.5f, esm3Space()), 8);

            EXPECT_EQ(storage.getCellWorldSize(esm4Space()), 4096.f);
            EXPECT_EQ(storage.getCellVertices(esm4Space()), 33);
            EXPECT_EQ(storage.getTextureTileCount(1.f, esm4Space()), 12);
            EXPECT_EQ(storage.getTextureTileCount(2.f, esm4Space()), 24);
        }

        TEST(ESMTerrainStorageTest, the_heights_of_a_chunk_are_the_lowest_and_the_highest_of_its_vertices)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setHeights(land, [](int x, int y) { return static_cast<float>(x + 100 * y); });
            storage.mLands[{ 0, 0 }] = object(land);

            float min = 0;
            float max = 0;
            EXPECT_TRUE(storage.getMinMaxHeights(1.f, osg::Vec2f(0.5f, 0.5f), esm3Space(), min, max));
            EXPECT_EQ(min, 0.f);
            EXPECT_EQ(max, 64.f + 6400.f);

            // A quarter of the cell, with the bottom corner of the cell
            EXPECT_TRUE(storage.getMinMaxHeights(0.5f, osg::Vec2f(0.25f, 0.25f), esm3Space(), min, max));
            EXPECT_EQ(min, 0.f);
            EXPECT_EQ(max, 32.f + 3200.f);

            // And the opposite one
            EXPECT_TRUE(storage.getMinMaxHeights(0.5f, osg::Vec2f(0.75f, 0.75f), esm3Space(), min, max));
            EXPECT_EQ(min, 32.f + 3200.f);
            EXPECT_EQ(max, 64.f + 6400.f);
        }

        TEST(ESMTerrainStorageTest, a_chunk_that_has_no_land_is_flat_and_has_no_data)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setFlatHeight(land, 100.f);
            storage.mLands[{ 0, 0 }] = object(land);
            // The land of a cell that has its normals only has no heights
            ESM::Land noHeights = makeEsm3Land(1, 0);
            noHeights.mLandData->mDataLoaded = ESM::Land::DATA_VNML;
            storage.mLands[{ 1, 0 }] = object(noHeights);

            float min = 1;
            float max = 2;
            EXPECT_FALSE(storage.getMinMaxHeights(1.f, osg::Vec2f(5.5f, 5.5f), esm3Space(), min, max));
            EXPECT_EQ(min, sDefaultHeight);
            EXPECT_EQ(max, sDefaultHeight);

            min = 1;
            max = 2;
            EXPECT_FALSE(storage.getMinMaxHeights(1.f, osg::Vec2f(1.5f, 0.5f), esm3Space(), min, max));
            EXPECT_EQ(min, sDefaultHeight);
            EXPECT_EQ(max, sDefaultHeight);

            EXPECT_TRUE(storage.hasData(ESM::ExteriorCellLocation(0, 0, esm3Space())));
            EXPECT_FALSE(storage.hasData(ESM::ExteriorCellLocation(1, 0, esm3Space())));
            EXPECT_FALSE(storage.hasData(ESM::ExteriorCellLocation(7, 7, esm3Space())));
        }

        TEST(ESMTerrainStorageTest, the_vertices_of_a_chunk_cannot_be_asked_for_at_a_level_or_size_that_makes_no_sense)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());

            EXPECT_THROW(fill(storage, -1, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space()), std::invalid_argument);
            EXPECT_THROW(fill(storage, 64, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space()), std::invalid_argument);
            EXPECT_THROW(fill(storage, 0, 0.f, osg::Vec2f(0.5f, 0.5f), esm3Space()), std::invalid_argument);
            EXPECT_THROW(fill(storage, 0, -1.f, osg::Vec2f(0.5f, 0.5f), esm3Space()), std::invalid_argument);
        }

        TEST(ESMTerrainStorageTest, the_vertices_of_a_cell_have_its_heights_normals_and_colours)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setHeights(land, [](int x, int y) { return static_cast<float>(x + 100 * y); });
            setNormal(land, 10, 20, 127, 0, 127);
            setColour(land, 10, 20, 1, 2, 3);
            storage.mLands[{ 0, 0 }] = object(land);

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space());

            ASSERT_EQ((*result.mPositions).size(), 65u * 65u);
            ASSERT_EQ((*result.mNormals).size(), 65u * 65u);
            ASSERT_EQ((*result.mColours).size(), 65u * 65u);
            // The first index is the position along the row
            const std::size_t index = 10 * 65 + 20;
            expectNear((*result.mPositions)[index], osg::Vec3f(-4096.f + 10 * 128.f, -4096.f + 20 * 128.f, 2010.f));
            expectNear((*result.mNormals)[index], osg::Vec3f(1.f, 0.f, 1.f) / std::sqrt(2.f));
            EXPECT_EQ((*result.mColours)[index], osg::Vec4ub(1, 2, 3, 255));

            // Where nothing was given a normal, it is straight up, and where nothing was given a colour, it is white
            const std::size_t other = 30 * 65 + 40;
            expectNear((*result.mNormals)[other], osg::Vec3f(0.f, 0.f, 1.f));
            EXPECT_EQ((*result.mColours)[other], osg::Vec4ub(255, 255, 255, 255));
            EXPECT_EQ((*result.mPositions)[other].z(), 40.f * 100.f + 30.f);
        }

        TEST(ESMTerrainStorageTest, a_cell_without_land_is_drawn_at_the_default_height)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space());

            ASSERT_EQ((*result.mPositions).size(), 65u * 65u);
            for (std::size_t i = 0; i < (*result.mPositions).size(); ++i)
            {
                EXPECT_EQ((*result.mPositions)[i].z(), sDefaultHeight);
                expectNear((*result.mNormals)[i], osg::Vec3f(0.f, 0.f, 1.f));
                EXPECT_EQ((*result.mColours)[i], osg::Vec4ub(255, 255, 255, 255));
            }
        }

        TEST(ESMTerrainStorageTest, a_cell_that_has_no_normals_or_colours_has_them_by_default)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setFlatHeight(land, 12.f);
            setColour(land, 5, 5, 9, 9, 9);
            setNormal(land, 5, 5, 127, 0, 127);
            land.mLandData->mDataLoaded = ESM::Land::DATA_VHGT;
            storage.mLands[{ 0, 0 }] = object(land);

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space());

            const std::size_t index = 5 * 65 + 5;
            EXPECT_EQ((*result.mPositions)[index].z(), 12.f);
            expectNear((*result.mNormals)[index], osg::Vec3f(0.f, 0.f, 1.f));
            EXPECT_EQ((*result.mColours)[index], osg::Vec4ub(255, 255, 255, 255));
        }

        TEST(ESMTerrainStorageTest, the_edges_of_a_cell_have_the_normals_and_colours_of_the_next_cell)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setFlatHeight(land, 1.f);
            for (int i = 0; i < sSize; ++i)
            {
                setColour(land, i, 0, 10, 10, 10);
                setColour(land, i, 64, 11, 11, 11);
                setColour(land, 64, i, 12, 12, 12);
            }
            storage.mLands[{ 0, 0 }] = object(land);

            // The cell above, which the top edge is the first row of
            ESM::Land above = makeEsm3Land(0, 1);
            setFlatHeight(above, 1.f);
            setNormal(above, 10, 0, 0, 127, 127);
            setColour(above, 10, 0, 200, 100, 50);
            storage.mLands[{ 0, 1 }] = object(above);
            // And the cell to the right
            ESM::Land right = makeEsm3Land(1, 0);
            setFlatHeight(right, 1.f);
            setNormal(right, 0, 20, 127, 0, 127);
            setColour(right, 0, 20, 5, 6, 7);
            storage.mLands[{ 1, 0 }] = object(right);

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space());

            const std::size_t top = 10 * 65 + 64;
            expectNear((*result.mNormals)[top], osg::Vec3f(0.f, 1.f, 1.f) / std::sqrt(2.f));
            EXPECT_EQ((*result.mColours)[top], osg::Vec4ub(200, 100, 50, 255));
            const std::size_t side = 64 * 65 + 20;
            expectNear((*result.mNormals)[side], osg::Vec3f(1.f, 0.f, 1.f) / std::sqrt(2.f));
            EXPECT_EQ((*result.mColours)[side], osg::Vec4ub(5, 6, 7, 255));
            // Where there is no cell next to it, the colour is white, as the normal is straight up
            const std::size_t noNeighbour = 30 * 65 + 64;
            expectNear((*result.mNormals)[noNeighbour], osg::Vec3f(0.f, 0.f, 1.f));
            EXPECT_EQ((*result.mColours)[noNeighbour], osg::Vec4ub(255, 255, 255, 255));
        }

        TEST(ESMTerrainStorageTest, the_normal_of_a_corner_is_the_average_of_those_around_it_as_the_data_is_garbage)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setFlatHeight(land, 1.f);
            setNormal(land, 0, 0, 0, 0, -127);
            setNormal(land, 1, 0, 127, 0, 127);
            setNormal(land, 0, 1, 0, 127, 127);
            storage.mLands[{ 0, 0 }] = object(land);

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space());

            osg::Vec3f expected = osg::Vec3f(1.f, 0.f, 1.f) / std::sqrt(2.f)
                + osg::Vec3f(0.f, 1.f, 1.f) / std::sqrt(2.f) + osg::Vec3f(0.f, 0.f, 2.f);
            expected.normalize();
            expectNear((*result.mNormals)[0], expected);
            EXPECT_GT((*result.mNormals)[0].z(), 0.f);
            // The corners that no cell has anything next to are straight up
            expectNear((*result.mNormals)[64 * 65 + 64], osg::Vec3f(0.f, 0.f, 1.f));
        }

        TEST(ESMTerrainStorageTest, a_level_of_detail_keeps_every_nth_vertex)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setHeights(land, [](int x, int y) { return static_cast<float>(x + 100 * y); });
            storage.mLands[{ 0, 0 }] = object(land);

            const Buffers result = fill(storage, 2, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space());

            ASSERT_EQ((*result.mPositions).size(), 17u * 17u);
            EXPECT_EQ((*result.mPositions)[0].z(), 0.f);
            EXPECT_EQ((*result.mPositions)[3 * 17 + 5].z(), 12.f + 2000.f);
            EXPECT_EQ((*result.mPositions)[3 * 17 + 5].x(), (3.f / 16.f - 0.5f) * 8192.f);
            EXPECT_EQ((*result.mPositions)[3 * 17 + 5].y(), (5.f / 16.f - 0.5f) * 8192.f);
        }

        TEST(ESMTerrainStorageTest, a_chunk_of_several_cells_has_the_land_of_each_of_them)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            const float heights[2][2] = { { 10.f, 20.f }, { 30.f, 40.f } };
            for (int y = 0; y < 2; ++y)
            {
                for (int x = 0; x < 2; ++x)
                {
                    ESM::Land land = makeEsm3Land(x, y);
                    setFlatHeight(land, heights[y][x]);
                    storage.mLands[{ x, y }] = object(land);
                }
            }

            const Buffers result = fill(storage, 0, 2.f, osg::Vec2f(1.f, 1.f), esm3Space());

            ASSERT_EQ((*result.mPositions).size(), 129u * 129u);
            EXPECT_EQ((*result.mPositions)[5 * 129 + 5].z(), 10.f);
            EXPECT_EQ((*result.mPositions)[100 * 129 + 5].z(), 20.f);
            EXPECT_EQ((*result.mPositions)[5 * 129 + 100].z(), 30.f);
            EXPECT_EQ((*result.mPositions)[100 * 129 + 100].z(), 40.f);
            EXPECT_EQ((*result.mPositions)[0].x(), -8192.f);
            EXPECT_EQ((*result.mPositions)[128 * 129 + 128].x(), 8192.f);
            // The land of a cell is not asked for again and again
            std::size_t asked = 0;
            for (const auto& cell : storage.mRequested)
                asked += cell == std::pair{ 0, 0 } ? 1 : 0;
            EXPECT_LE(asked, 1u);
        }

        TEST(ESMTerrainStorageTest, the_editor_can_alter_the_heights_and_colours_of_the_vertices)
        {
            const auto vfs = makeVfs();
            AlteringStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setFlatHeight(land, 100.f);
            storage.mLands[{ 0, 0 }] = object(land);

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm3Space());

            // The row and column are given, as the first index is the row
            const std::size_t index = 10 * 65 + 20;
            EXPECT_EQ((*result.mPositions)[index].z(), 100.f + 20.f * 2 + 10.f);
            EXPECT_EQ((*result.mColours)[index].r(), 7);
            EXPECT_EQ((*result.mColours)[index].g(), 255);
        }

        TEST(ESMTerrainStorageTest, the_height_at_a_place_is_between_the_heights_of_the_vertices_around_it)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            setHeights(land, [](int x, int) { return 10.f * x; });
            storage.mLands[{ 0, 0 }] = object(land);
            ESM::Land second = makeEsm3Land(-1, -1);
            setFlatHeight(second, 77.f);
            storage.mLands[{ -1, -1 }] = object(second);

            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(4096.f, 4096.f, 0.f), esm3Space()), 320.f, 0.01f);
            // The vertices are 128 units apart
            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(4096.f + 64.f, 100.f, 0.f), esm3Space()), 325.f, 0.01f);
            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(4096.f + 32.f, 4000.f, 0.f), esm3Space()), 322.5f, 0.01f);
            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(0.f, 0.f, 0.f), esm3Space()), 0.f, 0.01f);
            // The far edge is where the last vertex is
            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(8191.9f, 5.f, 0.f), esm3Space()), 640.f, 0.2f);
            // A cell at a negative position is found
            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(-100.f, -100.f, 0.f), esm3Space()), 77.f, 0.01f);
        }

        TEST(ESMTerrainStorageTest, the_height_where_there_is_no_land_depends_on_the_worldspace)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land noHeights = makeEsm3Land(1, 1);
            noHeights.mLandData->mDataLoaded = ESM::Land::DATA_VNML;
            storage.mLands[{ 1, 1 }] = object(noHeights);

            EXPECT_EQ(storage.getHeightAt(osg::Vec3f(100.f, 100.f, 0.f), esm3Space()), sDefaultHeight);
            EXPECT_EQ(storage.getHeightAt(osg::Vec3f(9000.f, 9000.f, 0.f), esm3Space()), sDefaultHeight);
            EXPECT_EQ(
                storage.getHeightAt(osg::Vec3f(100.f, 100.f, 0.f), esm4Space()), std::numeric_limits<float>::lowest());
        }

        TEST(ESMTerrainStorageTest, a_land_that_has_only_one_texture_needs_no_blending)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm3Space());

            EXPECT_TRUE(blendmaps.empty());
            ASSERT_EQ(layers.size(), 1u);
            EXPECT_EQ(layers[0].mDiffuseMap.value(), "textures/_land_default.dds");
            EXPECT_TRUE(layers[0].mNormalMap.empty());
            EXPECT_FALSE(layers[0].mParallax);
            EXPECT_FALSE(layers[0].mSpecular);
        }

        TEST(ESMTerrainStorageTest, a_layer_is_given_a_blendmap_that_is_opaque_where_its_texture_is)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            storage.mLandTextures[{ 4, 0 }] = VFS::Path::Normalized("textures/tx_sand.dds");
            ESM::Land land = makeEsm3Land(0, 0);
            // The numbers are what the land records give: one more than the number of the land texture
            land.mLandData->mTextures[2 * 16 + 3] = 5;
            storage.mLands[{ 0, 0 }] = object(land);

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm3Space());

            ASSERT_EQ(layers.size(), 2u);
            ASSERT_EQ(blendmaps.size(), 2u);
            EXPECT_EQ(layers[0].mDiffuseMap.value(), "textures/_land_default.dds");
            EXPECT_EQ(layers[1].mDiffuseMap.value(), "textures/tx_sand.dds");
            // 17 samples are 34 pixels, as the blendmap is made twice as large
            for (const auto& image : blendmaps)
            {
                EXPECT_EQ(image->s(), 34);
                EXPECT_EQ(image->t(), 34);
            }
            // The tile is 3 to the right and 2 up, and the samples start a tile before the cell
            const auto pixel = [&](std::size_t layer, int x, int y) { return blendmaps[layer]->data()[y * 34 + x]; };
            for (int dy = 0; dy < 2; ++dy)
            {
                for (int dx = 0; dx < 2; ++dx)
                {
                    EXPECT_EQ(pixel(1, 8 + dx, 4 + dy), 255);
                    EXPECT_EQ(pixel(0, 8 + dx, 4 + dy), 0);
                }
            }
            EXPECT_EQ(pixel(1, 0, 0), 0);
            EXPECT_EQ(pixel(0, 0, 0), 255);
            EXPECT_EQ(pixel(1, 10, 4), 0);
            EXPECT_EQ(pixel(0, 10, 4), 255);
        }

        TEST(ESMTerrainStorageTest, a_texture_that_the_plugins_have_under_two_numbers_is_one_layer)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            storage.mLandTextures[{ 4, 0 }] = VFS::Path::Normalized("textures/tx_sand.dds");
            storage.mLandTextures[{ 7, 1 }] = VFS::Path::Normalized("textures/tx_sand.dds");
            ESM::Land first = makeEsm3Land(0, 0, 0);
            first.mLandData->mTextures[3] = 5;
            storage.mLands[{ 0, 0 }] = object(first);
            ESM::Land second = makeEsm3Land(0, 1, 1);
            second.mLandData->mTextures[2] = 8;
            storage.mLands[{ 0, 1 }] = object(second);

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm3Space());

            ASSERT_EQ(layers.size(), 2u);
            EXPECT_EQ(blendmaps.size(), 2u);
            EXPECT_EQ(layers[1].mDiffuseMap.value(), "textures/tx_sand.dds");
        }

        TEST(ESMTerrainStorageTest, a_texture_that_no_plugin_has_is_replaced_by_the_default_one)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            ESM::Land land = makeEsm3Land(0, 0);
            land.mLandData->mTextures[10] = 9;
            storage.mLands[{ 0, 0 }] = object(land);

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm3Space());

            // The default texture is all that is there
            EXPECT_TRUE(blendmaps.empty());
            ASSERT_EQ(layers.size(), 1u);
            EXPECT_EQ(layers[0].mDiffuseMap.value(), "textures/_land_default.dds");
        }

        TEST(ESMTerrainStorageTest,
            the_normal_and_specular_maps_of_a_texture_are_those_that_have_its_name_and_the_pattern)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get(), "_n", "_nh", true, "_s", true);
            storage.mLandTextures[{ 0, 0 }] = VFS::Path::Normalized("textures/tx_sand.dds");
            storage.mLandTextures[{ 1, 0 }] = VFS::Path::Normalized("textures/tx_rock.dds");
            storage.mLandTextures[{ 2, 0 }] = VFS::Path::Normalized("textures/tx_grass.dds");
            storage.mLandTextures[{ 3, 0 }] = VFS::Path::Normalized("textures/tx_dirt.dds");
            ESM::Land land = makeEsm3Land(0, 0);
            land.mLandData->mTextures[0 * 16 + 0] = 1;
            land.mLandData->mTextures[1 * 16 + 0] = 2;
            land.mLandData->mTextures[2 * 16 + 0] = 3;
            land.mLandData->mTextures[3 * 16 + 0] = 4;
            storage.mLands[{ 0, 0 }] = object(land);

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm3Space());

            ASSERT_EQ(layers.size(), 5u);
            ASSERT_EQ(blendmaps.size(), 5u);
            // The default texture has nothing
            EXPECT_TRUE(layers[0].mNormalMap.empty());
            // A normal map that has the height is used for parallax
            EXPECT_EQ(layers[1].mDiffuseMap.value(), "textures/tx_sand.dds");
            EXPECT_EQ(layers[1].mNormalMap.value(), "textures/tx_sand_nh.dds");
            EXPECT_TRUE(layers[1].mParallax);
            EXPECT_FALSE(layers[1].mSpecular);
            EXPECT_TRUE(layers[1].requiresShaders());
            // One that has not is not
            EXPECT_EQ(layers[2].mDiffuseMap.value(), "textures/tx_rock.dds");
            EXPECT_EQ(layers[2].mNormalMap.value(), "textures/tx_rock_n.dds");
            EXPECT_FALSE(layers[2].mParallax);
            // A specular map takes the place of the diffuse map
            EXPECT_EQ(layers[3].mDiffuseMap.value(), "textures/tx_grass_s.dds");
            EXPECT_TRUE(layers[3].mSpecular);
            EXPECT_TRUE(layers[3].mNormalMap.empty());
            EXPECT_TRUE(layers[3].requiresShaders());
            // A texture that has neither is as it is
            EXPECT_EQ(layers[4].mDiffuseMap.value(), "textures/tx_dirt.dds");
            EXPECT_TRUE(layers[4].mNormalMap.empty());
            EXPECT_FALSE(layers[4].requiresShaders());
        }

        // The land of Fallout 3 and the games after it
        std::unique_ptr<ESM4::Land> makeEsm4Land(int plugin = 0)
        {
            auto land = std::make_unique<ESM4::Land>();
            land->mId = ESM::FormId{ 0x100, plugin };
            land->mDataTypes = ESM4::Land::LAND_VNML | ESM4::Land::LAND_VHGT | ESM4::Land::LAND_VCLR;
            land->mHeightMap.heightOffset = 1.f;
            for (int i = 0; i < sSize4 * sSize4; ++i)
                land->mVertNorm[i * 3 + 2] = 127;
            for (int i = 0; i < sSize4 * sSize4 * 3; ++i)
                land->mVertColr[i] = static_cast<std::uint8_t>(i % 3 + 10);
            return land;
        }

        TEST(ESMTerrainStorageTest, the_land_of_a_cell_of_fallout_has_its_own_vertices)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            auto land = makeEsm4Land();
            // Each vertex is one more than the one before in a row: 8 units
            for (int y = 0; y < sSize4; ++y)
                for (int x = 1; x < sSize4; ++x)
                    land->mHeightMap.gradientData[y * sSize4 + x] = 1;
            land->mVertNorm[(5 * sSize4 + 6) * 3 + 0] = 127;
            land->mVertColr[(5 * sSize4 + 6) * 3 + 0] = 200;
            storage.add(0, 0, std::move(land));

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm4Space());

            ASSERT_EQ((*result.mPositions).size(), 33u * 33u);
            const std::size_t index = 6 * 33 + 5;
            // A cell is 4096 units, and its vertices are 128 apart
            expectNear(
                (*result.mPositions)[index], osg::Vec3f(-2048.f + 6 * 128.f, -2048.f + 5 * 128.f, 8.f * (1 + 6)));
            expectNear((*result.mNormals)[index], osg::Vec3f(1.f, 0.f, 1.f) / std::sqrt(2.f));
            EXPECT_EQ((*result.mColours)[index], osg::Vec4ub(200, 11, 12, 255));
            expectNear((*result.mNormals)[0], osg::Vec3f(0.f, 0.f, 1.f));
            EXPECT_EQ((*result.mPositions)[0].z(), 8.f);
            EXPECT_EQ((*result.mPositions)[32 * 33].z(), 8.f * 33);

            float min = 0;
            float max = 0;
            EXPECT_TRUE(storage.getMinMaxHeights(1.f, osg::Vec2f(0.5f, 0.5f), esm4Space(), min, max));
            EXPECT_EQ(min, 8.f);
            EXPECT_EQ(max, 8.f * 33);
            EXPECT_TRUE(storage.getMinMaxHeights(0.5f, osg::Vec2f(0.75f, 0.75f), esm4Space(), min, max));
            EXPECT_EQ(min, 8.f * 17);
            EXPECT_EQ(max, 8.f * 33);

            // The height under a place is where the plane of its triangle is
            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(2048.f, 2048.f, 0.f), esm4Space()), 8.f * 17, 0.01f);
            EXPECT_NEAR(storage.getHeightAt(osg::Vec3f(2048.f + 64.f, 100.f, 0.f), esm4Space()), 8.f * 17.5f, 0.01f);
        }

        TEST(ESMTerrainStorageTest, a_cell_of_fallout_that_has_no_land_at_all_is_not_drawn)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());

            const Buffers result = fill(storage, 0, 1.f, osg::Vec2f(0.5f, 0.5f), esm4Space());

            ASSERT_EQ((*result.mPositions).size(), 33u * 33u);
            for (const osg::Vec3f& position : (*result.mPositions))
                EXPECT_EQ(position, osg::Vec3f());
            EXPECT_EQ(storage.mRequested.empty(), false);

            // One that is next to land has the land to draw
            auto land = makeEsm4Land();
            storage.add(1, 0, std::move(land));
            const Buffers next = fill(storage, 0, 1.f, osg::Vec2f(1.5f, 0.5f), esm4Space());
            EXPECT_EQ((*next.mPositions)[7].z(), 8.f);
        }

        TEST(ESMTerrainStorageTest, the_land_of_fallout_in_a_chunk_of_cells_is_that_of_each_cell)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            for (int y = 0; y < 2; ++y)
            {
                for (int x = 0; x < 2; ++x)
                {
                    auto land = makeEsm4Land();
                    land->mHeightMap.heightOffset = static_cast<float>(1 + x + 10 * y);
                    storage.add(x, y, std::move(land));
                }
            }

            const Buffers result = fill(storage, 1, 2.f, osg::Vec2f(1.f, 1.f), esm4Space());

            ASSERT_EQ((*result.mPositions).size(), 33u * 33u);
            EXPECT_EQ((*result.mPositions)[3 * 33 + 3].z(), 8.f * 1);
            EXPECT_EQ((*result.mPositions)[28 * 33 + 3].z(), 8.f * 2);
            EXPECT_EQ((*result.mPositions)[3 * 33 + 28].z(), 8.f * 11);
            EXPECT_EQ((*result.mPositions)[28 * 33 + 28].z(), 8.f * 12);
        }

        ESM4::Land::Texture makeTexture(std::uint32_t base)
        {
            ESM4::Land::Texture result;
            result.base.formId = base;
            return result;
        }

        VFS::Path::NormalizedView defaultDiffuse("textures/landscape/default.dds");
        VFS::Path::NormalizedView defaultNormal("textures/landscape/default_n.dds");

        TEST(ESMTerrainStorageTest, the_blendmaps_of_fallout_have_a_layer_for_the_base_texture_of_each_quadrant)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            auto land = makeEsm4Land();
            land->mTextures[0] = makeTexture(0x10);
            land->mTextures[1] = makeTexture(0x11);
            land->mTextures[2] = makeTexture(0x12);
            land->mTextures[3] = makeTexture(0x13);
            storage.add(0, 0, std::move(land));
            for (std::uint32_t id : { 0x10, 0x11, 0x12, 0x13 })
            {
                ESM4::LandTexture ltex;
                ltex.mEditorId = "Texture" + std::to_string(id);
                ltex.mTextureFile = "oblivion.dds";
                storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(id), ltex);
            }

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm4Space());

            // The land texture records of Oblivion have the file
            ASSERT_EQ(layers.size(), 4u);
            ASSERT_EQ(blendmaps.size(), 4u);
            for (const auto& layer : layers)
                EXPECT_EQ(layer.mDiffuseMap.value(), "textures/landscape/oblivion.dds");
            for (const auto& image : blendmaps)
            {
                EXPECT_EQ(image->s(), 33);
                EXPECT_EQ(image->t(), 33);
            }
            const auto pixel = [&](std::size_t layer, int x, int y) { return blendmaps[layer]->data()[y * 33 + x]; };
            // The first quadrant of the cell is at the bottom, to the left, of the blendmap, which the first row is
            // not part of
            EXPECT_EQ(pixel(0, 0, 1), 255);
            EXPECT_EQ(pixel(0, 15, 16), 255);
            EXPECT_EQ(pixel(0, 0, 0), 0);
            EXPECT_EQ(pixel(0, 16, 1), 0);
            EXPECT_EQ(pixel(0, 0, 17), 0);
            EXPECT_EQ(pixel(1, 16, 1), 255);
            EXPECT_EQ(pixel(1, 0, 1), 0);
            EXPECT_EQ(pixel(2, 0, 17), 255);
            EXPECT_EQ(pixel(3, 16, 17), 255);
            EXPECT_EQ(pixel(3, 15, 17), 0);
        }

        TEST(ESMTerrainStorageTest, the_layers_of_a_quadrant_of_fallout_take_from_the_base_texture_what_they_cover)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            auto land = makeEsm4Land();
            land->mTextures[0] = makeTexture(0x10);
            ESM4::Land::TxtLayer coverage;
            coverage.texture.formId = 0x20;
            // In a grid of 17 by 17: the row and the place in it
            coverage.data.push_back({ 3 * 17 + 4, 0, 0, 0.5f });
            coverage.data.push_back({ 5 * 17 + 6, 0, 0, 1.5f });
            coverage.data.push_back({ 7 * 17 + 8, 0, 0, -1.f });
            // The last column and the first row are not drawn
            coverage.data.push_back({ 2 * 17 + 16, 0, 0, 1.f });
            coverage.data.push_back({ 0 * 17 + 3, 0, 0, 1.f });
            land->mTextures[0].layers.push_back(coverage);
            land->mTextures[1] = makeTexture(0x10);
            land->mTextures[2] = makeTexture(0x10);
            land->mTextures[3] = makeTexture(0x10);
            storage.add(0, 0, std::move(land));
            for (std::uint32_t id : { 0x10, 0x20 })
            {
                ESM4::LandTexture ltex;
                ltex.mTextureFile = id == 0x10 ? "oblivion.dds" : "dirtwasteland01.dds";
                storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(id), ltex);
            }

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm4Space());

            ASSERT_EQ(layers.size(), 2u);
            ASSERT_EQ(blendmaps.size(), 2u);
            EXPECT_EQ(layers[0].mDiffuseMap.value(), "textures/landscape/oblivion.dds");
            EXPECT_EQ(layers[1].mDiffuseMap.value(), "textures/landscape/dirtwasteland01.dds");
            const auto pixel = [&](std::size_t layer, int x, int y) { return blendmaps[layer]->data()[y * 33 + x]; };
            // Half of it
            EXPECT_EQ(pixel(1, 4, 3), 127);
            EXPECT_EQ(pixel(0, 4, 3), 255 - 127);
            // All of it, and nothing more than that
            EXPECT_EQ(pixel(1, 6, 5), 255);
            EXPECT_EQ(pixel(0, 6, 5), 0);
            // A negative opacity is nothing
            EXPECT_EQ(pixel(1, 8, 7), 0);
            EXPECT_EQ(pixel(0, 8, 7), 255);
            EXPECT_EQ(pixel(1, 16, 2), 0);
            EXPECT_EQ(pixel(1, 3, 0), 0);
        }

        TEST(ESMTerrainStorageTest, a_texture_set_of_fallout_3_has_the_diffuse_and_normal_map_of_the_layer)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            auto land = makeEsm4Land();
            land->mTextures[0] = makeTexture(0x10);
            land->mTextures[1] = makeTexture(0x11);
            land->mTextures[2] = makeTexture(0x12);
            land->mTextures[3] = makeTexture(0x13);
            storage.add(0, 0, std::move(land));

            // A record that has a texture set with the paths of Fallout: backslashes, and the directory of the textures
            ESM4::LandTexture withSet;
            withSet.mTexture = ESM::FormId::fromUint32(0x30);
            withSet.mEditorId = "WithSet";
            storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(0x10), withSet);
            ESM4::TextureSet set;
            set.mEditorId = "Set";
            set.mDiffuse = "Landscape\\DirtWasteland01.dds";
            set.mNormalMap = "Landscape\\DirtWasteland01_n.dds";
            storage.mTextureSets.emplace_back(ESM::FormId::fromUint32(0x30), set);
            // One that has a set that is not there falls back on its file
            ESM4::LandTexture missingSet;
            missingSet.mTexture = ESM::FormId::fromUint32(0x31);
            missingSet.mTextureFile = "oblivion.dds";
            storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(0x11), missingSet);
            // One that has a set that has no diffuse map and no file at all has nothing
            ESM4::LandTexture emptySet;
            emptySet.mTexture = ESM::FormId::fromUint32(0x32);
            storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(0x12), emptySet);
            ESM4::TextureSet noDiffuse;
            noDiffuse.mEditorId = "NoDiffuse";
            storage.mTextureSets.emplace_back(ESM::FormId::fromUint32(0x32), noDiffuse);
            // And one that has a set of a file that is not in the data has the path where it would be
            ESM4::LandTexture missingFile;
            missingFile.mTexture = ESM::FormId::fromUint32(0x33);
            storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(0x13), missingFile);
            ESM4::TextureSet gone;
            gone.mEditorId = "Gone";
            gone.mDiffuse = "landscape/gone.dds";
            storage.mTextureSets.emplace_back(ESM::FormId::fromUint32(0x33), gone);

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm4Space());
            // The same again is the same
            Storage::ImageVector again;
            std::vector<Terrain::LayerInfo> layersAgain;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), again, layersAgain, esm4Space());

            ASSERT_EQ(layers.size(), 4u);
            EXPECT_EQ(layers[0].mDiffuseMap.value(), "textures/landscape/dirtwasteland01.dds");
            EXPECT_EQ(layers[0].mNormalMap.value(), "textures/landscape/dirtwasteland01_n.dds");
            EXPECT_FALSE(layers[0].mParallax);
            EXPECT_FALSE(layers[0].mSpecular);
            EXPECT_EQ(layers[1].mDiffuseMap.value(), "textures/landscape/oblivion.dds");
            EXPECT_TRUE(layers[1].mNormalMap.empty());
            EXPECT_TRUE(layers[2].mDiffuseMap.empty());
            EXPECT_EQ(layers[3].mDiffuseMap.value(), "textures/landscape/gone.dds");
            ASSERT_EQ(layersAgain.size(), 4u);
            EXPECT_EQ(layersAgain[3].mDiffuseMap.value(), "textures/landscape/gone.dds");
        }

        TEST(ESMTerrainStorageTest, a_layer_of_fallout_that_no_record_has_is_drawn_with_the_default_texture_of_the_cell)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            auto land = makeEsm4Land();
            land->mDefaultDiffuseMap = defaultDiffuse;
            land->mDefaultNormalMap = defaultNormal;
            // The first has no record, the second is not set, the others are the same
            land->mTextures[0] = makeTexture(0x99);
            land->mTextures[1] = makeTexture(0);
            land->mTextures[2] = makeTexture(0x10);
            land->mTextures[3] = makeTexture(0x10);
            storage.add(0, 0, std::move(land));
            ESM4::LandTexture ltex;
            ltex.mTextureFile = "oblivion.dds";
            storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(0x10), ltex);

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm4Space());

            ASSERT_EQ(layers.size(), 3u);
            EXPECT_EQ(layers[0].mDiffuseMap.value(), "textures/landscape/default.dds");
            EXPECT_EQ(layers[0].mNormalMap.value(), "textures/landscape/default_n.dds");
            EXPECT_EQ(layers[1].mDiffuseMap.value(), "textures/landscape/default.dds");
            EXPECT_EQ(layers[2].mDiffuseMap.value(), "textures/landscape/oblivion.dds");
            EXPECT_EQ(blendmaps.size(), 3u);
        }

        TEST(ESMTerrainStorageTest, a_chunk_of_fallout_that_has_a_single_texture_needs_no_blending)
        {
            const auto vfs = makeVfs();
            TestStorage storage(vfs.get());
            auto esm4 = makeEsm4Land();
            for (int i = 0; i < 4; ++i)
                esm4->mTextures[i] = makeTexture(0x10);
            storage.add(0, 0, std::move(esm4));
            ESM4::LandTexture ltex;
            ltex.mTextureFile = "oblivion.dds";
            storage.mEsm4LandTextures.emplace_back(ESM::FormId::fromUint32(0x10), ltex);

            Storage::ImageVector blendmaps;
            std::vector<Terrain::LayerInfo> layers;
            storage.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), blendmaps, layers, esm4Space());

            EXPECT_TRUE(blendmaps.empty());
            ASSERT_EQ(layers.size(), 1u);
            EXPECT_EQ(layers[0].mDiffuseMap.value(), "textures/landscape/oblivion.dds");

            // And a chunk that has no land is not drawn at all
            TestStorage empty(vfs.get());
            Storage::ImageVector noBlendmaps;
            std::vector<Terrain::LayerInfo> noLayers;
            empty.getBlendmaps(1.f, osg::Vec2f(0.5f, 0.5f), noBlendmaps, noLayers, esm4Space());
            EXPECT_TRUE(noBlendmaps.empty());
            EXPECT_TRUE(noLayers.empty());
        }

        TEST(ESMTerrainStorageTest, the_land_object_of_the_two_kinds_of_record_says_which_data_it_has)
        {
            const ESM::Land land = makeEsm3Land(0, 0, 3);
            const LandObject esm3(land, ESM::Land::DATA_VHGT);
            EXPECT_EQ(esm3.getPlugin(), 3);
            ASSERT_NE(esm3.getData(ESM::Land::DATA_VHGT), nullptr);
            EXPECT_FALSE(esm3.getData(ESM::Land::DATA_VHGT)->isEsm4());
            EXPECT_EQ(esm3.getData(ESM::Land::DATA_VHGT)->getLandSize(), 65);
            EXPECT_EQ(esm3.getData(ESM::Land::DATA_VHGT)->getHeights().size(), 65u * 65u);
            EXPECT_EQ(esm3.getData(ESM::Land::DATA_VTEX | 32), nullptr);
            EXPECT_THROW((void)esm3.getData(ESM::Land::DATA_VHGT)->getEsm4Texture(0), std::logic_error);

            const auto esm4Land = makeEsm4Land(2);
            esm4Land->mDefaultDiffuseMap = defaultDiffuse;
            const LandObject esm4(*esm4Land, 0);
            EXPECT_EQ(esm4.getPlugin(), 2);
            EXPECT_EQ(esm4.getEsm4DefaultLayerInfo().mDiffuseMap.value(), "textures/landscape/default.dds");
            ASSERT_NE(esm4.getData(ESM::Land::DATA_VHGT), nullptr);
            EXPECT_TRUE(esm4.getData(ESM::Land::DATA_VHGT)->isEsm4());
            EXPECT_EQ(esm4.getData(ESM::Land::DATA_VHGT)->getLandSize(), 33);
            EXPECT_EQ(
                esm4.getData(ESM::Land::DATA_VHGT)->getSize(), static_cast<float>(Constants::ESM4CellSizeInUnits));
            EXPECT_EQ(esm4.getData(ESM::Land::DATA_VHGT)->getMinHeight(), 8.f);
            EXPECT_EQ(esm4.getData(ESM::Land::DATA_VHGT)->getMaxHeight(), 8.f);
            EXPECT_EQ(esm4.getData(ESM::Land::DATA_VTEX), nullptr);
            EXPECT_THROW((void)esm4.getData(ESM::Land::DATA_VHGT)->getTextures(), std::logic_error);
            EXPECT_NO_THROW((void)esm4.getData(ESM::Land::DATA_VHGT)->getEsm4Texture(3));

            const LandObject empty{};
            EXPECT_NE(empty.getData(0), nullptr);
            EXPECT_EQ(empty.getData(ESM::Land::DATA_VHGT), nullptr);
        }
    }
}
