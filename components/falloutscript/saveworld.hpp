#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVEWORLD_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVEWORLD_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "savestate.hpp"

namespace FalloutScript
{
    class DialogueManager;
    class OpenStates;
    class TriggerVolumes;

    /// The sections of a saved game that hold what the world keeps of its own: what the player was told and what was
    /// said, who stood in the volume of a trigger, and which doors and switches rest open. Reading puts the saved state
    /// on top of an empty one (after clear() or reset()).

    /// The topics the player was given, in the order it was given them, the responses that were said (by the character
    /// that said them, and for those said once a day the hour), and the characters that talked to the player
    constexpr std::uint32_t dialogueSectionVersion = 1;
    void writeDialogue(DialogueManager& dialogue, SaveWriter& out);
    void readDialogue(DialogueManager& dialogue, SaveReader& in, std::uint32_t version);

    /// What a save holds of a trigger: the actors inside its volume, and the Enter and Leave events it has not raised
    /// yet (a trigger raises one on each pass, so several actors that came or went at once wait their turn)
    struct SavedTrigger
    {
        std::set<FormId> mOccupants;
        /// The block type (OnTriggerEnter or OnTriggerLeave) and the actor of each event, in the order they wait
        std::vector<std::pair<std::uint16_t, FormId>> mQueue;
    };

    /// The triggers that have an occupant or an event that waits. The volumes themselves come from the cells that are
    /// loaded, so a load only reads what is inside them: it is handed to the trigger when its volume is put in.
    constexpr std::uint32_t triggersSectionVersion = 1;
    void writeTriggers(TriggerVolumes& triggers, const std::map<FormId, SavedTrigger>& waiting, SaveWriter& out);
    std::map<FormId, SavedTrigger> readTriggers(SaveReader& in, std::uint32_t version);
    /// Puts the occupants and the events a save names in a trigger whose volume is in (the occupants are not asked for
    /// again)
    void restoreTrigger(TriggerVolumes& triggers, FormId trigger, const SavedTrigger& saved);

    /// The section of the references (what scripts changed in the world and what it has not made yet) is written by the
    /// host. Version 2 adds the count of a stack that a drop asked to put into the world; version 3 adds what is needed to
    /// give such a stack back to the holder when no object can be made after a load.
    constexpr std::uint32_t referencesSectionVersion = 3;

    /// What a request for an object keeps of the stack that a Drop or DropMe took out of the holder it is made next to: how
    /// worn the copies were and the mods in them, to give the stack back if the world makes no object. Not a drop (an
    /// ordinary PlaceAtMe) for a request that has none.
    struct SavedDrop
    {
        bool mDropped = false;
        int mCondition = 1000;
        int mMods = 0;
    };
    /// The bytes that a request holds of it in a section of that version (none before version 3)
    std::size_t savedDropBytes(std::uint32_t version);
    void writeSavedDrop(SaveWriter& out, const SavedDrop& drop);
    /// A request saved before version 3 was not a drop that could be given back: the stack of it is not known
    SavedDrop readSavedDrop(SaveReader& in, std::uint32_t version);

    /// Which of the objects with an Open and a Close animation rest open (or closed, when a script set it)
    constexpr std::uint32_t openStatesSectionVersion = 1;
    void writeOpenStates(OpenStates& states, SaveWriter& out);
    void readOpenStates(OpenStates& states, SaveReader& in, std::uint32_t version);
}

#endif
