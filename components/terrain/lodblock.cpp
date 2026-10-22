#include "lodblock.hpp"

#include <osg/BoundingBox>
#include <osg/ComputeBoundsVisitor>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/NodeVisitor>
#include <osg/TriangleIndexFunctor>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

            void operator()(unsigned int a, unsigned int b, unsigned int c)
            {
                const std::size_t size = mVertices->size();
                if (a >= size || b >= size || c >= size)
                    return;
                const osg::Vec3f center = ((*mVertices)[a] + (*mVertices)[b] + (*mVertices)[c]) / 3.f;
                const osg::Vec3f world = center * mToWorld;
                const int x = std::clamp(
                    static_cast<int>(std::floor(world.x() / mCellSize)) - mFirstCellX, 0, sCellsPerSide - 1);
                const int y = std::clamp(
                    static_cast<int>(std::floor(world.y() / mCellSize)) - mFirstCellY, 0, sCellsPerSide - 1);
                const int cell = y * sCellsPerSide + x;
                if ((mWanted & (1u << cell)) == 0)
                    return;
                mIndices[cell].insert(mIndices[cell].end(), { a, b, c });
                mBounds[cell].expandBy((*mVertices)[a]);
                mBounds[cell].expandBy((*mVertices)[b]);
                mBounds[cell].expandBy((*mVertices)[c]);
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

                osg::TriangleIndexFunctor<SortTriangles> sort;
                sort.mVertices = vertices;
                sort.mToWorld = osg::computeLocalToWorld(getNodePath());
                sort.mFirstCellX = mFirstCellX;
                sort.mFirstCellY = mFirstCellY;
                sort.mCellSize = mCellSize;
                sort.mWanted = mWanted;
                geometry.accept(sort);

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
                    if (!sort.mToWorld.isIdentity())
                    {
                        osg::ref_ptr<osg::MatrixTransform> transform(new osg::MatrixTransform(sort.mToWorld));
                        transform->addChild(part);
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

    osg::Vec3f getLodPlacement(const osg::Node& node, int x, int y, float cellSize)
    {
        osg::ComputeBoundsVisitor visitor;
        const_cast<osg::Node&>(node).accept(visitor);
        const osg::BoundingBox box = visitor.getBoundingBox();
        if (!box.valid())
            return osg::Vec3f();

        const osg::Vec3f corner(static_cast<float>(x) * cellSize, static_cast<float>(y) * cellSize, 0.f);
        const float local = std::abs(box.xMin()) + std::abs(box.yMin());
        const float world = std::abs(box.xMin() - corner.x()) + std::abs(box.yMin() - corner.y());
        return local < world ? corner : osg::Vec3f();
    }
}
