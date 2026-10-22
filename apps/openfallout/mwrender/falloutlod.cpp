#include "falloutlod.hpp"

#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Uniform>
#include <osgUtil/IncrementalCompileOperation>

#include <components/debug/debuglog.hpp>
#include <components/esm/util.hpp>
#include <components/resource/cachestats.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/terrain/lodblock.hpp>

#include <cmath>
#include <cstdint>

#include "vismask.hpp"

namespace OFRender
{
    namespace
    {
        constexpr int sBlockCells = Terrain::LodIndex::sSmallestLevel;
        /// How many of the nodes that have objects are logged
        constexpr int sReportedNodes = 12;
    }

    FalloutLod::FalloutLod(Resource::SceneManager* sceneManager, Terrain::LodIndex index, ESM::RefId worldspace,
        const Options& options, double expiryDelay)
        : Resource::NodeResourceManager<std::pair<osg::Vec2f, float>>(nullptr, expiryDelay)
        , Terrain::QuadTreeWorld::ChunkManager(worldspace)
        , mSceneManager(sceneManager)
        , mIndex(std::move(index))
        , mOptions(options)
        , mCellSize(static_cast<float>(ESM::getCellSize(worldspace)))
    {
    }

    unsigned int FalloutLod::getNodeMask()
    {
        return Mask_Static;
    }

    Terrain::LodIndex::Coverage FalloutLod::getTerrainCoverage(float size, const osg::Vec2f& center) const
    {
        if (size < Terrain::LodIndex::sSmallestLevel)
            return Terrain::LodIndex::Coverage::None;
        const float half = size / 2.f;
        const int x = static_cast<int>(std::lround(center.x() - half));
        const int y = static_cast<int>(std::lround(center.y() - half));
        return mIndex.getTerrainCoverage(x, y, static_cast<int>(std::lround(size)));
    }

    bool FalloutLod::replacesTerrain(float size, const osg::Vec2f& center) const
    {
        if (mOptions.mDebug.mNoLand || mOptions.mDebug.mNoRecords)
            return true;
        // The blocks alone: no land from the tiles, and none from the land records either
        if (mOptions.mDebug.mObjectsOnly && size >= Terrain::LodIndex::sSmallestLevel)
            return true;
        return getTerrainCoverage(size, center) == Terrain::LodIndex::Coverage::Full;
    }

    bool FalloutLod::splitsTerrain(float size, const osg::Vec2f& center) const
    {
        if (mOptions.mDebug.mObjectsOnly)
            return false;
        return getTerrainCoverage(size, center) == Terrain::LodIndex::Coverage::Partial;
    }

    osg::ref_ptr<osg::Node> FalloutLod::getChunk(float size, const osg::Vec2f& center, unsigned char /*lod*/,
        unsigned int /*lodFlags*/, bool activeGrid, const osg::Vec3f& /*viewPoint*/, bool compile)
    {
        // The cells that are loaded show their own land and objects
        if (activeGrid)
            return nullptr;

        const std::pair<osg::Vec2f, float> id(center, size);
        if (const osg::ref_ptr<osg::Object> obj = mCache->getRefFromObjectCache(id))
            return static_cast<osg::Node*>(obj.get());

        osg::ref_ptr<osg::Node> node = createChunk(size, center, compile);
        if (node)
            mCache->addEntryToObjectCache(id, node.get());
        return node;
    }

    osg::ref_ptr<osg::Node> FalloutLod::loadTile(const Terrain::LodTile& tile)
    {
        osg::ref_ptr<osg::Node> node = mSceneManager->getInstance(VFS::Path::NormalizedView(tile.mPath));
        // The loader of NIF files drops the translation of the root node, which is what places a tile
        const osg::Vec3f placement
            = Terrain::getLodPlacement(*node, tile.mName.mX, tile.mName.mY, tile.mName.mLevel, mCellSize);
        if (placement == osg::Vec3f())
            return node;
        osg::ref_ptr<osg::MatrixTransform> placed(new osg::MatrixTransform(osg::Matrix::translate(placement)));
        placed->addChild(node);
        return placed;
    }

    osg::ref_ptr<osg::Node> FalloutLod::createChunk(float size, const osg::Vec2f& center, bool compile)
    {
        osg::ref_ptr<osg::Group> group(new osg::Group);

        if (size >= Terrain::LodIndex::sSmallestLevel)
        {
            const float half = size / 2.f;
            const int x = static_cast<int>(std::lround(center.x() - half));
            const int y = static_cast<int>(std::lround(center.y() - half));
            const int cells = static_cast<int>(std::lround(size));

            // The tiles are drawn only where they fill the node, as it is the only place the land is not
            osg::ref_ptr<osg::Group> terrain(new osg::Group);
            if (!mOptions.mDebug.mObjectsOnly && !mOptions.mDebug.mNoLand && replacesTerrain(size, center))
                for (const Terrain::LodTile* tile : mIndex.getTerrain(x, y, cells))
                    terrain->addChild(loadTile(*tile));
            if (terrain->getNumChildren() > 0)
            {
                terrain->setNodeMask(Mask_Terrain);
                if (mOptions.mNormalMode != 0)
                    terrain->getOrCreateStateSet()->addUniform(new osg::Uniform("lodNormalMode", mOptions.mNormalMode));
                group->addChild(terrain);
            }

            if (mOptions.mObjects && size <= sLargestObjectsSize)
            {
                osg::ref_ptr<osg::Group> objects(new osg::Group);
                for (const bool high : { false, true })
                {
                    if (high && mOptions.mDebug.mNoHigh)
                        continue;
                    for (const Terrain::LodTile* tile : mIndex.getObjects(x, y, cells, high))
                        objects->addChild(loadTile(*tile));
                }
                if (objects->getNumChildren() > 0)
                {
                    objects->setNodeMask(Mask_Static);
                    group->addChild(objects);
                    reportObjects(size, x, y, *objects);
                }
            }
        }
        else if (mOptions.mObjects)
        {
            addObjects(*group, size, center);
        }

        if (group->getNumChildren() == 0)
            return nullptr;

        osg::ref_ptr<SceneUtil::PositionAttitudeTransform> transform(new SceneUtil::PositionAttitudeTransform);
        transform->setPosition(osg::Vec3f(-center.x() * mCellSize, -center.y() * mCellSize, 0.f));
        transform->addChild(group);

        if (compile)
            if (osgUtil::IncrementalCompileOperation* ico = mSceneManager->getIncrementalCompileOperation())
                ico->add(transform);

        return transform;
    }

    void FalloutLod::addObjects(osg::Group& group, float size, const osg::Vec2f& center)
    {
        // A node smaller than a block lies inside one block: take the cells of it whose middle is in the node
        const float half = size / 2.f;
        const float left = center.x() - half;
        const float bottom = center.y() - half;
        const int blockX = static_cast<int>(std::floor(left / sBlockCells)) * sBlockCells;
        const int blockY = static_cast<int>(std::floor(bottom / sBlockCells)) * sBlockCells;

        std::uint16_t wanted = 0;
        for (int y = 0; y < sBlockCells; ++y)
        {
            for (int x = 0; x < sBlockCells; ++x)
            {
                const float middleX = static_cast<float>(blockX + x) + 0.5f;
                const float middleY = static_cast<float>(blockY + y) + 0.5f;
                if (middleX >= left && middleX < left + size && middleY >= bottom && middleY < bottom + size)
                    wanted |= static_cast<std::uint16_t>(1u << (y * sBlockCells + x));
            }
        }
        if (wanted == 0)
            return;

        osg::ref_ptr<osg::Group> objects(new osg::Group);
        for (const bool high : { false, true })
        {
            if (high && mOptions.mDebug.mNoHigh)
                continue;
            const Terrain::LodTile* tile
                = mIndex.find(Terrain::LodTileName::Kind::Objects, sBlockCells, high, blockX, blockY);
            if (tile == nullptr)
                continue;
            osg::ref_ptr<osg::Node> block = loadTile(*tile);
            for (const osg::ref_ptr<osg::Node>& part :
                Terrain::splitBlockByCell(*block, blockX, blockY, mCellSize, wanted))
                if (part)
                    objects->addChild(part);
        }
        if (objects->getNumChildren() == 0)
            return;
        objects->setNodeMask(Mask_Static);
        group.addChild(objects);
        reportObjects(size, static_cast<int>(std::lround(left)), static_cast<int>(std::lround(bottom)), *objects);
    }

    void FalloutLod::reportObjects(float size, int x, int y, osg::Group& objects)
    {
        if (mReported.fetch_add(1) >= sReportedNodes)
            return;
        // Where the objects are drawn is told in the world's own units, to be compared with where the node is
        const osg::BoundingSphere bound = objects.getBound();
        Log(Debug::Info) << "Distant objects of the node of " << size << " cells from " << x << "," << y << ": "
                         << objects.getNumChildren() << " pieces of blocks, bound centre " << bound.center().x() << ","
                         << bound.center().y() << "," << bound.center().z() << " radius " << bound.radius();
    }

    void FalloutLod::reportStats(unsigned int frameNumber, osg::Stats* stats) const
    {
        Resource::reportStats("Lod Chunk", frameNumber, mCache->getStats(), *stats);
    }
}
