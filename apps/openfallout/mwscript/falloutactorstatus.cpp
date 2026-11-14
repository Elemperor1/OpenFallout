#include "falloutscripts.hpp"

#include <exception>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadrace.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    // What the actors are doing, and what scripts told them to do (components/falloutscript/actorstatus.hpp): the
    // rules are there, this is where the world is asked and told.

    bool FalloutScripts::actorAiOff(const OFWorld::Ptr& ptr)
    {
        if (ptr.isEmpty())
            return false;
        const FalloutScript::FormId reference = scriptReferenceOf(ptr);
        return reference != 0 && mActorStatus.aiOff(reference);
    }

    bool FalloutScripts::actorForcesRun(const OFWorld::Ptr& ptr)
    {
        if (ptr.isEmpty())
            return false;
        const FalloutScript::FormId reference = scriptReferenceOf(ptr);
        return reference != 0 && mActorStatus.overrideOf(reference).mForceRun;
    }

    bool FalloutScripts::actorForcesSneak(const OFWorld::Ptr& ptr)
    {
        if (ptr.isEmpty())
            return false;
        const FalloutScript::FormId reference = scriptReferenceOf(ptr);
        return reference != 0 && mActorStatus.overrideOf(reference).mForceSneak;
    }

    FalloutScript::FormId FalloutScripts::noteActivity(
        const OFWorld::Ptr& ptr, const std::function<void(FalloutScript::ActorActivity&)>& change)
    {
        if (ptr.isEmpty())
            return 0;
        const FalloutScript::FormId reference = scriptReferenceOf(ptr);
        if (reference != 0)
            mActorStatus.update(reference, change);
        return reference;
    }

    void FalloutScripts::applyPlayerStance()
    {
        const FalloutScript::ActorOverride state = mActorStatus.overrideOf(mPlayer);
        if (state.mForceSneak == mPlayerForcesSneak && state.mForceRun == mPlayerForcesRun)
            return;
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.isEmpty())
            return;
        // The stance of the player is read from its stats by the movement and the animation of its body: the flags that
        // the scripts of Morrowind force are the ones that carry SetForceRun and SetForceSneak
        try
        {
            OFMechanics::CreatureStats& stats = player.getClass().getCreatureStats(player);
            stats.setMovementFlag(OFMechanics::CreatureStats::Flag_ForceSneak, state.mForceSneak);
            stats.setMovementFlag(OFMechanics::CreatureStats::Flag_ForceRun, state.mForceRun);
        }
        catch (const std::exception& error)
        {
            static bool said = false;
            if (!std::exchange(said, true))
                Log(Debug::Warning) << "The stance of the player cannot be forced: " << error.what();
            return;
        }
        mPlayerForcesSneak = state.mForceSneak;
        mPlayerForcesRun = state.mForceRun;
        Log(Debug::Info) << "The player is forced to " << (state.mForceSneak ? "sneak" : "not sneak") << " and to "
                         << (state.mForceRun ? "run" : "not run");
    }

    int FalloutScripts::teammateCount()
    {
        return static_cast<int>(mObjectState.teammates().size());
    }

    bool FalloutScripts::isTeammate(FalloutScript::FormId actor)
    {
        return mObjectState.teammate(actor);
    }

    int FalloutScripts::furnitureMarker(FalloutScript::FormId actor)
    {
        // The kind of the entry of the marker that it uses (its place is not the executable's marker number, which is
        // the first field of the marker's record: see the notes), 0 when it uses none
        const std::lock_guard lock(mFurnitureMutex);
        const auto found = mFurniture.find(actor);
        return found != mFurniture.end() ? found->second.mKind : 0;
    }

    FalloutScript::FormId FalloutScripts::ownerOf(FalloutScript::FormId reference)
    {
        // An item is owned by the one that holds it (the holder that its script found last)
        const auto found = mItemHolders.find(reference);
        return found != mItemHolders.end() ? found->second : reference;
    }

    bool FalloutScripts::fatigue(FalloutScript::FormId actor, double& current, double& permanent)
    {
        namespace AV = FalloutScript::ActorValue;
        current = mActorValues.current(actor, AV::fatigue);
        permanent = mActorValues.permanent(actor, AV::fatigue);
        return true;
    }

    int FalloutScripts::actorsInHigh()
    {
        return static_cast<int>(mActorStatus.activeCount());
    }

    void FalloutScripts::disableAllActors(FalloutScript::FormId except)
    {
        // Every actor that is looked after, but the one the command is called on, the player and those that are
        // disabled already
        for (const FalloutScript::FormId actor : mActorStatus.active())
            if (actor != mPlayer && actor != except && !isDisabled(actor))
                setDisabled(actor, true);
    }

    bool FalloutScripts::isPlayableRace(FalloutScript::FormId race)
    {
        // The first flag of the race's record
        const ESM4::Race* record = mStore.get<ESM4::Race>().search(ESM::FormId::fromUint32(race));
        return record != nullptr && (record->mRaceFlags & 0x1) != 0;
    }

    FalloutScript::FormId FalloutScripts::raceOf(FalloutScript::FormId actor)
    {
        FalloutScript::ActorTraits traits;
        return traitsOf(actor, traits) && !traits.mCreature ? traits.mRace : 0;
    }
}
