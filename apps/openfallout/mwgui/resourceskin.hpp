#ifndef MWGUI_RESOURCESKIN_H
#define MWGUI_RESOURCESKIN_H

#include <MyGUI_ResourceSkin.h>

namespace OFGui
{
    class AutoSizedResourceSkin final : public MyGUI::ResourceSkin
    {
        MYGUI_RTTI_DERIVED(AutoSizedResourceSkin)

    public:
        void deserialization(MyGUI::xml::ElementPtr node, MyGUI::Version version) override;
    };

}

#endif
