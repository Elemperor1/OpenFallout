#ifndef MWLUA_SPECIALSCREENBINDINGS_H
#define MWLUA_SPECIALSCREENBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.specialScreen: the rules of the Vigor Tester and the SPECIAL book (FalloutScript::SpecialScreen::Screen)
    sol::table initCoreSpecialScreenBindings(const Context& context);
}

#endif // MWLUA_SPECIALSCREENBINDINGS_H
