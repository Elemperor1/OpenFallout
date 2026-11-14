#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORSTATUS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORSTATUS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "actorvalues.hpp"
#include "combatstate.hpp"
#include "interpreter.hpp"
#include "playerprogress.hpp"
#include "savestate.hpp"
#include "vanillacommands.hpp"

namespace FalloutScript
{
    /// The bits of the movement word of an actor: the word that the movement controller of the executable keeps
    /// (the controller at +0x190 of the actor, its slot 8, FalloutNV.exe 0x8846E0) and that IsMoving, IsTurning and
    /// IsRunning read.
    namespace Movement
    {
        constexpr unsigned forward = 0x1;
        constexpr unsigned backward = 0x2;
        constexpr unsigned left = 0x4;
        constexpr unsigned right = 0x8;
        constexpr unsigned turnLeft = 0x10;
        constexpr unsigned turnRight = 0x20;
        constexpr unsigned walk = 0x100;
        constexpr unsigned run = 0x200;
        constexpr unsigned sneak = 0x400;
    }

    /// IsMoving (FalloutNV.exe 0x5A40D0): which way the actor goes, the first of these that is set. 0 for none, 1
    /// ahead, 2 back, 3 to the left, 4 to the right.
    int movingAnswer(unsigned flags);
    /// IsTurning (0x5A4160): 1 turning left, 2 turning right (left first), 0 for none
    int turningAnswer(unsigned flags);
    /// IsRunning (0x884730): the run bit of the word
    bool runningFlag(unsigned flags);

    /// What GetAnimAction answers, the action the actor's animation is in (the word at +0x2EC of the high process,
    /// FalloutNV.exe 0x8A7570 through the slot 0x3E4 of the process): -1 when it is in none. The numbers are the
    /// executable's own; the engine sets the ones it can tell (the names are those of the games' editor for the same
    /// numbers).
    namespace AnimAction
    {
        constexpr int none = -1;
        constexpr int equip = 0;
        constexpr int unequip = 1;
        constexpr int attack = 2;
        constexpr int block = 7;
        constexpr int recoil = 8;
        constexpr int reload = 9;
        constexpr int stagger = 10;
        constexpr int dodge = 11;
        constexpr int waitLower = 12;
        constexpr int specialIdle = 13;
        constexpr int forcedByScript = 14;
    }

    /// GetCurrentAIProcedure (FalloutNV.exe 0x5A1210, FalloutNV.exe 0x5A1690 for the table of numbers; Fallout3.exe has
    /// the same arrays and the same numbers): the process of the actor keeps the stage it is at in the package it
    /// follows (its slot 0x280), the executable has for each type of package the list of the procedures that its stages
    /// are, and the number that is told is that of the procedure. A stage that is past the list is the end of the
    /// package, 17.
    namespace Procedure
    {
        constexpr int none = 0;
        constexpr int done = 17;
        /// The answer for a stage that has no number in the executable's switch, and for a package whose type is
        /// not in the table
        constexpr int unknown = -1;
    }
    /// How many stages a package of the type has before its end (0 for a type that is not in the table)
    int procedureStageCount(int packageType);

    /// IsFacingUp (FalloutNV.exe 0x5A0710, Fallout3.exe 0x514DD0) and IsLeftUp (0x5A0800, 0x514E80): the spine bone
    /// ("Bip01 Spine", the node whose name the executable keeps at 0x11C61B4; the first is looked for by the name of
    /// the process's own bone list, then "Bip01 Spine01") has the entry of its world rotation at +0x84 (row 2, column 1)
    /// above 0 for IsFacingUp, and the one at +0x88 (row 2, column 2) above 0 for IsLeftUp. 0 when there is no bone.
    bool spineYAboveGround(float value);

    /// The number of the procedure that the stage of a package of the type is: the stage that is at or past the count is
    /// the end of the package (17); -1 for a type that has no table or a stage that has no number. A negative stage is the
    /// first.
    int procedureNumber(int packageType, int stage);

    /// What a character is doing now, as the scripts and the conditions ask about it: told by the code that moves it
    /// and animates it, a frame at a time. All of it is of the moment: none of it goes into a saved game.
    struct ActorActivity
    {
        /// The bits of Movement
        unsigned mMovement = 0;
        bool mSwimming = false;
        /// One of AnimAction
        int mAnimAction = AnimAction::none;
        /// The stage of the package it follows (see Procedure)
        int mStage = 0;
        /// The type of the package it follows, -1 for none, and the flags of that package
        int mPackageType = -1;
        std::uint32_t mPackageFlags = 0;
        /// The reference that the package names as its target (GetPackageTarget), 0 when it names none or an object kind
        FormId mPackageTarget = 0;
        /// An idle that a script had it play is playing, and one of those has played to its end since the last began
        bool mIdlePlaying = false;
        bool mIdleDoneOnce = false;
        /// The last idle record that played on it (0 for none)
        FormId mLastIdle = 0;
        /// How fast the animation of its walk carries it, units a second (0 when it has none)
        float mWalkSpeed = 0.f;
        /// What the bone of the spine ("Bip01 Spine") says about how the character lies: the entries [2][1] and [2][2]
        /// of its world rotation, the up part of its Y and of its Z axis (IsFacingUp and IsLeftUp ask whether they are
        /// above 0). A character that has no such bone has 0 for both.
        float mSpineYUp = 0.f;
        float mSpineZUp = 0.f;

        bool operator==(const ActorActivity&) const = default;
    };

    /// What scripts told one actor to do over what its packages and the world would make it do: the overrides that
    /// go into a saved game.
    struct ActorOverride
    {
        /// SetActorsAI 0: the actor does nothing (FalloutNV.exe 0x5D6A60 clears the byte at +0xBC of the actor, the
        /// flag that its processing is on)
        bool mAiOff = false;
        /// SetForceRun (+0x124 of the actor) and SetForceSneak (+0x125)
        bool mForceRun = false;
        bool mForceSneak = false;

        bool empty() const { return !mAiOff && !mForceRun && !mForceSneak; }
        bool operator==(const ActorOverride&) const = default;
    };

    /// What the actors are doing and what scripts told them to do. It can be used from any thread.
    class ActorStatus
    {
    public:
        /// Forgets everything, as for a new game
        void clear();

        /// The overrides of an actor (none: all off). Setting the last override off forgets the actor.
        ActorOverride overrideOf(FormId actor) const;
        void setOverride(FormId actor, const ActorOverride& state);
        void setAiOff(FormId actor, bool off);
        void setForceRun(FormId actor, bool on);
        void setForceSneak(FormId actor, bool on);
        bool aiOff(FormId actor) const { return overrideOf(actor).mAiOff; }
        /// Every actor that has an override, in the order of the forms (for a saved game)
        std::vector<std::pair<FormId, ActorOverride>> overrides() const;

        /// SetNoAvoidance (FalloutNV.exe 0x5BC200 writes one global byte): the actors do not step aside for anything
        /// (path avoidance is off). A global of the program, not kept in a saved game.
        void setNoAvoidance(bool on);
        bool noAvoidance() const;

        /// StartCannibal and StartMisterSandMan (FalloutNV.exe 0x5CFEF0 and 0x5CFDF0): the actor that the command names
        /// is told that it feeds on, or puts to sleep, the one the command is called on, which GetCannibal and
        /// GetSandman (0x5A2890 and 0x5A2810: only for the player) read back. Of the moment, not in a saved game.
        enum class Feeding : std::uint8_t
        {
            Cannibal,
            Sandman,
        };
        void setFeeding(Feeding kind, FormId actor, bool on);
        bool feeding(Feeding kind, FormId actor) const;

        /// What an actor does now; the last thing told stays until it is told again or forgotten. The movement and the
        /// animation are told by the animation of the actor, the package by its mind: each changes its own fields.
        void publish(FormId actor, const ActorActivity& activity);
        void update(FormId actor, const std::function<void(ActorActivity&)>& change);
        std::optional<ActorActivity> activity(FormId actor) const;
        /// The actor is not looked after (its cell was unloaded, it was removed)
        void forget(FormId actor);
        /// The animation of the actor is gone (it is made again, or the actor is out of the scene): what the animation
        /// told is taken back and what the mind told stays. The actor is forgotten when nothing is left.
        void forgetBody(FormId actor);
        /// The mind of the actor is gone: what the mind told (the package) is taken back and what the animation told stays
        void forgetMind(FormId actor);
        /// Every actor that has an activity (in the order of the forms)
        std::vector<FormId> active() const;
        std::size_t activeCount() const;

    private:
        /// Sets one flag of the overrides of the actor, holding the lock for all of it
        void editOverride(FormId actor, bool ActorOverride::*flag, bool on);

        mutable std::mutex mMutex;
        std::map<FormId, ActorOverride> mOverrides;
        std::map<FormId, ActorActivity> mActivity;
        std::set<FormId> mCannibal;
        std::set<FormId> mSandman;
        bool mNoAvoidance = false;
    };

    /// The section of a saved game that holds the overrides
    constexpr std::uint32_t actorOverridesSectionVersion = 1;
    void writeActorOverrides(const ActorStatus& status, SaveWriter& out);
    /// An actor whose plugin is no longer loaded is left out
    void readActorOverrides(ActorStatus& status, SaveReader& in, std::uint32_t version);

    /// The combat group of an actor, the cooperating set of actors fighting together that GetGroupMemberCount and
    /// GetGroupTargetCount count (the executable keeps it as a group object that has a list of members at +0x18 and a
    /// list of targets at +0x8, FalloutNV.exe 0x5A4240 and 0x5A42B0; an actor that is a target is not a member of the
    /// group that it is the target of). An actor that is in no fight has no group (both counts 0).
    struct CombatGroup
    {
        std::vector<FormId> mMembers;
        std::vector<FormId> mTargets;
    };
    /// Which actors form the group of the actor, by the fights as they stand (`fights`: the actor and what it fights).
    /// Chosen, not read: the executable's rule of who joins a group is in the combat manager (0x990F20) and was not
    /// taken apart. Here the group is everyone who fights the target of the actor; when that target is on the side of
    /// the player (the player and its teammates, `party`), everyone who fights any of them, and the targets are those
    /// that are fought.
    CombatGroup combatGroupOf(FormId actor, const std::vector<std::pair<FormId, FormId>>& fights,
        const std::function<bool(FormId)>& party);

    /// What the commands about actors ask of the game world that is not in the rules above
    class ActorStatusWorld
    {
    public:
        virtual ~ActorStatusWorld() = default;

        /// Whether the reference is a character, a creature or the player (IsActor: the slot 0x100 of the form)
        virtual bool isActor(FormId reference) = 0;
        /// The player's teammates (the counter at +0xD68 of the player)
        virtual int teammateCount() = 0;
        /// Whether the actor is one of the player's teammates
        virtual bool isTeammate(FormId actor) = 0;
        /// The actor's furniture marker (GetFurnitureMarkerID: slot 0x4C4 of the process), 0 when it is on none
        virtual int furnitureMarker(FormId /*actor*/) { return 0; }
        /// The reference whose last target GetOwnerLastTarget tells: the actor itself, and for an item the actor that
        /// holds it
        virtual FormId ownerOf(FormId reference) { return reference; }

        /// The current and the permanent value of the actor's fatigue (ActorValue 22), for GetFatiguePercentage
        virtual bool fatigue(FormId /*actor*/, double& /*current*/, double& /*permanent*/) { return false; }
        /// The number of actors that are looked after now (the high process list of the executable, count at +0x103B0 of
        /// the process lists)
        virtual int actorsInHigh() { return 0; }
        /// DisableAllActors (FalloutNV.exe 0x5C47E0): every actor of the high process list that is not the actor the
        /// command is called on and is not disabled already is disabled
        virtual void disableAllActors(FormId /*except*/) {}
        /// Whether the race can be played (the flag of its record)
        virtual bool isPlayableRace(FormId /*race*/) { return false; }
        /// The race that the record of a character names (0 for a creature or what is not known)
        virtual FormId raceOf(FormId /*actor*/) { return 0; }
    };

    /// Gives the commands about what actors do and what scripts steer them to do to the interpreter: the movement
    /// queries (IsMoving, IsTurning, IsRunning, IsSwimming, IsWaiting, GetWalkSpeed), the animation (GetAnimAction,
    /// IsIdlePlaying, GetIdleDoneOnce, IsLastIdlePlayed), the procedure (GetCurrentAIProcedure), the overrides
    /// (SetActorsAI, ToggleActorsAI, IsActorsAIOff, SetForceRun, GetForceRun, SetForceSneak, GetForceSneak,
    /// SetNoAvoidance), the combat groups (GetGroupMemberCount, GetGroupTargetCount, GetOwnerLastTarget) and the
    /// small ones that are fixed answers in the executables; also AdvancePCLevel, ModPCAttribute (the player) and
    /// GetIsPlayableRace.
    void addActorStatusCommands(Interpreter& interpreter, ActorStatus& status, CombatState& combat,
        PlayerProgress& progress, ActorValues& values, ActorStatusWorld& world, Game game);
}

#endif
