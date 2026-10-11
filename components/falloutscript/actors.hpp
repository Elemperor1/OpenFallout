#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORS_H

#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

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
    };

    /// What scripts do to characters, creatures and containers, as far as nothing else in the game does it yet: the
    /// items they hold, whether they are dead and the factions they are in. It can be used from any thread. A reference
    /// takes what its record says the first time a script asks about it, and then keeps what scripts did to it. A
    /// reference whose record the host can not read yet (its cell is not loaded) keeps what scripts do to it, which is
    /// applied to what the record says when it can be read. Items are kept by the form that scripts name; nothing here
    /// knows what an item is.
    class ActorState
    {
    public:
        explicit ActorState(ActorRecords& records);

        /// Forgets everything, as for a new game
        void clear();

        /// How many of the item the holder has
        int itemCount(FormId holder, FormId item);
        void addItem(FormId holder, FormId item, int count);
        /// Takes away up to `count` of the item (all it has when it has fewer) and tells how many it took
        int removeItem(FormId holder, FormId item, int count);

        bool isDead(FormId actor) const;
        void setDead(FormId actor, bool dead);

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
            std::map<FormId, ItemEdit> mItemEdits; // what scripts did until then
            std::map<FormId, int> mFactions; // the same for the factions
            std::map<FormId, int> mFactionEdits; // until then: the rank scripts gave, -1 for out of the faction
            bool mItemsKnown = false;
            bool mFactionsKnown = false;
        };

        Holder& holder(FormId id) { return mHolders[id]; }
        void knowItems(FormId id, Holder& entry);
        void knowFactions(FormId id, Holder& entry);

        ActorRecords& mRecords;
        /// Scripts run on more than one thread (the frame and Lua); one at a time touches this, but the characters ask
        /// whether they are dead from the thread of the frame
        mutable std::mutex mMutex;
        std::map<FormId, Holder> mHolders;
        std::set<FormId> mDead;
    };

    /// Gives the commands about items, deaths and factions to the interpreter: AddItem, RemoveItem, GetItemCount,
    /// GetDead, KillActor, ResurrectActor, AddToFaction, RemoveFromFaction, SetFactionRank, GetInFaction and
    /// GetFactionRank. Those that the command table does not have are left out.
    class Interpreter;
    void addActorCommands(Interpreter& interpreter, ActorState& state);
}

#endif
