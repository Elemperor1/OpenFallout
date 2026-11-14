#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadexpl.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadproj.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/combatcommands.hpp>
#include <components/falloutscript/placedexplosives.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        namespace EX = FalloutScript::Explosives;

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        /// The perk entry point that changes the chance that the player sets a mine off (entry point 4 of the executable's
        /// list, "Calculate Mine Explode Chance"); the value it starts from is 100
        constexpr int mineChanceEntryPoint = 4;
        /// The actor value of the skill Explosives, in both games
        constexpr int explosivesSkill = 35;

        /// How far in front of a character a mine is laid, and how far a grenade with no target is thrown
        constexpr float layDistance = 70.f;
        constexpr float throwDistance = 400.f;

        osg::Vec3f forwardOf(const ESM::Position& position)
        {
            return osg::Vec3f(static_cast<float>(std::sin(position.rot[2])), static_cast<float>(std::cos(position.rot[2])), 0.f);
        }

        bool isCharacterOrCreature(const OFWorld::Ptr& ptr)
        {
            return !ptr.isEmpty()
                && (ptr.getType() == ESM4::Npc::sRecordId || ptr.getType() == ESM4::Creature::sRecordId);
        }
    }

    bool FalloutScripts::layExplosive(
        FalloutScript::FormId attacker, FalloutScript::FormId weapon, const OFWorld::Ptr& target)
    {
        if (attacker == 0 || weapon == 0)
            return false;
        const ESM4::Weapon* record = mStore.get<ESM4::Weapon>().search(refId(weapon));
        if (record == nullptr || !record->mHasGameData)
            return false;
        const std::uint32_t type = record->mGame.mAnimationType;
        const bool mine = type == ESM4::Weapon::GameData::LandMine || type == ESM4::Weapon::GameData::MineThrown;
        const bool grenade = type == ESM4::Weapon::GameData::GrenadeThrown;
        // A grenade thrown at a target is the strike at that target; the one with none flies where the attacker looks
        if (!mine && !(grenade && target.isEmpty()))
            return false;
        const ESM4::Projectile* projectile = mStore.get<ESM4::Projectile>().search(record->mGame.mProjectile);
        if (projectile == nullptr)
            return false;
        const OFWorld::Ptr holder = findReference(attacker);
        if (holder.isEmpty() || !holder.isInCell())
            return false;
        const ESM4::Projectile::GameData data = projectile->gameData();
        EX::Placed placed;
        placed.mOwner = attacker;
        placed.mProjectile = projectile->mId.toUint32();
        placed.mKind.mAltTrigger = (data.mFlags & EX::flagAltTrigger) != 0;
        placed.mKind.mDetonates = (data.mFlags & EX::flagDetonates) != 0;
        placed.mKind.mCanBeDisabled = (data.mFlags & EX::flagCanBeDisabled) != 0;
        placed.mKind.mHasExplosion = !data.mExplosion.isZeroOrUnset();
        placed.mKind.mProximity = data.mProximityFuse;
        placed.mKind.mFuseTimer = data.mFuseTimer;
        const ESM::Position& at = holder.getRefData().getPosition();
        const osg::Vec3f spot = at.asVec3() + forwardOf(at) * (mine ? layDistance : throwDistance);
        placed.mPosition[0] = spot.x();
        placed.mPosition[1] = spot.y();
        placed.mPosition[2] = spot.z();
        placed.mCell = cellOf(attacker);
        placed.mExterior = holder.getCell()->isExterior();
        std::uint32_t id = 0;
        {
            const std::lock_guard lock(mExplosivesMutex);
            id = mLaid.plant(placed);
        }
        Log(Debug::Info) << "Explosive " << id << " of " << FalloutScript::hex(placed.mProjectile) << " laid by "
                         << FalloutScript::hex(attacker) << " at " << spot.x() << " " << spot.y() << " " << spot.z()
                         << (placed.mKind.mDetonates ? ", a charge that waits for a script"
                                 : placed.mKind.mProximity > 0 ? ", a mine"
                                                               : ", a timed charge");
        return true;
    }

    void FalloutScripts::explodeLaid(const EX::Placed& laid, FalloutScript::FormId cause)
    {
        namespace AV = FalloutScript::ActorValue;
        const ESM4::Projectile* projectile = mStore.get<ESM4::Projectile>().search(refId(laid.mProjectile));
        if (projectile == nullptr)
            return;
        const ESM4::Explosion* explosion = mStore.get<ESM4::Explosion>().search(projectile->gameData().mExplosion);
        if (explosion == nullptr)
            return;
        const ESM4::Explosion::GameData data = explosion->gameData();
        const double damage = std::max<double>(data.mDamage, 0.0);
        const double radius = std::max<double>(data.mRadius, 0.0);
        const osg::Vec3f centre(laid.mPosition[0], laid.mPosition[1], laid.mPosition[2]);
        const double buffer = gameSetting("fExplosionLOSBuffer", 16.0);
        OFBase::World* world = OFBase::Environment::get().getWorld();
        // What stands in the way of the blast keeps it off (a ray from just above the explosion to the middle of the target)
        const auto reaches = [&](const osg::Vec3f& at) {
            const osg::Vec3f from = centre + osg::Vec3f(0.f, 0.f, 8.f);
            const osg::Vec3f to = at + osg::Vec3f(0.f, 0.f, 48.f);
            const float apart = (to - from).length();
            return apart < 1.f || world->getDistToNearestRayHit(from, to - from, apart) >= apart - static_cast<float>(buffer);
        };
        int actorsHit = 0;
        int objectsHit = 0;
        std::vector<FalloutScript::FormId> actors;
        actors.push_back(mPlayer);
        for (const FalloutScript::FormId actor : mTriggerActors)
            if (actor != mPlayer)
                actors.push_back(actor);
        for (const FalloutScript::FormId victim : actors)
        {
            if (victim == 0 || mActors.isDead(victim))
                continue;
            const OFWorld::Ptr ptr = findReference(victim);
            if (ptr.isEmpty() || !ptr.isInCell() || !ptr.getRefData().isEnabled())
                continue;
            if (ptr.getCell()->isExterior() != laid.mExterior)
                continue;
            const osg::Vec3f where = ptr.getRefData().getPosition().asVec3();
            if ((where - centre).length() > radius || !reaches(where))
                continue;
            if (damage > 0)
            {
                // The god mode of the console keeps the health of the player
                if (victim == mPlayer && world->getGodModeState())
                    continue;
                const std::lock_guard strikeLock(mStrikeMutex);
                const auto transitions = mActors.holdTransitions();
                if (mActors.isDead(victim))
                    continue;
                FalloutScript::CombatState::Hit struck;
                struck.mAttacker = laid.mOwner;
                struck.mDamage = damage;
                mCombat.noteHit(victim, struck, false);
                mActorValues.damage(victim, AV::health, damage);
                mDestruction.damage(victim, damage, mActorValues.current(victim, AV::health));
                if (mActorValues.current(victim, AV::health) <= 0 && !fallsInsteadOfDying(victim))
                {
                    mActors.setDead(victim, true, laid.mOwner, -1);
                    evaluatePackage(victim);
                }
            }
            ++actorsHit;
        }
        // The things that can be destroyed take the damage as a blow, by the rule of destruction
        if (damage > 0)
        {
            std::vector<FalloutScript::FormId> objects;
            for (const auto& [reference, ptr] : mReferences)
            {
                if (ptr.isEmpty() || !ptr.isInCell() || isCharacterOrCreature(ptr) || !ptr.getRefData().isEnabled()
                    || ptr.mRef->isDeleted() || ptr.getCell()->isExterior() != laid.mExterior)
                    continue;
                if ((ptr.getRefData().getPosition().asVec3() - centre).length() <= radius)
                    objects.push_back(reference);
            }
            for (const FalloutScript::FormId reference : objects)
            {
                const OFWorld::Ptr ptr = findReference(reference);
                if (ptr.isEmpty() || !reaches(ptr.getRefData().getPosition().asVec3()))
                    continue;
                mDestruction.damage(reference, damage);
                ++objectsHit;
            }
        }
        Log(Debug::Info) << "Explosion of " << FalloutScript::hex(laid.mProjectile) << " laid by "
                         << FalloutScript::hex(laid.mOwner) << " at " << centre.x() << " " << centre.y() << " "
                         << centre.z() << ", set off by " << FalloutScript::hex(cause) << ": damage " << damage
                         << ", radius " << radius << ", " << actorsHit << " characters, " << objectsHit << " objects";
    }

    void FalloutScripts::updateExplosives(float frametime)
    {
        std::vector<EX::Event> events;
        {
            const std::lock_guard lock(mExplosivesMutex);
            if (mLaid.count() == 0)
                return;
            const EX::Rules rules = EX::rules([this](std::string_view name) { return gameSetting(name); });
            EX::Surroundings around;
            // The characters of the loaded cells, by what the one who laid the mine makes of them
            const auto layerOf = [this](const EX::Placed& laid, FalloutScript::FormId comer) {
                EX::Layer layer;
                layer.mActor = laid.mOwner;
                if (laid.mOwner != 0 && laid.mOwner != comer)
                {
                    switch (FalloutScript::relationOfActors(mCombat, mActors, *this, laid.mOwner, comer))
                    {
                        case FalloutScript::CombatState::Relation::Ally:
                            layer.mReaction = EX::Ally;
                            break;
                        case FalloutScript::CombatState::Relation::Friend:
                            layer.mReaction = EX::Friend;
                            break;
                        case FalloutScript::CombatState::Relation::Enemy:
                            layer.mReaction = EX::Enemy;
                            break;
                        default:
                            layer.mReaction = EX::Neutral;
                            break;
                    }
                }
                return layer;
            };
            const auto near = [](const EX::Placed& laid, const OFWorld::Ptr& ptr, double radius) {
                if (ptr.isEmpty() || !ptr.isInCell() || !ptr.getRefData().isEnabled())
                    return false;
                if (ptr.getCell()->isExterior() != laid.mExterior)
                    return false;
                const osg::Vec3f centre(laid.mPosition[0], laid.mPosition[1], laid.mPosition[2]);
                return (ptr.getRefData().getPosition().asVec3() - centre).length() <= radius;
            };
            around.character = [&](const EX::Placed& laid, double radius) -> std::optional<std::uint32_t> {
                for (const FalloutScript::FormId actor : mTriggerActors)
                {
                    if (actor == mPlayer || actor == 0 || mActors.isDead(actor))
                        continue;
                    const auto found = mReferences.find(actor);
                    if (found == mReferences.end() || !near(laid, found->second, radius))
                        continue;
                    if (EX::triggers(rules, actor, layerOf(laid, actor)))
                        return actor;
                }
                return std::nullopt;
            };
            around.player = [&](const EX::Placed& laid, double radius) {
                if (mPlayer == 0 || mActors.isDead(mPlayer))
                    return false;
                const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
                return near(laid, player, radius) && EX::triggers(rules, mPlayer, layerOf(laid, mPlayer));
            };
            around.playerChance = [this](const EX::Placed&) { return mPerks.apply(mineChanceEntryPoint, mPlayer, 100.0); };
            around.roll = [this] {
                const std::lock_guard lock(mCombatRandomMutex);
                return std::uniform_int_distribution<int>(0, 99)(mCombatRandom);
            };
            around.skill = [this] { return mActorValues.current(mPlayer, explosivesSkill); };
            events = mLaid.update(rules, frametime, around);
        }
        for (const EX::Event& event : events)
        {
            if (event.mKind == EX::Event::Kind::Expire)
            {
                Log(Debug::Info) << "Explosive " << event.mId << " of " << FalloutScript::hex(event.mPlaced.mProjectile)
                                 << " has lain long enough and is gone";
                continue;
            }
            explodeLaid(event.mPlaced, event.mCause);
        }
    }

    void FalloutScripts::setUpExplosives()
    {
        // DisableAllMines (0x5DE110): every mine whose record can be disabled is switched off, without a sound
        mInterpreter.setHandler("DisableAllMines", [this](FalloutScript::CallContext&) {
            const std::lock_guard lock(mExplosivesMutex);
            mLaid.disableAllMines();
            return 0.0;
        });
        // DetonatePlacedExplosives (0x5DDF90): the explosives that the actor laid, whose record detonates, go off
        mInterpreter.setHandler("DetonatePlacedExplosives", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId owner = call.mReference != 0 ? call.mReference : mPlayer;
            const std::lock_guard lock(mExplosivesMutex);
            mLaid.detonate(owner);
            return 0.0;
        });
        // GetPlantedExplosive: laid explosives of the port are not objects of the world, so there is no reference to give
        mInterpreter.setHandler("GetPlantedExplosive", [](FalloutScript::CallContext&) { return 0.0; });
    }
}
