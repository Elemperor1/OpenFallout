#ifndef MWLUA_INTERACTBINDINGS_H
#define MWLUA_INTERACTBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.interact: the locks, terminals, waiting and sleeping, and the map of Fallout 3 and New Vegas, for the
    /// screens that show them
    sol::table initCoreInteractBindings(const Context& context);
}

#endif // MWLUA_INTERACTBINDINGS_H
