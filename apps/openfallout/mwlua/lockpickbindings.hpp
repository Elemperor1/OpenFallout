#ifndef MWLUA_LOCKPICKBINDINGS_H
#define MWLUA_LOCKPICKBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.lockpick: the game of picking one lock (the rules are FalloutScript::Lockpick::Game's)
    sol::table initCoreLockpickBindings(const Context& context);
}

#endif // MWLUA_LOCKPICKBINDINGS_H
