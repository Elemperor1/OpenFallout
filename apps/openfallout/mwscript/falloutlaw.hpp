#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWSCRIPT_FALLOUTLAW_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWSCRIPT_FALLOUTLAW_H

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

#include <components/falloutscript/crime.hpp>
#include <components/falloutscript/standing.hpp>

#include "../mwworld/ptr.hpp"

namespace OFScript
{
    class FalloutScripts;

    /// The player took an item: told to the crime rule when the game has one. False when it has none (a game of the
    /// engine that is not Fallout), which leaves the taking to the rules of that game.
    bool noteTaking(const OFWorld::Ptr& actor, const OFWorld::Ptr& item, const OFWorld::Ptr& container, int count);

    /// The crime and the standing of the games in the running engine: what the rules of components/falloutscript
    /// (Law, Standing) ask of the records and of the world, and the events of the world that they are told of (a blow
    /// at a character, a kill, something taken that is someone's, the player in a place that is someone's). Every
    /// event is told from any thread and worked out on the main thread, where the characters of the cells are.
    class LawGlue final : public FalloutScript::LawRecords,
                          public FalloutScript::LawWorld,
                          public FalloutScript::StandingRecords,
                          public FalloutScript::StandingWorld
    {
    public:
        explicit LawGlue(FalloutScripts& host);

        FalloutScript::Law& law() { return mLaw; }
        FalloutScript::Standing& standing() { return mStanding; }

        /// Forgets everything, as for a new game
        void clear();
        /// Gives the interpreter the commands of the rules, over the stand-ins that were registered before
        void addCommands();

        // The events
        /// The player hit the character with a blow that nobody let pass. `fighting` is whether the character fought
        /// already (somebody, not necessarily the player).
        void blow(FalloutScript::FormId victim, FalloutScript::FormId weapon, bool fighting);
        /// The player killed the character (told when the death happens)
        void killed(FalloutScript::FormId victim);
        /// The player took an item, from the ground or from a container
        void taken(const OFWorld::Ptr& actor, const OFWorld::Ptr& item, const OFWorld::Ptr& container, int count);
        /// The player took a loose item (`item` is its base form), or took `count` of an item from a container; what comes out
        /// of a character is not taken from anyone's place
        void pickedUp(FalloutScript::FormId reference, FalloutScript::FormId item, int count);
        void takenFrom(FalloutScript::FormId container, FalloutScript::FormId item, int count);
        /// The player opened a lock or hacked a terminal that is someone's: a theft with no goods. A hacked terminal
        /// (`karmaOnly`) costs the karma of a theft and nothing else.
        void tamperedWith(FalloutScript::FormId reference, bool karmaOnly = false);
        /// A frame goes by: the characters of a place that is not the player's warn it
        void update(float frametime);
        /// What the character thinks of the player (0 to 100), or -1 when it cannot be worked out
        int dispositionToPlayer(FalloutScript::FormId self);
        /// Whether a character that an alarm turned on the player still fights it (what the restrictions on waiting and
        /// travel ask)
        bool alarmSounding();

        // LawRecords and StandingRecords
        FalloutScript::FormId player() override;
        FalloutScript::LawSettings lawSettings() override;
        bool tracksCrime(FalloutScript::FormId faction) override;
        bool evil(FalloutScript::FormId faction) override;
        FalloutScript::FormId reputationOf(FalloutScript::FormId faction) override;
        FalloutScript::StandingSettings standingSettings() override;
        std::optional<int> recordedReaction(FalloutScript::FormId faction, FalloutScript::FormId other) override;

        // LawWorld
        std::vector<FalloutScript::FormId> factionsOf(FalloutScript::FormId actor) override;
        FalloutScript::FormId ownerForm(FalloutScript::FormId form) override;
        FalloutScript::FormId cellOf(FalloutScript::FormId reference) override;
        bool isGuard(FalloutScript::FormId actor) override;
        int detectionLevel(FalloutScript::FormId watcher, FalloutScript::FormId target) override;
        bool alarmed(FalloutScript::FormId actor) override;
        bool detected(FalloutScript::FormId actor) override;
        bool trespassingNow(FalloutScript::FormId actor) override;
        std::vector<FalloutScript::Witness> responders(FalloutScript::FormId caller, FalloutScript::FormId subject) override;
        std::vector<FalloutScript::Witness> watchers(FalloutScript::FormId victim) override;
        void carryOut(const FalloutScript::Outcome& outcome, FalloutScript::FormId victim) override;

        // StandingWorld
        bool dispositionInput(
            FalloutScript::FormId self, FalloutScript::FormId other, FalloutScript::DispositionInput& input) override;
        int relation(FalloutScript::FormId self, FalloutScript::FormId other) override;
        double adjustDisposition(FalloutScript::FormId self, FalloutScript::FormId other, double value) override;
        double karmaOf(FalloutScript::FormId actor) override;
        bool isEvil(FalloutScript::FormId actor) override;

    private:
        /// The characters near the player, as the witnesses of something done to `victim` (0 for none): those that can
        /// see the player, with their factions. Main thread.
        std::vector<FalloutScript::Witness> gather(FalloutScript::FormId victim);
        /// The nearest character that is (or is of the faction that is) the owner of what was taken, loaded and alive
        OFWorld::Ptr nearestOwner(FalloutScript::FormId owner);
        /// What one character does about the crime or the alarm: a line, a fight
        void react(const FalloutScript::Reaction& reaction, FalloutScript::FormId victim);
        void theft(FalloutScript::FormId owner, std::int64_t value, bool karmaOnly = false);
        /// Something of the reference (its own owner, else its cell's) was taken: a theft if the owner is not the player's
        void theftOf(FalloutScript::FormId reference, std::int64_t value);
        bool inCellOwnedByOthers(FalloutScript::FormId& cell, FalloutScript::FormId& owner);

        FalloutScripts& mHost;
        FalloutScript::Law mLaw;
        FalloutScript::Standing mStanding;
        /// The time since the last look at the trespass
        float mSinceLook = 0.f;
        /// The characters that an alarm turned on the player. Asked from any thread.
        std::mutex mAlarmMutex;
        std::set<FalloutScript::FormId> mAlarmed;
    };
}

#endif
