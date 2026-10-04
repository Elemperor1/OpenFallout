#ifndef GAME_MWMECHANICS_ACTIVATORS_H
#define GAME_MWMECHANICS_ACTIVATORS_H

#include "character.hpp"

#include <list>
#include <map>
#include <string>
#include <vector>

namespace osg
{
    class Vec3f;
}

namespace OFWorld
{
    class Ptr;
    class CellStore;
}

namespace OFMechanics
{
    class Objects
    {
        std::list<CharacterController> mObjects;
        std::map<const OFWorld::LiveCellRefBase*, std::list<CharacterController>::iterator> mIndex;

    public:
        void addObject(const OFWorld::Ptr& ptr);
        ///< Register an animated object

        void removeObject(const OFWorld::Ptr& ptr);
        ///< Deregister an object

        void updateObject(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr);
        ///< Updates an object with a new Ptr

        void dropObjects(const OFWorld::CellStore* cellStore);
        ///< Deregister all objects in the given cell.

        void update(float duration, bool paused);
        ///< Update object animations

        bool onOpen(const OFWorld::Ptr& ptr);
        void onClose(const OFWorld::Ptr& ptr);

        bool playAnimationGroup(
            const OFWorld::Ptr& ptr, std::string_view groupName, int mode, uint32_t number, bool scripted = false);
        bool playAnimationGroupLua(const OFWorld::Ptr& ptr, std::string_view groupName, uint32_t loops, float speed,
            std::string_view startKey, std::string_view stopKey, bool forceLoop);
        void enableLuaAnimations(const OFWorld::Ptr& ptr, bool enable);
        void skipAnimation(const OFWorld::Ptr& ptr);
        void persistAnimationStates();
        void clearAnimationQueue(const OFWorld::Ptr& ptr, bool clearScripted);

        void getObjectsInRange(const osg::Vec3f& position, float radius, std::vector<OFWorld::Ptr>& out) const;

        std::size_t size() const { return mObjects.size(); }
    };
}

#endif
