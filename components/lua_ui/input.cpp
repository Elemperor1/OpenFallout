#include "input.hpp"

#include "widget.hpp"

namespace LuaUi
{
    void dispatchMouseWheel(MyGUI::Widget* widget, const MouseWheel& wheel)
    {
        while (widget)
        {
            if (auto* ext = dynamic_cast<WidgetExtension*>(widget))
            {
                ext->mouseWheel(wheel);
                break;
            }
            widget = widget->getParent();
        }
    }
}
