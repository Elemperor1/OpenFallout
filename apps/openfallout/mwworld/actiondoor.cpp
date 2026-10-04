#include "actiondoor.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

namespace OFWorld
{
    ActionDoor::ActionDoor(const OFWorld::Ptr& object)
        : Action(false, object)
    {
    }

    void ActionDoor::executeImp(const OFWorld::Ptr& actor)
    {
        OFBase::Environment::get().getWorld()->activateDoor(getTarget());
    }
}
