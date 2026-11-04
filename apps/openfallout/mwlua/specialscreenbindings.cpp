#include "specialscreenbindings.hpp"

#include <array>
#include <memory>
#include <string>

#include <components/falloutscript/specialscreen.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "context.hpp"

namespace OFLua
{
    namespace
    {
        using FalloutScript::SpecialScreen::Outcome;
        using FalloutScript::SpecialScreen::Screen;

        sol::table outcomeTable(sol::state_view lua, const Outcome& outcome)
        {
            sol::table result(lua, sol::create);
            const char* sound = FalloutScript::SpecialScreen::soundName(outcome.mSound);
            if (*sound != '\0')
                result["sound"] = sound;
            if (outcome.mAttribute >= 0)
            {
                result["attribute"] = outcome.mAttribute;
                result["value"] = outcome.mValue;
            }
            if (outcome.mClose)
                result["close"] = true;
            return result;
        }
    }

    sol::table initCoreSpecialScreenBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        sol::usertype<Screen> type = lua.new_usertype<Screen>("SpecialScreen", sol::no_constructor);
        // The clip of the model runs while time passes
        type["update"] = [](Screen& self, float seconds) { self.update(seconds); };
        // Where the model is: the page, the sequence and the time in it, whether a page can be turned, the points
        type["view"] = [lua](Screen& self) {
            sol::table view(lua, sol::create);
            view["page"] = self.page();
            view["sequence"] = std::string(self.sequence());
            view["time"] = self.sequenceTime();
            view["ready"] = self.ready();
            view["total"] = self.total();
            view["left"] = self.left();
            view["limit"] = self.limit();
            view["cursor"] = self.cursor() + 1;
            return view;
        };
        // The sound that comes with the opening ("" for none)
        type["opening"] = [](Screen& self) { return std::string(FalloutScript::SpecialScreen::soundName(self.opening())); };
        type["value"] = [](Screen& self, int attribute) { return self.value(attribute - 1); };
        // The names of the shapes of the page that a pointer can find
        type["buttons"] = [lua](Screen& self) {
            sol::table list(lua, sol::create);
            int index = 1;
            for (const std::string& name : self.buttons())
                list[index++] = name;
            return list;
        };
        // Every shape the screen decides for, with whether it is shown and the picture it has ("" for its own)
        type["shapes"] = [lua](Screen& self) {
            sol::table list(lua, sol::create);
            int index = 1;
            for (const auto& shape : self.shapes())
            {
                sol::table row(lua, sol::create);
                row["shape"] = shape.mShape;
                row["visible"] = shape.mVisible;
                row["texture"] = shape.mTexture;
                list[index++] = row;
            }
            return list;
        };
        // The actions answer with a table: `sound` (the editor id, or none), `attribute` (1 to 7) and `value` when an
        // attribute changed, and `close` when the player is done
        const auto numbered = [](Outcome outcome) {
            // (the attributes are numbered from 1 in Lua)
            if (outcome.mAttribute >= 0)
                ++outcome.mAttribute;
            return outcome;
        };
        type["next"] = [lua, numbered](Screen& self) { return outcomeTable(lua, numbered(self.next())); };
        type["previous"] = [lua, numbered](Screen& self) { return outcomeTable(lua, numbered(self.previous())); };
        type["increase"] = [lua, numbered](Screen& self, sol::optional<bool> fromKey) {
            return outcomeTable(lua, numbered(self.increase(fromKey.value_or(false))));
        };
        type["decrease"] = [lua, numbered](Screen& self, sol::optional<bool> fromKey) {
            return outcomeTable(lua, numbered(self.decrease(fromKey.value_or(false))));
        };
        type["exit"] = [lua, numbered](Screen& self) { return outcomeTable(lua, numbered(self.exitAttempt())); };
        type["setPad"] = [](Screen& self, bool pad) { self.setPad(pad); };
        type["moveCursor"] = [lua, numbered](Screen& self, int delta) {
            return outcomeTable(lua, numbered(self.moveCursor(delta)));
        };
        type["press"] = [lua, numbered](Screen& self, const std::string& shape) {
            return outcomeTable(lua, numbered(self.press(shape)));
        };

        // A screen of `kind` ("tester" or "book") with `limit` points, the seven `values` (a list) and whether a pad is
        // in use
        api["new"] = [](const sol::table& options) {
            const std::string kind = options.get_or<std::string>("kind", "tester");
            std::array<int, Screen::attributeCount> values;
            values.fill(5);
            sol::object list = options["values"];
            if (list.is<sol::table>())
            {
                const sol::table table = list.as<sol::table>();
                for (size_t i = 0; i < values.size(); ++i)
                    values[i] = table.get_or<int>(i + 1, values[i]);
            }
            return std::make_shared<Screen>(kind == "book" ? FalloutScript::SpecialScreen::Kind::Book
                                                           : FalloutScript::SpecialScreen::Kind::Tester,
                options.get_or("limit", 35), values, options.get_or("pad", false));
        };
        return LuaUtil::makeReadOnly(api);
    }
}
