#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVEWORLD_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVEWORLD_H

#include <cstdint>
#include <map>
#include <set>

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

    /// The actors inside the volume of each trigger that has any. The volumes themselves come from the cells that are
    /// loaded, so a load only reads the occupants: they are handed to the trigger when its volume is put in.
    constexpr std::uint32_t triggersSectionVersion = 1;
    void writeTriggers(TriggerVolumes& triggers, const std::map<FormId, std::set<FormId>>& waiting, SaveWriter& out);
    std::map<FormId, std::set<FormId>> readTriggers(SaveReader& in, std::uint32_t version);
    /// Puts the occupants a save names in a trigger whose volume is in (they are not asked for again)
    void restoreOccupants(TriggerVolumes& triggers, FormId trigger, const std::set<FormId>& occupants);

    /// Which of the objects with an Open and a Close animation rest open (or closed, when a script set it)
    constexpr std::uint32_t openStatesSectionVersion = 1;
    void writeOpenStates(OpenStates& states, SaveWriter& out);
    void readOpenStates(OpenStates& states, SaveReader& in, std::uint32_t version);
}

#endif
