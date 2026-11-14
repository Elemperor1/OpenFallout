#ifndef MWLUA_ACTORSTATUSBINDINGS_H
#define MWLUA_ACTORSTATUSBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.actorStatus: what the commands of a script ask about a character (call), and the state the engine tells
    /// those commands (activity), for a game of Fallout 3 or New Vegas. Used by the hidden rounds.
    sol::table initCoreActorStatusBindings(const Context& context);
}

#endif // MWLUA_ACTORSTATUSBINDINGS_H
