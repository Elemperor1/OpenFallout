#ifndef GAME_MWWORLD_ACTIONTAKE_H
#define GAME_MWWORLD_ACTIONTAKE_H

#include "action.hpp"

namespace OFWorld
{
    class ActionTake : public Action
    {
        void executeImp(const Ptr& actor) override;

    public:
        ActionTake(const OFWorld::Ptr& object);
    };
}

#endif
