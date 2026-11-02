#ifndef MWLUA_MENUBINDINGS_H
#define MWLUA_MENUBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.menus: the menus of Fallout 3 and New Vegas, opened from the game's own menu files
    sol::table initCoreMenuBindings(const Context& context);
}

#endif // MWLUA_MENUBINDINGS_H
