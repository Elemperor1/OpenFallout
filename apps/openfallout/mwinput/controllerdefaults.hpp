#ifndef MWINPUT_CONTROLLERDEFAULTS_H
#define MWINPUT_CONTROLLERDEFAULTS_H

#include <map>

#include <components/sdlutil/desktopconventions.hpp>

namespace OFInput
{
    /// The default pad buttons of the actions (action -> SDL_GameControllerButton). On macOS the Home (Guide) button
    /// belongs to the system's game overlay and is bound to nothing; elsewhere it quick-saves, as before.
    std::map<int, int> defaultControllerButtons(SDLUtil::DesktopPlatform platform);

    /// Whether the game reads a pad button at all: on macOS the Home (Guide) button is the system's, also when an older
    /// configuration binds it; elsewhere every button is the game's.
    bool isGameControllerButton(int button, SDLUtil::DesktopPlatform platform);
}

#endif
