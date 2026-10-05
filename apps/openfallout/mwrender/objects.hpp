#ifndef GAME_RENDER_OBJECTS_H
#define GAME_RENDER_OBJECTS_H

#include <map>
#include <string>

#include <osg/ref_ptr>

#include "../mwworld/ptr.hpp"

namespace osg
{
    class Group;
}

namespace Resource
{
    class ResourceSystem;
}

namespace OFWorld
{
    class CellStore;
}

namespace SceneUtil
{
    class UnrefQueue;
}

namespace OFRender
{

    class Animation;

    class Objects
    {
        using PtrAnimationMap = std::map<const OFWorld::LiveCellRefBase*, osg::ref_ptr<Animation>>;

        typedef std::map<const OFWorld::CellStore*, osg::ref_ptr<osg::Group>> CellMap;
        CellMap mCellSceneNodes;
        PtrAnimationMap mObjects;
        osg::ref_ptr<osg::Group> mRootNode;
        Resource::ResourceSystem* mResourceSystem;
        SceneUtil::UnrefQueue& mUnrefQueue;

        void insertBegin(const OFWorld::Ptr& ptr);

    public:
        Objects(Resource::ResourceSystem* resourceSystem, const osg::ref_ptr<osg::Group>& rootNode,
            SceneUtil::UnrefQueue& unrefQueue);
        ~Objects();

        /// @param allowLight If false, no lights will be created, and particles systems will be removed.
        void insertModel(const OFWorld::Ptr& ptr, const std::string& model, bool allowLight = true);

        void insertNPC(const OFWorld::Ptr& ptr);
        void insertCreature(const OFWorld::Ptr& ptr, const std::string& model, bool weaponsShields);

        Animation* getAnimation(const OFWorld::Ptr& ptr);
        const Animation* getAnimation(const OFWorld::ConstPtr& ptr) const;

        bool removeObject(const OFWorld::Ptr& ptr);
        ///< \return found?

        void removeCell(const OFWorld::CellStore* store);

        /// Updates containing cell for object rendering data
        void updatePtr(const OFWorld::Ptr& old, const OFWorld::Ptr& cur);

    private:
        void operator=(const Objects&);
        Objects(const Objects&);
    };
}
#endif
