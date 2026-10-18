#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ANIMGROUPS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ANIMGROUPS_H

#include <string_view>

#include "vanillacommands.hpp"

namespace FalloutScript
{
    /// The name of the animation group that a compiled script calls by number (PlayGroup, IsAnimPlaying), as the table
    /// of the executable of the game has it (183 groups in Fallout 3, 245 in New Vegas); empty for a number beyond
    /// it. The numbers up to 96 are the same in both games, from 97 on they differ (New Vegas has five custom power
    /// attacks there), so a plugin is ported by the name.
    std::string_view animationGroupName(Game game, int number);

    /// Whether the name of a group in a model is the name of a group in the table or a variant of it: a model names a
    /// variant `<name>_<what>` (Forward_A, SpecialIdle_Hit), and a script that plays `name` gets one of them when the
    /// model has no group of the name itself. The comparison ignores case.
    bool isGroupVariant(std::string_view group, std::string_view name);
}

#endif
