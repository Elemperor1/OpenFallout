#ifndef OPENFALLOUT_MWRENDER_WATER_H
#define OPENFALLOUT_MWRENDER_WATER_H

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <osg/Vec3d>
#include <osg/Vec3f>
#include <osg/ref_ptr>

#include <components/settings/settings.hpp>
#include <components/vfs/pathutil.hpp>

namespace osg
{
    class Group;
    class PositionAttitudeTransform;
    class Drawable;
    class Geometry;
    class Node;
    class Callback;
}

namespace osgUtil
{
    class IncrementalCompileOperation;
}

namespace Resource
{
    class ResourceSystem;
}

namespace OFWorld
{
    class CellStore;
    class Ptr;
}

namespace Fallback
{
    class Map;
}

namespace OFRender
{

    class Reflection;
    class RippleSimulation;
    class RainSettingsUpdater;
    class Ripples;

    /// Water rendering
    class Water
    {
        osg::ref_ptr<RainSettingsUpdater> mRainSettingsUpdater;

        osg::ref_ptr<osg::Group> mParent;
        osg::ref_ptr<osg::Group> mSceneRoot;
        osg::ref_ptr<osg::PositionAttitudeTransform> mWaterNode;
        osg::ref_ptr<osg::Geometry> mWaterGeom;
        Resource::ResourceSystem* mResourceSystem;
        osg::ref_ptr<osgUtil::IncrementalCompileOperation> mIncrementalCompileOperation;

        std::unique_ptr<RippleSimulation> mSimulation;

        osg::ref_ptr<Reflection> mReflection;
        osg::ref_ptr<Ripples> mRipples;

        bool mEnabled;
        bool mToggled;
        float mTop;
        bool mInterior;
        bool mShowWorld;

        /// The water of a worldspace of Fallout: a square for each cell, at the height of water of the cell, instead of
        /// one plane of the whole world at one height. Then mTop is the height of the water at or nearest to the view
        /// point, which is what the reflection and the order of drawing use.
        struct Tile
        {
            osg::ref_ptr<osg::PositionAttitudeTransform> mNode;
            float mHeight;
        };
        std::map<std::pair<int, int>, Tile> mTiles;
        osg::ref_ptr<osg::Group> mTileGroup;
        osg::ref_ptr<osg::Geometry> mTileGeom;
        osg::ref_ptr<osg::Geometry> mSimpleWaterGeom;
        bool mTileMode;
        float mViewLevel;

        std::optional<float> getTileHeightAt(float x, float y) const;

        osg::Callback* mCullCallback;
        osg::ref_ptr<osg::Callback> mShaderWaterStateSetUpdater;

        osg::Vec3f getSceneNodeCoordinates(int gridX, int gridY);
        void updateVisible();

        void createSimpleWaterStateSet(osg::Node* node, float alpha);

        void createShaderWaterStateSet(osg::Node* node);

        void updateWaterMaterial();

    public:
        Water(osg::Group* parent, osg::Group* sceneRoot, Resource::ResourceSystem* resourceSystem,
            osgUtil::IncrementalCompileOperation* ico);
        ~Water();

        void setCullCallback(osg::Callback* callback);

        void listAssetsToPreload(std::vector<VFS::Path::Normalized>& textures);

        void setEnabled(bool enabled);

        bool toggle();

        bool isVisible() const { return hasWater() && mToggled; }

        /// Whether there is water to draw: the water of the cell the player is in, or the water of any cell loaded
        /// when the water has a height for each cell.
        bool hasWater() const { return mTileMode ? !mTiles.empty() : mEnabled; }

        bool isUnderwater(const osg::Vec3f& pos) const;

        /// Whether the water has a height for each cell (the worldspaces of Fallout).
        bool isTiled() const { return mTileMode; }

        /// The water of a cell of a worldspace of Fallout, replacing the water the cell had.
        void addTile(int gridX, int gridY, float height);
        /// Returns whether the cell had a tile.
        bool removeTile(int gridX, int gridY);

        /// Tell the water where the camera is. With the water in tiles the height used for the reflection follows it.
        void setViewPoint(const osg::Vec3f& position);

        /// The height of the water of the cell around the view point, the lowest float when it has none. Only with the
        /// water in tiles.
        float getViewLevel() const { return mViewLevel; }

        /// adds an emitter, position will be tracked automatically using its scene node
        void addEmitter(const OFWorld::Ptr& ptr, float scale = 1.f, float force = 1.f);
        void removeEmitter(const OFWorld::Ptr& ptr);
        void updateEmitterPtr(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr);
        void emitRipple(const osg::Vec3f& pos);

        void removeCell(const OFWorld::CellStore* store); ///< remove all emitters in this cell

        void clearRipples();

        void changeCell(const OFWorld::CellStore* store);
        void setHeight(const float height);
        void setRainIntensity(const float rainIntensity);

        void update(float dt, bool paused);

        osg::Vec3d getPosition() const;

        float getHeight() const { return mTop; }

        osg::Drawable* getDrawable() const;

        void processChangedSettings(const Settings::CategorySettingVector& settings);

        void showWorld(bool show);
    };

}

#endif
