#ifndef OPENFALLOUT_MWRENDER_FALLOUTLOD_H
#define OPENFALLOUT_MWRENDER_FALLOUTLOD_H

#include <components/esm/refid.hpp>
#include <components/resource/resourcemanager.hpp>
#include <components/terrain/lodindex.hpp>
#include <components/terrain/quadtreeworld.hpp>

#include <utility>

namespace Resource
{
    class SceneManager;
}

namespace OFRender
{
    /// The distant land of a Fallout 3 or New Vegas worldspace, drawn from the level of detail files of the game:
    /// the terrain tiles for the nodes of the quad tree that are as large as a tile or larger, and the blocks of
    /// distant objects for the nodes that are not too large and not in the cells that are loaded.
    class FalloutLod : public Resource::NodeResourceManager<std::pair<osg::Vec2f, float>>,
                       public Terrain::QuadTreeWorld::ChunkManager
    {
    public:
        /// The largest node, in cells, that draws distant objects.
        static constexpr float sLargestObjectsSize = 8.f;

        FalloutLod(Resource::SceneManager* sceneManager, Terrain::LodIndex index, ESM::RefId worldspace,
            bool objects, double expiryDelay);

        osg::ref_ptr<osg::Node> getChunk(float size, const osg::Vec2f& center, unsigned char lod, unsigned int lodFlags,
            bool activeGrid, const osg::Vec3f& viewPoint, bool compile) override;

        unsigned int getNodeMask() override;

        /// Whether tiles fill the whole node, so that it is drawn from them and the terrain of the land records is
        /// not.
        bool replacesTerrain(float size, const osg::Vec2f& center) const;

        /// Whether tiles fill a part of the node only: it has to be taken apart, so that the parts that no tile fills
        /// keep the terrain of the land records.
        bool splitsTerrain(float size, const osg::Vec2f& center) const;

        const Terrain::LodIndex& getIndex() const { return mIndex; }

        void reportStats(unsigned int frameNumber, osg::Stats* stats) const override;

    private:
        osg::ref_ptr<osg::Node> loadTile(const Terrain::LodTile& tile);
        Terrain::LodIndex::Coverage getTerrainCoverage(float size, const osg::Vec2f& center) const;
        osg::ref_ptr<osg::Node> createChunk(float size, const osg::Vec2f& center, bool compile);
        void addObjects(osg::Group& group, float size, const osg::Vec2f& center);

        Resource::SceneManager* mSceneManager;
        Terrain::LodIndex mIndex;
        bool mObjects;
        float mCellSize;
    };
}

#endif
