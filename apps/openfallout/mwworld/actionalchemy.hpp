#ifndef GAME_MWWORLD_ACTIONALCHEMY_H
#define GAME_MWWORLD_ACTIONALCHEMY_H

#include "action.hpp"

namespace OFWorld
{
    class ActionAlchemy : public Action
    {
        bool mForce;
        void executeImp(const Ptr& actor) override;

    public:
        ActionAlchemy(bool force = false);
    };
}

#endif
