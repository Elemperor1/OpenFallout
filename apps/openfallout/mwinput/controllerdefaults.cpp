#include "controllerdefaults.hpp"

#include <SDL_gamecontroller.h>

#include "actions.hpp"

namespace OFInput
{
    std::map<int, int> defaultControllerButtons(SDLUtil::DesktopPlatform platform)
    {
        // using hardcoded key defaults is inevitable, if we want the configuration files to stay valid
        // across different versions of OpenMW (in the case where another input action is added)
        std::map<int, int> buttons;

        buttons[A_Activate] = SDL_CONTROLLER_BUTTON_A;
        buttons[A_ToggleWeapon] = SDL_CONTROLLER_BUTTON_X;
        buttons[A_ToggleSpell] = SDL_CONTROLLER_BUTTON_Y;
        // buttons[A_QuickButtonsMenu] = SDL_GetButtonFromScancode(SDL_SCANCODE_F1); // Need to implement,
        // should be ToggleSpell(5) AND Wait(9)
        buttons[A_Sneak] = SDL_CONTROLLER_BUTTON_LEFTSTICK;
        buttons[A_Journal] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        buttons[A_Rest] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        buttons[A_TogglePOV] = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
        buttons[A_Inventory] = SDL_CONTROLLER_BUTTON_B;
        buttons[A_GameMenu] = SDL_CONTROLLER_BUTTON_START;
        if (platform != SDLUtil::DesktopPlatform::Mac)
            buttons[A_QuickSave] = SDL_CONTROLLER_BUTTON_GUIDE;

        return buttons;
    }

    bool isGameControllerButton(int button, SDLUtil::DesktopPlatform platform)
    {
        return platform != SDLUtil::DesktopPlatform::Mac || button != SDL_CONTROLLER_BUTTON_GUIDE;
    }
}
