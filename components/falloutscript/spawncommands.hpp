#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SPAWNCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SPAWNCOMMANDS_H

#include <map>
#include <mutex>

#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class ActorState;
    class FormLists;
    class Interpreter;
    class ObjectWorld;
    class RespawnState;

    /// The values that scripts gave to items (SetItemValue), which the game then uses for the item instead of the value
    /// of its record, everywhere it asks (trade, the Pip-Boy, barter). It can be used from any thread.
    class ItemValues
    {
    public:
        void clear();
        void set(FormId item, int value);
        bool find(FormId item, int& value) const;
        std::size_t size() const;

        void write(SaveWriter& out) const;
        void read(SaveReader& in, std::uint32_t version);

    private:
        mutable std::mutex mMutex;
        std::map<FormId, int> mValues;
    };

    /// The names that scripts gave to characters, creatures and cells (SetActorFullName, SetCellFullName): a name is
    /// the text of a message of the files, which is what the commands take. It can be used from any thread.
    class NameOverrides
    {
    public:
        void clear();
        void set(FormId target, FormId message);
        /// The message that gives the name; false when scripts did not change it
        bool find(FormId target, FormId& message) const;
        std::size_t size() const;

        void write(SaveWriter& out) const;
        void read(SaveReader& in, std::uint32_t version);

    private:
        mutable std::mutex mMutex;
        std::map<FormId, FormId> mMessages;
    };

    /// What the commands that make things and take them out of the world ask of the game world
    class SpawnWorld
    {
    public:
        virtual ~SpawnWorld() = default;

        /// Whether the reference exists: a reference of the files or one made while playing that was not deleted. The
        /// host that does not know says yes for any reference it was given.
        virtual bool exists(FormId reference) { return reference != 0; }

        /// Makes a character or creature of the base form at the place of `near`, as PlaceLeveledActorAtMe does: its
        /// levelled lists are rolled for the level of the zone (or the player) and what the modifier says, and the
        /// reference is told. 0 when nothing is made.
        virtual FormId placeLeveledActor(FormId near, FormId base, int levelMod, FormId zone)
        {
            (void)near;
            (void)base;
            (void)levelMod;
            (void)zone;
            return 0;
        }

        /// The character or container that holds the item whose script runs; 0 for an item that lies in the world
        virtual FormId holderOfItem(FormId item)
        {
            (void)item;
            return 0;
        }

        /// Puts `count` of the item into the world as one object next to the holder, in the condition and with the mods
        /// that the copies had (a condition of 1000 is full, 0 mods none). 0 when nothing was put there.
        virtual FormId dropItem(FormId holder, FormId item, int count, int condition = 1000, int mods = 0)
        {
            (void)holder;
            (void)item;
            (void)count;
            (void)condition;
            (void)mods;
            return 0;
        }

        /// The number that the games give the kind of record of the form (the form type: 24 armor, 31 miscellaneous
        /// items, 40 weapons, 41 ammunition, 47 aid), 0 for a form the host does not know
        virtual int formTypeOf(FormId form)
        {
            (void)form;
            return 0;
        }

        /// The reference that the reference follows as its enable parent; 0 for none
        virtual FormId parentOf(FormId reference)
        {
            (void)reference;
            return 0;
        }

        /// A script asked to drop the cells that the game keeps in memory for a quick return
        virtual void purgeCellBuffers() {}
    };

    /// Gives the commands that bring things into the world and take them out to the interpreter: PlaceLeveledActorAtMe,
    /// PlaceAtMeHealthPercent, Exists, RemoveMe, DropMe, Drop, RemoveAllTypedItems, ResetInventory, ResetInterior,
    /// SetZoneRespawns, PurgeCellBuffers, GetInCellParam, GetParentRef, GetContainerInventoryCount, SetItemValue,
    /// SetActorFullName and SetCellFullName. Those that the command table does not have are left out.
    struct SpawnParts
    {
        ActorState& mActors;
        FormLists& mFormLists;
        ObjectWorld& mObjects;
        SpawnWorld& mWorld;
        RespawnState& mRespawn;
        ItemValues& mItemValues;
        NameOverrides& mNames;
    };
    void addSpawnCommands(Interpreter& interpreter, const SpawnParts& parts);
}

#endif
