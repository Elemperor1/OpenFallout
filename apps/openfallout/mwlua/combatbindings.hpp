#ifndef MWLUA_COMBATBINDINGS_H
#define MWLUA_COMBATBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.combat: who fights whom, and the blows that characters strike, for a game of Fallout 3 or New Vegas
    sol::table initCoreCombatBindings(const Context& context);
}

#endif // MWLUA_COMBATBINDINGS_H
