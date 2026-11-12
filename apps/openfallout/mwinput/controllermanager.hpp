#ifndef MWINPUT_MWCONTROLLERMANAGER_H
#define MWINPUT_MWCONTROLLERMANAGER_H

#include <array>
#include <filesystem>
#include <string>

#include <components/sdlutil/events.hpp>
#include <components/settings/settings.hpp>

namespace OFInput
{
    class BindingsManager;
    class MouseManager;

    class ControllerManager : public SDLUtil::ControllerListener
    {
    public:
        ControllerManager(BindingsManager* bindingsManager, MouseManager* mouseManager,
            const std::filesystem::path& userControllerBindingsFile,
            const std::filesystem::path& controllerBindingsFile);

        virtual ~ControllerManager() = default;

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
    };
}
#endif
