#ifndef MWLUA_CASINOBINDINGS_H
#define MWLUA_CASINOBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.casino: the games of the casinos of Fallout 3 and New Vegas (the roulette table, and later blackjack and
    /// the slot machines): the rules and the winnings are the engine's, the screens only show them
    sol::table initCoreCasinoBindings(const Context& context);
}

#endif // MWLUA_CASINOBINDINGS_H
