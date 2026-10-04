#ifndef MWLUA_MWSCRIPTBINDINGS_H
#define MWLUA_MWSCRIPTBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    sol::table initMWScriptBindings(const Context& context);
}

#endif // MWLUA_MWSCRIPTBINDINGS_H
