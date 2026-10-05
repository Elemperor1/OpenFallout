#ifndef MWLUA_MARKUPBINDINGS_H
#define MWLUA_MARKUPBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    sol::table initMarkupPackage(const Context& context);
}

#endif // MWLUA_MARKUPBINDINGS_H
