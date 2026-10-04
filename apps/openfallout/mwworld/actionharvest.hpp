#ifndef GAME_MWWORLD_ACTIONHARVEST_H
#define GAME_MWWORLD_ACTIONHARVEST_H

#include "action.hpp"
#include "ptr.hpp"

namespace OFWorld
{
    class ActionHarvest : public Action
    {
        void executeImp(const OFWorld::Ptr& actor) override;

    public:
        ActionHarvest(const Ptr& container);
        ///< \param container The Container the Player has activated.
    };
}

#endif // ACTIONOPEN_H
