#include "apps/openfallout/mwinput/actions.hpp"
#include "apps/openfallout/mwinput/controllerdefaults.hpp"

#include <SDL_gamecontroller.h>

#include <gtest/gtest.h>

namespace
{
    using namespace OFInput;
    using SDLUtil::DesktopPlatform;

    TEST(OFInputControllerDefaultsTest, onTheMacTheHomeButtonIsBoundToNothing)
    {
        // The Home (Guide) button opens the system's game overlay on macOS
        for (const auto& [action, button] : defaultControllerButtons(DesktopPlatform::Mac))
            EXPECT_NE(button, SDL_CONTROLLER_BUTTON_GUIDE) << "action " << action;
        EXPECT_EQ(defaultControllerButtons(DesktopPlatform::Mac).count(A_QuickSave), 0u);
    }

    TEST(OFInputControllerDefaultsTest, elsewhereTheHomeButtonQuickSavesAsBefore)
    {
        const std::map<int, int> buttons = defaultControllerButtons(DesktopPlatform::Other);
        ASSERT_EQ(buttons.count(A_QuickSave), 1u);
        EXPECT_EQ(buttons.at(A_QuickSave), SDL_CONTROLLER_BUTTON_GUIDE);
    }

    TEST(OFInputControllerDefaultsTest, theOtherButtonsAreTheSameEverywhere)
    {
        std::map<int, int> mac = defaultControllerButtons(DesktopPlatform::Mac);
        std::map<int, int> other = defaultControllerButtons(DesktopPlatform::Other);
        other.erase(A_QuickSave);
        EXPECT_EQ(mac, other);
        EXPECT_EQ(mac.at(A_Activate), SDL_CONTROLLER_BUTTON_A);
        EXPECT_EQ(mac.at(A_GameMenu), SDL_CONTROLLER_BUTTON_START);
    }

    TEST(OFInputControllerDefaultsTest, onTheMacTheHomeButtonIsNotTheGames)
    {
        EXPECT_FALSE(isGameControllerButton(SDL_CONTROLLER_BUTTON_GUIDE, DesktopPlatform::Mac));
        EXPECT_TRUE(isGameControllerButton(SDL_CONTROLLER_BUTTON_A, DesktopPlatform::Mac));
        EXPECT_TRUE(isGameControllerButton(SDL_CONTROLLER_BUTTON_START, DesktopPlatform::Mac));
    }

    TEST(OFInputControllerDefaultsTest, elsewhereEveryButtonIsTheGames)
    {
        EXPECT_TRUE(isGameControllerButton(SDL_CONTROLLER_BUTTON_GUIDE, DesktopPlatform::Other));
        EXPECT_TRUE(isGameControllerButton(SDL_CONTROLLER_BUTTON_A, DesktopPlatform::Other));
    }
}
