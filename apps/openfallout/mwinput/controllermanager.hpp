#ifndef MWINPUT_MWCONTROLLERMANAGER_H
#define MWINPUT_MWCONTROLLERMANAGER_H

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <components/sdlutil/events.hpp>
#include <components/settings/settings.hpp>

#include "triggerfeel.hpp"

namespace OFInput
{
    class BindingsManager;
    class MouseManager;
    class TriggerDevice;

    class ControllerManager : public SDLUtil::ControllerListener
    {
    public:
        ControllerManager(BindingsManager* bindingsManager, MouseManager* mouseManager,
            const std::filesystem::path& userControllerBindingsFile,
            const std::filesystem::path& controllerBindingsFile);

        virtual ~ControllerManager();

        void update(float dt);

        void buttonPressed(int deviceID, const SDL_ControllerButtonEvent& arg) override;
        void buttonReleased(int deviceID, const SDL_ControllerButtonEvent& arg) override;
        void axisMoved(int deviceID, const SDL_ControllerAxisEvent& arg) override;
        void controllerAdded(int deviceID, const SDL_ControllerDeviceEvent& arg) override;
        void controllerRemoved(const SDL_ControllerDeviceEvent& arg) override;

        void touchpadMoved(int deviceId, const SDLUtil::TouchEvent& arg) override;
        void touchpadPressed(int deviceId, const SDLUtil::TouchEvent& arg) override;
        void touchpadReleased(int deviceId, const SDLUtil::TouchEvent& arg) override;

        void setJoystickLastUsed(bool enabled) { mJoystickLastUsed = enabled; }
        bool joystickLastUsed() const { return mJoystickLastUsed; }

        void setGuiCursorEnabled(bool enabled) { mGuiCursorEnabled = enabled; }

        void setGamepadGuiCursorEnabled(bool enabled) { mGamepadGuiCursorEnabled = enabled; }
        bool gamepadGuiCursorEnabled() const { return mGamepadGuiCursorEnabled; }

        // The pad holds a pick on a screen of a script (the D-pad moved it): A then acts on the pick, which the script
        // does, and is no click at the pointer. The stick moving the pointer lets the pick go (the last of the two
        // that was used is the one A goes to), and so does the end of the screens.
        void setPadPick(bool pick) { mPadPick = pick; }

        /// Vibrates the pad (both motors) with the strength (0 to 1) for the seconds; nothing without a pad, with the
        /// setting for it off, or with a strength or time of nothing
        void rumble(float left, float right, float seconds);
        bool padPick() const { return mPadPick; }

        /// The feel of the fire trigger (the right one) for the game as it is and the weapon in the player's hands
        /// (none: no weapon): the resistance of the weapon is set on a pad that can resist (a DualSense) while the
        /// weapon is out in play with the window in front, and cleared in menus, in the holster and out of focus
        void updateTriggers(const TriggerFeel::Situation& situation, const std::optional<TriggerFeel::Weapon>& weapon);

        /// The player's weapon fired or swung: the fire trigger of a pad with trigger motors (an Xbox pad) buzzes with
        /// the rumble of the weapon, when the vibration setting and the trigger effects are on
        void weaponFired();

        // The buttons that a script hears for the two triggers, which are axes of the pad (they are no SDL button)
        static constexpr int sLeftTriggerButton = 100;
        static constexpr int sRightTriggerButton = 101;

        float getAxisValue(SDL_GameControllerAxis axis) const; // returns value in range [-1, 1]
        bool isButtonPressed(SDL_GameControllerButton button) const;

        bool isGyroAvailable() const;
        std::array<float, 3> getGyroValues() const;

        std::string getControllerButtonIcon(int button);
        std::string getControllerAxisIcon(int axis);

    private:
        // Return true if GUI consumes input.
        bool gamepadToGuiControl(const SDL_ControllerButtonEvent& arg);
        bool gamepadToGuiControl(const SDL_ControllerAxisEvent& arg);

        void enableGyroSensor();

        // A trigger that is pressed or let go while a screen is open is told to the scripts as a button
        void tellTrigger(const SDL_ControllerAxisEvent& arg);

        // The sticks and triggers away from the middle, as a line of the log twice a second
        void logSticks(float dt);

        int getControllerType();

        // The triggers of the pad in use, made again when the pad changes
        TriggerDevice* triggerDevice();

        BindingsManager* mBindingsManager;
        MouseManager* mMouseManager;

        bool mGyroAvailable;
        bool mGamepadGuiCursorEnabled;
        bool mGuiCursorEnabled;
        bool mJoystickLastUsed;
        bool mGamepadMousePressed;
        bool mLeftTriggerGuiPressed;
        bool mRightTriggerGuiPressed;
        bool mPadPick = false;
        bool mPadPickPressed = false;
        float mSticksLogTime = 0.f;
        // Whether the scripts were told that the trigger is down (the press is told only while a screen is open, and
        // the release of a trigger they heard)
        bool mLeftTriggerTold = false;
        bool mRightTriggerTold = false;
        std::unique_ptr<TriggerDevice> mTriggerDevice;
        SDL_GameController* mTriggerController = nullptr;
        TriggerFeel::Planner mTriggerPlanner;
        TriggerFeel::Situation mTriggerSituation;
        TriggerFeel::Feel mTriggerFeel;
    };
}
#endif
