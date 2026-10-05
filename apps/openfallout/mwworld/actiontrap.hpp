#ifndef GAME_MWWORLD_ACTIONTRAP_H
#define GAME_MWWORLD_ACTIONTRAP_H

#include <string>

#include "action.hpp"

namespace OFWorld
{
    class ActionTrap : public Action
    {
        ESM::RefId mSpellId;
        OFWorld::Ptr mTrapSource;

        void executeImp(const Ptr& actor) override;

    public:
        /// @param spellId
        /// @param trapSource
        ActionTrap(const ESM::RefId& spellId, const Ptr& trapSource)
            : Action(false, trapSource)
            , mSpellId(spellId)
            , mTrapSource(trapSource)
        {
        }
    };
}

#endif
