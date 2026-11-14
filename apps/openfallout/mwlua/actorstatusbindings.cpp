#include "actorstatusbindings.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/falloutscript/interpreter.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"

namespace OFLua
{
    namespace
    {
        using FalloutScript::FormId;

        OFScript::FalloutScripts& scripts()
        {
            return *OFBase::Environment::get().getFalloutScripts();
        }

        FormId parseId(std::string_view text)
        {
            if (text.empty())
                return 0;
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
            const FormId number = form->toUint32();
            return number == FalloutScript::playerReference ? scripts().playerReference() : number;
        }

        std::string idText(FormId id)
        {
            return id == 0 ? std::string() : ESM::RefId::formIdRefId(ESM::FormId::fromUint32(id)).serializeText();
        }
    }

    sol::table initCoreActorStatusBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // Calls the command of that code (0x10DB for GetAnimAction) on a character given by its form id ("" for no
        // object), with the arguments that follow: a number, or a form id as text. Tells the value of the command.
        api["call"] = [](int opcode, std::string_view actor, sol::variadic_args args) {
            std::vector<FalloutScript::Value> values;
            for (const sol::stack_proxy argument : args)
            {
                FalloutScript::Value value;
                if (argument.get_type() == sol::type::string)
                    value.mNumber = parseId(argument.get<std::string_view>());
                else
                    value.mNumber = argument.get<double>();
                values.push_back(std::move(value));
            }
            const FormId reference = parseId(actor);
            return scripts().interpreter().call(
                static_cast<std::uint16_t>(opcode), reference, !actor.empty(), std::move(values));
        };

        // What the engine tells the commands about the character now (nil when it told nothing)
        api["activity"] = [lua](std::string_view actor) -> sol::object {
            const auto activity = scripts().actorStatus().activity(parseId(actor));
            if (!activity)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["movement"] = activity->mMovement;
            row["swimming"] = activity->mSwimming;
            row["animAction"] = activity->mAnimAction;
            row["stage"] = activity->mStage;
            row["packageType"] = activity->mPackageType;
            row["packageFlags"] = activity->mPackageFlags;
            row["packageTarget"] = idText(activity->mPackageTarget);
            row["idlePlaying"] = activity->mIdlePlaying;
            row["idleDoneOnce"] = activity->mIdleDoneOnce;
            row["lastIdle"] = idText(activity->mLastIdle);
            row["walkSpeed"] = activity->mWalkSpeed;
            return row;
        };

        return LuaUtil::makeReadOnly(api);
    }
}
