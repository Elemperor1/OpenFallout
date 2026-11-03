#include "keyboardmanager.hpp"

#include <cctype>

#include <MyGUI_InputManager.h>

#include <components/sdlutil/sdlmappings.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "actions.hpp"
#include "bindingsmanager.hpp"

namespace OFInput
{
    KeyboardManager::KeyboardManager(BindingsManager* bindingsManager)
        : mBindingsManager(bindingsManager)
    {
    }

    void KeyboardManager::textInput(const SDL_TextInputEvent& arg)
    {
        MyGUI::UString ustring(&arg.text[0]);
        MyGUI::UString::utf32string utf32string = ustring.asUTF32();
        for (MyGUI::UString::utf32string::const_iterator it = utf32string.begin(); it != utf32string.end(); ++it)
            MyGUI::InputManager::getInstance().injectKeyPress(MyGUI::KeyCode::None, *it);
    }

    void KeyboardManager::keyPressed(const SDL_KeyboardEvent& arg)
    {
        // HACK: to make default keybinding for the console work without printing an extra "^" upon closing
        // This assumes that SDL_TextInput events always come *after* the key event
        // (which is somewhat reasonable, and hopefully true for all SDL platforms)
        auto kc = SDLUtil::sdlKeyToMyGUI(arg.keysym.sym);
        // Who has the key is taken now: what the key does (Escape closes the console or a dialog) comes before the
        // scripts are given it
        const bool taken = OFBase::Environment::get().getWindowManager()->isConsoleMode()
            || MyGUI::InputManager::getInstance().isModalAny();
        if (mBindingsManager->getKeyBinding(A_Console) == arg.keysym.scancode
            // HACK: allow upper case variant of console keybinding.
            && (arg.keysym.mod & KMOD_SHIFT) == 0 && OFBase::Environment::get().getWindowManager()->isConsoleMode())
            SDL_StopTextInput();

        bool consumed = SDL_IsTextInputActive() && // Little trick to check if key is printable
            (!(SDLK_SCANCODE_MASK & arg.keysym.sym) &&
                // Don't trust isprint for symbols outside the extended ASCII range
                ((kc == MyGUI::KeyCode::None && arg.keysym.sym > 0xff)
                    || (arg.keysym.sym >= 0 && arg.keysym.sym <= 255 && std::isprint(arg.keysym.sym))));
        // Tab is the key of the Pip-Boy, which it closes as it opens it: while the Pip-Boy is the screen (the mode of
        // the interface, in a game that has one: the inventory window of the engine keeps its key navigation) the key
        // navigation of the windows would take it (it moves the key focus), so it is not given to them then; in any
        // other window, with a text that is typed and with a window of the engine that has the keys it does what it did
        const bool pipboyKey = kc == MyGUI::KeyCode::Tab && !taken && !SDL_IsTextInputActive()
            && OFBase::Environment::get().hasFalloutScripts()
            && OFBase::Environment::get().getWindowManager()->getMode() == OFGui::GM_Inventory
            && mBindingsManager->getKeyBinding(A_Inventory) == arg.keysym.scancode;
        if (kc != MyGUI::KeyCode::None && !mBindingsManager->isDetectingBindingState() && !pipboyKey)
        {
            if (OFBase::Environment::get().getWindowManager()->injectKeyPress(kc, 0, arg.repeat))
                consumed = true;
            mBindingsManager->setPlayerControlsEnabled(!consumed);
        }

        if (arg.repeat)
            return;

        OFBase::InputManager* input = OFBase::Environment::get().getInputManager();
        if (!input->controlsDisabled() && !consumed)
            mBindingsManager->keyPressed(arg);

        // Tab moves the key focus of the windows, which takes it from the scripts: but the screen that a script draws
        // (V.A.T.S.) cycles its choices with it, so it reaches the scripts however the windows took it
        if (!consumed || kc == MyGUI::KeyCode::Tab)
        {
            OFBase::Environment::get().getLuaManager()->inputEvent(
                { OFBase::LuaManager::InputEvent::KeyPressed, arg.keysym, taken });
        }

        input->setJoystickLastUsed(false);
    }

    void KeyboardManager::keyReleased(const SDL_KeyboardEvent& arg)
    {
        OFBase::Environment::get().getInputManager()->setJoystickLastUsed(false);
        auto kc = SDLUtil::sdlKeyToMyGUI(arg.keysym.sym);

        if (!mBindingsManager->isDetectingBindingState())
            mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyRelease(kc));
        mBindingsManager->keyReleased(arg);
        OFBase::Environment::get().getLuaManager()->inputEvent(
            { OFBase::LuaManager::InputEvent::KeyReleased, arg.keysym });
    }
}
