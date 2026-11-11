#ifndef OPENFALLOUT_COMPONENTS_TERRAIN_DRAWABLE_H
#define OPENFALLOUT_COMPONENTS_TERRAIN_DRAWABLE_H

#include <osg/Geometry>

namespace osg
{
    class ClusterCullingCallback;
}

namespace osgUtil
{
    class CullVisitor;
}

namespace SceneUtil
{
    class LightListCallback;
}

namespace Terrain
{

    class CompositeMap;
    class CompositeMapRenderer;

    /**
     * Subclass of Geometry that supports built in multi-pass rendering and built in LightListCallback.
     */
    class TerrainDrawable : public osg::Geometry
    {
    public:
        osg::Object* cloneType() const override { return new TerrainDrawable(); }
        osg::Object* clone(const osg::CopyOp& copyop) const override { return new TerrainDrawable(*this, copyop); }
        bool isSameKindAs(const osg::Object* obj) const override
        {
            return dynamic_cast<const TerrainDrawable*>(obj) != nullptr;
        }
        const char* className() const override { return "TerrainDrawable"; }
        const char* libraryName() const override { return "Terrain"; }

        TerrainDrawable();
        ~TerrainDrawable(); // has to be defined in the cpp file because we only forward declared some members.
        TerrainDrawable(const TerrainDrawable& copy, const osg::CopyOp& copyop);

        void accept(osg::NodeVisitor& nv) override;
        void cull(osgUtil::CullVisitor* cv);

        typedef std::vector<osg::ref_ptr<osg::StateSet>> PassVector;
        void setPasses(const PassVector& passes);
        const PassVector& getPasses() const { return mPasses; }

        void setLightListCallback(SceneUtil::LightListCallback* lightListCallback);

        void createClusterCullingCallback();

        /// Whether a chunk that faces away from the eye is left out (it is, unless a debug word of the distant land
        /// says otherwise), and whether the first time one is left out is told to the log
        static void setClusterCulling(bool enabled, bool report);
        /// Where the chunk is, in cells, to tell it in the log
        void setChunk(float size, const osg::Vec2f& center)
        {
            mChunkSize = size;
            mChunkCenter = center;
        }

        void compileGLObjects(osg::RenderInfo& renderInfo) const override;

        void setupWaterBoundingBox(float waterheight, float margin);
        const osg::BoundingBox& getWaterBoundingBox() const { return mWaterBoundingBox; }

        void setCompositeMap(CompositeMap* map) { mCompositeMap = map; }
        CompositeMap* getCompositeMap() const { return mCompositeMap; }
        void setCompositeMapRenderer(CompositeMapRenderer* renderer) { mCompositeMapRenderer = renderer; }

    private:
        osg::BoundingBox mWaterBoundingBox;
        PassVector mPasses;

        osg::ref_ptr<osg::ClusterCullingCallback> mClusterCullingCallback;
        float mChunkSize = 0.f;
        osg::Vec2f mChunkCenter;
        bool mClusterCullReported = false;

        osg::ref_ptr<SceneUtil::LightListCallback> mLightListCallback;
        osg::ref_ptr<CompositeMap> mCompositeMap;
        osg::ref_ptr<CompositeMapRenderer> mCompositeMapRenderer;
    };

}

#endif
