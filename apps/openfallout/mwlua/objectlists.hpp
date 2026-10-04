#ifndef MWLUA_OBJECTLISTS_H
#define MWLUA_OBJECTLISTS_H

#include <set>

#include "object.hpp"

namespace OFLua
{

    // ObjectLists is used to track lists of game objects like nearby.items, nearby.actors, etc.
    class ObjectLists
    {
    public:
        void update(); // Should be called every frame.
        void clear(); // Should be called every time before starting or loading a new game.

        ObjectIdList getActivatorsInScene() const { return mActivatorsInScene.mList; }
        ObjectIdList getActorsInScene() const { return mActorsInScene.mList; }
        ObjectIdList getContainersInScene() const { return mContainersInScene.mList; }
        ObjectIdList getDoorsInScene() const { return mDoorsInScene.mList; }
        ObjectIdList getItemsInScene() const { return mItemsInScene.mList; }
        ObjectIdList getPlayers() const { return mPlayers; }

        void objectAddedToScene(const OFWorld::Ptr& ptr);
        void objectRemovedFromScene(const OFWorld::Ptr& ptr);

        void setPlayer(const OFWorld::Ptr& player) { *mPlayers = { getId(player) }; }

    private:
        struct ObjectGroup
        {
            void updateList();
            void clear();

            bool mChanged = false;
            ObjectIdList mList = std::make_shared<std::vector<ObjectId>>();
            std::set<ObjectId> mSet;
        };

        ObjectGroup* chooseGroup(const OFWorld::Ptr& ptr);
        void addToGroup(ObjectGroup& group, const OFWorld::Ptr& ptr);
        void removeFromGroup(ObjectGroup& group, const OFWorld::Ptr& ptr);

        ObjectGroup mActivatorsInScene;
        ObjectGroup mActorsInScene;
        ObjectGroup mContainersInScene;
        ObjectGroup mDoorsInScene;
        ObjectGroup mItemsInScene;
        ObjectIdList mPlayers = std::make_shared<std::vector<ObjectId>>();
    };

}

#endif // MWLUA_OBJECTLISTS_H
