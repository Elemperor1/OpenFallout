#ifndef GAME_MWWORLD_ACTIONSOULGEM_H
#define GAME_MWWORLD_ACTIONSOULGEM_H

#include "action.hpp"

namespace OFWorld
{
    class ActionSoulgem : public Action
    {
        void executeImp(const OFWorld::Ptr& actor) override;

    public:
        /// @param soulgem to use
        ActionSoulgem(const Ptr& object);
    };
}

#endif
