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
            const char* mParameters = "";
        };

        // The names are the ones the source text of the scripts that call them has (Tale of Two Wastelands). A call
        // without arguments is the code and a length of 0; the ones that have arguments say their parameters.
        constexpr Entry entries[] = {
            // The Gene Projector of the birth scene of Tale of Two Wastelands, which opens the menu for the face and
            // the race where Fallout 3 calls ShowRaceMenu (the stage 60 of the quest CG00, and the exit of Vault 101)
            { 0x3800, "TTW_ShowGeneProjector", false },
            // The commands of xNVSE and of JIP LN NVSE that the generic handler of Tale of Two Wastelands and the
            // scripts of its intro call; the games' own scripts have no use of them
            { 0x146D, "GetParentCell", true },
            { 0x146E, "GetParentWorldspace", true },
            { 0x14A8, "GetGameRestarted", false },
            { 0x14AC, "GetGameLoaded", false },
            { 0x278A, "IsSoundPlaying", false, "12 4?" },
            { 0x2862, "IsMusicPlaying", false, "61?" },
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
            parseParameterTypes(entry.mParameters, info.mParameters);
            table.add(std::move(info));
        }
    }
}
