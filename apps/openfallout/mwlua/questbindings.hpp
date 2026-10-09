#ifndef MWLUA_QUESTBINDINGS_H
#define MWLUA_QUESTBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// world.quests and world.globals: the quests and global variables of the Fallout games
    void addQuestBindings(sol::table& api, const Context& context);
}

#endif // MWLUA_QUESTBINDINGS_H
