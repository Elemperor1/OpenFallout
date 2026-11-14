#include "falloutscripts.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <random>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadmstt.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadtact.hpp>
#include <components/esm4/loadterm.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/hitreactions.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/modeloverride.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        /// The local number of the weapon "Fists" in the first master
        constexpr FalloutScript::FormId fistsNumber = 0x1F4;

        /// The local number of the activator DefaultAshPile1 in the first master
        constexpr FalloutScript::FormId defaultAshPileNumber = 0x1B;

        /// The actor value of the skill Unarmed and of Bloody Mess
        constexpr int unarmedSkill = 45;
        constexpr int bloodyMessValue = 55;

        /// The magic effect archetype of a concussion
        constexpr std::uint32_t concussionArchetype = 33;

        template <class Record>
        const ESM4::Destruction* destructionOf(const OFWorld::ESMStore& store, const ESM::FormId& base)
        {
            const Record* record = store.get<Record>().search(base);
            return record != nullptr && record->mDestruction.mPresent ? &record->mDestruction : nullptr;
        }

        bool isActorRecord(const OFWorld::Ptr& ptr)
        {
            return !ptr.isEmpty() && (ptr.getType() == ESM4::Npc::sRecordId || ptr.getType() == ESM4::Creature::sRecordId);
        }
    }

    bool FalloutScripts::destructionData(FalloutScript::FormId reference, FalloutScript::DestructionData& data)
    {
        ESM::FormId base;
        if (!baseOf(reference, base) || base.isZeroOrUnset())
            return false;
        const ESM4::Destruction* source = destructionOf<ESM4::Activator>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::MovableStatic>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::Container>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::Creature>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::Npc>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::Door>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::TalkingActivator>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::Furniture>(mStore, base);
        if (source == nullptr)
            source = destructionOf<ESM4::Terminal>(mStore, base);
        if (source == nullptr)
            return false;
        data = FalloutScript::DestructionData();
        data.mHealth = source->mHeader.mHealth;
        data.mFlags = source->mHeader.mFlags;
        for (std::size_t i = 0; i < source->mStages.size(); ++i)
        {
            const ESM4::DestructionStage& stage = source->mStages[i];
            FalloutScript::DestructionStage made;
            made.mHealthPercent = stage.mHealthPercent;
            made.mDamageStage = stage.mDamageStage;
            made.mFlags = stage.mFlags;
            made.mSelfDamage = stage.mSelfDamagePerSecond;
            made.mExplosion = stage.mExplosion;
            made.mDebris = stage.mDebris;
            made.mDebrisCount = stage.mDebrisCount;
            if (i < source->mStageModels.size())
                made.mModel = source->mStageModels[i].mModel.getNormalized().value();
            data.mStages.push_back(std::move(made));
        }
        return true;
    }

    double FalloutScripts::destructionBaseHealth(FalloutScript::FormId reference)
    {
        // The most that a character or creature has is the health it starts with (the executable reads it as the actor
        // value, FalloutNV.exe 0x475B20)
        if (!isActor(reference))
            return -1.0;
        return mActorValues.permanent(reference, FalloutScript::ActorValue::health);
    }

    std::optional<double> FalloutScripts::destructionActorHealth(FalloutScript::FormId reference)
    {
        if (!isActor(reference))
            return std::nullopt;
        return mActorValues.current(reference, FalloutScript::ActorValue::health);
    }

    bool FalloutScripts::dismemberIsActor(FalloutScript::FormId reference)
    {
        return isActor(reference);
    }

    void FalloutScripts::criticalStageSet(FalloutScript::FormId actor, int stage)
    {
        // What a stage does to the body (goo, the body that wastes away) is the executable's 0x8A1A70; the port keeps the
        // stage and what the scripts ask about it, and shows nothing
        static std::atomic<int> sLogged{ 0 };
        if (sLogged.fetch_add(1) < 20)
            Log(Debug::Info) << "SetCriticalStage: " << FalloutScript::hex(actor) << " is in stage " << stage
                             << " (the state is kept, the body is not changed)";
    }

    void FalloutScripts::setUpDamage()
    {
        FalloutScript::addDestructionCommands(mInterpreter, mDestruction);
        FalloutScript::addDismemberCommands(mInterpreter, mDismemberment, *this);
        mDestruction.setListener([this](FalloutScript::FormId reference, const FalloutScript::DestructionOutcome& outcome) {
            destructionChanged(reference, outcome);
        });
        mDestruction.setRestoreListener([this](FalloutScript::FormId reference) { destructionRestored(reference); });

        // IsWeaponSkillType: the weapon that the reference is (or none: the hands) is used with the skill. FalloutNV.exe
        // 0x5A0AB0.
        mInterpreter.setHandler("IsWeaponSkillType", [this](FalloutScript::CallContext& call) {
            const int skill = FalloutScript::intArgument(call, 0);
            FalloutScript::FormId base = 0;
            const ESM4::Weapon* weapon = nullptr;
            if (call.mReference != 0 && baseFormOf(call.mReference, base) && base != 0)
                weapon = mStore.get<ESM4::Weapon>().search(refId(base));
            if (weapon == nullptr)
                return skill == unarmedSkill ? 1.0 : 0.0;
            return weapon->mHasGameData && static_cast<int>(weapon->mGame.mSkill) == skill ? 1.0 : 0.0;
        });
        // IsWeaponInList: the weapon that the reference is (the fists when it is none) is in the list. 0x5A05E0.
        mInterpreter.setHandler("IsWeaponInList", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId list = FalloutScript::formArgument(call, 0);
            if (list == 0)
                return 0.0;
            FalloutScript::FormId base = 0;
            FalloutScript::FormId weapon = 0;
            if (call.mReference != 0 && baseFormOf(call.mReference, base) && base != 0
                && mStore.get<ESM4::Weapon>().search(refId(base)) != nullptr)
                weapon = base;
            else
                weapon = masterForm(fistsNumber);
            return mFormLists.contains(list, weapon) ? 1.0 : 0.0;
        });
        // GetConcussed: a magic effect of the concussion archetype (33) runs on the character. 0x5A5020.
        mInterpreter.setHandler("GetConcussed", [this](FalloutScript::CallContext& call) {
            if (call.mReference == 0 || !isActor(call.mReference))
                return 0.0;
            for (const ESM4::MagicEffect& effect : mStore.get<ESM4::MagicEffect>())
                if (effect.mData.mArchetype == concussionArchetype
                    && mMagic.hasEffect(call.mReference, effect.mId.toUint32()))
                    return 1.0;
            return 0.0;
        });
        // GetTimeDead: the hours since the character died (whole hours of the game, as the executable counts them: the
        // days passed with their fraction times 24, floored, which here is the days and the hour of the day), 0 for one
        // that is not dead. 0x5A3A30.
        mInterpreter.setHandler("GetTimeDead", [this](FalloutScript::CallContext& call) {
            if (call.mReference == 0 || !isActor(call.mReference) || !mActors.isDead(call.mReference))
                return 0.0;
            const double died = mDismemberment.deathHour(call.mReference);
            if (!(died > 0))
                return 0.0;
            return FalloutScript::DismemberRule::wholeHours(gameDaysPassed(), gameHour()) - died;
        });
        // IsKillerObject: the character is dead and the form is what killed it (or a list that has it). 0x5A3FA0.
        mInterpreter.setHandler("IsKillerObject", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId form = FalloutScript::formArgument(call, 0);
            if (call.mReference == 0 || form == 0 || !isActor(call.mReference) || !mActors.isDead(call.mReference))
                return 0.0;
            const FalloutScript::FormId killer = mActors.killerOf(call.mReference);
            if (killer == 0)
                return 0.0;
            if (killer == form)
                return 1.0;
            FalloutScript::FormId base = 0;
            if (baseFormOf(killer, base) && base == form)
                return 1.0;
            return mFormLists.contains(form, killer) || mFormLists.contains(form, base) ? 1.0 : 0.0;
        });
        // The executables answer with the byte of the character (+0x1B1) and the state of its knock (the process's, whose
        // states 3 and 4 are a knock); nothing in the port keeps either, so the answer is 0
        mInterpreter.setHandler("GetForceHitReaction", [](FalloutScript::CallContext&) { return 0.0; });
        mInterpreter.setHandler("GetKnockedState", [](FalloutScript::CallContext&) { return 0.0; });
        // ResetFallDamageTimer only reads the timer of the character's process (FalloutNV.exe 0x5D10A0 and 0x5D10F0) and
        // changes nothing
        mInterpreter.setHandler("ResetFallDamageTimer", [](FalloutScript::CallContext&) { return 0.0; });
        // PushActorAway actor, force: the reference throws the character away from itself with the force that the
        // blows use (0x5D6B60 reads the force from the character's agility and the number, 0x646580)
        mInterpreter.setHandler("PushActorAway", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId actor = FalloutScript::formArgument(call, 0);
            const double force = FalloutScript::argument(call, 1);
            if (call.mReference != 0 && actor != 0 && isActor(actor) && !std::isnan(force))
                pushActorAway(call.mReference, actor, force);
            return 0.0;
        });
        // FireWeapon weapon: the calling character fires the weapon once (0x5DA570: the player is given the action, a
        // character the shot of the weapon itself)
        mInterpreter.setHandler("FireWeapon", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId weapon = FalloutScript::formArgument(call, 0);
            if (call.mReference != 0 && weapon != 0 && isActor(call.mReference))
                fireWeaponFor(call.mReference, weapon);
            return 0.0;
        });
        // KillAllActors [killer]: every character of the cells that are loaded dies, the player not (0x5BE4F0)
        mInterpreter.setHandler("KillAllActors", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId killer = FalloutScript::formArgument(call, 0);
            runOnMainThread([this, killer] { killAllActors(killer); });
            return 0.0;
        });
        // AttachAshPile [form]: the ash pile is put where the character lies (0x5DB870 places a reference at the position of
        // the character). Without a form the executable takes the default one, whose record the port could not find in the
        // default objects; it takes the activator the files name DefaultAshPile1 (the local number 0x1B of the first
        // master), which is the one of a character of a human size.
        mInterpreter.setHandler("AttachAshPile", [this](FalloutScript::CallContext& call) {
            if (call.mReference == 0 || !isActor(call.mReference))
                return 0.0;
            FalloutScript::FormId pile = FalloutScript::formArgument(call, 0);
            if (pile == 0)
                pile = masterForm(defaultAshPileNumber);
            if (mStore.get<ESM4::Activator>().search(refId(pile)) == nullptr)
            {
                Log(Debug::Info) << "AttachAshPile: " << FalloutScript::hex(pile) << " is no activator: no ash pile";
                return 0.0;
            }
            runOnMainThread([this, actor = call.mReference, pile] {
                const FalloutScript::FormId made = placeAtMe(actor, pile, 1, 0, 0);
                Log(Debug::Info) << "AttachAshPile: " << FalloutScript::hex(made) << " (the form "
                                 << FalloutScript::hex(pile) << ") is put at " << FalloutScript::hex(actor);
            });
            return 0.0;
        });
    }

    void FalloutScripts::updateDamage(float frametime)
    {
        mDismemberment.newFrame();
        mDestruction.update(frametime);
    }

    void FalloutScripts::noteDeathTime(FalloutScript::FormId actor)
    {
        mDismemberment.setDeathHour(actor, FalloutScript::DismemberRule::wholeHours(gameDaysPassed(), gameHour()));
    }

    void FalloutScripts::destructionChanged(
        FalloutScript::FormId reference, const FalloutScript::DestructionOutcome& outcome)
    {
        // The self-damage of a burning thing ticks every frame: only what changes the thing is told
        static std::atomic<int> sLogged{ 0 };
        const bool shows = !outcome.mCrossed.empty() || outcome.mModelStage >= 0 || outcome.mDisable || outcome.mDestroyed;
        if (shows && sLogged.fetch_add(1) < 200)
        {
            Log log(Debug::Info);
            log << "Destruction: " << FalloutScript::hex(reference) << " health " << outcome.mHealth << " of "
                << outcome.mMaxHealth << ", stage " << outcome.mStageBefore << " -> " << outcome.mStageAfter;
            if (!outcome.mCrossed.empty())
                log << ", crossed " << outcome.mCrossed.size() << " stage(s)";
            if (outcome.mModelStage >= 0)
                log << ", model of stage " << outcome.mModelStage;
            if (outcome.mDamageStage != 0)
                log << ", damage stage " << outcome.mDamageStage;
            if (outcome.mDisable)
                log << ", disabled";
            if (outcome.mDestroyed)
                log << ", destroyed";
        }
        runOnMainThread([this, reference, outcome] { showDestruction(reference, outcome); });
    }

    void FalloutScripts::showDestruction(
        FalloutScript::FormId reference, const FalloutScript::DestructionOutcome& outcome)
    {
        FalloutScript::DestructionData data;
        const bool known = mDestruction.dataOf(reference, data);
        bool modelChanged = false;
        if (known && outcome.mModelStage >= 0 && outcome.mModelStage < static_cast<int>(data.mStages.size()))
            modelChanged = OFClass::ModelOverrides::get().set(
                worldIdOf(reference), data.mStages[outcome.mModelStage].mModel);
        // The explosions and the debris of the stages that were crossed are not made: the port has no explosions
        for (const int stage : outcome.mCrossed)
        {
            if (!known || stage >= static_cast<int>(data.mStages.size()))
                continue;
            const FalloutScript::DestructionStage& crossed = data.mStages[stage];
            static std::atomic<int> sLogged{ 0 };
            if ((crossed.mExplosion != 0 || crossed.mDebris != 0) && sLogged.fetch_add(1) < 10)
                Log(Debug::Info) << "Destruction: stage " << stage << " of " << FalloutScript::hex(reference)
                                 << " has an explosion " << FalloutScript::hex(crossed.mExplosion) << " and "
                                 << crossed.mDebrisCount << " debris " << FalloutScript::hex(crossed.mDebris)
                                 << ": not made";
        }
        const OFWorld::Ptr ptr = findReference(reference);
        if (ptr.isEmpty() || !ptr.isInCell())
            return;
        if (outcome.mDisable)
        {
            if (setEnabled(ptr, false))
            {
                const std::lock_guard lock(mDamageMutex);
                mDisabledByDestruction.insert(reference);
            }
        }
        else if (modelChanged && ptr.getRefData().isEnabled() && !isActorRecord(ptr))
            reinsertObject(ptr);
    }

    void FalloutScripts::reinsertObject(const OFWorld::Ptr& ptr)
    {
        // The model of a reference is taken again when it goes into the scene: out and in again shows the model it has
        // now (the thing is not disabled for the scripts: they run on this thread, between the two)
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (world == nullptr || ptr == world->getPlayerPtr() || ptr.getCellRef().getCount() == 0)
            return;
        world->disable(ptr);
        world->enable(ptr);
    }

    void FalloutScripts::destructionRestored(FalloutScript::FormId reference)
    {
        // The model goes back and the mark of a disabled thing is taken in the queued action, in the order of the queue: a
        // blow of another thread that is still waiting there is undone by it, not left on top of it
        runOnMainThread([this, reference] {
            const bool changed = OFClass::ModelOverrides::get().set(worldIdOf(reference), {});
            bool wasDisabled = false;
            {
                const std::lock_guard lock(mDamageMutex);
                wasDisabled = mDisabledByDestruction.erase(reference) != 0;
            }
            const OFWorld::Ptr ptr = findReference(reference);
            if (ptr.isEmpty() || !ptr.isInCell())
                return;
            if (wasDisabled)
                setEnabled(ptr, true);
            else if (changed && ptr.getRefData().isEnabled() && !isActorRecord(ptr))
                reinsertObject(ptr);
        });
    }

    void FalloutScripts::showDamagedObjects()
    {
        // After a game is loaded the things that took damage show the models of their stages again: the models are given
        // before their cells are loaded, so the scene takes them
        for (const FalloutScript::FormId reference : mDestruction.damaged())
        {
            // What a stage took out of the world is told from the health, since the mark of it is not saved: a clear of
            // the destruction after a load puts the thing back
            if (mDestruction.disabledByStage(reference))
            {
                const std::lock_guard lock(mDamageMutex);
                mDisabledByDestruction.insert(reference);
            }
            // (what scripts made while the game was played is not known to the world yet: adoptObject shows it when it is)
            constexpr FalloutScript::FormId madeWhilePlaying = 0xFF000000u;
            if ((reference & madeWhilePlaying) != madeWhilePlaying)
                showSavedDamage(reference, OFWorld::Ptr());
        }
    }

    void FalloutScripts::showSavedDamage(FalloutScript::FormId reference, const OFWorld::Ptr& placed)
    {
        const int stage = mDestruction.modelStage(reference);
        FalloutScript::DestructionData data;
        if (stage < 0 || !mDestruction.dataOf(reference, data) || stage >= static_cast<int>(data.mStages.size()))
            return;
        const bool changed = OFClass::ModelOverrides::get().set(worldIdOf(reference), data.mStages[stage].mModel);
        // A thing that is in the scene already (an object that a load made, in a cell that is loaded) shows the model it
        // had when it went in: out and in again takes the model of the stage
        if (changed && !placed.isEmpty() && placed.isInCell() && placed.getRefData().isEnabled() && !isActorRecord(placed))
            reinsertObject(placed);
    }

    void FalloutScripts::resetDamage()
    {
        mDestruction.clear();
        mDismemberment.clear();
        OFClass::ModelOverrides::get().clear();
        const std::lock_guard lock(mDamageMutex);
        mDisabledByDestruction.clear();
    }

    FalloutScript::DismemberSettings FalloutScripts::dismemberSettings()
    {
        return FalloutScript::DismemberSettings::fromGameSettings(
            [this](std::string_view name) -> std::optional<double> { return gameSetting(name); });
    }

    void FalloutScripts::goreOfKill(
        FalloutScript::FormId victim, FalloutScript::FormId attacker, FalloutScript::FormId weapon, int limb, bool melee)
    {
        namespace AV = FalloutScript::ActorValue;
        // The rule runs for the blow that killed (0x899CB0 calls it with what the actor's damage function answered)
        const FalloutScript::DismemberSettings settings = dismemberSettings();
        if (victim == 0)
            return;
        // What the blow came from is kept for every kill, whether or not it takes a limb off (0x89C0B1 stores the cause
        // with the death, not with the gore): a shot is a projectile and a swing the weapon itself
        mDismemberment.recordDeath(victim,
            FalloutScript::DismemberRule::causeOf(false, !melee && weapon != 0, melee), attacker);
        if (settings.mGoreOff || victim == mPlayer)
            return;
        const std::vector<FalloutScript::BodyPart> parts = mLimbs.partsOf(victim);
        // The blow's limb must have no condition left, or have none (the executable skips the rule for a limb that still
        // has some)
        const FalloutScript::BodyPart* hitPart = nullptr;
        for (const FalloutScript::BodyPart& part : parts)
            if (part.mType == limb)
                hitPart = &part;
        if (hitPart != nullptr && hitPart->mCondition != -1 && mActorValues.current(victim, hitPart->mCondition) > 0)
            return;
        if (!mDismemberment.takeSlot(settings.mPerFrame))
            return;

        FalloutScript::DismemberHit hit;
        hit.mLimb = limb;
        hit.mBloodyMess = attacker != 0 && mActorValues.current(attacker, bloodyMessValue) > 0;
        if (const ESM4::Weapon* record = weapon != 0 ? mStore.get<ESM4::Weapon>().search(refId(weapon)) : nullptr)
            if (record->mHasGameData)
                hit.mOnHit = static_cast<int>(record->mGame.mOnHit);
        const auto condition = [this, victim](int value) { return mActorValues.current(victim, value); };
        const auto isGone = [this, victim](int part) { return mDismemberment.isGone(victim, part); };
        const auto random = [this] {
            const std::lock_guard lock(mCombatRandomMutex);
            return std::uniform_int_distribution<int>(0, 99)(mCombatRandom);
        };
        const std::vector<FalloutScript::Cut> cuts
            = FalloutScript::DismemberRule::cuts(settings, parts, hit, condition, isGone, random);
        for (const FalloutScript::Cut& cut : cuts)
        {
            mDismemberment.markGone(victim, cut.mLimb, cut.mExploded);
            static std::atomic<int> sLogged{ 0 };
            if (sLogged.fetch_add(1) < 100)
                Log(Debug::Info) << "Dismember: " << FalloutScript::hex(victim) << " loses limb " << cut.mLimb
                                 << (cut.mExploded ? " (burst)" : "") << " (the part is not hidden in the model)";
        }
    }

    void FalloutScripts::pushActorAway(
        FalloutScript::FormId origin, FalloutScript::FormId actor, double force)
    {
        runOnMainThread([this, origin, actor, force] {
            OFBase::World* world = OFBase::Environment::get().getWorld();
            const OFWorld::Ptr target = actor == mPlayer ? world->getPlayerPtr() : findReference(actor);
            const OFWorld::Ptr from = origin == mPlayer ? world->getPlayerPtr() : findReference(origin);
            if (target.isEmpty() || from.isEmpty() || !target.isInCell())
                return;
            const FalloutScript::HitReactions::Settings reaction
                = FalloutScript::HitReactions::Settings::fromGameSettings(
                    [this](std::string_view name) -> std::optional<double> { return gameSetting(name); });
            // The number goes the way a blow's damage does into the force: with the agility of the one pushed
            const double thrust = FalloutScript::HitReactions::knockbackForce(
                reaction, force, mActorValues.current(actor, FalloutScript::ActorValue::agility));
            osg::Vec3f away = target.getRefData().getPosition().asVec3() - from.getRefData().getPosition().asVec3();
            away.z() = 0.f;
            if (thrust <= 0 || away.length2() < 1.f)
            {
                Log(Debug::Info) << "PushActorAway: " << FalloutScript::hex(actor) << " is not pushed (force " << thrust
                                 << ")";
                return;
            }
            away.normalize();
            Log(Debug::Info) << "PushActorAway: " << FalloutScript::hex(actor) << " away from " << FalloutScript::hex(origin)
                             << " with force " << thrust;
            OFBase::Environment::get().getMechanicsManager()->knockFallout(
                target, away * static_cast<float>(thrust), static_cast<float>(reaction.mKnockbackTime));
        });
    }

    void FalloutScripts::fireWeaponFor(FalloutScript::FormId actor, FalloutScript::FormId weapon)
    {
        runOnMainThread([this, actor, weapon] {
            if (mStore.get<ESM4::Weapon>().search(refId(weapon)) == nullptr)
            {
                Log(Debug::Warning) << "FireWeapon: " << FalloutScript::hex(weapon) << " is no weapon";
                return;
            }
            OFBase::World* world = OFBase::Environment::get().getWorld();
            const OFWorld::Ptr shooter = actor == mPlayer ? world->getPlayerPtr() : findReference(actor);
            if (shooter.isEmpty() || !shooter.isInCell() || mActors.isDead(actor))
                return;
            // It shoots at whom it fights, and into the air when it has no one to
            const FalloutScript::FormId opponent = mCombat.target(actor);
            const OFWorld::Ptr target = opponent != 0 ? findReference(opponent) : OFWorld::Ptr();
            // The shot is the one of an order of UseWeapon that has one attack: it holds the weapon that was named and
            // takes no round of ammunition. An order the character has already (for another weapon, or for no damage)
            // is put aside for the shot and comes back after it, as it was.
            std::optional<ScriptedOrder> previous;
            if (const auto found = mScriptedAttacks.find(actor); found != mScriptedAttacks.end())
                previous = found->second;
            FalloutScript::UseWeaponOrder order;
            order.mWeapon = weapon;
            order.mAttacks = 1;
            order.mAlwaysHit = true;
            mScriptedAttacks[actor] = ScriptedOrder{ order, 0, ++mScriptedSerial };
            const FireResult result = fire(actor, target, -1, 1.0, true, false);
            if (previous)
                mScriptedAttacks[actor] = *previous;
            else
                mScriptedAttacks.erase(actor);
            Log(Debug::Info) << "FireWeapon: " << FalloutScript::hex(actor) << " fires " << FalloutScript::hex(weapon)
                             << (result.mFired ? "" : ", it did not fire") << (result.mHit ? ", it hit " : ", it hit no one")
                             << (result.mHit ? FalloutScript::hex(opponent) : std::string());
        });
    }

    void FalloutScripts::killAllActors(FalloutScript::FormId killer)
    {
        // Every character and creature of the loaded cells but the player: the executables go through the list of the actors
        // that are loaded with a body (0x11E0E80) and kill each as Kill does (0x89D900)
        std::vector<FalloutScript::FormId> actors;
        for (const auto& [reference, ptr] : mReferences)
            if (reference != mPlayer && isActorRecord(ptr) && ptr.isInCell())
                actors.push_back(reference);
        int killed = 0;
        int dead = 0;
        for (const FalloutScript::FormId reference : actors)
        {
            if (mActors.isDead(reference))
                ++dead;
            else if (mActors.kill(reference, killer))
                ++killed;
        }
        Log(Debug::Info) << "KillAllActors: " << killed << " of " << actors.size() << " character(s) killed, " << dead
                         << " were dead";
    }
}
