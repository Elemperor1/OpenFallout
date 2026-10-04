#include "actor.hpp"

#include <components/esm3/loadmgef.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/magiceffects.hpp"
#include "../mwmechanics/movement.hpp"

#include "../mwphysics/physicssystem.hpp"

#include "../mwworld/inventorystore.hpp"
#include "../mwworld/worldmodel.hpp"

namespace OFClass
{
    void Actor::adjustPosition(const OFWorld::Ptr& ptr, bool force) const
    {
        OFBase::Environment::get().getWorld()->adjustPosition(ptr, force);
    }

    void Actor::insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        physics.addActor(ptr, VFS::Path::toNormalized(model));
        if (getCreatureStats(ptr).isDead() && getCreatureStats(ptr).isDeathAnimationFinished())
            OFBase::Environment::get().getWorld()->enableActorCollision(ptr, false);
    }

    bool Actor::useAnim() const
    {
        return true;
    }

    osg::Vec3f Actor::getRotationVector(const OFWorld::Ptr& ptr) const
    {
        OFMechanics::Movement& movement = getMovementSettings(ptr);
        osg::Vec3f vec(movement.mRotation[0], movement.mRotation[1], movement.mRotation[2]);
        movement.mRotation[0] = 0.0f;
        movement.mRotation[1] = 0.0f;
        movement.mRotation[2] = 0.0f;
        return vec;
    }

    float Actor::getEncumbrance(const OFWorld::Ptr& ptr) const
    {
        float weight = getContainerStore(ptr).getWeight();
        const OFMechanics::MagicEffects& effects = getCreatureStats(ptr).getMagicEffects();
        weight -= effects.getOrDefault(OFMechanics::EffectKey(ESM::MagicEffect::Feather)).getMagnitude();
        if (ptr != OFMechanics::getPlayer() || !OFBase::Environment::get().getWorld()->getGodModeState())
            weight += effects.getOrDefault(OFMechanics::EffectKey(ESM::MagicEffect::Burden)).getMagnitude();
        return (weight < 0) ? 0.0f : weight;
    }

    bool Actor::allowTelekinesis(const OFWorld::ConstPtr& ptr) const
    {
        return false;
    }

    bool Actor::isActor() const
    {
        return true;
    }

    float Actor::getCurrentSpeed(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::Movement& movementSettings = ptr.getClass().getMovementSettings(ptr);
        float moveSpeed = this->getMaxSpeed(ptr) * movementSettings.mSpeedFactor;
        if (movementSettings.mIsStrafing)
            moveSpeed *= 0.75f;
        return moveSpeed;
    }

    bool Actor::consume(const OFWorld::Ptr& consumable, const OFWorld::Ptr& actor) const
    {
        OFMechanics::CastSpell cast(actor, actor);
        const ESM::RefId& recordId = consumable.getCellRef().getRefId();
        OFBase::Environment::get().getWorldModel()->registerPtr(consumable);
        OFBase::Environment::get().getLuaManager()->itemConsumed(consumable, actor);
        actor.getClass().getContainerStore(actor).remove(consumable, 1);
        if (cast.cast(recordId))
        {
            OFBase::Environment::get().getWorld()->breakInvisibility(actor);
            return true;
        }
        return false;
    }
}
