#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORS_H

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

#include "saveaccess.hpp"
#include "script.hpp"

namespace FalloutScript
{
    /// What the records of the game say about a character, creature or container before a script changes anything
    class ActorRecords
    {
    public:
        virtual ~ActorRecords() = default;

        /// The items the reference holds (the form of the item and how many). False when the host can not tell, for a
        /// reference in a cell that is not loaded: it is asked again by the next command that needs it.
        virtual bool initialItems(FormId holder, std::vector<std::pair<FormId, int>>& items) = 0;
        /// The factions the reference is in (the form of the faction and the rank). False when the host can not tell.
        virtual bool initialFactions(FormId actor, std::vector<std::pair<FormId, int>>& factions) = 0;
        /// The form the reference was made from (the base of a character or creature). False when the host can not
        /// tell, which counts the reference as nothing.
        virtual bool baseFormOf(FormId, FormId&) { return false; }
        /// Whether the reference starts dead: the games make a character or creature whose record gives it no health
        /// at all (a base health of 0 or less) dead when it comes into being. False when the host can not tell yet.
        virtual bool initiallyDead(FormId, bool& dead)
        {
            dead = false;
            return true;
        }
    };

    /// What scripts do to characters, creatures and containers, as far as nothing else in the game does it yet: the
    /// items they hold, whether they are dead and the factions they are in. It can be used from any thread. A reference
    /// takes what its record says the first time a script asks about it, and then keeps what scripts did to it. A
    /// reference whose record the host can not read yet (its cell is not loaded) keeps what scripts do to it, which is
    /// applied to what the record says when it can be read. Items are kept by the form that scripts name; nothing here
    /// knows what an item is.
    class ActorState
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit ActorState(ActorRecords& records);

        /// The condition of a copy of an item in tenths of a percent of its health: 1000 is full health, 0 is broken
        static constexpr int fullCondition = 1000;
        /// The copies of an item that share a condition
        struct ConditionStack
        {
            int mCondition = fullCondition;
            int mCount = 0;
        };

        /// Forgets everything, as for a new game
        void clear();

        /// How many of the item the holder has
        int itemCount(FormId holder, FormId item);
        void addItem(FormId holder, FormId item, int count);
        /// Takes away up to `count` of the item (all it has when it has fewer) and tells how many it took: the copies of
        /// full condition first, then the worn ones from the best to the worst
        int removeItem(FormId holder, FormId item, int count);
        /// Takes away all of the items (the form and how many) or none: false, and nothing taken, when the holder has
        /// fewer of any. It is one step, so of two calls at once for the same items only as many succeed as the
        /// holder has the items for.
        bool takeItems(FormId holder, const std::vector<std::pair<FormId, int>>& wanted);
        /// How the copies of the item that the holder has stand: the stacks by condition, the best first (a copy has full
        /// condition unless it was worn or given less: weapons and armor wear out, and AddItemHealthPercent gives
        /// less). The counts add up to itemCount.
        std::vector<ConditionStack> conditions(FormId holder, FormId item);
        /// Adds copies of the item with that condition (a condition from 0 to 1000; more is full)
        void addItem(FormId holder, FormId item, int count, int condition);
        /// Takes away up to `count` copies of the item that have exactly that condition and tells how many it took
        int removeItem(FormId holder, FormId item, int count, int condition);
        /// Changes the condition of up to `count` copies from one to another (they wear out, or are mended) and tells how
        /// many it changed
        int setCondition(FormId holder, FormId item, int from, int to, int count = 1);
        /// Everything the holder has (the form of the item and how many, in the order of the forms). False when the
        /// host can not tell yet what the record holds, for a reference in a cell that is not loaded.
        bool items(FormId holder, std::vector<std::pair<FormId, int>>& result);

        /// A change that a script made to what a holder has: a positive count was added, a negative one taken away
        struct ItemChange
        {
            FormId mHolder = 0;
            FormId mItem = 0;
            int mCount = 0;
        };
        /// The changes since this was last called (the first of them kept at most 4096): the game raises the events
        /// of the items (OnAdd, OnDrop) from them when it is ready for scripts to run, not in the middle of one.
        std::vector<ItemChange> takeChanges();

        /// Whether the actor is dead; one that starts dead by its record (no health) is, from the first time it is
        /// asked
        bool isDead(FormId actor) const;
        /// Marks the actor dead (killed by `killer`, 0 when no one is known) or alive again. An actor that was alive
        /// is told in takeDeaths(). True when this call changed the state: of two calls at once that kill the same
        /// actor, only one gets true. `limb` is the part of the body the killing blow struck, -1 when none did (a
        /// script killed the actor) or it is not known.
        bool setDead(FormId actor, bool dead, FormId killer = 0, int limb = -1);
        /// Kills the actor as the game's Kill does (KillActor): one that is dead already stays so, one that the host
        /// lets fall instead of dying (an essential one) falls and does not die, and any other dies. True when this
        /// call made the actor dead.
        bool kill(FormId actor, FormId killer = 0);
        /// The part of the body that the killing blow struck, -1 when the actor is alive or none did
        int killingLimb(FormId actor) const;

        /// What the host does at the moment an actor dies (setDead(true) that changed the state), once per death and
        /// with the state unlocked (it can ask it, but not mark a death or a revival): it ends the fights of the actor
        /// there, not when the death is told, which may be after the actor was brought back. A revival waits for the
        /// listener of a death in progress, so it comes after the cleanup of the death. Set before scripts run.
        void setDeathListener(std::function<void(FormId)> listener);
        /// Holds every death and revival back for as long as the returned lock is kept, for a check that must not be
        /// overtaken by one (whether a fight can start, whether a blow can land). The lock may be taken again by the
        /// thread that has it, so the holder can mark a death itself.
        std::unique_lock<std::recursive_mutex> holdTransitions();
        /// What the host does when a character is brought back by revive(): wakes it, if it was unconscious. Set
        /// before scripts run.
        void setReviveHandler(std::function<void(FormId)> handler);
        /// Brings the character back to life (and to its senses, as the host says)
        void revive(FormId actor);
        /// What the host says to a character whose health scripts took to nothing: true when it falls and is up again
        /// later (an essential character) and does not die. Set before scripts run.
        void setFallHandler(std::function<bool(FormId)> handler);
        /// Asks the handler; false (the actor dies) when there is none
        bool falls(FormId actor);
        /// Who killed the actor, 0 when it is alive or the killer is not known
        FormId killerOf(FormId actor) const;

        /// An actor that died
        struct Death
        {
            FormId mActor = 0;
            FormId mKiller = 0;
        };
        /// The actors that died since this was last called (the first 1024 kept): the game raises their OnDeath blocks
        /// when it is ready for scripts to run, not in the middle of one.
        std::vector<Death> takeDeaths();

        /// The factions the actor is in (the form of the faction and the rank), in the order of the forms. False when
        /// the host can not tell yet what the record says.
        bool factions(FormId actor, std::vector<std::pair<FormId, int>>& result);
        /// The form the reference was made from; false when the host can not tell
        bool baseOf(FormId reference, FormId& base);
        /// How many of the characters or creatures made from the base form are dead
        int deadCount(FormId base) const;

        /// The rank of the actor in the faction, -1 when it is not in it
        int factionRank(FormId actor, FormId faction);
        /// Puts the actor in the faction at that rank, or changes its rank
        void setFactionRank(FormId actor, FormId faction, int rank);
        void removeFromFaction(FormId actor, FormId faction);

    private:
        /// What scripts did to the count of an item before the records could be asked what the holder starts with: the
        /// count that results is clamp(start + mChange, mLowest, mHighest), whatever the start is (adding and
        /// removing, with the rules that a count does not go below 0 or above the most there is, compose to that)
        struct ItemEdit
        {
            std::int64_t mChange = 0;
            std::int64_t mLowest = 0;
            std::int64_t mHighest = std::numeric_limits<int>::max();

            std::int64_t apply(std::int64_t start) const;
            void add(std::int64_t count);
            void remove(std::int64_t count);
        };

        struct Holder
        {
            std::map<FormId, int> mItems; // what the holder has, once the records have been asked
            /// The copies that are below full condition: condition (0 to 999) -> how many. The rest of the count is full.
            std::map<FormId, std::map<int, int>> mWorn;
            std::map<FormId, ItemEdit> mItemEdits; // what scripts did until then
            std::map<FormId, int> mFactions; // the same for the factions
            std::map<FormId, int> mFactionEdits; // until then: the rank scripts gave, -1 for out of the faction
            bool mItemsKnown = false;
            bool mFactionsKnown = false;
        };

        Holder& holder(FormId id) { return mHolders[id]; }
        void knowItems(FormId id, Holder& entry);
        /// The caller holds the lock
        void noteChange(FormId holder, FormId item, int count);
        /// The caller holds the lock
        int heldLocked(FormId id, FormId item);
        /// The caller holds the lock
        int removeLocked(FormId id, FormId item, int count);
        /// Takes `count` copies off the count without asking which: the caller says which stacks it took them from. The
        /// caller holds the lock.
        int removeCountLocked(FormId id, FormId item, int count);
        /// Adds to the count only, and tells how many were added. The caller holds the lock.
        int addCountLocked(FormId id, FormId item, int count);
        /// The copies that are below full condition (condition -> how many), cut to what the holder has. The caller holds
        /// the lock.
        std::map<int, int> wornLocked(FormId id, FormId item);
        /// How many copies of exactly that condition the holder has. The caller holds the lock.
        int availableLocked(FormId id, FormId item, int condition);
        /// Forgets `count` copies of that worn condition. The caller holds the lock.
        void takeWornLocked(FormId id, FormId item, int condition, int count);
        void knowFactions(FormId id, Holder& entry);

        ActorRecords& mRecords;
        /// Scripts run on more than one thread (the frame and Lua); one at a time touches this, but the characters ask
        /// whether they are dead from the thread of the frame
        mutable std::mutex mMutex;
        /// One death or revival at a time, from the change of the state to the end of the listener
        std::recursive_mutex mTransitionMutex;
        std::map<FormId, Holder> mHolders;
        /// Mutable since a reference that starts dead is found out the first time it is asked about (the caller holds
        /// the lock)
        mutable std::set<FormId> mDead;
        /// The references that have been asked about whether they start dead
        mutable std::set<FormId> mStartKnown;
        /// Whether the actor is dead, finding out first whether it starts so (the caller holds the lock)
        bool deadLocked(FormId actor) const;
        std::map<FormId, FormId> mKillers;
        std::map<FormId, int> mKillingLimbs;
        std::vector<Death> mDeaths;
        std::vector<ItemChange> mChanges;
        std::function<void(FormId)> mDeathListener;
        std::function<void(FormId)> mReviveHandler;
        std::function<bool(FormId)> mFallHandler;
    };

    /// Gives the commands about items, deaths and factions to the interpreter: AddItem, AddItemHealthPercent,
    /// RemoveItem, RemoveAllItems, DuplicateAllItems, GetItemCount, GetDead, GetDeadCount, KillActor, AddToFaction,
    /// RemoveFromFaction, SetFactionRank, GetInFaction and GetFactionRank (the commands about the health of a
    /// character are with the actor values). Those that the command table does not have are left out.
    class Interpreter;
    void addActorCommands(Interpreter& interpreter, ActorState& state);
}

#endif
