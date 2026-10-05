#ifndef MWLUA_WEATHERBINDINGS_H
#define MWLUA_WEATHERBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    sol::table initCoreWeatherBindings(const Context&);

}

#endif // MWLUA_WEATHERBINDINGS_H
