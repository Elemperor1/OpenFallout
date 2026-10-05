#ifndef GAME_MWWORLD_ACTIONREAD_H
#define GAME_MWWORLD_ACTIONREAD_H

#include "action.hpp"

namespace OFWorld
{
    class ActionRead : public Action
    {
        void executeImp(const OFWorld::Ptr& actor) override;

    public:
        /// @param book or scroll to read
        ActionRead(const Ptr& object);
    };
}

#endif // ACTIONREAD_H
