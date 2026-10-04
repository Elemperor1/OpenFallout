#ifndef OPENFALLOUT_LUAUI_REGISTERSCRIPTSETTINGS
#define OPENFALLOUT_LUAUI_REGISTERSCRIPTSETTINGS

#include <sol/sol.hpp>

namespace LuaUi
{
    // implemented in scriptsettings.cpp
    void registerSettingsPage(const sol::table& options);
    void clearSettings();
    void removeSettingsPage(const sol::table& options);
}

#endif // !OPENFALLOUT_LUAUI_REGISTERSCRIPTSETTINGS
