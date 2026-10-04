#ifndef OPENFALLOUT_MWLUA_DEBUGBINDINGS_H
#define OPENFALLOUT_MWLUA_DEBUGBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    sol::table initDebugPackage(const Context& context);
}

#endif // OPENFALLOUT_MWLUA_DEBUGBINDINGS_H
