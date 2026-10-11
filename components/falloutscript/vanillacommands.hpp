#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VANILLACOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VANILLACOMMANDS_H

#include <cstdint>

#include "commandtable.hpp"

namespace FalloutScript
{
    enum class Game : std::uint8_t
    {
        Fallout3,
        NewVegas,
    };

    /// The script commands of a game as its executable lists them: the code a compiled script calls each by, its name,
    /// short name and parameters. The extension commands of NVSE and the like (codes from 0x1400) are not in it.
    CommandTable vanillaCommands(Game game);
}

#endif
