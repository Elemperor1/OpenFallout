#ifndef OPENFALLOUT_COMPONENTS_LUA_UI_INPUT_H
#define OPENFALLOUT_COMPONENTS_LUA_UI_INPUT_H

namespace MyGUI
{
    class Widget;
}

namespace LuaUi
{
    /// `x`, `y`: the precise wheel delta; `notchesX`, `notchesY`: the whole notches it makes up, counted from the
    /// precise deltas (a trackpad sends many small ones)
    void dispatchMouseWheel(MyGUI::Widget* widget, float left, float top, float x, float y, int notchesX, int notchesY);
}

#endif
