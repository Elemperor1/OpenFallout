#include "controllermanager.hpp"

#include <MyGUI_Button.h>
#include <MyGUI_InputManager.h>

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <components/debug/debuglog.hpp>
#include <components/esm/refid.hpp>
#include <components/files/conversion.hpp>
#include <components/sdlutil/sdlmappings.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwgui/windowbase.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "actions.hpp"
#include "bindingsmanager.hpp"
#include "controllerdefaults.hpp"
#include "inputlog.hpp"
#include "mousemanager.hpp"
#include "sdltriggerdevice.hpp"

namespace OFInput
{
    namespace
    {
        // A stick this near to the middle (of 32,768) is let go: the window that reads the sticks takes no notice of it
        constexpr int stickNeutralValue = 2000;
    }

    ControllerManager::ControllerManager(BindingsManager* bindingsManager, MouseManager* mouseManager,
        const std::filesystem::path& userControllerBindingsFile, const std::filesystem::path& controllerBindingsFile)
        : mBindingsManager(bindingsManager)
        , mMouseManager(mouseManager)
        , mGyroAvailable(false)
        , mGamepadGuiCursorEnabled(true)
        , mGuiCursorEnabled(true)
        , mJoystickLastUsed(false)
        , mGamepadMousePressed(false)
        , mLeftTriggerGuiPressed(false)
        , mRightTriggerGuiPressed(false)
    {
        if (!controllerBindingsFile.empty())
        {
            const int result
                = SDL_GameControllerAddMappingsFromFile(Files::pathToUnicodeString(controllerBindingsFile).c_str());
            if (result < 0)
                Log(Debug::Error) << "Failed to add game controller mappings from file \"" << controllerBindingsFile
                                  << "\": " << SDL_GetError();
        }

        if (!userControllerBindingsFile.empty())
        {
            const int result
                = SDL_GameControllerAddMappingsFromFile(Files::pathToUnicodeString(userControllerBindingsFile).c_str());
            if (result < 0)
                Log(Debug::Error) << "Failed to add game controller mappings from user file \""
                                  << userControllerBindingsFile << "\": " << SDL_GetError();
        }

        // Open all presently connected sticks
        const int numSticks = SDL_NumJoysticks();
        if (numSticks < 0)
            Log(Debug::Error) << "Failed to get number of joysticks: " << SDL_GetError();

        for (int i = 0; i < numSticks; i++)
        {
            if (SDL_IsGameController(i))
            {
                SDL_ControllerDeviceEvent evt;
                evt.which = i;
                static const int fakeDeviceID = 1;
                ControllerManager::controllerAdded(fakeDeviceID, evt);
                if (const char* name = SDL_GameControllerNameForIndex(i))
                    Log(Debug::Info) << "Detected game controller: " << name;
                else
                    Log(Debug::Warning) << "Detected game controller without a name: " << SDL_GetError();
            }
            else
            {
                if (const char* name = SDL_JoystickNameForIndex(i))
                    Log(Debug::Info) << "Detected unusable controller: " << name;
                else
                    Log(Debug::Warning) << "Detected unusable controller without a name: " << SDL_GetError();
            }
        }

        mBindingsManager->setJoystickDeadZone(Settings::input().mJoystickDeadZone);
    }

    ControllerManager::~ControllerManager()
    {
        // A DualSense keeps a resistance it was given: the game does not leave it behind, even on a pad that refused a
        // later report (a pad that never could resist takes the clear as nothing)
        if (mTriggerDevice != nullptr)
            mTriggerDevice->clear(TriggerFeel::Trigger::Right);
    }

    void ControllerManager::logSticks(float dt)
    {
        // Twice a second, while a stick or a trigger is away from the middle: where they were, and who had the pointer
        mSticksLogTime += dt;
        if (mSticksLogTime < 0.5f)
            return;
        mSticksLogTime = 0.f;
        const float lx = getAxisValue(SDL_CONTROLLER_AXIS_LEFTX);
        const float ly = getAxisValue(SDL_CONTROLLER_AXIS_LEFTY);
        const float rx = getAxisValue(SDL_CONTROLLER_AXIS_RIGHTX);
        const float ry = getAxisValue(SDL_CONTROLLER_AXIS_RIGHTY);
        const float lt = getAxisValue(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        const float rt = getAxisValue(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        constexpr float away = 0.15f;
        if (std::abs(lx) < away && std::abs(ly) < away && std::abs(rx) < away && std::abs(ry) < away && lt < away
            && rt < away)
            return;
        char text[160];
        std::snprintf(text, sizeof(text), "pad sticks left=(%.2f,%.2f) right=(%.2f,%.2f) triggers=(%.2f,%.2f) gui=%d pick=%d",
            lx, ly, rx, ry, lt, rt, OFBase::Environment::get().getWindowManager()->isGuiMode() ? 1 : 0, mPadPick ? 1 : 0);
        logInput(InputLogKind::Sticks, text);
    }

    void ControllerManager::update(float dt)
    {
        logSticks(dt);

        // The screens are shut: the pick of the pad goes with them
        if (mPadPick && !OFBase::Environment::get().getWindowManager()->isGuiMode())
            mPadPick = false;

        if (mGuiCursorEnabled && !(mJoystickLastUsed && !mGamepadGuiCursorEnabled))
        {
            float xAxis = mBindingsManager->getActionValue(A_MoveLeftRight) * 2.0f - 1.0f;
            float yAxis = mBindingsManager->getActionValue(A_MoveForwardBackward) * 2.0f - 1.0f;
            float zAxis = mBindingsManager->getActionValue(A_LookUpDown) * 2.0f - 1.0f;

            xAxis *= (1.5f - mBindingsManager->getActionValue(A_Use));
            yAxis *= (1.5f - mBindingsManager->getActionValue(A_Use));

            // We keep track of our own mouse position, so that moving the mouse while in
            // game mode does not move the position of the GUI cursor
            float uiScale = OFBase::Environment::get().getWindowManager()->getScalingFactor();
            const float gamepadCursorSpeed = Settings::input().mGamepadCursorSpeed;
            const float xMove = xAxis * dt * 1500.0f / uiScale * gamepadCursorSpeed;
            const float yMove = yAxis * dt * 1500.0f / uiScale * gamepadCursorSpeed;

            float mouseWheelMove = -zAxis * dt * 1500.0f;
            if (xMove != 0 || yMove != 0 || mouseWheelMove != 0)
            {
                // The stick has the pointer: A clicks where it is
                if (xMove != 0 || yMove != 0)
                    mPadPick = false;
                mMouseManager->injectMouseMove(xMove, yMove, mouseWheelMove);
                mMouseManager->warpMouse();
                OFBase::Environment::get().getWindowManager()->setCursorActive(true);
            }
        }

        if (!OFBase::Environment::get().getWindowManager()->isGuiMode()
            && OFBase::Environment::get().getStateManager()->getState() == OFBase::StateManager::State_Running
            && OFBase::Environment::get().getInputManager()->getControlSwitch("playercontrols"))
        {
            float xAxis = mBindingsManager->getActionValue(A_MoveLeftRight);
            float yAxis = mBindingsManager->getActionValue(A_MoveForwardBackward);
            if (xAxis != 0.5 || yAxis != 0.5)
            {
                mJoystickLastUsed = true;
                OFBase::Environment::get().getInputManager()->resetIdleTime();
            }
        }
    }

    void ControllerManager::buttonPressed(int deviceID, const SDL_ControllerButtonEvent& arg)
    {
        // macOS: the Home button opens the system's game overlay; the game neither acts on it nor binds it
        if (!isGameControllerButton(arg.button, SDLUtil::thisDesktopPlatform()))
            return;
        if (!Settings::input().mEnableController || mBindingsManager->isDetectingBindingState())
            return;

        // (who has the button is taken before the button is acted on, as for a key)
        OFBase::Environment::get().getLuaManager()->inputEvent(
            { OFBase::LuaManager::InputEvent::ControllerPressed, arg.button,
                OFBase::Environment::get().getWindowManager()->isConsoleMode()
                    || MyGUI::InputManager::getInstance().isModalAny() });

        {
            OFBase::WindowManager* winMgr = OFBase::Environment::get().getWindowManager();
            const char* name = SDL_GameControllerGetStringForButton(static_cast<SDL_GameControllerButton>(arg.button));
            logInput(InputLogKind::Button,
                std::string("pad button=") + (name != nullptr ? name : "?") + " gui=" + (winMgr->isGuiMode() ? "1" : "0")
                    + " pick=" + (mPadPick ? "1" : "0"));
        }

        mJoystickLastUsed = true;
        if (OFBase::Environment::get().getWindowManager()->isGuiMode())
        {
            // A screen of a script holds the pick that the D-pad made: A acts on it (the script has had the button)
            // and is no click where the pointer was left. A box of the engine that opens over the screen (a modal of
            // MyGUI) is not a screen of a script: A goes to it as it did.
            if (mPadPick && arg.button == SDL_CONTROLLER_BUTTON_A && !MyGUI::InputManager::getInstance().isModalAny())
            {
                mPadPickPressed = true;
                return;
            }

            if (gamepadToGuiControl(arg))
                return;

            if (mGamepadGuiCursorEnabled)
            {
                // Temporary mouse binding until keyboard controls are available:
                if (arg.button == SDL_CONTROLLER_BUTTON_A) // We'll pretend that A is left click.
                {
                    bool mousePressSuccess = mMouseManager->injectMouseButtonPress(SDL_BUTTON_LEFT);
                    mGamepadMousePressed = true;
                    if (MyGUI::InputManager::getInstance().getMouseFocusWidget())
                    {
                        MyGUI::Button* b
                            = MyGUI::InputManager::getInstance().getMouseFocusWidget()->castType<MyGUI::Button>(false);
                        if (b && b->getEnabled())
                            OFBase::Environment::get().getWindowManager()->playSound(
                                ESM::RefId::stringRefId("Menu Click"));
                    }

                    mBindingsManager->setPlayerControlsEnabled(!mousePressSuccess);
                }
            }
        }
        else
            mBindingsManager->setPlayerControlsEnabled(true);

        // esc, to leave initial movie screen
        auto kc = SDLUtil::sdlKeyToMyGUI(SDLK_ESCAPE);
        mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyPress(kc, 0));

        if (!OFBase::Environment::get().getInputManager()->controlsDisabled())
            mBindingsManager->controllerButtonPressed(deviceID, arg);
    }

    void ControllerManager::buttonReleased(int deviceID, const SDL_ControllerButtonEvent& arg)
    {
        if (!isGameControllerButton(arg.button, SDLUtil::thisDesktopPlatform()))
            return;
        if (mBindingsManager->isDetectingBindingState())
        {
            mBindingsManager->controllerButtonReleased(deviceID, arg);
            return;
        }

        if (Settings::input().mEnableController)
        {
            OFBase::Environment::get().getLuaManager()->inputEvent(
                { OFBase::LuaManager::InputEvent::ControllerReleased, arg.button });
        }

        // The A that acted on a pick was no press of the pointer, so its release is none either. The latch is let go
        // where the release is seen, also when the controls are off, so that a release that was missed cannot swallow
        // the release of a later A that did press the pointer.
        const bool pickRelease = arg.button == SDL_CONTROLLER_BUTTON_A && mPadPickPressed;
        if (arg.button == SDL_CONTROLLER_BUTTON_A)
            mPadPickPressed = false;

        if (!Settings::input().mEnableController || OFBase::Environment::get().getInputManager()->controlsDisabled())
            return;

        mJoystickLastUsed = true;
        if (pickRelease)
            return;
        if (OFBase::Environment::get().getWindowManager()->isGuiMode())
        {
            if (mGamepadGuiCursorEnabled && (!Settings::gui().mControllerMenus || mGamepadMousePressed))
            {
                // Temporary mouse binding until keyboard controls are available:
                if (arg.button == SDL_CONTROLLER_BUTTON_A) // We'll pretend that A is left click.
                {
                    bool mousePressSuccess = mMouseManager->injectMouseButtonRelease(SDL_BUTTON_LEFT);
                    mGamepadMousePressed = false;
                    if (mBindingsManager->isDetectingBindingState()) // If the player just triggered binding, don't let
                                                                     // button release bind.
                        return;

                    mBindingsManager->setPlayerControlsEnabled(!mousePressSuccess);
                }
            }
        }
        else
            mBindingsManager->setPlayerControlsEnabled(true);

        // esc, to leave initial movie screen
        auto kc = SDLUtil::sdlKeyToMyGUI(SDLK_ESCAPE);
        mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyRelease(kc));

        mBindingsManager->controllerButtonReleased(deviceID, arg);
    }

    void ControllerManager::tellTrigger(const SDL_ControllerAxisEvent& arg)
    {
        const bool left = arg.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT;
        if (!left && arg.axis != SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            return;

        const int press = Settings::gui().mControllerTriggerPressThreshold;
        const int rawRelease = Settings::gui().mControllerTriggerReleaseThreshold;
        const int release = std::clamp(rawRelease, 0, press - 1);
        bool& told = left ? mLeftTriggerTold : mRightTriggerTold;
        const int button = left ? sLeftTriggerButton : sRightTriggerButton;
        OFBase::WindowManager* winMgr = OFBase::Environment::get().getWindowManager();

        if (arg.value >= press && !told && winMgr->isGuiMode())
        {
            told = true;
            logInput(InputLogKind::Button,
                std::string("pad button=") + (left ? "lefttrigger" : "righttrigger") + " gui=1 pick=" + (mPadPick ? "1" : "0"));
            OFBase::Environment::get().getLuaManager()->inputEvent(
                { OFBase::LuaManager::InputEvent::ControllerPressed, button,
                    winMgr->isConsoleMode() || MyGUI::InputManager::getInstance().isModalAny() });
        }
        else if (arg.value <= release && told)
        {
            told = false;
            OFBase::Environment::get().getLuaManager()->inputEvent(
                { OFBase::LuaManager::InputEvent::ControllerReleased, button });
        }
    }

    void ControllerManager::axisMoved(int deviceID, const SDL_ControllerAxisEvent& arg)
    {
        if (mBindingsManager->isDetectingBindingState())
        {
            mBindingsManager->controllerAxisMoved(deviceID, arg);
            return;
        }

        if (!Settings::input().mEnableController || OFBase::Environment::get().getInputManager()->controlsDisabled())
            return;

        // (a trigger is a button to the screens of the scripts, as the D-pad and the bumpers are)
        tellTrigger(arg);

        mJoystickLastUsed = true;
        if (OFBase::Environment::get().getWindowManager()->isGuiMode())
        {
            if (gamepadToGuiControl(arg))
            {
                // A stick that is let go under a window that takes its events (or under the box of a script, whose
                // pushes the bindings were given) would keep the value it had, and the pointer or the player would go
                // on by itself when the window is gone. The bindings are told of the rest.
                const bool stick = arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY
                    || arg.axis == SDL_CONTROLLER_AXIS_RIGHTX || arg.axis == SDL_CONTROLLER_AXIS_RIGHTY;
                // A trigger that is let go under a window is told to the bindings the same way: one that was pressed
                // while a message was open would stay pressed. Only its rest is told (the release threshold of the
                // windows can be well above what a binding takes as pressed, and a half-pressed trigger must not
                // press one while a window owns it)
                const bool trigger
                    = arg.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || arg.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
                if ((stick || trigger) && std::abs(arg.value) < stickNeutralValue)
                    mBindingsManager->controllerAxisMoved(deviceID, arg);
                return;
            }
        }
        else if (mBindingsManager->actionIsActive(A_TogglePOV)
            && (arg.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT || arg.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT))
        {
            // Preview Mode Gamepad Zooming; do not propagate to mBindingsManager
            return;
        }
        mBindingsManager->controllerAxisMoved(deviceID, arg);
    }

    void ControllerManager::controllerAdded(int deviceID, const SDL_ControllerDeviceEvent& arg)
    {
        mBindingsManager->controllerAdded(deviceID, arg);
        enableGyroSensor();
    }

    void ControllerManager::controllerRemoved(const SDL_ControllerDeviceEvent& arg)
    {
        mBindingsManager->controllerRemoved(arg);
        // The pad in use may be gone: its triggers are found again on the next frame
        mTriggerDevice.reset();
        mTriggerController = nullptr;
        mTriggerPlanner.forget();
    }

    bool ControllerManager::gamepadToGuiControl(const SDL_ControllerButtonEvent& arg)
    {
        OFBase::WindowManager* winMgr = OFBase::Environment::get().getWindowManager();

        // A message box that a script draws is modal: the buttons do not reach the screens beneath it (the script has
        // had the button already), as the keys do not (ActionManager::executeAction)
        if (const OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
            scripts != nullptr && scripts->luaMessageOpen())
            return true;

        // The pause menu that a script draws takes the buttons: they do not reach the window manager's own menu, which
        // is not shown then. B leaves a screen of it as Escape does, and closes the menu when the script has no screen
        // to go back from. A dialog of the window manager that is open over it (the one that tells a save failed) is
        // modal and has the buttons first.
        if (winMgr->getMode() == OFGui::GM_MainMenu && winMgr->isMainMenuDrawnByLua()
            && !MyGUI::InputManager::getInstance().isModalAny()
            && (arg.button != SDL_CONTROLLER_BUTTON_B || winMgr->isMainMenuEscapeLua()))
            return true;

        if (Settings::gui().mControllerMenus)
        {
            // Update cursor state.
            bool treatAsMouse = winMgr->getCursorVisible();
            winMgr->setCursorActive(false);

            OFGui::WindowBase* topWin = winMgr->getActiveControllerWindow();
            if (topWin && topWin->isVisible())
            {
                // When the inventory tooltip is visible, we don't actually want the A button to
                // act like a mouse button; it should act normally.
                if (treatAsMouse && arg.button == SDL_CONTROLLER_BUTTON_A && winMgr->getControllerTooltipVisible())
                    treatAsMouse = false;

                mGamepadGuiCursorEnabled = topWin->isGamepadCursorAllowed();

                // Fall through to mouse click
                if (mGamepadGuiCursorEnabled && treatAsMouse && arg.button == SDL_CONTROLLER_BUTTON_A)
                    return false;

                if (topWin->onControllerButtonEvent(arg))
                    return true;
            }
        }

        // Presumption of GUI mode will be removed in the future.
        // MyGUI KeyCodes *may* change.
        MyGUI::KeyCode key = MyGUI::KeyCode::None;
        switch (arg.button)
        {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                key = MyGUI::KeyCode::ArrowUp;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                key = MyGUI::KeyCode::ArrowRight;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                key = MyGUI::KeyCode::ArrowDown;
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                key = MyGUI::KeyCode::ArrowLeft;
                break;
            case SDL_CONTROLLER_BUTTON_A:
                // If we are using the joystick as a GUI mouse, A must be handled via mouse.
                if (mGamepadGuiCursorEnabled)
                    return false;
                key = MyGUI::KeyCode::Space;
                break;
            case SDL_CONTROLLER_BUTTON_B:
                if (MyGUI::InputManager::getInstance().isModalAny())
                    winMgr->exitCurrentModal();
                else
                    winMgr->exitCurrentGuiMode();
                return true;
            case SDL_CONTROLLER_BUTTON_X:
                key = MyGUI::KeyCode::Semicolon;
                break;
            case SDL_CONTROLLER_BUTTON_Y:
                key = MyGUI::KeyCode::Apostrophe;
                break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                MyGUI::InputManager::getInstance().injectKeyPress(MyGUI::KeyCode::LeftShift);
                winMgr->injectKeyPress(MyGUI::KeyCode::Tab, 0, false);
                MyGUI::InputManager::getInstance().injectKeyRelease(MyGUI::KeyCode::LeftShift);
                return true;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                OFBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::Tab, 0, false);
                return true;
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                mGamepadGuiCursorEnabled = !mGamepadGuiCursorEnabled;
                winMgr->setCursorActive(mGamepadGuiCursorEnabled);
                return true;
            default:
                return false;
        }

        // Some keys will work even when Text Input windows/modals are in focus.
        if (SDL_IsTextInputActive())
            return false;

        winMgr->injectKeyPress(key, 0, false);
        return true;
    }

    bool ControllerManager::gamepadToGuiControl(const SDL_ControllerAxisEvent& arg)
    {
        const int triggerPressThreshold = Settings::gui().mControllerTriggerPressThreshold;
        const int rawTriggerReleaseThreshold = Settings::gui().mControllerTriggerReleaseThreshold;
        const int triggerReleaseThreshold = std::clamp(rawTriggerReleaseThreshold, 0, triggerPressThreshold - 1);

        auto handleTriggerPress = [&](Sint16 value, bool& triggerGuiPressed, const auto& onPress) {
            if (value >= triggerPressThreshold && !triggerGuiPressed)
            {
                onPress();
                triggerGuiPressed = true;
            }
            else if (value <= triggerReleaseThreshold)
            {
                triggerGuiPressed = false;
            }
        };

        // A message box that a script draws is modal: the triggers do not cycle the windows beneath it and the sticks
        // do not scroll them, as the buttons do not. A trigger is still followed, so one let go under the box can cycle
        // the windows again when it is gone.
        // Every axis still reaches the bindings, though: the pointer is moved by the value that the bindings hold for
        // the left stick (ControllerManager::update), and a stick that was pushed when the box opened and let go under
        // it would keep the value it had then, so the pointer drifted on by itself and the player walked on by itself
        // after the box was closed (the Mac hour of round 13P: "when a popup opens while my cursor is moving, it stays
        // in that moving state")
        if (const OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
            scripts != nullptr && scripts->luaMessageOpen())
        {
            if (arg.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
                handleTriggerPress(arg.value, mRightTriggerGuiPressed, [] {});
            else if (arg.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
                handleTriggerPress(arg.value, mLeftTriggerGuiPressed, [] {});
            return false;
        }

        OFBase::WindowManager* winMgr = OFBase::Environment::get().getWindowManager();

        if (Settings::gui().mControllerMenus)
        {
            // Left and right triggers toggle through open GUI windows.
            if (arg.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            {
                handleTriggerPress(
                    arg.value, mRightTriggerGuiPressed, [&] { winMgr->cycleActiveControllerWindow(true); });
                return true;
            }
            else if (arg.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
            {
                handleTriggerPress(
                    arg.value, mLeftTriggerGuiPressed, [&] { winMgr->cycleActiveControllerWindow(false); });
                return true;
            }

            OFGui::WindowBase* topWin = winMgr->getActiveControllerWindow();
            if (topWin && topWin->isVisible())
            {
                // Update cursor state
                mGamepadGuiCursorEnabled = topWin->isGamepadCursorAllowed();
                if (!mGamepadGuiCursorEnabled)
                    winMgr->setCursorActive(false);

                // Deadzone check
                if (std::abs(arg.value) < stickNeutralValue)
                    return !mGamepadGuiCursorEnabled;

                if (mGamepadGuiCursorEnabled
                    && (arg.axis == SDL_CONTROLLER_AXIS_LEFTX || arg.axis == SDL_CONTROLLER_AXIS_LEFTY))
                {
                    // Treat the left stick like a cursor, which is the default behavior.
                    winMgr->setControllerTooltipVisible(false);
                    winMgr->setCursorVisible(true);
                    return false;
                }

                // Some windows have a specific widget to scroll with the right stick. Move the mouse there.
                if (arg.axis == SDL_CONTROLLER_AXIS_RIGHTY && topWin->getControllerScrollWidget() != nullptr)
                {
                    mMouseManager->warpMouseToWidget(topWin->getControllerScrollWidget());
                    winMgr->setCursorVisible(false);
                }

                if (topWin->onControllerThumbstickEvent(arg))
                {
                    // Window handled the event.
                    return true;
                }
                else if (arg.axis == SDL_CONTROLLER_AXIS_RIGHTX || arg.axis == SDL_CONTROLLER_AXIS_RIGHTY)
                {
                    // Only right-stick scroll if mouse is visible or there's a widget to scroll.
                    return !winMgr->getCursorVisible() && topWin->getControllerScrollWidget() == nullptr;
                }
            }
        }

        switch (arg.axis)
        {
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                handleTriggerPress(arg.value, mRightTriggerGuiPressed,
                    [&] { winMgr->injectKeyPress(MyGUI::KeyCode::Minus, 0, false); });
                break;
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                handleTriggerPress(arg.value, mLeftTriggerGuiPressed,
                    [&] { winMgr->injectKeyPress(MyGUI::KeyCode::Equals, 0, false); });
                break;
            case SDL_CONTROLLER_AXIS_LEFTX:
            case SDL_CONTROLLER_AXIS_LEFTY:
            case SDL_CONTROLLER_AXIS_RIGHTX:
            case SDL_CONTROLLER_AXIS_RIGHTY:
                // If we are using the joystick as a GUI mouse, process mouse movement elsewhere.
                if (mGamepadGuiCursorEnabled)
                    return false;
                break;
            default:
                return false;
        }

        return true;
    }

    void ControllerManager::rumble(float left, float right, float seconds)
    {
        if (!Settings::input().mGamepadRumble || (left <= 0.f && right <= 0.f) || seconds <= 0.f)
            return;
        SDL_GameController* cntrl = mBindingsManager->getControllerOrNull();
        if (cntrl == nullptr)
            return;
        const auto leftMotor = static_cast<Uint16>(std::clamp(left, 0.f, 1.f) * 65535.f);
        const auto rightMotor = static_cast<Uint16>(std::clamp(right, 0.f, 1.f) * 65535.f);
        const auto milliseconds = static_cast<Uint32>(std::clamp(seconds, 0.f, 10.f) * 1000.f);
        SDL_GameControllerRumble(cntrl, leftMotor, rightMotor, milliseconds);
    }

    TriggerDevice* ControllerManager::triggerDevice()
    {
        SDL_GameController* cntrl = mBindingsManager->getControllerOrNull();
        if (cntrl != mTriggerController)
        {
            mTriggerController = cntrl;
            mTriggerDevice = cntrl != nullptr ? std::make_unique<SdlTriggerDevice>(cntrl) : nullptr;
            mTriggerPlanner.forget();
        }
        return mTriggerDevice.get();
    }

    void ControllerManager::updateTriggers(
        const TriggerFeel::Situation& situation, const std::optional<TriggerFeel::Weapon>& weapon)
    {
        mTriggerSituation = situation;
        mTriggerFeel = weapon ? TriggerFeel::forWeapon(*weapon) : TriggerFeel::Feel{};
        TriggerDevice* device = triggerDevice();
        if (device != nullptr)
            driveTriggers(mTriggerPlanner, *device, situation, mTriggerFeel.mResistance);
    }

    void ControllerManager::weaponFired()
    {
        if (!Settings::input().mGamepadRumble || !TriggerFeel::Planner::pulseAllowed(mTriggerSituation))
            return;
        if (TriggerDevice* device = triggerDevice(); device != nullptr && device->pulseSupported())
            device->pulse(TriggerFeel::Trigger::Right, mTriggerFeel.mShot);
    }

    float ControllerManager::getAxisValue(SDL_GameControllerAxis axis) const
    {
        SDL_GameController* cntrl = mBindingsManager->getControllerOrNull();
        constexpr float axisMaxAbsoluteValue = 32768;
        if (cntrl != nullptr)
            return SDL_GameControllerGetAxis(cntrl, axis) / axisMaxAbsoluteValue;
        return 0;
    }

    bool ControllerManager::isButtonPressed(SDL_GameControllerButton button) const
    {
        SDL_GameController* cntrl = mBindingsManager->getControllerOrNull();
        if (cntrl)
            return SDL_GameControllerGetButton(cntrl, button) > 0;
        else
            return false;
    }

    void ControllerManager::enableGyroSensor()
    {
        mGyroAvailable = false;
        SDL_GameController* cntrl = mBindingsManager->getControllerOrNull();
        if (!cntrl)
            return;
        if (!SDL_GameControllerHasSensor(cntrl, SDL_SENSOR_GYRO))
            return;
        if (const int result = SDL_GameControllerSetSensorEnabled(cntrl, SDL_SENSOR_GYRO, SDL_TRUE); result < 0)
        {
            Log(Debug::Error) << "Failed to enable game controller sensor: " << SDL_GetError();
            return;
        }
        mGyroAvailable = true;
    }

    bool ControllerManager::isGyroAvailable() const
    {
        return mGyroAvailable;
    }

    std::array<float, 3> ControllerManager::getGyroValues() const
    {
        float gyro[3] = { 0.f };
        SDL_GameController* cntrl = mBindingsManager->getControllerOrNull();
        if (cntrl && mGyroAvailable)
        {
            const int result = SDL_GameControllerGetSensorData(cntrl, SDL_SENSOR_GYRO, gyro, 3);
            if (result < 0)
                Log(Debug::Error) << "Failed to get game controller sensor data: " << SDL_GetError();
        }
        return std::array<float, 3>({ gyro[0], gyro[1], gyro[2] });
    }

    int ControllerManager::getControllerType()
    {
        SDL_GameController* cntrl = mBindingsManager->getControllerOrNull();
        if (cntrl)
            return SDL_GameControllerGetType(cntrl);
        return 0;
    }

    std::string ControllerManager::getControllerButtonIcon(int button)
    {
        int controllerType = ControllerManager::getControllerType();

        bool isXbox = controllerType == SDL_CONTROLLER_TYPE_XBOX360 || controllerType == SDL_CONTROLLER_TYPE_XBOXONE;
        bool isPsx = controllerType == SDL_CONTROLLER_TYPE_PS3 || controllerType == SDL_CONTROLLER_TYPE_PS4
            || controllerType == SDL_CONTROLLER_TYPE_PS5;
        bool isSwitch = controllerType == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO;

        switch (button)
        {
            case SDL_CONTROLLER_BUTTON_A:
                if (isPsx)
                    return "textures/omw_psx_button_x.dds";
                return "textures/omw_steam_button_a.dds";
            case SDL_CONTROLLER_BUTTON_B:
                if (isPsx)
                    return "textures/omw_psx_button_circle.dds";
                return "textures/omw_steam_button_b.dds";
            case SDL_CONTROLLER_BUTTON_BACK:
                return "textures/omw_steam_button_view.dds";
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
                if (isPsx)
                    return "textures/omw_psx_button_dpad.dds";
                return "textures/omw_steam_button_dpad.dds";
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                if (isXbox)
                    return "textures/omw_xbox_button_lb.dds";
                else if (isSwitch)
                    return "textures/omw_switch_button_l.dds";
                return "textures/omw_steam_button_l1.dds";
            case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                return "textures/omw_steam_button_l3.dds";
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                if (isXbox)
                    return "textures/omw_xbox_button_rb.dds";
                else if (isSwitch)
                    return "textures/omw_switch_button_r.dds";
                return "textures/omw_steam_button_r1.dds";
            case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                return "textures/omw_steam_button_r3.dds";
            case SDL_CONTROLLER_BUTTON_START:
                return "textures/omw_steam_button_menu.dds";
            case SDL_CONTROLLER_BUTTON_X:
                if (isPsx)
                    return "textures/omw_psx_button_square.dds";
                return "textures/omw_steam_button_x.dds";
            case SDL_CONTROLLER_BUTTON_Y:
                if (isPsx)
                    return "textures/omw_psx_button_triangle.dds";
                return "textures/omw_steam_button_y.dds";
            case SDL_CONTROLLER_BUTTON_GUIDE:
            case SDL_CONTROLLER_BUTTON_MISC1:
            case SDL_CONTROLLER_BUTTON_PADDLE1:
            case SDL_CONTROLLER_BUTTON_PADDLE2:
            case SDL_CONTROLLER_BUTTON_PADDLE3:
            case SDL_CONTROLLER_BUTTON_PADDLE4:
            case SDL_CONTROLLER_BUTTON_TOUCHPAD:
            default:
                return {};
        }
    }

    std::string ControllerManager::getControllerAxisIcon(int axis)
    {
        int controllerType = ControllerManager::getControllerType();

        bool isXbox = controllerType == SDL_CONTROLLER_TYPE_XBOX360 || controllerType == SDL_CONTROLLER_TYPE_XBOXONE;
        bool isSwitch = controllerType == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO;

        switch (axis)
        {
            case SDL_CONTROLLER_AXIS_LEFTX:
            case SDL_CONTROLLER_AXIS_LEFTY:
                return "textures/omw_steam_button_lstick.dds";
            case SDL_CONTROLLER_AXIS_RIGHTX:
            case SDL_CONTROLLER_AXIS_RIGHTY:
                return "textures/omw_steam_button_rstick.dds";
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                if (isXbox)
                    return "textures/omw_xbox_button_lt.dds";
                else if (isSwitch)
                    return "textures/omw_switch_button_zl.dds";
                return "textures/omw_steam_button_l2.dds";
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                if (isXbox)
                    return "textures/omw_xbox_button_rt.dds";
                else if (isSwitch)
                    return "textures/omw_switch_button_zr.dds";
                return "textures/omw_steam_button_r2.dds";
            default:
                return {};
        }
    }

    void ControllerManager::touchpadMoved(int deviceId, const SDLUtil::TouchEvent& arg)
    {
        OFBase::Environment::get().getLuaManager()->inputEvent({ OFBase::LuaManager::InputEvent::TouchMoved, arg });
    }

    void ControllerManager::touchpadPressed(int deviceId, const SDLUtil::TouchEvent& arg)
    {
        OFBase::Environment::get().getLuaManager()->inputEvent({ OFBase::LuaManager::InputEvent::TouchPressed, arg });
    }

    void ControllerManager::touchpadReleased(int deviceId, const SDLUtil::TouchEvent& arg)
    {
        OFBase::Environment::get().getLuaManager()->inputEvent({ OFBase::LuaManager::InputEvent::TouchReleased, arg });
    }
}
