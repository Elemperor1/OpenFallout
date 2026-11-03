#ifndef MWLUA_MENUSCENEBINDINGS_H
#define MWLUA_MENUSCENEBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.menuScene: models of the game drawn to a texture that a menu shows (the lock and pick of the lockpicking
    /// screen)
    sol::table initCoreMenuSceneBindings(const Context& context);
}

#endif // MWLUA_MENUSCENEBINDINGS_H
