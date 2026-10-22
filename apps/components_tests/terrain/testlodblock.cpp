#include <components/terrain/lodblock.hpp>

#include <gtest/gtest.h>

#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/NodeVisitor>

#include <cstddef>
#include <limits>

namespace Terrain
{
    namespace
    {
        constexpr float cellSize = 100.f;

        // One triangle in every cell of a block that starts at the cell firstX, firstY
        osg::ref_ptr<osg::Geometry> makeBlockGeometry(int firstX, int firstY)
        {
            osg::ref_ptr<osg::Vec3Array> vertices(new osg::Vec3Array);
            osg::ref_ptr<osg::DrawElementsUInt> indices(new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES));
            for (int y = 0; y < 4; ++y)
            {
                for (int x = 0; x < 4; ++x)
                {
                    const float left = (firstX + x) * cellSize + 10.f;
                    const float bottom = (firstY + y) * cellSize + 10.f;
                    const unsigned int first = static_cast<unsigned int>(vertices->size());
                    vertices->push_back(osg::Vec3f(left, bottom, 0.f));
                    vertices->push_back(osg::Vec3f(left + 50.f, bottom, 0.f));
                    vertices->push_back(osg::Vec3f(left, bottom + 50.f, 10.f * (y * 4 + x)));
                    indices->push_back(first);
                    indices->push_back(first + 1);
                    indices->push_back(first + 2);
                }
            }
            osg::ref_ptr<osg::Geometry> geometry(new osg::Geometry);
            geometry->setVertexArray(vertices);
            geometry->addPrimitiveSet(indices);
            return geometry;
        }

        struct CountTriangles : public osg::NodeVisitor
        {
            std::size_t mTriangles = 0;
            std::size_t mDrawables = 0;

            CountTriangles()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }

            void apply(osg::Geometry& geometry) override
            {
                ++mDrawables;
                for (unsigned int i = 0; i < geometry.getNumPrimitiveSets(); ++i)
                    mTriangles += geometry.getPrimitiveSet(i)->getNumIndices() / 3;
            }
        };

        std::size_t countTriangles(osg::Node& node)
        {
            CountTriangles count;
            node.accept(count);
            return count.mTriangles;
        }

        TEST(TerrainLodBlock, shouldSkipATriangleWithVerticesThatAreNotNumbers)
        {
            osg::ref_ptr<osg::Geometry> geometry = makeBlockGeometry(0, 0);
            osg::Vec3Array& vertices = *static_cast<osg::Vec3Array*>(geometry->getVertexArray());
            vertices[0].x() = std::numeric_limits<float>::quiet_NaN();
            vertices[3].y() = std::numeric_limits<float>::infinity();
            osg::ref_ptr<osg::Group> block(new osg::Group);
            block->addChild(geometry);

            const auto cells = splitBlockByCell(*block, 0, 0, cellSize);
            std::size_t triangles = 0;
            for (const auto& cell : cells)
                if (cell != nullptr)
                    triangles += countTriangles(*cell);
            EXPECT_EQ(triangles, 14u);
        }

        TEST(TerrainLodBlock, shouldPutEveryTriangleIntoTheCellItIsIn)
        {
            osg::ref_ptr<osg::Group> block(new osg::Group);
            block->addChild(makeBlockGeometry(8, -4));

            const auto cells = splitBlockByCell(*block, 8, -4, cellSize);
            for (const auto& cell : cells)
            {
                ASSERT_NE(cell, nullptr);
                EXPECT_EQ(countTriangles(*cell), 1u);
            }

            // The cell at x 2, y 1 of the block holds the triangle that rises 60 units, no other does
            osg::ref_ptr<osg::Node> cell = cells[1 * 4 + 2];
            osg::BoundingSphere sphere = cell->getBound();
            EXPECT_GT(sphere.center().x(), (8 + 2) * cellSize);
            EXPECT_LT(sphere.center().x(), (8 + 3) * cellSize);
            EXPECT_GT(sphere.center().y(), (-4 + 1) * cellSize);
            EXPECT_LT(sphere.center().y(), (-4 + 2) * cellSize);
        }

        TEST(TerrainLodBlock, shouldOnlyMakeTheCellsThatAreWanted)
        {
            osg::ref_ptr<osg::Group> block(new osg::Group);
            block->addChild(makeBlockGeometry(0, 0));

            const std::uint16_t wanted = (1u << 0) | (1u << 5) | (1u << 15);
            const auto cells = splitBlockByCell(*block, 0, 0, cellSize, wanted);
            for (int cell = 0; cell < 16; ++cell)
                EXPECT_EQ(cells[cell] != nullptr, (wanted & (1u << cell)) != 0) << cell;
        }

        TEST(TerrainLodBlock, shouldLeaveCellsWithoutTrianglesNull)
        {
            osg::ref_ptr<osg::Geometry> geometry(new osg::Geometry);
            osg::ref_ptr<osg::Vec3Array> vertices(new osg::Vec3Array);
            vertices->push_back(osg::Vec3f(110.f, 10.f, 0.f));
            vertices->push_back(osg::Vec3f(150.f, 10.f, 0.f));
            vertices->push_back(osg::Vec3f(110.f, 50.f, 0.f));
            geometry->setVertexArray(vertices);
            geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));

            const auto cells = splitBlockByCell(*geometry, 0, 0, cellSize);
            for (int cell = 0; cell < 16; ++cell)
                EXPECT_EQ(cells[cell] != nullptr, cell == 1) << cell;
        }

        TEST(TerrainLodBlock, shouldFollowTheTransformsAboveTheGeometry)
        {
            // The block is placed at the cell 4, 4 by the node above its vertices, which start at the origin
            osg::ref_ptr<osg::MatrixTransform> block(
                new osg::MatrixTransform(osg::Matrix::translate(4 * cellSize, 4 * cellSize, 0.f)));
            block->addChild(makeBlockGeometry(0, 0));

            const auto cells = splitBlockByCell(*block, 4, 4, cellSize, 1u << 6);
            for (int cell = 0; cell < 16; ++cell)
                EXPECT_EQ(cells[cell] != nullptr, cell == 6) << cell;
            ASSERT_NE(cells[6], nullptr);
            const osg::BoundingSphere sphere = cells[6]->getBound();
            EXPECT_GT(sphere.center().x(), (4 + 2) * cellSize);
            EXPECT_LT(sphere.center().x(), (4 + 3) * cellSize);
            EXPECT_GT(sphere.center().y(), (4 + 1) * cellSize);
            EXPECT_LT(sphere.center().y(), (4 + 2) * cellSize);
        }

        TEST(TerrainLodBlock, shouldKeepTheGeometryOfTheBlockShared)
        {
            osg::ref_ptr<osg::Geometry> geometry = makeBlockGeometry(0, 0);
            const osg::Array* vertices = geometry->getVertexArray();
            const auto cells = splitBlockByCell(*geometry, 0, 0, cellSize);

            CountTriangles count;
            cells[0]->accept(count);
            EXPECT_EQ(count.mDrawables, 1u);
            osg::Node* node = cells[0].get();
            const osg::Geometry* part = node->asGeometry();
            if (part == nullptr)
            {
                const osg::Group* group = node->asGroup();
                ASSERT_NE(group, nullptr);
                ASSERT_GT(group->getNumChildren(), 0u);
                part = group->getChild(0)->asGeometry();
            }
            ASSERT_NE(part, nullptr);
            EXPECT_EQ(part->getVertexArray(), vertices);
        }

        osg::ref_ptr<osg::Geometry> makeSquare(float left, float bottom, float size)
        {
            osg::ref_ptr<osg::Vec3Array> vertices(new osg::Vec3Array);
            vertices->push_back(osg::Vec3f(left, bottom, 0.f));
            vertices->push_back(osg::Vec3f(left + size, bottom, 0.f));
            vertices->push_back(osg::Vec3f(left + size, bottom + size, 0.f));
            vertices->push_back(osg::Vec3f(left, bottom + size, -14098.f));
            osg::ref_ptr<osg::Geometry> geometry(new osg::Geometry);
            geometry->setVertexArray(vertices);
            geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));
            return geometry;
        }

        TEST(TerrainLodBlock, placementShouldBeTheCornerWhenTheVerticesRunFromZero)
        {
            osg::ref_ptr<osg::Group> tile(new osg::Group);
            tile->addChild(makeSquare(0.f, 0.f, 8 * cellSize));
            EXPECT_EQ(getLodPlacement(*tile, 8, -16, 8, cellSize), osg::Vec3f(8 * cellSize, -16 * cellSize, 0.f));
        }

        TEST(TerrainLodBlock, placementShouldBeNoneWhenTheVerticesAreInTheWorld)
        {
            osg::ref_ptr<osg::Group> tile(new osg::Group);
            tile->addChild(makeSquare(8 * cellSize, -16 * cellSize, 8 * cellSize));
            EXPECT_EQ(getLodPlacement(*tile, 8, -16, 8, cellSize), osg::Vec3f());
        }

        TEST(TerrainLodBlock, placementShouldTellABlockWithGeometryInOnePartOfItsSquare)
        {
            // a block of four by four cells at the cell 4, 4 with something only in its north east cell
            osg::ref_ptr<osg::Group> local(new osg::Group);
            local->addChild(makeSquare(3 * cellSize, 3 * cellSize, cellSize));
            EXPECT_EQ(getLodPlacement(*local, 4, 4, 4, cellSize), osg::Vec3f(4 * cellSize, 4 * cellSize, 0.f));

            osg::ref_ptr<osg::Group> world(new osg::Group);
            world->addChild(makeSquare(7 * cellSize, 7 * cellSize, cellSize));
            EXPECT_EQ(getLodPlacement(*world, 4, 4, 4, cellSize), osg::Vec3f());

            // and at the south west, in the world, then from zero
            osg::ref_ptr<osg::Group> southWest(new osg::Group);
            southWest->addChild(makeSquare(4 * cellSize, 4 * cellSize, cellSize));
            EXPECT_EQ(getLodPlacement(*southWest, 4, 4, 4, cellSize), osg::Vec3f());
            osg::ref_ptr<osg::Group> southWestLocal(new osg::Group);
            southWestLocal->addChild(makeSquare(0.f, 0.f, cellSize));
            EXPECT_EQ(getLodPlacement(*southWestLocal, 4, 4, 4, cellSize), osg::Vec3f(4 * cellSize, 4 * cellSize, 0.f));
        }

        TEST(TerrainLodBlock, placementShouldTellABlockWestOfTheOrigin)
        {
            osg::ref_ptr<osg::Group> local(new osg::Group);
            local->addChild(makeSquare(0.f, 0.f, 4 * cellSize));
            EXPECT_EQ(getLodPlacement(*local, -4, 0, 4, cellSize), osg::Vec3f(-4 * cellSize, 0.f, 0.f));

            osg::ref_ptr<osg::Group> world(new osg::Group);
            world->addChild(makeSquare(-4 * cellSize, 0.f, 4 * cellSize));
            EXPECT_EQ(getLodPlacement(*world, -4, 0, 4, cellSize), osg::Vec3f());
        }

        TEST(TerrainLodBlock, placementShouldBeNoneAtTheOrigin)
        {
            osg::ref_ptr<osg::Group> tile(new osg::Group);
            tile->addChild(makeSquare(0.f, 0.f, 4 * cellSize));
            EXPECT_EQ(getLodPlacement(*tile, 0, 0, 4, cellSize), osg::Vec3f());
        }

        TEST(TerrainLodBlock, placementShouldBeNoneWithoutGeometry)
        {
            osg::ref_ptr<osg::Group> tile(new osg::Group);
            EXPECT_EQ(getLodPlacement(*tile, 8, 8, 4, cellSize), osg::Vec3f());
        }
    }
}
