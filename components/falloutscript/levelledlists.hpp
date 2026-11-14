#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LEVELLEDLISTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LEVELLEDLISTS_H

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include <components/esm4/levelled.hpp>

#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// What the records of the game say about levelled lists
    class LevelledListRecords
    {
    public:
        virtual ~LevelledListRecords() = default;

        enum class Kind : std::uint8_t
        {
            None, // not a levelled list
            Item, // LVLI
            Character, // LVLN
            Creature, // LVLC
        };

        /// What kind of levelled list the form is (None when no record of one is that form)
        virtual Kind kindOf(FormId form) = 0;
        /// Whether the form is something that the kind of list takes as an entry: an item for a list of items (not
        /// another list), a character for a list of characters, a creature for a list of creatures
        virtual bool isEntryOf(Kind kind, FormId form) = 0;
    };

    /// An entry that a script put in a levelled list (the commands AddItemToLeveledList, AddNPCToLeveledList and
    /// AddCreatureToLeveledList). Level and count are kept as the 16 bits that the executable keeps them in.
    struct LevelledAddition
    {
        FormId mForm = 0;
        std::uint16_t mLevel = 1;
        std::uint16_t mCount = 1;
        /// What condition an item from the list has, in percent (items only)
        float mHealth = 100.f;

        bool operator==(const LevelledAddition&) const = default;
    };

    /// The levelled lists as scripts changed them: what a list gives is its record and what was added to it, so every
    /// roll after the command, for everything that rolls on the list, sees the new entry. The entries of a list are in
    /// the order of their levels and an added entry goes behind those of its level (see ESM4::insertLevelledEntry), as
    /// in the executables (0x487A60 of the New Vegas one). It can be used from any thread.
    class LevelledLists
    {
    public:
        /// Forgets everything, as for a new game
        void clear();

        void add(FormId list, const LevelledAddition& entry);
        /// What was added to the list, in the order it was added
        std::vector<LevelledAddition> additions(FormId list) const;
        std::size_t listCount() const;

        /// The entries of the record with what was added put in. False when nothing was added to the list, and
        /// `merged` is not touched. It is what ESM4::setLevelledAdditions wants.
        bool merge(FormId list, const std::vector<ESM4::LVLO>& entries, std::vector<ESM4::LVLO>& merged) const;

        void write(SaveWriter& out) const;
        void read(SaveReader& in, std::uint32_t version);

    private:
        mutable std::mutex mMutex;
        std::map<FormId, std::vector<LevelledAddition>> mAdded;
    };

    /// Gives the commands AddItemToLeveledList, AddNPCToLeveledList and AddCreatureToLeveledList to the interpreter.
    /// Those that the command table does not have are left out. A command whose list is not a list of its kind, or
    /// whose entry is not of its kind, does nothing.
    void addLevelledListCommands(Interpreter& interpreter, LevelledLists& lists, LevelledListRecords& records);
}

#endif
