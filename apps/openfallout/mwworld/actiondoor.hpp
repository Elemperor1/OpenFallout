#ifndef GAME_MWWORLD_ACTIONDOOR_H
#define GAME_MWWORLD_ACTIONDOOR_H

#include "action.hpp"
#include "ptr.hpp"

namespace OFWorld
{
    class ActionDoor : public Action
    {
        void executeImp(const OFWorld::Ptr& actor) override;

    public:
        ActionDoor(const Ptr& object);
    };
}

#endif
