#include "combatbindings.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwscript/falloutlaw.hpp"
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

        /// The player is 0x14 in the games' own files, and the reference of the player here has the number of the plugin
        /// that holds it in front: a script that says 0x14 means the player (round 14D: a fight against "0x14" ended
        /// half a second after it started, for there is no character of that number)
        FormId playerOrSelf(FormId id)
        {
            return id == FalloutScript::playerReference ? scripts().playerReference() : id;
        }

        FormId parseId(std::string_view text)
        {
            if (text.empty())
                return 0;
            const ESM::RefId id = ESM::RefId::deserializeText(text);
            const ESM::FormId* form = id.getIf<ESM::FormId>();
            if (form == nullptr)
                throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
            return playerOrSelf(form->toUint32());
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
        // The reference of the player, as the other functions here are told it
        api["player"] = []() { return idText(scripts().playerReference()); };
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
            const FormId self = parseId(actor);
            if (FalloutScript::startFight(scripts().combat(), scripts().actors(), self, parseId(target)))
                scripts().wakeMind(self);
        };
        api["stop"] = [](std::string_view actor) { scripts().combat().stop(parseId(actor)); };
        // Two factions are enemies, as SetEnemy of both says (rounds that need a character that attacks the player on
        // sight: the data of the games has the Powder Gangers neutral to the player)
        api["setEnemy"] = [](std::string_view first, std::string_view second) {
            using Relation = FalloutScript::CombatState::Relation;
            scripts().combat().setRelation(parseId(first), parseId(second), Relation::Enemy);
            scripts().combat().setRelation(parseId(second), parseId(first), Relation::Enemy);
        };
        api["isDead"] = [](std::string_view actor) { return scripts().actors().isDead(parseId(actor)); };

        // Damage and destruction (what the commands DamageObject, GetDestructionStage, IsLimbGone, FireWeapon and
        // PushActorAway do, for the rounds): a blow to a thing that can be destroyed, with the table of what it did
        api["damageObject"] = [lua](std::string_view reference, double amount) {
            const FalloutScript::DestructionOutcome outcome = scripts().destruction().damage(parseId(reference), amount);
            sol::table row(lua, sol::create);
            row["applied"] = outcome.mApplied;
            row["health"] = outcome.mHealth;
            row["maxHealth"] = outcome.mMaxHealth;
            row["stageBefore"] = outcome.mStageBefore;
            row["stageAfter"] = outcome.mStageAfter;
            row["crossed"] = static_cast<int>(outcome.mCrossed.size());
            row["modelStage"] = outcome.mModelStage;
            row["damageStage"] = outcome.mDamageStage;
            row["disabled"] = outcome.mDisable;
            row["destroyed"] = outcome.mDestroyed;
            return row;
        };
        // The stage as GetDestructionStage tells it (-1 for a thing with none) and the health it has (-1 for a thing that
        // was never damaged)
        api["destructionStage"] = [](std::string_view reference) { return scripts().destruction().stage(parseId(reference)); };
        api["destructionHealth"] = [](std::string_view reference) { return scripts().destruction().health(parseId(reference)); };
        api["clearDestruction"] = [](std::string_view reference) { return scripts().destruction().restore(parseId(reference)); };
        // The limbs of a character that are gone (IsLimbGone, and the mask of them), and what killed it
        api["isLimbGone"] = [](std::string_view actor, int limb) { return scripts().dismemberment().isGone(parseId(actor), limb); };
        api["limbMask"] = [](std::string_view actor) { return static_cast<int>(scripts().dismemberment().mask(parseId(actor))); };
        // The character is made whole again as far as the limbs and the kept death go (what a revival leaves alone)
        api["forgetLimbs"] = [](std::string_view actor) { scripts().dismemberment().restore(parseId(actor)); };
        api["causeOfDeath"] = [](std::string_view actor) { return scripts().dismemberment().cause(parseId(actor)); };
        // Any command of the script language, run as a script runs it: the name, the reference it is called on ("" for none) and
        // the numbers it is given (a form is its number, 0x... as core.getFormId writes it, given in the number form: the
        // form ids are the numbers of the games' load order)
        api["command"] = [](std::string_view name, std::string_view reference, sol::variadic_args numbers) {
            std::vector<double> arguments;
            for (const sol::stack_proxy value : numbers)
            {
                if (value.is<double>())
                    arguments.push_back(value.get<double>());
                else if (value.is<std::string_view>())
                    arguments.push_back(static_cast<double>(parseId(value.get<std::string_view>())));
            }
            return scripts().runCommand(name, parseId(reference), arguments);
        };
        api["fireWeapon"] = [](std::string_view actor, std::string_view weapon) {
            scripts().fireWeaponOf(parseId(actor), parseId(weapon));
        };
        api["pushActorAway"] = [](std::string_view origin, std::string_view actor, double force) {
            scripts().pushActorAwayFrom(parseId(origin), parseId(actor), force);
        };

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
        // What the world holds against the player (M16): the crimes of the player, whether it is a murderer, and, when a
        // character is given, how that character stands: its factions, whether any of them is an enemy of the player, the
        // crimes of those, what it thinks of the player and the steps of its trespass warning
        api["law"] = [lua](sol::optional<std::string_view> actor) {
            sol::table row(lua, sol::create);
            OFScript::LawGlue* glue = scripts().lawGlue();
            row["available"] = glue != nullptr;
            if (glue == nullptr)
                return row;
            FalloutScript::Law& law = glue->law();
            row["minor"] = law.playerMinorCrimes();
            row["major"] = law.playerMajorCrimes();
            row["murderer"] = law.murderer();
            row["crimes"] = static_cast<int>(law.crimeCount());
            row["amountSoldStolen"] = law.amountSoldStolen();
            if (actor && !actor->empty())
            {
                const FormId id = parseId(*actor);
                const std::vector<FormId> factions = glue->factionsOf(id);
                row["factions"] = static_cast<int>(factions.size());
                row["enemy"] = law.anyEnemy(factions);
                row["factionMinor"] = law.minorCrimes(factions);
                row["factionMajor"] = law.majorCrimes(factions);
                row["disposition"] = glue->dispositionToPlayer(id);
                row["trespassLevel"] = law.trespassLevel(id);
            }
            return row;
        };
        // The player draws the weapon (or puts it away), for the scripts that ask whether it is out
        api["setDrawn"] = [](bool drawn) { scripts().equipment().setWeaponDrawn(scripts().playerReference(), drawn); };
        return LuaUtil::makeReadOnly(api);
    }
}
