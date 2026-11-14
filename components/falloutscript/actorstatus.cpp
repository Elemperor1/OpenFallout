#include "actorstatus.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        /// The number of the procedure of each stage of a package, by the type of the package (FalloutNV.exe 0x11A3FF0 is
        /// the table of lists, the numbers are what the switch of 0x5A1690 makes of each entry; Fallout3.exe 0xF6EE80
        /// has the same lists and the same numbers). Where the executable's list holds an entry that the switch has no
        /// number for the number is -1; the stage after the last is the end of the package (17).
        constexpr int maxStages = 4;
        constexpr int packageTypes = 48;
        struct StageList
        {
            int mNumbers[maxStages];
            int mCount;
        };
        constexpr StageList stageLists[packageTypes] = {
            /*  0 */ { { 0 }, 1 },
            /*  1 */ { { 0, 7 }, 2 },
            /*  2 */ { { 0, 1 }, 2 },
            /*  3 */ { { 0, 2 }, 2 },
            /*  4 */ { { 0, 8, 7 }, 3 },
            /*  5 */ { { 0, 10, 7 }, 3 },
            /*  6 */ { { 3, 11 }, 2 },
            /*  7 */ { { 0, 3, 11 }, 3 },
            /*  8 */ { { 20, 19, 3, 12 }, 4 },
            /*  9 */ { { 2, 12 }, 2 },
            /* 10 */ { { 0, 43, 3, 4 }, 4 },
            /* 11 */ { { 14, 15, 34, 47 }, 4 },
            /* 12 */ { { 13 }, 1 },
            /* 13 */ { { 19, 1 }, 2 },
            /* 14 */ { { 5 }, 1 },
            /* 15 */ { { 9 }, 1 },
            /* 16 */ { { -1 }, 1 },
            /* 17 */ { { 0, 6 }, 2 },
            /* 18 */ { { 15, 13 }, 2 },
            /* 19 */ { { 16 }, 1 },
            /* 20 */ { { 15, -1 }, 2 },
            /* 21 */ { { 21 }, 1 },
            /* 22 */ { { 0, 1, 3 }, 3 },
            /* 23 */ { { 0, 22 }, 2 },
            /* 24 */ { { 23 }, 1 },
            /* 25 */ { { 24 }, 1 },
            /* 26 */ { { 0, 2, 7 }, 3 },
            /* 27 */ { { 19, 27 }, 2 },
            /* 28 */ { { 2, 0, 28 }, 3 },
            /* 29 */ { { 29 }, 1 },
            /* 30 */ { { 0, 30 }, 2 },
            /* 31 */ { { 31 }, 1 },
            /* 32 */ { { 34 }, 1 },
            /* 33 */ { { -1, 36 }, 2 },
            /* 34 */ { { 37 }, 1 },
            /* 35 */ { { 38 }, 1 },
            /* 36 */ { { 39 }, 1 },
            /* 37 */ { { 0, 45 }, 2 },
            /* 38 */ { { 41 }, 1 },
            /* 39 */ { { 0, -1 }, 2 },
            /* 40 */ { { -1, 16 }, 2 },
            /* 41 */ { { 44 }, 1 },
            /* 42 */ { { -1, 47 }, 2 },
            /* 43 */ { { -1, 47 }, 2 },
            /* 44 */ { { 2, 0, 42 }, 3 },
            /* 45 */ { { 11 }, 1 },
            /* 46 */ { { 49 }, 1 },
            /* 47 */ { { 50 }, 1 },
        };

        /// The package flag that IsWaiting also takes for waiting (FalloutNV.exe 0x8A6290 tests it)
        constexpr std::uint32_t waitingPackageFlag = 0x10000;
        /// The number of the procedure that is waiting (the entry 13 of the lists: IsWaiting, 0x8A6210)
        constexpr int waitingProcedure = 3;

        std::vector<FormId> sorted(std::set<FormId> forms)
        {
            return std::vector<FormId>(forms.begin(), forms.end());
        }
    }

    int movingAnswer(unsigned flags)
    {
        const unsigned directions = flags & 0xf;
        if ((directions & Movement::forward) != 0)
            return 1;
        if ((directions & Movement::backward) != 0)
            return 2;
        if ((directions & Movement::left) != 0)
            return 3;
        if ((directions & Movement::right) != 0)
            return 4;
        return 0;
    }

    int turningAnswer(unsigned flags)
    {
        if ((flags & Movement::turnLeft) != 0)
            return 1;
        if ((flags & Movement::turnRight) != 0)
            return 2;
        return 0;
    }

    bool runningFlag(unsigned flags)
    {
        return (flags & Movement::run) != 0;
    }

    bool spineYAboveGround(float value)
    {
        return value > 0.f;
    }

    int procedureStageCount(int packageType)
    {
        return packageType >= 0 && packageType < packageTypes ? stageLists[packageType].mCount : 0;
    }

    int procedureNumber(int packageType, int stage)
    {
        if (packageType < 0 || packageType >= packageTypes)
            return Procedure::unknown;
        const StageList& list = stageLists[packageType];
        if (stage >= list.mCount)
            return Procedure::done;
        return list.mNumbers[static_cast<std::size_t>(std::max(stage, 0))];
    }

    void ActorStatus::clear()
    {
        const std::lock_guard lock(mMutex);
        mOverrides.clear();
        mActivity.clear();
        mCannibal.clear();
        mSandman.clear();
        mNoAvoidance = false;
    }

    ActorOverride ActorStatus::overrideOf(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mOverrides.find(actor);
        return found == mOverrides.end() ? ActorOverride() : found->second;
    }

    void ActorStatus::setOverride(FormId actor, const ActorOverride& state)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        if (state.empty())
            mOverrides.erase(actor);
        else
            mOverrides[actor] = state;
    }

    void ActorStatus::setAiOff(FormId actor, bool off)
    {
        ActorOverride state = overrideOf(actor);
        state.mAiOff = off;
        setOverride(actor, state);
    }

    void ActorStatus::setForceRun(FormId actor, bool on)
    {
        ActorOverride state = overrideOf(actor);
        state.mForceRun = on;
        setOverride(actor, state);
    }

    void ActorStatus::setForceSneak(FormId actor, bool on)
    {
        ActorOverride state = overrideOf(actor);
        state.mForceSneak = on;
        setOverride(actor, state);
    }

    std::vector<std::pair<FormId, ActorOverride>> ActorStatus::overrides() const
    {
        const std::lock_guard lock(mMutex);
        return std::vector<std::pair<FormId, ActorOverride>>(mOverrides.begin(), mOverrides.end());
    }

    void ActorStatus::setNoAvoidance(bool on)
    {
        const std::lock_guard lock(mMutex);
        mNoAvoidance = on;
    }

    bool ActorStatus::noAvoidance() const
    {
        const std::lock_guard lock(mMutex);
        return mNoAvoidance;
    }

    void ActorStatus::setFeeding(Feeding kind, FormId actor, bool on)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        std::set<FormId>& who = kind == Feeding::Cannibal ? mCannibal : mSandman;
        if (on)
            who.insert(actor);
        else
            who.erase(actor);
    }

    bool ActorStatus::feeding(Feeding kind, FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return (kind == Feeding::Cannibal ? mCannibal : mSandman).count(actor) != 0;
    }

    void ActorStatus::publish(FormId actor, const ActorActivity& activity)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        mActivity[actor] = activity;
    }

    void ActorStatus::update(FormId actor, const std::function<void(ActorActivity&)>& change)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        change(mActivity[actor]);
    }

    std::optional<ActorActivity> ActorStatus::activity(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mActivity.find(actor);
        if (found == mActivity.end())
            return std::nullopt;
        return found->second;
    }

    void ActorStatus::forget(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        mActivity.erase(actor);
    }

    void ActorStatus::forgetBody(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mActivity.find(actor);
        if (found == mActivity.end())
            return;
        const ActorActivity kept;
        ActorActivity& activity = found->second;
        activity.mMovement = kept.mMovement;
        activity.mSwimming = kept.mSwimming;
        activity.mAnimAction = kept.mAnimAction;
        activity.mIdlePlaying = kept.mIdlePlaying;
        activity.mIdleDoneOnce = kept.mIdleDoneOnce;
        activity.mLastIdle = kept.mLastIdle;
        activity.mWalkSpeed = kept.mWalkSpeed;
        if (activity == kept)
            mActivity.erase(found);
    }

    void ActorStatus::forgetMind(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mActivity.find(actor);
        if (found == mActivity.end())
            return;
        const ActorActivity kept;
        ActorActivity& activity = found->second;
        activity.mStage = kept.mStage;
        activity.mPackageType = kept.mPackageType;
        activity.mPackageFlags = kept.mPackageFlags;
        activity.mPackageTarget = kept.mPackageTarget;
        if (activity == kept)
            mActivity.erase(found);
    }

    std::vector<FormId> ActorStatus::active() const
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> actors;
        actors.reserve(mActivity.size());
        for (const auto& [actor, activity] : mActivity)
            actors.push_back(actor);
        return actors;
    }

    std::size_t ActorStatus::activeCount() const
    {
        const std::lock_guard lock(mMutex);
        return mActivity.size();
    }

    void writeActorOverrides(const ActorStatus& status, SaveWriter& out)
    {
        const std::vector<std::pair<FormId, ActorOverride>> all = status.overrides();
        out.count(all.size());
        for (const auto& [actor, state] : all)
        {
            out.form(actor);
            out.boolean(state.mAiOff);
            out.boolean(state.mForceRun);
            out.boolean(state.mForceSneak);
        }
    }

    void readActorOverrides(ActorStatus& status, SaveReader& in, std::uint32_t)
    {
        const std::size_t count = in.count(7);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::optional<FormId> actor = in.form();
            ActorOverride state;
            state.mAiOff = in.boolean();
            state.mForceRun = in.boolean();
            state.mForceSneak = in.boolean();
            if (actor)
                status.setOverride(*actor, state);
        }
    }

    CombatGroup combatGroupOf(FormId actor, const std::vector<std::pair<FormId, FormId>>& fights,
        const std::function<bool(FormId)>& party)
    {
        CombatGroup group;
        FormId target = 0;
        for (const auto& [attacker, fought] : fights)
            if (attacker == actor)
                target = fought;
        if (target == 0)
            return group;
        // The targets of the group: the one the actor fights, or all that are fought on the side of the player when
        // that is where it is
        std::set<FormId> targets{ target };
        if (party && party(target))
            for (const auto& [attacker, fought] : fights)
                if (party(fought))
                    targets.insert(fought);
        std::set<FormId> members;
        for (const auto& [attacker, fought] : fights)
            if (targets.count(fought) != 0 && targets.count(attacker) == 0)
                members.insert(attacker);
        // An actor that is its own group's target and fights on (two that fight each other) is in the group of the one
        // it fights
        members.insert(actor);
        group.mMembers = sorted(std::move(members));
        group.mTargets = sorted(std::move(targets));
        return group;
    }

    namespace
    {
        /// The actor that a call is about, 0 when there is none or the host does not know an actor of that reference
        FormId actorOf(const CallContext& call, ActorStatusWorld& world)
        {
            return call.mReference != 0 && world.isActor(call.mReference) ? call.mReference : 0;
        }
    }

    void addActorStatusCommands(Interpreter& interpreter, ActorStatus& status, CombatState& combat,
        PlayerProgress& progress, ActorValues& values, ActorStatusWorld& world, Game game)
    {
        const auto activityOf = [&status, &world](const CallContext& call) -> std::optional<ActorActivity> {
            const FormId actor = actorOf(call, world);
            return actor != 0 ? status.activity(actor) : std::nullopt;
        };

        // The movement word (FalloutNV.exe 0x5A40D0, 0x5A4160, 0x5A3010): which way, whether it turns, whether it runs
        interpreter.setHandler("IsMoving", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity ? static_cast<double>(movingAnswer(activity->mMovement)) : 0.0;
        });
        interpreter.setHandler("IsTurning", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity ? static_cast<double>(turningAnswer(activity->mMovement)) : 0.0;
        });
        interpreter.setHandler("IsRunning", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity && runningFlag(activity->mMovement) ? 1.0 : 0.0;
        });
        // The flag at +0x14D of the actor (0x5A2030)
        interpreter.setHandler("IsSwimming", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity && activity->mSwimming ? 1.0 : 0.0;
        });
        // A package is followed and it is at the procedure of waiting or says it waits (0x8A6210)
        interpreter.setHandler("IsWaiting", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            if (!activity || activity->mPackageType < 0)
                return 0.0;
            return (activity->mPackageFlags & waitingPackageFlag) != 0
                    || procedureNumber(activity->mPackageType, activity->mStage) == waitingProcedure
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler("IsFacingUp", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity && spineYAboveGround(activity->mSpineYUp) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsLeftUp", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity && spineYAboveGround(activity->mSpineZUp) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetWalkSpeed", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity ? static_cast<double>(activity->mWalkSpeed) : 0.0;
        });

        // The action of the animation: -1 for an actor that has none, and for what is not an actor (0x8A7570)
        interpreter.setHandler("GetAnimAction", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity ? static_cast<double>(activity->mAnimAction) : -1.0;
        });
        // The idle that scripts and packages have it play (the object at +0x1B8 of the process, FalloutNV.exe 0x5A0F10):
        // one is playing and has not ended
        interpreter.setHandler("IsIdlePlaying", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity && activity->mIdlePlaying ? 1.0 : 0.0;
        });
        // The flag of the process that is set when an idle has played to its end once (0x5A3570)
        interpreter.setHandler("GetIdleDoneOnce", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity && activity->mIdleDoneOnce ? 1.0 : 0.0;
        });
        // The form of the idle that played last is the one the script names (0x5A4780)
        interpreter.setHandler("IsLastIdlePlayed", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            const FormId idle = formArgument(call, 0);
            return activity && idle != 0 && activity->mLastIdle == idle ? 1.0 : 0.0;
        });

        // The procedure the actor's package is at: 0 for an actor that follows none (0x5A1210)
        interpreter.setHandler("GetCurrentAIProcedure", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            if (!activity || activity->mPackageType < 0)
                return 0.0;
            return static_cast<double>(procedureNumber(activity->mPackageType, activity->mStage));
        });
        // The target of the package, as a reference (0x5CE520)
        interpreter.setHandler("GetPackageTarget", [activityOf](CallContext& call) {
            const auto activity = activityOf(call);
            return activity ? static_cast<double>(activity->mPackageTarget) : 0.0;
        });

        // The processing of the actor (the byte at +0xBC of the actor: FalloutNV.exe 0x5D6A60 sets it, 0x5BBE10 turns it
        // over, 0x5BBEC0 asks whether it is off)
        interpreter.setHandler("SetActorsAI", [&status, &world](CallContext& call) {
            if (const FormId actor = actorOf(call, world); actor != 0)
                status.setAiOff(actor, intArgument(call, 0) == 0);
            return 0.0;
        });
        interpreter.setHandler("ToggleActorsAI", [&status, &world](CallContext& call) {
            if (const FormId actor = actorOf(call, world); actor != 0)
                status.setAiOff(actor, !status.aiOff(actor));
            return 0.0;
        });
        interpreter.setHandler("IsActorsAIOff", [&status, &world](CallContext& call) {
            const FormId actor = actorOf(call, world);
            return actor != 0 && status.aiOff(actor) ? 1.0 : 0.0;
        });
        // Every actor that the high process list holds, but the one the command is called on, is disabled (0x5C47E0)
        interpreter.setHandler("DisableAllActors", [&world](CallContext& call) {
            world.disableAllActors(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("GetActorsInHigh",
            [&world](CallContext&) { return static_cast<double>(std::max(world.actorsInHigh(), 0)); });

        // The two flags that make the actor run or sneak whatever its AI would have it do (the bytes at +0x124 and
        // +0x125 of the actor, FalloutNV.exe 0x5CE7B0 and 0x5CE910; the getters read them back)
        interpreter.setHandler("SetForceRun", [&status, &world](CallContext& call) {
            if (const FormId actor = actorOf(call, world); actor != 0)
                status.setForceRun(actor, intArgument(call, 0) != 0);
            return 0.0;
        });
        interpreter.setHandler("GetForceRun", [&status, &world](CallContext& call) {
            const FormId actor = actorOf(call, world);
            return actor != 0 && status.overrideOf(actor).mForceRun ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetForceSneak", [&status, &world](CallContext& call) {
            if (const FormId actor = actorOf(call, world); actor != 0)
                status.setForceSneak(actor, intArgument(call, 0) != 0);
            return 0.0;
        });
        interpreter.setHandler("GetForceSneak", [&status, &world](CallContext& call) {
            const FormId actor = actorOf(call, world);
            return actor != 0 && status.overrideOf(actor).mForceSneak ? 1.0 : 0.0;
        });
        // One global byte of the program (FalloutNV.exe 0x5BC200): path avoidance
        interpreter.setHandler("SetNoAvoidance", [&status](CallContext& call) {
            status.setNoAvoidance(intArgument(call, 0) != 0);
            return 0.0;
        });
        // The command parses its argument and changes nothing in the executables (0x5BC150)
        interpreter.setHandler("SetAllReachable", [](CallContext&) { return 0.0; });

        // The combat group (0x5A4240 and 0x5A42B0): how many actors are in the group of the one the command is called
        // on, and how many it has for targets; 0 for one that is in no fight
        const auto fightsAndParty = [&combat, &world](const CallContext& call) {
            const FormId actor = actorOf(call, world);
            if (actor == 0)
                return CombatGroup();
            const FormId player = call.mHost.player();
            const std::function<bool(FormId)> party = [&world, player](FormId form) {
                return form == player || world.isTeammate(form);
            };
            return combatGroupOf(actor, combat.fights(), party);
        };
        interpreter.setHandler("GetGroupMemberCount", [fightsAndParty](CallContext& call) {
            return static_cast<double>(fightsAndParty(call).mMembers.size());
        });
        interpreter.setHandler("GetGroupTargetCount", [fightsAndParty](CallContext& call) {
            return static_cast<double>(fightsAndParty(call).mTargets.size());
        });
        interpreter.setHandler("GetPlayerTeammateCount",
            [&world](CallContext&) { return static_cast<double>(std::max(world.teammateCount(), 0)); });
        // The last target of the actor that owns the reference (FalloutNV.exe 0x5DEAA0); only New Vegas has the command
        if (game == Game::NewVegas)
            interpreter.setHandler("GetOwnerLastTarget", [&combat, &world](CallContext& call) {
                if (call.mReference == 0)
                    return 0.0;
                const FormId owner = world.ownerOf(call.mReference);
                return owner != 0 && world.isActor(owner) ? static_cast<double>(combat.lastTarget(owner)) : 0.0;
            });

        // What the executable answers whatever the state: an actor is what its form says it is (0x5A39D0)
        interpreter.setHandler("IsActor", [&world](CallContext& call) {
            return call.mReference != 0 && world.isActor(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetFurnitureMarkerID", [&world](CallContext& call) {
            return call.mReference != 0 && world.isActor(call.mReference)
                ? static_cast<double>(world.furnitureMarker(call.mReference))
                : 0.0;
        });
        // The fatigue of the actor, its current value over its permanent value (the value 22 of the actor values: 0x893530,
        // which gives 1 for an actor whose permanent value is 0)
        interpreter.setHandler("GetFatiguePercentage", [&world](CallContext& call) {
            double current = 0;
            double permanent = 0;
            if (call.mReference == 0 || !world.isActor(call.mReference)
                || !world.fatigue(call.mReference, current, permanent))
                return 0.0;
            return permanent != 0 ? current / permanent : 1.0;
        });

        // Feeding on a corpse, and putting someone to sleep: the command is called on the one that is fed on and names the
        // actor that does it (the player, in every plugin line), which GetCannibal and GetSandman tell for the player
        // (FalloutNV.exe 0x5CFEF0, 0x5CFDF0, 0x5A2890, 0x5A2810)
        const auto startFeeding = [&status, &world](ActorStatus::Feeding kind) {
            return [&status, &world, kind](CallContext& call) {
                const FormId feeder = formArgument(call, 0);
                if (call.mReference != 0 && world.isActor(call.mReference) && feeder != 0 && world.isActor(feeder))
                    status.setFeeding(kind, feeder, true);
                return 0.0;
            };
        };
        interpreter.setHandler("StartCannibal", startFeeding(ActorStatus::Feeding::Cannibal));
        interpreter.setHandler("StartMisterSandMan", startFeeding(ActorStatus::Feeding::Sandman));
        interpreter.setHandler("GetCannibal", [&status](CallContext& call) {
            return call.mReference == call.mHost.player() && status.feeding(ActorStatus::Feeding::Cannibal, call.mReference)
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler("GetSandman", [&status](CallContext& call) {
            return call.mReference == call.mHost.player() && status.feeding(ActorStatus::Feeding::Sandman, call.mReference)
                ? 1.0
                : 0.0;
        });

        // The player: AdvancePCLevel gives it the next level (FalloutNV.exe 0x5CE9F0 adds one to the level number of the
        // player); ModPCAttribute changes one of its SPECIAL values, and only those (the check of 0x5D1260: the numbers 5
        // to 11), by a whole number (0x5D11B0)
        interpreter.setHandler("AdvancePCLevel", [&progress](CallContext&) {
            progress.setLevel(progress.level() + 1);
            return 0.0;
        });
        interpreter.setHandler("ModPCAttribute", [&values](CallContext& call) {
            const int value = intArgument(call, 0);
            if (value >= ActorValue::strength && value <= ActorValue::luck)
                values.addBase(call.mHost.player(), value, intArgument(call, 1), 0.0);
            return 0.0;
        });
        // The race of the character is one that the player can be (the flag of its record, 0x59F610, for a character: a
        // creature has no race that counts)
        interpreter.setHandler("GetIsPlayableRace", [&world](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const FormId race = world.raceOf(call.mReference);
            return race != 0 && world.isPlayableRace(race) ? 1.0 : 0.0;
        });

        // The commands whose answer is a constant in the executables: there are no horses, no torches or shields to carry,
        // and the retail executables do not keep a disease or a default class for an actor (the evaluators 0x5A2A50 and
        // 0x59D250 only clear the result, 0x5A3670 and 0x5A2C30 answer 0 after the check of the form); the two skill
        // commands of the player are the empty command (0x5D4A40)
        interpreter.setHandler("IsRidingHorse", [](CallContext&) { return 0.0; });
        interpreter.setHandler("GetPlayerHasLastRiddenHorse", [](CallContext&) { return 0.0; });
        interpreter.setHandler("IsPlayersLastRiddenHorse", [](CallContext&) { return 0.0; });
        interpreter.setHandler("IsHorseStolen", [](CallContext&) { return 0.0; });
        interpreter.setHandler("IsActorUsingATorch", [](CallContext&) { return 0.0; });
        interpreter.setHandler("IsTorchOut", [](CallContext&) { return 0.0; });
        interpreter.setHandler("IsShieldOut", [](CallContext&) { return 0.0; });
        interpreter.setHandler("GetDisease", [](CallContext&) { return 0.0; });
        interpreter.setHandler("GetIsClassDefault", [](CallContext&) { return 0.0; });
        interpreter.setHandler("ModPCSkill", [](CallContext&) { return 0.0; });
        interpreter.setHandler("AdvancePCSkill", [](CallContext&) { return 0.0; });
    }
}
