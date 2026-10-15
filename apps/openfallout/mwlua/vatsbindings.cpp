#include "vatsbindings.hpp"

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
        OFScript::FalloutScripts& scripts()
        {
            return *OFBase::Environment::get().getFalloutScripts();
        }
    }

    sol::table initCoreVatsBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // Whether the content is that of Fallout, which has V.A.T.S. (it is not for Morrowind's)
        api["available"] = []() { return OFBase::Environment::get().getFalloutScripts() != nullptr; };

        // What the weapon and the action points of the player allow: whether V.A.T.S. can be used, whether it is open,
        // the action points the player has and has at most, what one attack costs, how far the weapon reaches, and the
        // name of the weapon ("" for the hands)
        api["state"] = [lua]() {
            const OFScript::FalloutScripts::VatsState state = scripts().vatsState();
            sol::table row(lua, sol::create);
            row["usable"] = state.mAvailable;
            row["open"] = state.mOpen;
            row["actionPoints"] = state.mActionPoints;
            row["maxActionPoints"] = state.mMaxActionPoints;
            row["cost"] = state.mCost;
            row["range"] = state.mRange;
            row["weapon"] = state.mWeaponName;
            return row;
        };

        // Whether the object is a character or creature that is alive and within the reach of the weapon
        api["canTarget"]
            = [](sol::optional<LObject> target) { return target && scripts().vatsCanTarget(target->ptr()); };

        // What the screen shows of a target: its name, how far it is from the player and its health
        api["describe"] = [lua](sol::optional<LObject> target) {
            sol::table row(lua, sol::create);
            if (!target)
                return row;
            const OFScript::FalloutScripts::VatsTarget info = scripts().vatsTarget(target->ptr());
            row["name"] = info.mName;
            row["distance"] = info.mDistance;
            row["health"] = info.mHealth;
            row["maxHealth"] = info.mMaxHealth;
            return row;
        };

        // The parts of the body of the target that can be aimed at, each with the number the engine knows it by, its
        // name, what V.A.T.S. calls it, the chance in percent of a hit with the weapon the player holds, and the
        // condition of the part in percent
        api["parts"] = [lua](sol::optional<LObject> target) {
            sol::table list(lua, sol::create);
            if (!target)
                return list;
            int index = 1;
            for (const OFScript::FalloutScripts::VatsPart& part : scripts().vatsParts(target->ptr()))
            {
                sol::table row(lua, sol::create);
                row["index"] = part.mIndex;
                row["name"] = part.mName;
                row["target"] = part.mTarget;
                row["chance"] = part.mChance;
                row["condition"] = part.mCondition;
                list[index++] = row;
            }
            return list;
        };

        // The player attacks the part of the target: the action points are spent and the blow struck when the roll is
        // below the chance. `refused` says why nothing was done ("points", "ammo", "range", "dead", "none"), and is
        // nil when the attack was made.
        api["attack"] = [lua](sol::optional<LObject> target, int part) {
            const OFScript::FalloutScripts::VatsAttackResult result
                = scripts().vatsAttack(target ? target->ptr() : OFWorld::Ptr(), part);
            sol::table row(lua, sol::create);
            if (!result.mRefused.empty())
                row["refused"] = result.mRefused;
            row["hit"] = result.mHit;
            row["cost"] = result.mCost;
            row["chance"] = result.mChance;
            row["damage"] = result.mStrike.mDamage;
            row["critical"] = result.mStrike.mCritical;
            row["killed"] = result.mStrike.mKilled;
            row["downed"] = result.mStrike.mDowned;
            row["crippled"] = result.mStrike.mCrippled;
            return row;
        };

        // The screen of V.A.T.S. tells the engine that it is open, which GetVATSMode answers and which stops the action
        // points from coming back
        api["setOpen"] = [](bool open) { scripts().setVatsOpen(open); };
        return LuaUtil::makeReadOnly(api);
    }
}
