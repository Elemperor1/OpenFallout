#include "messagebindings.hpp"

#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"

namespace OFLua
{
    sol::table initCoreMessageBoxBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // The box that is open: a table with its text (the name of the message and its words, which are also given
        // apart as `title` and `body`), the buttons the player can pick from (in the order they are listed), the form
        // of its message (as core.getFormId writes one) and a number that is another for each box that opens. A box
        // with no button of its own has `timed` and the `time` in seconds after which the game's box closes itself (a
        // script that draws it presses its button then). `lua` is true when the script that claimed the boxes draws
        // this one: a box that opened before the claim, or after its lease ran out, has the window manager's own
        // window, and a script must not draw it a second time. Nothing when no box is open.
        api["current"] = [lua]() -> sol::object {
            OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
            if (scripts == nullptr)
                return sol::nil;
            const auto open = scripts->openMessage();
            if (!open)
                return sol::nil;
            sol::table result(lua, sol::create);
            result["serial"] = open->mSerial;
            result["form"] = open->mForm;
            result["text"] = open->mText;
            result["title"] = open->mTitle;
            result["body"] = open->mBody;
            result["timed"] = open->mTimed;
            result["time"] = open->mTime;
            result["lua"] = open->mLua;
            sol::table buttons(lua, sol::create);
            int index = 1;
            for (const std::string& button : open->mButtons)
                buttons[index++] = button;
            result["buttons"] = buttons;
            return result;
        };
        // The script draws the boxes (true), or the window manager does (false, as before): with true no window is made
        // for the boxes that open, and the button that is picked with `press` is their answer. A script that cannot
        // show a box (its menu file is not there) gives them back with false, and an open box gets its window then.
        api["claim"] = [](bool drawn) {
            if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
                scripts->setMessagesDrawnByLua(drawn);
        };
        // Picks a button of the box, as a click on it would (counted from 1); false when no box is open or it has no
        // such button
        api["press"] = [](int button) {
            OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
            return scripts != nullptr && button >= 1
                && scripts->pressMessageButton(static_cast<std::size_t>(button - 1));
        };
        return LuaUtil::makeReadOnly(api);
    }
}
