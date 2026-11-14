#ifndef MWLUA_SPAWNBINDINGS_H
#define MWLUA_SPAWNBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.spawn: what the rules that bring characters, creatures and items into the world and back keep (the level
    /// a character was placed at, when it died, what a holder carries) and a way to call a script command of Fallout
    /// from Lua, so that a round can run the commands of the plugins on the real data
    sol::table initCoreSpawnBindings(const Context& context);
}

#endif // MWLUA_SPAWNBINDINGS_H
