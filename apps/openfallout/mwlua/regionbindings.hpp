#ifndef MWLUA_REGIONBINDINGS_H
#define MWLUA_REGIONBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;
    sol::table initCoreRegionBindings(const Context& context);
}

#endif // MWLUA_REGIONBINDINGS_H
