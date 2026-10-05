#ifndef MWLUA_ANIMATIONBINDINGS_H
#define MWLUA_ANIMATIONBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    sol::table initAnimationPackage(const Context& context);
    sol::table initWorldVfxBindings(const Context& context);
}

#endif // MWLUA_ANIMATIONBINDINGS_H
