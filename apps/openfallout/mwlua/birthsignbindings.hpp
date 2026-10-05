#ifndef MWLUA_BIRTHSIGNBINDINGS_H
#define MWLUA_BIRTHSIGNBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    sol::table initBirthSignRecordBindings(const Context& context);
}

#endif // MWLUA_BIRTHSIGNBINDINGS_H
