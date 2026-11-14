#ifndef MWLUA_FALLOUTWORLDBINDINGS_H
#define MWLUA_FALLOUTWORLDBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.worldCommands: runs the commands of the script language by their names (the maths, the sky, the cells and the
    /// odd corners of M16) as a script would, so that a round on real data can ask what a condition answers
    sol::table initCoreWorldCommandBindings(const Context& context);
}

#endif // MWLUA_FALLOUTWORLDBINDINGS_H
