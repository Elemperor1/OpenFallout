#include "lodblock.hpp"

#include <osg/BoundingBox>
#include <osg/ComputeBoundsVisitor>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/NodeVisitor>
#include <osg/TriangleIndexFunctor>
#include <osg/Vec2f>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Terrain
{
    namespace
    {
        constexpr int sCellsPerSide = 4;

        class FixedBound : public osg::Drawable::ComputeBoundingBoxCallback
        {
        public:
            explicit FixedBound(const osg::BoundingBox& box)
                : mBox(box)
            {
            }

            osg::BoundingBox computeBound(const osg::Drawable&) const override { return mBox; }

        private:
            osg::BoundingBox mBox;
        };

        struct CollectTriangles
        {
            std::vector<std::array<unsigned int, 3>> mTriangles;

            void operator()(unsigned int a, unsigned int b, unsigned int c) { mTriangles.push_back({ a, b, c }); }
        };

        // The triangles of one geometry, sorted into the cells. The triangles that share vertices are the parts of
        // one object, and an object belongs to the cell its middle is in (as the reference of the game does): sorted
        // by the center of each triangle, an object that stands on the edge of a loaded cell would lose the part of it
        // in that cell, and the rest of it would hang in the air.
        struct SortTriangles
        {
            const osg::Vec3Array* mVertices = nullptr;
            osg::Matrix mToWorld;
            int mFirstCellX = 0;
            int mFirstCellY = 0;
            float mCellSize = 1.f;
            std::uint16_t mWanted = 0xffff;
            std::array<std::vector<unsigned int>, 16> mIndices;
            std::array<osg::BoundingBox, 16> mBounds;

            struct Extent
            {
                float mMinX = std::numeric_limits<float>::max();
                float mMinY = std::numeric_limits<float>::max();
                float mMaxX = std::numeric_limits<float>::lowest();
                float mMaxY = std::numeric_limits<float>::lowest();

                void expandBy(const osg::Vec2f& point)
                {
                    mMinX = std::min(mMinX, point.x());
                    mMinY = std::min(mMinY, point.y());
                    mMaxX = std::max(mMaxX, point.x());
                    mMaxY = std::max(mMaxY, point.y());
                }
            };

            static unsigned int findRoot(std::vector<unsigned int>& parent, unsigned int vertex)
            {
                while (parent[vertex] != vertex)
                {
                    parent[vertex] = parent[parent[vertex]];
                    vertex = parent[vertex];
                }
                return vertex;
            }

            int getCell(float worldX, float worldY) const
            {
                constexpr float last = static_cast<float>(sCellsPerSide - 1);
                const int x = static_cast<int>(
                    std::clamp(std::floor(worldX / mCellSize) - static_cast<float>(mFirstCellX), 0.f, last));
                const int y = static_cast<int>(
                    std::clamp(std::floor(worldY / mCellSize) - static_cast<float>(mFirstCellY), 0.f, last));
                return y * sCellsPerSide + x;
            }

            void add(int cell, const std::array<unsigned int, 3>& triangle)
            {
                if ((mWanted & (1u << cell)) == 0)
                    return;
                mIndices[cell].insert(mIndices[cell].end(), triangle.begin(), triangle.end());
                for (const unsigned int index : triangle)
                    mBounds[cell].expandBy((*mVertices)[index]);
            }

            void sort(const std::vector<std::array<unsigned int, 3>>& triangles)
            {
                const std::size_t size = mVertices->size();
                // The vertices come from a file: a coordinate that is not a number or is far out is not made an int
                std::vector<osg::Vec2f> world(size);
                std::vector<bool> usable(size, false);
                for (std::size_t i = 0; i < size; ++i)
                {
                    const osg::Vec3f point = (*mVertices)[i] * mToWorld;
                    world[i] = osg::Vec2f(point.x(), point.y());
                    usable[i] = std::isfinite(point.x()) && std::isfinite(point.y());
                }

                std::vector<std::array<unsigned int, 3>> valid;
                valid.reserve(triangles.size());
                std::vector<unsigned int> parent(size);
                for (std::size_t i = 0; i < size; ++i)
                    parent[i] = static_cast<unsigned int>(i);
                for (const auto& triangle : triangles)
                {
                    if (triangle[0] >= size || triangle[1] >= size || triangle[2] >= size)
                        continue;
                    if (!usable[triangle[0]] || !usable[triangle[1]] || !usable[triangle[2]])
                        continue;
                    valid.push_back(triangle);
                    const unsigned int root = findRoot(parent, triangle[0]);
                    parent[findRoot(parent, triangle[1])] = root;
                    parent[findRoot(parent, triangle[2])] = root;
                }

                std::vector<Extent> extents(size);
                for (const auto& triangle : valid)
                {
                    Extent& extent = extents[findRoot(parent, triangle[0])];
                    for (const unsigned int index : triangle)
                        extent.expandBy(world[index]);
                }

                // An object that is wider than the block is not an object, and goes by its triangles
                const float widest = static_cast<float>(sCellsPerSide) * mCellSize;
                for (const auto& triangle : valid)
                {
                    const Extent& extent = extents[findRoot(parent, triangle[0])];
                    if (extent.mMaxX - extent.mMinX <= widest && extent.mMaxY - extent.mMinY <= widest)
                    {
                        add(getCell((extent.mMinX + extent.mMaxX) / 2.f, (extent.mMinY + extent.mMaxY) / 2.f), triangle);
                        continue;
                    }
                    const osg::Vec2f center = (world[triangle[0]] + world[triangle[1]] + world[triangle[2]]) / 3.f;
                    add(getCell(center.x(), center.y()), triangle);
                }
            }
        };

        class SplitVisitor : public osg::NodeVisitor
        {
        public:
            SplitVisitor(int firstCellX, int firstCellY, float cellSize, std::uint16_t wanted)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mFirstCellX(firstCellX)
                , mFirstCellY(firstCellY)
                , mCellSize(cellSize)
                , mWanted(wanted)
            {
            }

            std::array<osg::ref_ptr<osg::Group>, 16> mCells;

            void apply(osg::Geometry& geometry) override
            {
                const osg::Vec3Array* vertices = dynamic_cast<const osg::Vec3Array*>(geometry.getVertexArray());
                if (vertices == nullptr)
                    return;

                osg::TriangleIndexFunctor<CollectTriangles> collect;
                geometry.accept(collect);

                SortTriangles sort;
                sort.mVertices = vertices;
                sort.mToWorld = osg::computeLocalToWorld(getNodePath());
                sort.mFirstCellX = mFirstCellX;
                sort.mFirstCellY = mFirstCellY;
                sort.mCellSize = mCellSize;
                sort.mWanted = mWanted;
                sort.sort(collect.mTriangles);

                for (int cell = 0; cell < 16; ++cell)
                {
                    if (sort.mIndices[cell].empty())
                        continue;

                    osg::ref_ptr<osg::Geometry> part(new osg::Geometry(geometry, osg::CopyOp::SHALLOW_COPY));
                    part->removePrimitiveSet(0, part->getNumPrimitiveSets());
                    part->addPrimitiveSet(new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES,
                        sort.mIndices[cell].begin(), sort.mIndices[cell].end()));
                    // The arrays are the ones of the whole block, so the bound has to be told
                    part->setComputeBoundingBoxCallback(new FixedBound(sort.mBounds[cell]));

                    osg::ref_ptr<osg::Node> node = part;
                    // The textures, material and shaders of a block are on the nodes above its geometry, and a
                    // part is drawn white and unlit without them
                    const osg::NodePath& path = getNodePath();
                    for (auto it = path.rbegin() + 1; it != path.rend(); ++it)
                    {
                        osg::StateSet* stateSet = (*it)->getStateSet();
                        if (stateSet == nullptr)
                            continue;
                        osg::ref_ptr<osg::Group> holder(new osg::Group);
                        holder->setStateSet(stateSet);
                        holder->addChild(node);
                        node = holder;
                    }
                    if (!sort.mToWorld.isIdentity())
                    {
                        osg::ref_ptr<osg::MatrixTransform> transform(new osg::MatrixTransform(sort.mToWorld));
                        transform->addChild(node);
                        node = transform;
                    }
                    if (!mCells[cell])
                        mCells[cell] = new osg::Group;
                    mCells[cell]->addChild(node);
                }
            }

        private:
            int mFirstCellX;
            int mFirstCellY;
            float mCellSize;
            std::uint16_t mWanted;
        };
    }

    std::array<osg::ref_ptr<osg::Node>, 16> splitBlockByCell(
        osg::Node& block, int firstCellX, int firstCellY, float cellSize, std::uint16_t wanted)
    {
        SplitVisitor visitor(firstCellX, firstCellY, cellSize, wanted);
        block.accept(visitor);
        std::array<osg::ref_ptr<osg::Node>, 16> result;
        std::copy(visitor.mCells.begin(), visitor.mCells.end(), result.begin());
        return result;
    }

    osg::Vec3f getLodPlacement(const osg::Node& node, int x, int y, int cells, float cellSize)
    {
        osg::ComputeBoundsVisitor visitor;
        const_cast<osg::Node&>(node).accept(visitor);
        const osg::BoundingBox box = visitor.getBoundingBox();
        if (!box.valid())
            return osg::Vec3f();

        const float extent = static_cast<float>(cells) * cellSize;
        const osg::Vec3f corner(static_cast<float>(x) * cellSize, static_cast<float>(y) * cellSize, 0.f);

        // Whether the middle of the geometry on one axis is in the square that runs from zero rather than the one
        // that runs from the corner
        const auto fromZero = [extent](float min, float max, float start) {
            const float middle = (min + max) / 2.f;
            return std::abs(middle - extent / 2.f) < std::abs(middle - (start + extent / 2.f));
        };

        // At the origin the two squares are one
        if (corner.x() != 0.f)
            return fromZero(box.xMin(), box.xMax(), corner.x()) ? corner : osg::Vec3f();
        if (corner.y() != 0.f)
            return fromZero(box.yMin(), box.yMax(), corner.y()) ? corner : osg::Vec3f();
        return osg::Vec3f();
    }
}
