#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVESCRIPTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVESCRIPTS_H

#include <cstdint>

#include "savestate.hpp"

namespace FalloutScript
{
    class Instance;
    class ObjectScripts;
    class QuestManager;

    /// The sections of a saved game that hold the quests and the scripts. Reading puts the saved state on top of what
    /// the manager has (a quest or an object that the records no longer have is left out), after reset() or clear().

    /// How far the player is in each quest: whether it runs, its stage and the stages done, the objectives, the
    /// journal, the time to the next run of its script and the variables of the script
    constexpr std::uint32_t questsSectionVersion = 1;
    void writeQuests(const QuestManager& quests, SaveWriter& out);
    void readQuests(QuestManager& quests, SaveReader& in, std::uint32_t version);

    /// The variables of the scripts of objects. A script that no object has yet (its cell was not loaded since) gets
    /// its variables when it is given to the object.
    constexpr std::uint32_t objectScriptsSectionVersion = 1;
    void writeObjectScripts(const ObjectScripts& objects, SaveWriter& out);
    void readObjectScripts(ObjectScripts& objects, SaveReader& in, std::uint32_t version);
}

#endif
