#ifndef MWLUA_PIPBOYBINDINGS_H
#define MWLUA_PIPBOYBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.pipboy: what the player's character is and carries, for the screens of the Pip-Boy, the level up and the
    /// loot of containers, in a game of Fallout 3 or New Vegas
    sol::table initCorePipboyBindings(const Context& context);
}

#endif // MWLUA_PIPBOYBINDINGS_H
