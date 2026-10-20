#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_EXTENDERCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_EXTENDERCOMMANDS_H

#include "commandtable.hpp"
#include "vanillacommands.hpp"

namespace FalloutScript
{
    /// Adds the commands of a script extender or of the plugin of a mod that the engine knows by name. Their codes are
    /// not in the table of any executable (from 0x1400 on); each is here because a file of a mod calls it by its name
    /// and the engine does what the game does without the extender for it. The others stay codes the table lacks, which
    /// the interpreter steps over and counts.
    void addExtenderCommands(CommandTable& table, Game game);
}

#endif
