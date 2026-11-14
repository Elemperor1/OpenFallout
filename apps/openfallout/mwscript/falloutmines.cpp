#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadexpl.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadproj.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/combatcommands.hpp>
#include <components/falloutscript/placedexplosives.hpp>

#include "falloutlaw.hpp"

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

        /// The number a form has in a saved game, 0 for none
        FalloutScript::FormId formNumber(const ESM::RefId& id)
        {
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            return form == nullptr || form->isZeroOrUnset() || !form->hasContentFile() ? 0 : form->toUint32();
        }

        bool isCharacterOrCreature(const OFWorld::Ptr& ptr)
        {
            return !ptr.isEmpty()
                && (ptr.getType() == ESM4::Npc::sRecordId || ptr.getType() == ESM4::Creature::sRecordId);
        }
    }

    bool FalloutScripts::inSpaceOfLaid(const EX::Placed& laid, const OFWorld::Ptr& ptr)
    {
        if (ptr.isEmpty() || !ptr.isInCell())
            return false;
        const OFWorld::CellStore& cell = *ptr.getCell();
        if (cell.isExterior() != laid.mExterior)
            return false;
        // What the explosive was laid in: its cell for an interior, the worldspace of its cell outside
        FalloutScript::FormId space = laid.mCell;
        if (laid.mExterior && laid.mCell != 0)
        {
            const ESM4::Cell* record = mStore.get<ESM4::Cell>().search(ESM::FormId::fromUint32(laid.mCell));
            space = record != nullptr ? formNumber(record->mParent) : 0;
        }
        // (a cell that cannot be told stands for the one of its kind, as before the place was kept)
        if (space == 0)
            return true;
        const FalloutScript::FormId here = formNumber(cell.isExterior() ? cell.getCell()->getWorldSpace() : cell.getCell()->getId());
        return here == 0 || here == space;
    }

    bool FalloutScripts::layExplosive(
        FalloutScript::FormId attacker, FalloutScript::FormId weapon, const OFWorld::Ptr& target, bool noDamage)
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
        placed.mNoDamage = noDamage;
        placed.mWeapon = weapon;
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
        // (a blast of an attack that was to do no damage does none)
        const double damage = laid.mNoDamage ? 0.0 : std::max<double>(data.mDamage, 0.0);
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
            // A ghost takes no blow, a blast included (as in strike)
            if (victim == 0 || mActors.isDead(victim) || mObjectState.ghost(victim))
                continue;
            const OFWorld::Ptr ptr = findReference(victim);
            if (ptr.isEmpty() || !ptr.isInCell() || !ptr.getRefData().isEnabled())
                continue;
            if (!inSpaceOfLaid(laid, ptr))
                continue;
            const osg::Vec3f where = ptr.getRefData().getPosition().asVec3();
            if ((where - centre).length() > radius || !reaches(where))
                continue;
            // A blast of an attack that was to do no damage is still a hit (the victim hears of it, the law judges it and it
            // fights back, as for a blow); only its health and its destruction stay as they were
            if (damage > 0 || laid.mNoDamage)
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
                struck.mWeapon = laid.mWeapon;
                struck.mDamage = damage;
                // A blast of the player's that an ally or friend lets pass is not an attack on it (as for a blow)
                const bool tolerated = toleratesBlow(laid.mOwner, victim);
                const bool wasFighting = mCombat.inCombat(victim);
                mCombat.noteHit(victim, struck, tolerated);
                // A blast of the player's that hurts a character is judged by the law as a blow is, whether the victim lets it
                // pass or not (one that ignores friendly hits has none); the kill is the death listener's
                if (laid.mOwner != 0 && laid.mOwner == mPlayer && victim != mPlayer && mLaw
                    && !(tolerated && mCombat.ignoresFriendlyHits(victim)))
                    mLaw->blow(victim, laid.mWeapon, wasFighting);
                if (damage > 0)
                {
                    const double healthBefore = mActorValues.current(victim, AV::health);
                    mActorValues.damage(victim, AV::health, damage);
                    // What the player's charge took off a character is the player's damage, whoever lands the last of it:
                    // the experience and the statistics of a kill follow it (as for a blow)
                    if (laid.mOwner == mPlayer && mPlayer != 0 && victim != mPlayer)
                    {
                        const double dealt
                            = std::max(0.0, healthBefore - std::max(mActorValues.current(victim, AV::health), 0.0));
                        const std::lock_guard damageLock(mPlayerDamageMutex);
                        mPlayerDamage[victim] += dealt;
                    }
                    mDestruction.damage(victim, damage, mActorValues.current(victim, AV::health));
                }
                if (damage > 0 && mActorValues.current(victim, AV::health) <= 0 && !fallsInsteadOfDying(victim))
                {
                    mActors.setDead(victim, true, laid.mOwner, -1, laid.mWeapon);
                    evaluatePackage(victim);
                }
                else if (mActorValues.current(victim, AV::health) > 0 && laid.mOwner != 0 && laid.mOwner != victim
                    && !tolerated)
                {
                    // A character that survives a blast fights the one that laid it, and that one is in the fight too (as
                    // for a blow in strike; one with no package gets the one that is to fight)
                    if (!mCombat.inCombat(victim) && FalloutScript::startFight(mCombat, mActors, victim, laid.mOwner))
                        evaluatePackage(victim);
                    if (!mCombat.inCombat(laid.mOwner)
                        && FalloutScript::startFight(mCombat, mActors, laid.mOwner, victim))
                        evaluatePackage(laid.mOwner);
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
                    || ptr.mRef->isDeleted() || !inSpaceOfLaid(laid, ptr))
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
            const auto near = [this](const EX::Placed& laid, const OFWorld::Ptr& ptr, double radius) {
                if (ptr.isEmpty() || !ptr.isInCell() || !ptr.getRefData().isEnabled())
                    return false;
                if (!inSpaceOfLaid(laid, ptr))
                    return false;
                const osg::Vec3f centre(laid.mPosition[0], laid.mPosition[1], laid.mPosition[2]);
                return (ptr.getRefData().getPosition().asVec3() - centre).length() <= radius;
            };
            around.character = [&](const EX::Placed& laid, double radius) -> std::optional<std::uint32_t> {
                for (const FalloutScript::FormId actor : mTriggerActors)
                {
                    if (actor == mPlayer || actor == 0 || mActors.isDead(actor))
                        continue;
                    // (found the way the blast finds who it hits, not by the map of the references alone)
                    const OFWorld::Ptr ptr = findReference(actor);
                    if (ptr.isEmpty() || !near(laid, ptr, radius))
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
