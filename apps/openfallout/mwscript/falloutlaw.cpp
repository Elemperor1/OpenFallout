#include "falloutlaw.hpp"

#include <algorithm>
#include <cmath>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadclas.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/combatcommands.hpp>
#include <components/falloutscript/perks.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "falloutscripts.hpp"

namespace OFScript
{
    namespace
    {
        using FalloutScript::FormId;

        /// How far from the player the characters that can have seen a crime are looked for (the executables' alarm
        /// distance is much larger than what any character sees)
        constexpr double maxWitnessDistance = 8000.0;
        /// The seconds between the looks at who is in a place that is not the player's
        constexpr float trespassLookInterval = 0.5f;
        /// The record flag that makes the interior cell off limits
        constexpr std::uint32_t offLimitsFlag = 0x00020000;
        /// The actor value 3 is the responsibility of a character
        constexpr int responsibilityValue = 3;

        FormId numberOf(const ESM::FormId& form)
        {
            return form.isZeroOrUnset() || !form.hasContentFile() ? 0 : form.toUint32();
        }
    }

    bool FalloutScripts::playerEnemyOf(FalloutScript::FormId faction, FalloutScript::FormId target)
    {
        // A faction that the player made its enemy (a crime that was seen, SetPCEnemyofFaction) turns its members on the
        // player whatever the records say
        return target == mPlayer && mPlayer != 0 && mObjectState.enemyOfFaction(faction);
    }

    void FalloutScripts::alarmStopped(FalloutScript::FormId actor)
    {
        // The crimes the character is the victim of or a witness to are forgotten, and the enemy flags they set with them
        if (mLaw != nullptr)
            mLaw->law().stopAlarm(actor);
    }

    bool noteTaking(const OFWorld::Ptr& actor, const OFWorld::Ptr& item, const OFWorld::Ptr& container, int count)
    {
        if (!OFBase::Environment::get().hasFalloutScripts())
            return false;
        FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
        if (scripts == nullptr || scripts->lawGlue() == nullptr)
            return false;
        scripts->lawGlue()->taken(actor, item, container, count);
        return true;
    }

    LawGlue::LawGlue(FalloutScripts& host)
        : mHost(host)
        , mLaw(*this, host.mObjectState)
        , mStanding(*this)
    {
    }

    void LawGlue::clear()
    {
        mLaw.clear();
        mStanding.clear();
        const std::lock_guard lock(mAlarmMutex);
        mAlarmed.clear();
        mSinceLook = 0.f;
    }

    void LawGlue::addCommands()
    {
        FalloutScript::addLawCommands(mHost.mInterpreter, mLaw, mHost.mObjectState, *this, *this);
        FalloutScript::addStandingCommands(mHost.mInterpreter, mStanding, *this, *this, mHost.mActors, mHost);
    }

    // ---- what the records say ----

    FormId LawGlue::player()
    {
        return mHost.mPlayer;
    }

    FalloutScript::LawSettings LawGlue::lawSettings()
    {
        return FalloutScript::LawSettings::fromGameSettings(
            mHost.mGame, [this](std::string_view name) { return mHost.gameSetting(name); });
    }

    bool LawGlue::tracksCrime(FormId faction)
    {
        const ESM4::Faction* record = mHost.mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(faction));
        return record != nullptr && (record->mFactionFlags2 & ESM4::Faction::Flag_TrackCrime) != 0;
    }

    bool LawGlue::evil(FormId faction)
    {
        const ESM4::Faction* record = mHost.mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(faction));
        return record != nullptr && (record->mFactionFlags & ESM4::Faction::Flag_Evil) != 0;
    }

    FormId LawGlue::reputationOf(FormId faction)
    {
        const ESM4::Faction* record = mHost.mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(faction));
        return record != nullptr ? numberOf(record->mReputation) : 0;
    }

    FalloutScript::StandingSettings LawGlue::standingSettings()
    {
        return FalloutScript::StandingSettings::fromGameSettings(
            [this](std::string_view name) { return mHost.gameSetting(name); });
    }

    std::optional<int> LawGlue::recordedReaction(FormId faction, FormId other)
    {
        const ESM4::Faction* record = mHost.mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(faction));
        if (record == nullptr)
            return std::nullopt;
        const ESM::FormId target = ESM::FormId::fromUint32(other);
        for (const ESM4::Faction::Relation& relation : record->mRelations)
            if (relation.mTarget == target)
                return relation.mModifier;
        return std::nullopt;
    }

    // ---- what the world says ----

    std::vector<FormId> LawGlue::factionsOf(FormId actor)
    {
        std::vector<std::pair<FormId, int>> factions;
        std::vector<FormId> result;
        if (actor != 0 && mHost.mActors.factions(actor, factions))
            for (const auto& [faction, rank] : factions)
                if (rank >= 0)
                    result.push_back(faction);
        return result;
    }

    FormId LawGlue::ownerForm(FormId form)
    {
        return mHost.ownerForm(form);
    }

    FormId LawGlue::cellOf(FormId reference)
    {
        return mHost.cellOf(reference);
    }

    bool LawGlue::isGuard(FormId actor)
    {
        ESM::FormId base;
        if (!mHost.baseOf(actor, base) || base.isZeroOrUnset())
            return false;
        const ESM4::Npc* npc = mHost.mStore.get<ESM4::Npc>().search(base);
        if (npc == nullptr)
            return false;
        // The class of the character says so (the guard flag of its record)
        const ESM4::Class* record = mHost.mStore.get<ESM4::Class>().search(npc->mClass);
        return record != nullptr && (record->mFo3Flags & 2) != 0;
    }

    int LawGlue::detectionLevel(FormId watcher, FormId target)
    {
        using FalloutScript::Detection::Sighting;
        const OFWorld::Ptr from = mHost.findReference(watcher);
        const OFWorld::Ptr to = mHost.findReference(target);
        if (from.isEmpty() || to.isEmpty())
            return 0;
        switch (mHost.sightOf(from, to))
        {
            case Sighting::Seen:
                return 3;
            case Sighting::Noticed:
                return 2;
            default:
                return 0;
        }
    }

    bool LawGlue::alarmed(FormId actor)
    {
        // The package of the alarm is not one that characters run here; a character that an alarm turned on the player and
        // that still fights it stands for it
        {
            const std::lock_guard lock(mAlarmMutex);
            if (mAlarmed.count(actor) == 0)
                return false;
        }
        return mHost.mCombat.inCombat(actor) && mHost.mCombat.target(actor) == mHost.mPlayer;
    }

    bool LawGlue::alarmSounding()
    {
        std::vector<FormId> asked;
        {
            const std::lock_guard lock(mAlarmMutex);
            asked.assign(mAlarmed.begin(), mAlarmed.end());
        }
        std::vector<FormId> over;
        bool sounding = false;
        for (const FormId actor : asked)
        {
            if (mHost.mCombat.inCombat(actor) && mHost.mCombat.target(actor) == mHost.mPlayer)
                sounding = true;
            else
                over.push_back(actor);
        }
        if (!over.empty())
        {
            // The ones whose fight is over are not the alarm any more
            const std::lock_guard lock(mAlarmMutex);
            for (const FormId actor : over)
                mAlarmed.erase(actor);
        }
        return sounding;
    }

    bool LawGlue::detected(FormId actor)
    {
        // 0x973460: of the actors that are loaded, those that are characters (the virtual at +0x218 is false for a creature)
        // and see the subject in full (the level of detection 3, 0x8A81E0), except one that fights the subject already (its
        // combat target is the subject, 0x973550). The actor is not itself in the list that is asked.
        for (const FalloutScript::Witness& witness : watchers(0))
        {
            FalloutScript::ActorTraits traits;
            if (witness.mActor == actor || (mHost.traitsOf(witness.mActor, traits) && traits.mCreature))
                continue;
            if (mHost.mCombat.target(witness.mActor) == actor)
                continue;
            if (detectionLevel(witness.mActor, actor) >= 3)
                return true;
        }
        return false;
    }

    bool LawGlue::trespassingNow(FormId actor)
    {
        // IsTrespassing asks of the character that trespasses (0x5A1900 asks the actor itself): here only the player can,
        // and it does while someone minds it in a place that is not its own. The trespass that the law keeps is that of
        // the watchers (the characters that warn), which are not the trespassers.
        return actor != 0 && actor == mHost.mPlayer && mLaw.anyTrespass();
    }

    std::vector<FalloutScript::Witness> LawGlue::gather(FormId victim)
    {
        std::vector<FalloutScript::Witness> result;
        OFBase::World* world = OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return result;
        const double radius = std::min(lawSettings().mAlarmDistance, maxWitnessDistance);
        std::vector<OFWorld::Ptr> actors;
        OFBase::Environment::get().getMechanicsManager()->getFalloutActorsInRange(
            player.getRefData().getPosition().asVec3(), static_cast<float>(radius), actors);
        for (const OFWorld::Ptr& ptr : actors)
        {
            const FormId id = mHost.scriptReferenceOf(ptr);
            if (id == 0 || id == mHost.mPlayer || !mHost.sharesSpace(ptr.getCell(), player.getCell()))
                continue;
            FalloutScript::Witness witness;
            witness.mActor = id;
            witness.mFactions = factionsOf(id);
            witness.mWatching = !mHost.mActors.isDead(id) && !mHost.mCombat.unconscious(id)
                && !mHost.mObjectState.ignoresCrime(id)
                && mHost.sightOf(ptr, player) != FalloutScript::Detection::Sighting::Unseen;
            if (victim != 0 && id != victim)
            {
                const auto relation = FalloutScript::relationOfActors(mHost.mCombat, mHost.mActors, mHost, id, victim);
                witness.mFriendOfVictim = relation == FalloutScript::CombatState::Relation::Friend
                    || relation == FalloutScript::CombatState::Relation::Ally;
            }
            result.push_back(std::move(witness));
        }
        return result;
    }

    std::vector<FalloutScript::Witness> LawGlue::watchers(FormId victim)
    {
        return gather(victim);
    }

    std::vector<FalloutScript::Witness> LawGlue::responders(FormId caller, FormId subject)
    {
        // Those that answer an alarm are the character that sounds it (or the one that is named) and everyone that shares
        // a faction with the one that sounds it
        const std::vector<FormId> callerFactions = factionsOf(caller);
        std::vector<FalloutScript::Witness> result = gather(0);
        result.erase(std::remove_if(result.begin(), result.end(),
                         [&](const FalloutScript::Witness& witness) {
                             if (subject != 0)
                                 return witness.mActor != subject;
                             if (witness.mActor == caller)
                                 return false;
                             return std::none_of(witness.mFactions.begin(), witness.mFactions.end(),
                                 [&](FormId faction) {
                                     return std::find(callerFactions.begin(), callerFactions.end(), faction)
                                         != callerFactions.end();
                                 });
                         }),
            result.end());
        return result;
    }

    void LawGlue::react(const FalloutScript::Reaction& reaction, FormId victim)
    {
        using Kind = FalloutScript::Reaction::Kind;
        switch (reaction.mKind)
        {
            case Kind::Attack:
                // Every character that answers fights the player: one that fights someone else turns on the player (the
                // fight is started over with the new target), and one that fights the player already is left as it is
                if (mHost.mCombat.target(reaction.mActor) != mHost.mPlayer
                    && FalloutScript::startFight(mHost.mCombat, mHost.mActors, reaction.mActor, mHost.mPlayer))
                    mHost.evaluatePackage(reaction.mActor);
                // One that fights the player because of an alarm is what the alarm sounding stands for
                if (mHost.mCombat.inCombat(reaction.mActor) && mHost.mCombat.target(reaction.mActor) == mHost.mPlayer)
                {
                    const std::lock_guard lock(mAlarmMutex);
                    mAlarmed.insert(reaction.mActor);
                }
                break;
            case Kind::Say:
            case Kind::Warn:
            case Kind::SayNoCrime:
            {
                const FormId topic = FalloutScript::Law::lineFor(reaction.mCrime,
                    reaction.mKind == Kind::SayNoCrime ? FalloutScript::CrimeLine::NoCrime
                                                       : FalloutScript::CrimeLine::Crime);
                // The conditions of the line ask who the victim is while it is chosen
                mLaw.setLineVictim(victim);
                mHost.sayTopic(reaction.mActor, topic, mHost.mPlayer);
                mLaw.setLineVictim(0);
                break;
            }
        }
    }

    void LawGlue::carryOut(const FalloutScript::Outcome& outcome, FormId victim)
    {
        mHost.runOnMainThread([this, outcome, victim] {
            for (const FalloutScript::Reaction& reaction : outcome.mReactions)
                react(reaction, victim);
            for (const FalloutScript::Infamy& infamy : outcome.mInfamy)
            {
                const FalloutScript::Reputation::Bump bump = mHost.mReputation.addExact(infamy.mReputation, 0, infamy.mAmount);
                if (bump.mApplied)
                    mHost.reputationChanged(bump);
            }
            if (outcome.mKarma != 0)
                mHost.mProgress.rewardKarma(outcome.mKarma);
            if (outcome.mVictimDisposition != 0 && victim != 0)
            {
                FalloutScript::DispositionInput input;
                int now = 0;
                if (dispositionInput(victim, mHost.mPlayer, input))
                    now = mStanding.disposition(victim, mHost.mPlayer, input,
                        [this, victim](double value) { return adjustDisposition(victim, mHost.mPlayer, value); });
                mStanding.modifyDisposition(victim, mHost.mPlayer, outcome.mVictimDisposition, now);
            }
            if (outcome.mCrime != 0 || !outcome.mEnemies.empty())
                Log(Debug::Info) << "Crime " << outcome.mCrime << ": " << outcome.mReactions.size() << " reactions, "
                                 << outcome.mEnemies.size() << " factions turned, karma " << outcome.mKarma;
        });
    }

    // ---- standing ----

    bool LawGlue::dispositionInput(FormId self, FormId other, FalloutScript::DispositionInput& input)
    {
        namespace AV = FalloutScript::ActorValue;
        FalloutScript::ActorTraits mine;
        FalloutScript::ActorTraits theirs;
        if (!mHost.traitsOf(self, mine) || !mHost.traitsOf(other, theirs))
            return false;
        ESM::FormId base;
        if (mHost.baseOf(self, base) && !base.isZeroOrUnset())
            if (const ESM4::Npc* npc = mHost.mStore.get<ESM4::Npc>().search(base))
                input.mBase = npc->mBaseConfig.fo3.dispositionBase;
        input.mCreature = mine.mCreature;
        input.mCharisma = mHost.mActorValues.current(other, AV::charisma);
        input.mKarma = karmaOf(other);
        input.mResponsibility = mHost.mActorValues.current(self, responsibilityValue);
        if (const ESM4::Race* race = mHost.mStore.get<ESM4::Race>().search(ESM::FormId::fromUint32(mine.mRace)))
        {
            const auto found = race->mDisposition.find(ESM::FormId::fromUint32(theirs.mRace));
            if (found != race->mDisposition.end())
                input.mRace = found->second;
        }
        const FalloutScript::Standing::PairReaction pair
            = mStanding.pairReaction(factionsOf(self), factionsOf(other));
        input.mFactionPair = pair.mFound;
        input.mFactionReaction = pair.mReaction;
        input.mWeaponDrawn = mHost.mEquipment.weaponDrawn(other);
        // The same character is the same reference (two characters of one base record are not the same)
        input.mSame = self == other;
        return true;
    }

    int LawGlue::dispositionToPlayer(FormId self)
    {
        FalloutScript::DispositionInput input;
        if (!dispositionInput(self, mHost.mPlayer, input))
            return -1;
        // With the perks that change it, as GetDisposition does, so that the answers of both agree
        return mStanding.disposition(
            self, mHost.mPlayer, input, [this, self](double value) { return adjustDisposition(self, mHost.mPlayer, value); });
    }

    double LawGlue::adjustDisposition(FormId self, FormId other, double value)
    {
        // The entry point 14 of the perks (FalloutNV.exe 0x88032F, called with the disposition as a float; the result is cut
        // and held to 0 to 100 by the caller): the perks of the character that is asked about, or those of the player for one
        // that is a teammate (0x5E5911 to 0x5E5934), with the other character as the one that the conditions of the entry
        // see as the target
        if (self == 0 || other == 0)
            return value;
        const FormId owner = self != mHost.mPlayer && mHost.isTeammate(self) ? mHost.mPlayer : self;
        return mHost.mPerks.apply(FalloutScript::EntryPoint::disposition, owner, value,
            FalloutScript::PerkEngine::Context{ other, 0 });
    }

    int LawGlue::relation(FormId self, FormId other)
    {
        using Relation = FalloutScript::CombatState::Relation;
        switch (FalloutScript::relationOfActors(mHost.mCombat, mHost.mActors, mHost, self, other))
        {
            case Relation::Enemy:
                return 1;
            case Relation::Ally:
                return 2;
            case Relation::Friend:
                return 3;
            default:
                return 0;
        }
    }

    double LawGlue::karmaOf(FormId actor)
    {
        namespace AV = FalloutScript::ActorValue;
        return actor == mHost.mPlayer ? mHost.mProgress.karma() : mHost.mActorValues.current(actor, AV::karma);
    }

    bool LawGlue::isEvil(FormId actor)
    {
        return FalloutScript::Standing::actorEvil(factionsOf(actor), [this](FormId faction) { return evil(faction); });
    }

    // ---- the events ----

    void LawGlue::blow(FormId victim, FormId weapon, bool fighting)
    {
        mHost.runOnMainThread([this, victim, weapon, fighting] {
            if (mHost.mActors.isDead(victim))
                return;
            // The hit routine (FalloutNV.exe 0x898DD5 to 0x898E26) asks whether the victim is a creature (the
            // virtual at +0x21C, true only for creatures): a creature is no one that the law protects, so a blow at
            // one makes no attack crime and no witness hears of it; only a creature that has an owner counts, as
            // the owner's property is hit (the steal crime creator 0x8BFA40, which only warns)
            FalloutScript::ActorTraits traits;
            const bool creature = mHost.traitsOf(victim, traits) && traits.mCreature;
            const FormId owner = creature ? mHost.mObjectState.owner(victim) : 0;
            switch (FalloutScript::Law::judgeBlow(fighting, creature, owner != 0))
            {
                case FalloutScript::Law::BlowKind::Nothing:
                    // (a victim that fights already only thinks less of the player: that is the law's own step)
                    if (!fighting)
                        return;
                    break;
                case FalloutScript::Law::BlowKind::Property:
                    // What belongs to the player, or to someone the player may take from, is no one's loss
                    if (!FalloutScript::Law::mayTake(owner, mHost.playerBase(), factionsOf(mHost.mPlayer)))
                        theft(owner, 0, false);
                    return;
                case FalloutScript::Law::BlowKind::Attack:
                    break;
            }
            FalloutScript::Law::Blow blow;
            blow.mVictim = victim;
            blow.mVictimFactions = factionsOf(victim);
            blow.mFighting = fighting;
            // A weapon whose record has the minor crime flag (0x898FD0 reads flags2 & 0x10 and the hit routine passes
            // it to the creator at 0x8C0460): only the one that was hit minds, and the crime is a minor one
            if (weapon != 0)
                if (const ESM4::Weapon* record = mHost.mStore.get<ESM4::Weapon>().search(ESM::FormId::fromUint32(weapon)))
                    blow.mMinor = record->mHasGameData
                        && (record->mGame.mFlags2 & ESM4::Weapon::GameData::MinorCrime) != 0;
            const FalloutScript::Outcome outcome = mLaw.attack(blow, gather(victim));
            carryOut(outcome, victim);
        });
    }

    void LawGlue::killed(FormId victim)
    {
        mHost.runOnMainThread([this, victim] {
            namespace AV = FalloutScript::ActorValue;
            FalloutScript::Law::Killing killing;
            killing.mVictim = victim;
            killing.mVictimFactions = factionsOf(victim);
            FalloutScript::ActorTraits traits;
            killing.mCharacter = !(mHost.traitsOf(victim, traits) && traits.mCreature);
            killing.mEvil = isEvil(victim);
            killing.mKarmaClass = FalloutScript::Standing::karmaClass(standingSettings(), karmaOf(victim));
            // A death does not count against a friend or ally of the player that does not mind friendly hits
            const auto relation = FalloutScript::relationOfActors(mHost.mCombat, mHost.mActors, mHost, victim, mHost.mPlayer);
            const bool kin = relation == FalloutScript::CombatState::Relation::Friend
                || relation == FalloutScript::CombatState::Relation::Ally;
            killing.mCrimeOnDeath = !(kin && mHost.mCombat.ignoresFriendlyHits(victim));
            const FalloutScript::Outcome outcome = mLaw.murder(killing, gather(victim));
            carryOut(outcome, victim);
        });
    }

    OFWorld::Ptr LawGlue::nearestOwner(FormId owner)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world->getPlayerPtr();
        OFWorld::Ptr best;
        double bestDistance = 0;
        std::vector<OFWorld::Ptr> actors;
        OFBase::Environment::get().getMechanicsManager()->getFalloutActorsInRange(
            player.getRefData().getPosition().asVec3(), static_cast<float>(maxWitnessDistance), actors);
        const bool faction = mHost.mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(owner)) != nullptr;
        for (const OFWorld::Ptr& ptr : actors)
        {
            const FormId id = mHost.scriptReferenceOf(ptr);
            if (id == 0 || id == mHost.mPlayer || mHost.mActors.isDead(id)
                || !mHost.sharesSpace(ptr.getCell(), player.getCell()))
                continue;
            bool owns = false;
            if (faction)
            {
                const std::vector<FormId> factions = factionsOf(id);
                owns = std::find(factions.begin(), factions.end(), owner) != factions.end();
            }
            else
                owns = mHost.ownerForm(id) == owner;
            if (!owns)
                continue;
            const double distance
                = (ptr.getRefData().getPosition().asVec3() - player.getRefData().getPosition().asVec3()).length();
            if (best.isEmpty() || distance < bestDistance)
            {
                best = ptr;
                bestDistance = distance;
            }
        }
        return best;
    }

    void LawGlue::theft(FormId owner, std::int64_t value, bool karmaOnly)
    {
        const OFWorld::Ptr victim = karmaOnly ? OFWorld::Ptr() : nearestOwner(owner);
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        FalloutScript::Law::Theft theft;
        theft.mValue = value;
        theft.mDay = static_cast<int>(std::floor(mHost.gameDaysPassed()));
        // What belonged to a faction that is evil, or to a character that is, costs no karma
        const bool faction = mHost.mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(owner)) != nullptr;
        if (faction)
            theft.mOwnerNotEvil = !evil(owner);
        if (!victim.isEmpty())
        {
            theft.mVictim = mHost.scriptReferenceOf(victim);
            theft.mVictimFactions = factionsOf(theft.mVictim);
            theft.mVictimWatching = mHost.sightOf(victim, player) != FalloutScript::Detection::Sighting::Unseen;
            if (!faction)
            {
                const int karmaClass = FalloutScript::Standing::karmaClass(standingSettings(), karmaOf(theft.mVictim));
                // (a character of an evil faction is evil whatever its karma is)
                theft.mOwnerNotEvil = karmaClass != 2 && karmaClass != 4 && !isEvil(theft.mVictim);
            }
        }
        const FalloutScript::Outcome outcome = mLaw.steal(theft);
        carryOut(outcome, theft.mVictim);
    }

    void LawGlue::taken(const OFWorld::Ptr& actor, const OFWorld::Ptr& item, const OFWorld::Ptr& container, int count)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (actor.isEmpty() || actor != world->getPlayerPtr() || item.isEmpty())
            return;
        const FormId reference = mHost.scriptReferenceOf(container.isEmpty() ? item : container);
        // What is taken from a character's pockets or body is not a theft of the owner of the place (the class of a
        // character of Fallout is not an actor of the engine, so the records say whether it is one)
        if (!container.isEmpty() && (container.getClass().isActor() || mHost.isActor(reference)))
            return;
        const bool gold = item.getClass().isGold(item);
        const std::int64_t value = gold ? count : static_cast<std::int64_t>(count) * item.getClass().getValue(item);
        theftOf(reference, value);
    }

    void LawGlue::pickedUp(FormId reference, FormId item, int count)
    {
        // The owner and the character that is robbed are looked up in the world, which only the thread of the frame may ask
        mHost.runOnMainThread([this, reference, item, count] {
            theftOf(reference, static_cast<std::int64_t>(count) * mHost.itemValue(item));
        });
    }

    void LawGlue::takenFrom(FormId container, FormId item, int count)
    {
        mHost.runOnMainThread([this, container, item, count] {
            // What is taken from a character (a body, its pockets) is not taken from anyone's place
            if (mHost.isActor(container))
                return;
            theftOf(container, static_cast<std::int64_t>(count) * mHost.itemValue(item));
        });
    }

    void LawGlue::theftOf(FormId reference, std::int64_t value)
    {
        if (reference == 0)
            return;
        // The reference's own owner, else the owner of the cell it is in
        FormId owner = mHost.mObjectState.owner(reference);
        if (owner == 0)
        {
            const FormId cell = mHost.cellOf(reference);
            if (cell != 0)
                owner = mHost.mObjectState.cellOwner(cell);
        }
        if (owner == 0 || FalloutScript::Law::mayTake(owner, mHost.playerBase(), factionsOf(mHost.mPlayer)))
            return;
        theft(owner, value);
    }

    void LawGlue::tamperedWith(FormId reference, bool karmaOnly)
    {
        mHost.runOnMainThread([this, reference, karmaOnly] {
            // The reference's own owner, else the owner of the cell it is in, as for a thing that is taken
            FormId owner = mHost.mObjectState.owner(reference);
            if (owner == 0)
                if (const FormId cell = mHost.cellOf(reference); cell != 0)
                    owner = mHost.mObjectState.cellOwner(cell);
            if (FalloutScript::Law::mayTake(owner, mHost.playerBase(), factionsOf(mHost.mPlayer)))
                return;
            theft(owner, 0, karmaOnly);
        });
    }

    bool LawGlue::inCellOwnedByOthers(FormId& cell, FormId& owner)
    {
        cell = mHost.cellOf(mHost.mPlayer);
        if (cell == 0)
            return false;
        owner = mHost.mObjectState.cellOwner(cell);
        if (owner == 0 || mHost.mObjectState.cellPublic(cell))
            return false;
        return !FalloutScript::Law::mayTake(owner, mHost.playerBase(), factionsOf(mHost.mPlayer));
    }

    void LawGlue::update(float frametime)
    {
        mLaw.advance(frametime);
        mSinceLook += frametime;
        if (mSinceLook < trespassLookInterval || mHost.mPlayer == 0)
            return;
        const float seconds = mSinceLook;
        mSinceLook = 0.f;
        OFBase::World* world = OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return;
        FormId cell = 0;
        FormId owner = 0;
        std::set<FormId> watching;
        if (inCellOwnedByOthers(cell, owner))
        {
            const ESM::FormId cellId = ESM::FormId::fromUint32(cell);
            const ESM4::Cell* record = mHost.mStore.get<ESM4::Cell>().search(cellId);
            const bool offLimits = record != nullptr && (record->mFlags & offLimitsFlag) != 0;
            const bool faction = mHost.mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(owner)) != nullptr;
            std::vector<FalloutScript::Witness> alarmed;
            for (FalloutScript::Witness& witness : gather(0))
            {
                if (!witness.mWatching || mHost.mCombat.inCombat(witness.mActor))
                    continue;
                // Only the owner, or someone of the faction that owns the place, minds
                const bool owns = faction
                    ? std::find(witness.mFactions.begin(), witness.mFactions.end(), owner) != witness.mFactions.end()
                    : mHost.ownerForm(witness.mActor) == owner;
                if (!owns)
                    continue;
                watching.insert(witness.mActor);
                switch (mLaw.trespass(witness.mActor, seconds, offLimits))
                {
                    case FalloutScript::Law::TrespassStep::Warn:
                        mLaw.setLineVictim(0);
                        mHost.sayTopic(witness.mActor, FalloutScript::Law::lineFor(
                                                           FalloutScript::CrimeKind::Trespass,
                                                           FalloutScript::CrimeLine::GuardTrespass),
                            mHost.mPlayer);
                        break;
                    case FalloutScript::Law::TrespassStep::Alarm:
                        alarmed.push_back(witness);
                        break;
                    default:
                        break;
                }
            }
            if (!alarmed.empty())
            {
                FalloutScript::Law::Alarm alarm;
                alarm.mResponders = std::move(alarmed);
                carryOut(mLaw.alarm(alarm), 0);
            }
        }
        // The ones that no longer have the player in a place that is not its own have their warnings forgotten. Every
        // entry of the law is checked, not only the ones seen earlier, so a warning that a save brought back for a
        // character that is gone, fighting or out of sight does not stay.
        for (const FormId gone : mLaw.trespassWatchers())
            if (watching.count(gone) == 0)
                mLaw.endTrespass(gone);
    }
}
