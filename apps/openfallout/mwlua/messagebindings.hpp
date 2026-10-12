#ifndef MWLUA_MESSAGEBINDINGS_H
#define MWLUA_MESSAGEBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.messageBox: the message box that a script of Fallout 3 or New Vegas has put on the screen
    sol::table initCoreMessageBoxBindings(const Context& context);
}

#endif // MWLUA_MESSAGEBINDINGS_H
