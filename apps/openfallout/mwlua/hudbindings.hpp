#ifndef MWLUA_HUDBINDINGS_H
#define MWLUA_HUDBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.hud: what the head-up display of Fallout 3 and New Vegas shows besides the stats of the player
    sol::table initCoreHudBindings(const Context& context);
}

#endif // MWLUA_HUDBINDINGS_H
