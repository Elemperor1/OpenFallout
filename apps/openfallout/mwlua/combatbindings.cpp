#include "combatbindings.hpp"

#include <stdexcept>
#include <string>

#include <components/esm/refid.hpp>
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

        std::string idText(FormId id)
        {
            return id == 0 ? std::string() : ESM::RefId::formIdRefId(ESM::FormId::fromUint32(id)).serializeText();
        }

        FormId parseId(std::string_view text)
        {
            if (text.empty())
                return 0;
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
            return form->toUint32();
        }
    }

    sol::table initCoreCombatBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // The characters are told by their form ids, as core.getFormId writes one ("" for none)

        // The attacker strikes the victim with the weapon (the weapon it holds when none is given, or its hands), in
        // the part of the body `limb` (-1 when not known): a table with whether a blow was struck, the damage, whether
        // it was critical, and whether the victim is dead or fell unconscious.
        api["strike"] = [lua](std::string_view attacker, std::string_view victim,
                            sol::optional<std::string_view> weapon, sol::optional<int> limb) {
            const OFScript::FalloutScripts::StrikeResult result = scripts().strike(
                parseId(attacker), parseId(victim), weapon ? parseId(*weapon) : 0, limb.value_or(-1));
            sol::table row(lua, sol::create);
            row["hit"] = result.mHit;
            row["damage"] = result.mDamage;
            row["critical"] = result.mCritical;
            row["killed"] = result.mKilled;
            row["downed"] = result.mDowned;
            return row;
        };
        // Whom the character fights ("" when it is at peace)
        api["target"] = [](std::string_view actor) { return idText(scripts().combat().target(parseId(actor))); };
        api["inCombat"] = [](std::string_view actor) { return scripts().combat().inCombat(parseId(actor)); };
        // The characters that fight the target
        api["attackers"] = [lua](std::string_view target) {
            sol::table list(lua, sol::create);
            int index = 1;
            for (const FormId attacker : scripts().combat().attackersOf(parseId(target)))
                list[index++] = idText(attacker);
            return list;
        };
        api["start"] = [](std::string_view actor, std::string_view target) {
            // A dead character fights no one and is not fought, and one that is out does not start a fight
            const FormId first = parseId(actor);
            const FormId second = parseId(target);
            if (FalloutScript::mayStartFight(scripts().combat(), scripts().actors(), first, second))
                scripts().combat().start(first, second);
        };
        api["stop"] = [](std::string_view actor) { scripts().combat().stop(parseId(actor)); };
        api["isDead"] = [](std::string_view actor) { return scripts().actors().isDead(parseId(actor)); };
        return LuaUtil::makeReadOnly(api);
    }
}
