#ifndef MWLUA_CONVERSATIONBINDINGS_H
#define MWLUA_CONVERSATIONBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.conversation: the conversation of the player with a character of Fallout 3 or New Vegas
    sol::table initCoreConversationBindings(const Context& context);
}

#endif // MWLUA_CONVERSATIONBINDINGS_H
