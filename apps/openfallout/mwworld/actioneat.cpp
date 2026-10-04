#include "actioneat.hpp"

#include <components/esm3/loadskil.hpp>

#include "../mwmechanics/actorutil.hpp"

#include "class.hpp"

namespace OFWorld
{
    void ActionEat::executeImp(const Ptr& actor)
    {
        if (actor.getClass().consume(getTarget(), actor) && actor == OFMechanics::getPlayer())
            actor.getClass().skillUsageSucceeded(actor, ESM::Skill::Alchemy, ESM::Skill::Alchemy_UseIngredient);
    }

    ActionEat::ActionEat(const OFWorld::Ptr& object)
        : Action(false, object)
    {
    }
}
