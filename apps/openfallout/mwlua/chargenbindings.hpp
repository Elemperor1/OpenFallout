#ifndef MWLUA_CHARGENBINDINGS_H
#define MWLUA_CHARGENBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.chargen: the screens of the making of the player's character in Fallout 3 and New Vegas
    sol::table initCoreChargenBindings(const Context& context);
}

#endif // MWLUA_CHARGENBINDINGS_H
