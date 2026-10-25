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
            // The ones of Tale of Two Wastelands' own plugin that its generic handler, its start button and its
            // statistics script call (names and arguments read from the source text and the compiled calls of the
            // scripts): a switch, a statistic by name, a form, and two event handlers of a script for a form or a list
            { 0x3808, "TTW_EnableRadioFix", false, "1" },
            { 0x31EE, "InitExtraMiscStat", false, "0" },
            { 0x28D7, "InitRockItLauncher", false, "21" },
            { 0x28B0, "SetOnProjectileImpactEventHandler", false, "21 1 21" },
            { 0x28B3, "SetOnUseAidItemEventHandler", false, "21 1 21" },
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
