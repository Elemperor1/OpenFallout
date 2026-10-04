#include "actorutil.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"

#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/magiceffects.hpp"

#include <components/esm3/loadmgef.hpp>

namespace OFMechanics
{
    OFWorld::Ptr getPlayer()
    {
        return OFBase::Environment::get().getWorld()->getPlayerPtr();
    }

    bool isPlayerInCombat()
    {
        return OFBase::Environment::get().getWorld()->getPlayer().isInCombat();
    }

    bool canActorMoveByZAxis(const OFWorld::Ptr& actor)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();
        return (actor.getClass().canSwim(actor) && world->isSwimming(actor)) || world->isFlying(actor);
    }

    bool hasWaterWalking(const OFWorld::Ptr& actor)
    {
        const OFMechanics::MagicEffects& effects = actor.getClass().getCreatureStats(actor).getMagicEffects();
        return effects.getOrDefault(ESM::MagicEffect::WaterWalking).getMagnitude() > 0;
    }

    bool isTargetMagicallyHidden(const OFWorld::Ptr& actor)
    {
        const MagicEffects& magicEffects = actor.getClass().getCreatureStats(actor).getMagicEffects();
        return (magicEffects.getOrDefault(ESM::MagicEffect::Invisibility).getMagnitude() > 0)
            || (magicEffects.getOrDefault(ESM::MagicEffect::Chameleon).getMagnitude() >= 75);
    }
}
