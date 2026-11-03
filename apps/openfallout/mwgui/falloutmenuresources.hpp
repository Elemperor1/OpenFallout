#ifndef OPENFALLOUT_MWGUI_FALLOUTMENURESOURCES_H
#define OPENFALLOUT_MWGUI_FALLOUTMENURESOURCES_H

#include <memory>

#include <components/falloutmenu/openmenu.hpp>

namespace OFGui
{
    /// What the menus of the game share (its fonts, the atlas of small pictures, the sizes of textures) read from the
    /// data of the running game and its settings
    std::shared_ptr<FalloutMenu::Resources> makeFalloutMenuResources();
}

#endif
