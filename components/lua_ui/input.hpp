#ifndef OPENFALLOUT_COMPONENTS_LUA_UI_INPUT_H
#define OPENFALLOUT_COMPONENTS_LUA_UI_INPUT_H

namespace MyGUI
{
    class Widget;
}

namespace LuaUi
{
    /// A turn of the wheel over the interface, as a Lua `MouseWheelEvent` gets it
    struct MouseWheel
    {
        float mLeft = 0.f; // where the cursor is
        float mTop = 0.f;
        float mDeltaX = 0.f; // the precise wheel delta
        float mDeltaY = 0.f;
        int mNotchesX = 0; // the whole notches it makes up (SDLUtil::WheelNotches)
        int mNotchesY = 0;
    };

    void dispatchMouseWheel(MyGUI::Widget* widget, const MouseWheel& wheel);
}

#endif
