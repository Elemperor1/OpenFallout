#include "objectlists.hpp"

#include <components/misc/resourcehelpers.hpp>

#include "../mwbase/environment.hpp"

#include "../mwclass/container.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/worldmodel.hpp"

namespace OFLua
{

    void ObjectLists::update()
    {
        mActivatorsInScene.updateList();
        mActorsInScene.updateList();
        mContainersInScene.updateList();
        mDoorsInScene.updateList();
        mItemsInScene.updateList();
    }

    void ObjectLists::clear()
    {
        mActivatorsInScene.clear();
        mActorsInScene.clear();
        mContainersInScene.clear();
        mDoorsInScene.clear();
        mItemsInScene.clear();
    }

    ObjectLists::ObjectGroup* ObjectLists::chooseGroup(const OFWorld::Ptr& ptr)
    {
        // It is important to check `isMarker` first.
        // For example "prisonmarker" has class "Door" despite that it is only an invisible marker.
        if (Misc::ResourceHelpers::isHiddenMarker(ptr.getCellRef().getRefId()))
            return nullptr;
        const OFWorld::Class& cls = ptr.getClass();
        if (cls.isActivator())
            return &mActivatorsInScene;
        if (cls.isActor())
            return &mActorsInScene;
        if (ptr.mRef->getType() == ESM::REC_DOOR || ptr.mRef->getType() == ESM::REC_DOOR4)
            return &mDoorsInScene;
        if (typeid(cls) == typeid(OFClass::Container))
            return &mContainersInScene;
        if (cls.isItem(ptr) || ptr.mRef->getType() == ESM::REC_LIGH)
            return &mItemsInScene;
        return nullptr;
    }

    void ObjectLists::objectAddedToScene(const OFWorld::Ptr& ptr)
    {
        OFBase::Environment::get().getWorldModel()->registerPtr(ptr);
        ObjectGroup* group = chooseGroup(ptr);
        if (group)
            addToGroup(*group, ptr);
    }

    void ObjectLists::objectRemovedFromScene(const OFWorld::Ptr& ptr)
    {
        ObjectGroup* group = chooseGroup(ptr);
        if (group)
            removeFromGroup(*group, ptr);
    }

    void ObjectLists::ObjectGroup::updateList()
    {
        if (mChanged)
        {
            mList->clear();
            for (ObjectId id : mSet)
                mList->push_back(id);
            mChanged = false;
        }
    }

    void ObjectLists::ObjectGroup::clear()
    {
        mChanged = false;
        mList->clear();
        mSet.clear();
    }

    void ObjectLists::addToGroup(ObjectGroup& group, const OFWorld::Ptr& ptr)
    {
        group.mSet.insert(getId(ptr));
        group.mChanged = true;
    }

    void ObjectLists::removeFromGroup(ObjectGroup& group, const OFWorld::Ptr& ptr)
    {
        group.mSet.erase(getId(ptr));
        group.mChanged = true;
    }
}
