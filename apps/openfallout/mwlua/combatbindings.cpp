#include "combatbindings.hpp"

#include <stdexcept>
#include <string>

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"
#include "object.hpp"

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
            row["crippled"] = result.mCrippled;
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
            FalloutScript::startFight(scripts().combat(), scripts().actors(), parseId(actor), parseId(target));
        };
        api["stop"] = [](std::string_view actor) { scripts().combat().stop(parseId(actor)); };
        api["isDead"] = [](std::string_view actor) { return scripts().actors().isDead(parseId(actor)); };

        // Whether the content is that of Fallout, which has combat to run (it is not for Morrowind's)
        api["available"] = []() { return scripts().hasCombat(); };

        // The weapon the player holds and what it allows: the weapon ("" for the hands), whether it is a melee weapon,
        // automatic, how far a blow or shot reaches, the least seconds between two, and the rounds the player has for
        // it
        api["weapon"] = [lua]() {
            const OFScript::FalloutScripts::WeaponStats stats = scripts().weaponStats(scripts().playerReference());
            sol::table row(lua, sol::create);
            row["weapon"] = idText(stats.mWeapon);
            row["melee"] = stats.mMelee;
            row["automatic"] = stats.mAutomatic;
            row["range"] = stats.mRange;
            row["interval"] = stats.mInterval;
            row["ammoUse"] = stats.mAmmoUse;
            row["rounds"] = stats.mRounds;
            return row;
        };
        // The player attacks with its weapon at the object the ray of its aim hit (nothing when it hit none): a round
        // is used up, and a character or creature that was hit loses health. Nothing happens while the weapon is not
        // ready again. How far the shot goes is up to the aim: the ray is as long as the range of the weapon
        // (combat.weapon().range), and the object it hit is what is passed here.
        api["fire"] = [lua](sol::optional<LObject> target, sol::optional<int> limb) {
            const OFScript::FalloutScripts::FireResult result = scripts().fire(
                scripts().playerReference(), target ? target->ptr() : OFWorld::Ptr(), limb.value_or(-1));
            sol::table row(lua, sol::create);
            row["fired"] = result.mFired;
            row["outOfAmmo"] = result.mOutOfAmmo;
            row["hit"] = result.mHit;
            row["damage"] = result.mStrike.mDamage;
            row["critical"] = result.mStrike.mCritical;
            row["killed"] = result.mStrike.mKilled;
            row["downed"] = result.mStrike.mDowned;
            row["crippled"] = result.mStrike.mCrippled;
            return row;
        };
        // The player draws the weapon (or puts it away), for the scripts that ask whether it is out
        api["setDrawn"] = [](bool drawn) { scripts().equipment().setWeaponDrawn(scripts().playerReference(), drawn); };
        return LuaUtil::makeReadOnly(api);
    }
}
