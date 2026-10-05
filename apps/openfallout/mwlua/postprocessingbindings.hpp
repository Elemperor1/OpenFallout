#ifndef MWLUA_POSTPROCESSINGBINDINGS_H
#define MWLUA_POSTPROCESSINGBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    sol::table initPostprocessingPackage(const Context& context);
}

#endif // MWLUA_POSTPROCESSINGBINDINGS_H
