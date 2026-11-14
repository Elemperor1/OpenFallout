#include "falloutworldbindings.hpp"

#include <charconv>
#include <cstdio>
#include <cstdint>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/falloutscript/commandtable.hpp>
#include <components/falloutscript/interpreter.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"

namespace OFLua
{
    namespace
    {
        /// The number a form id has as text from Lua: "0x" and the number in hexadecimal, or "FormId:0x" and the number;
        /// false for anything else
        bool parseForm(std::string_view text, FalloutScript::FormId& result)
        {
            if (text.empty())
                return false;
            if (text.starts_with("0x"))
            {
                std::uint32_t value = 0;
                const char* end = text.data() + text.size();
                const auto [stop, error] = std::from_chars(text.data() + 2, end, value, 16);
                if (error != std::errc() || stop != end)
                    return false;
                result = value;
                return true;
            }
            if (!text.starts_with("FormId:"))
                return false;
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                return false;
            result = form->toUint32();
            return true;
        }

        /// What the calls have answered, by the number the call was given
        struct Answers
        {
            std::mutex mMutex;
            std::map<int, double> mDone;
            int mNext = 1;
        };

        Answers& answers()
        {
            static Answers instance;
            return instance;
        }
    }

    sol::table initCoreWorldCommandBindings(const Context& context)
    {
        sol::state_view lua = context.mLua->unsafeState();
        sol::table api(lua, sol::create);

        api["available"] = []() { return OFBase::Environment::get().getFalloutScripts() != nullptr; };

        // Runs the command `name` on the thread of the frame, on the reference (a form id as text, "" or nil for none)
        // with the arguments (numbers, forms as text, or text), the way a condition would call it. Gives the number of
        // the call: its answer is asked with `result` once the frame has run it.
        api["run"] = [context](std::string name, sol::optional<std::string> reference,
                         sol::optional<sol::table> arguments) -> int {
            OFScript::FalloutScripts* host = OFBase::Environment::get().getFalloutScripts();
            if (host == nullptr)
                throw std::runtime_error("core.worldCommands is only available with the content of Fallout 3 and New Vegas");
            const FalloutScript::CommandInfo* info = host->interpreter().commands().find(name);
            if (info == nullptr)
                throw std::runtime_error("\"" + name + "\" is not a command of this game");
            FalloutScript::FormId target = 0;
            if (reference && !reference->empty() && !parseForm(*reference, target))
                throw std::runtime_error("\"" + *reference + "\" is not a form id");
            std::vector<FalloutScript::Value> values;
            if (arguments)
                for (std::size_t i = 1; i <= arguments->size(); ++i)
                {
                    const sol::object item = (*arguments)[i];
                    FalloutScript::Value value;
                    if (item.is<double>())
                        value.mNumber = item.as<double>();
                    else if (item.is<std::string>())
                    {
                        value.mIsText = true;
                        value.mText = item.as<std::string>();
                        FalloutScript::FormId form = 0;
                        if (parseForm(value.mText, form))
                            value.mNumber = form;
                    }
                    else if (item.is<bool>())
                        value.mNumber = item.as<bool>() ? 1.0 : 0.0;
                    values.push_back(std::move(value));
                }
            int serial = 0;
            {
                const std::lock_guard lock(answers().mMutex);
                serial = answers().mNext++;
            }
            context.mLuaManager->addAction([serial, opcode = info->mOpcode, target, values = std::move(values)]() mutable {
                OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
                double answer = 0;
                if (scripts != nullptr)
                    answer = scripts->interpreter().call(opcode, target, target != 0, std::move(values));
                const std::lock_guard lock(answers().mMutex);
                answers().mDone[serial] = answer;
            });
            return serial;
        };

        // The player as a script knows it (the form id of PlayerRef in the numbering of the content that is loaded), as
        // text for `run`
        api["player"] = []() -> std::string {
            OFScript::FalloutScripts* host = OFBase::Environment::get().getFalloutScripts();
            if (host == nullptr)
                return std::string();
            char text[16];
            std::snprintf(text, sizeof(text), "0x%08x", static_cast<unsigned>(host->playerReference()));
            return text;
        };

        // The answer of a call, once it has run (nil before); asking takes it
        api["result"] = [lua](int serial) -> sol::object {
            const std::lock_guard lock(answers().mMutex);
            const auto found = answers().mDone.find(serial);
            if (found == answers().mDone.end())
                return sol::nil;
            const double value = found->second;
            answers().mDone.erase(found);
            return sol::make_object<double>(lua, value);
        };

        return api;
    }
}
