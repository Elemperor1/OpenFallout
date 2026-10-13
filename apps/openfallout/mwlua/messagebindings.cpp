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

        // The box that is open: a table with its text, the buttons the player can pick from (in the order they are
        // listed), the form of its message (as core.getFormId writes one) and a number that is another for each box that
        // opens. Nothing when no box is open.
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
            sol::table buttons(lua, sol::create);
            int index = 1;
            for (const std::string& button : open->mButtons)
                buttons[index++] = button;
            result["buttons"] = buttons;
            return result;
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
