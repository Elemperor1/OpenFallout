#include "extendercommands.hpp"

#include <utility>

namespace FalloutScript
{
    namespace
    {
        struct Entry
        {
            std::uint16_t mOpcode;
            const char* mName;
            bool mNeedsReference;
        };

        // The names are the ones the source text of the scripts that call them has (Tale of Two Wastelands), and the
        // calls have no arguments (the bytes of the call are the code and a length of 0)
        constexpr Entry entries[] = {
            // The Gene Projector of the birth scene of Tale of Two Wastelands, which opens the menu for the face and
            // the race where Fallout 3 calls ShowRaceMenu (the stage 60 of the quest CG00, and the exit of Vault 101)
            { 0x3800, "TTW_ShowGeneProjector", false },
        };
    }

    void addExtenderCommands(CommandTable& table, Game game)
    {
        // Fallout 3 has no extender in the files of the games; the commands are those of Tale of Two Wastelands, which
        // runs on New Vegas
        if (game != Game::NewVegas)
            return;
        for (const Entry& entry : entries)
        {
            CommandInfo info;
            info.mOpcode = entry.mOpcode;
            info.mName = entry.mName;
            info.mNeedsReference = entry.mNeedsReference;
            table.add(std::move(info));
        }
    }
}
