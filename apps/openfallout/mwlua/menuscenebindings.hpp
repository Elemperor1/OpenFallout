#ifndef MWLUA_MENUSCENEBINDINGS_H
#define MWLUA_MENUSCENEBINDINGS_H

#include <sol/forward.hpp>

namespace OFLua
{
    struct Context;

    /// core.menuScene: models of the game drawn to a texture that a menu shows (the lock and pick of the lockpicking
    /// screen)
    sol::table initCoreMenuSceneBindings(const Context& context);

    /// The scenes are gone with the game (their scripts are): no script is told that a scene is ready any more
    void clearMenuSceneReady();
}

#endif // MWLUA_MENUSCENEBINDINGS_H
