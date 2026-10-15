#ifndef MWLUA_VATSBINDINGS_H
#define MWLUA_VATSBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.vats: V.A.T.S., the targeting that stops the game for attacks at parts of the body, for a game of Fallout 3
    /// or New Vegas
    sol::table initCoreVatsBindings(const Context& context);
}

#endif // MWLUA_VATSBINDINGS_H
