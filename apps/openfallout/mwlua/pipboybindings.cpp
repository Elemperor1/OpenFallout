#include "pipboybindings.hpp"

#include <stdexcept>
#include <string>

#include <components/esm/refid.hpp>
#include <components/falloutscript/chargen.hpp>
#include <components/falloutscript/pipboy.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwscript/falloutscripts.hpp"

#include "context.hpp"

namespace OFLua
{
    namespace
    {
        using FalloutScript::Chargen;
        using FalloutScript::FormId;

        OFScript::FalloutScripts& scripts()
        {
            OFScript::FalloutScripts* host = OFBase::Environment::get().getFalloutScripts();
            if (host == nullptr)
                throw std::runtime_error("core.pipboy is only available with the content of Fallout 3 and New Vegas");
            return *host;
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

        /// A form id from Lua: the player when it is empty
        FormId holderOf(sol::optional<std::string_view> text)
        {
            const FormId id = text ? parseId(*text) : 0;
            return id != 0 ? id : scripts().playerReference();
        }

        // The editor ids of the attributes in the order of the numbers 5 to 11, and the names they have when the files
        // have none
        struct Attribute
        {
            const char* mEditorId;
            const char* mName;
        };
        constexpr Attribute attributes[Chargen::attributeCount] = {
            { "Strength", "Strength" },
            { "Perception", "Perception" },
            { "Endurance", "Endurance" },
            { "Charisma", "Charisma" },
            { "Intelligence", "Intelligence" },
            { "Agility", "Agility" },
            { "Luck", "Luck" },
        };

        sol::table perkRows(const sol::state_view& lua, const std::vector<OFScript::FalloutScripts::PipboyPerk>& perks)
        {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::PipboyPerk& perk : perks)
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(perk.mId);
                row["name"] = perk.mName;
                row["description"] = perk.mDescription;
                row["rank"] = perk.mRank;
                row["ranks"] = perk.mRanks;
                row["available"] = perk.mAvailable;
                rows[index++] = row;
            }
            return rows;
        }
    }

    sol::table initCorePipboyBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // Whether the content is that of Fallout, which has a Pip-Boy to show
        api["available"] = []() { return OFBase::Environment::get().getFalloutScripts() != nullptr; };
        // The reference of the player, as the other functions take the holders of items
        api["player"] = []() { return idText(scripts().playerReference()); };
        // The text of a game setting of the files ("" when they have none), for the words the screens show
        api["text"] = [](std::string_view name) { return scripts().gameSettingText(name); };
        // The value of a number setting, nothing when the files have none
        api["setting"] = [](std::string_view name) { return scripts().gameSetting(name); };

        // How the player's character is: level, experience (and where this level started and the next starts),
        // karma and its title, health, action points, what is carried and what can be, caps, the points that can be
        // spent, and what the armor worn takes away
        api["stats"] = [lua]() {
            const OFScript::FalloutScripts::PipboyStats stats = scripts().pipboyStats();
            sol::table row(lua, sol::create);
            row["level"] = stats.mLevel;
            row["experience"] = stats.mExperience;
            row["levelStart"] = stats.mLevelStart;
            row["nextLevel"] = stats.mNextLevel;
            row["karma"] = stats.mKarma;
            row["karmaTitle"] = stats.mKarmaTitle;
            row["health"] = stats.mHealth;
            row["maxHealth"] = stats.mMaxHealth;
            row["actionPoints"] = stats.mActionPoints;
            row["maxActionPoints"] = stats.mMaxActionPoints;
            row["weight"] = stats.mWeight;
            row["maxWeight"] = stats.mMaxWeight;
            row["caps"] = stats.mCaps;
            row["skillPoints"] = stats.mSkillPoints;
            row["perkPoints"] = stats.mPerkPoints;
            row["armorRating"] = stats.mArmorRating;
            row["threshold"] = stats.mThreshold;
            return row;
        };
        // The seven attributes of SPECIAL: number, name, description, value now and base
        api["attributes"] = [lua]() {
            OFScript::FalloutScripts& host = scripts();
            sol::table rows(lua, sol::create);
            for (int i = 0; i < Chargen::attributeCount; ++i)
            {
                const int number = Chargen::firstAttribute + i;
                const OFScript::FalloutScripts::ActorValueText text
                    = host.actorValueText(attributes[i].mEditorId, attributes[i].mName);
                sol::table row(lua, sol::create);
                row["number"] = number;
                row["name"] = text.mName;
                row["description"] = text.mDescription;
                row["value"] = host.actorValues().current(host.playerReference(), number);
                row["base"] = host.actorValues().base(host.playerReference(), number);
                rows[i + 1] = row;
            }
            return rows;
        };
        // The skills: number, name, description, value now and base, whether it is tagged, and the word for how good
        // it is
        api["skills"] = [lua]() {
            OFScript::FalloutScripts& host = scripts();
            const FormId player = host.playerReference();
            const auto lookup = [&host](std::string_view name) { return host.gameSetting(name); };
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::ChargenSkill& skill : host.chargen().skills())
            {
                const OFScript::FalloutScripts::ActorValueText text = host.actorValueText(skill.mEditorId, skill.mName);
                const double value = host.actorValues().current(player, skill.mNumber);
                sol::table row(lua, sol::create);
                row["number"] = skill.mNumber;
                row["name"] = text.mName;
                row["description"] = text.mDescription;
                row["value"] = value;
                row["base"] = host.actorValues().base(player, skill.mNumber);
                row["tagged"] = host.chargen().tagged(skill.mNumber);
                row["level"] = host.gameSettingText(FalloutScript::Pipboy::skillLevelSetting(lookup, value));
                rows[index++] = row;
            }
            return rows;
        };
        // The perks the player has, with their ranks
        api["perks"] = [lua]() { return perkRows(lua, scripts().pipboyPerks()); };
        // The quests the player follows: id, name, whether it is done, and the objectives it was told about
        api["quests"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::PipboyQuest& quest : scripts().pipboyQuests())
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(quest.mId);
                row["name"] = quest.mName;
                row["completed"] = quest.mCompleted;
                sol::table objectives(lua, sol::create);
                int objective = 1;
                for (const OFScript::FalloutScripts::PipboyObjective& entry : quest.mObjectives)
                {
                    sol::table line(lua, sol::create);
                    line["text"] = entry.mText;
                    line["completed"] = entry.mCompleted;
                    objectives[objective++] = line;
                }
                row["objectives"] = objectives;
                rows[index++] = row;
            }
            return rows;
        };
        // What a holder carries (the player when none is named): id, name, count, weight, value, the tab it is listed
        // under ("weapons", "apparel", "aid", "misc" or "ammo"), whether it is worn or held, and for weapons the
        // damage and for armor its rating and threshold
        api["items"] = [lua](sol::optional<std::string_view> holder) {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::PipboyItem& item : scripts().pipboyItems(holderOf(holder)))
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(item.mId);
                row["name"] = item.mName;
                row["count"] = item.mCount;
                row["weight"] = item.mWeight;
                row["value"] = item.mValue;
                row["category"] = FalloutScript::Pipboy::categoryName(item.mCategory);
                row["equipped"] = item.mEquipped;
                row["damage"] = item.mDamage;
                row["rating"] = item.mRating;
                row["threshold"] = item.mThreshold;
                rows[index++] = row;
            }
            return rows;
        };
        // The player puts on or takes off, eats or reads the item; what happened ("equipped", "unequipped", "used",
        // "read"), or "" when nothing did
        api["use"] = [](std::string_view item) { return scripts().useItemFromList(parseId(item)); };
        // The player drops items in front of it; how many
        api["drop"] = [](std::string_view item, sol::optional<int> count) {
            return scripts().dropItem(parseId(item), count.value_or(1));
        };
        // Items go from one holder to another (a container to the player and back); how many moved
        api["transfer"] = [](std::string_view from, std::string_view to, std::string_view item, int count) {
            return scripts().transferItems(parseId(from), parseId(to), parseId(item), count);
        };
        // The player opens or closes a container: its scripts hear of it
        api["containerOpened"]
            = [](std::string_view container, bool opened) { scripts().containerOpened(parseId(container), opened); };

        // What the player has open to take things from (a container, or a character that is dead): the holder, its
        // name and a number that is another for each time something is opened; nothing when nothing is open
        api["loot"] = [lua]() -> sol::object {
            const auto loot = scripts().loot();
            if (!loot)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["holder"] = idText(loot->mHolder);
            row["name"] = loot->mName;
            row["serial"] = loot->mSerial;
            return row;
        };
        api["closeLoot"] = []() { scripts().closeLoot(); };

        // The menu of recipes a script showed (a workbench, a reloading bench, a campfire): the category, its name, a
        // number that is another for each time a menu opens, and the recipes the player may use with what each takes
        // and gives; nothing when no menu is open
        api["recipes"] = [lua]() -> sol::object {
            const auto menu = scripts().recipeMenu();
            if (!menu)
                return sol::nil;
            const auto itemRows = [&lua](const std::vector<OFScript::FalloutScripts::RecipeItem>& items) {
                sol::table rows(lua, sol::create);
                int index = 1;
                for (const OFScript::FalloutScripts::RecipeItem& item : items)
                {
                    sol::table row(lua, sol::create);
                    row["id"] = idText(item.mItem);
                    row["name"] = item.mName;
                    row["count"] = item.mCount;
                    row["have"] = item.mHave;
                    rows[index++] = row;
                }
                return rows;
            };
            sol::table recipes(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::RecipeLine& line : scripts().recipeLines())
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(line.mId);
                row["name"] = line.mName;
                row["subCategory"] = idText(line.mSubCategory);
                row["subCategoryName"] = line.mSubCategoryName;
                row["status"] = line.mStatus;
                row["skill"] = line.mSkill;
                row["level"] = line.mLevel;
                row["skillValue"] = line.mSkillValue;
                row["ingredients"] = itemRows(line.mIngredients);
                row["outputs"] = itemRows(line.mOutputs);
                recipes[index++] = row;
            }
            sol::table result(lua, sol::create);
            result["category"] = idText(menu->mCategory);
            result["name"] = menu->mName;
            result["serial"] = menu->mSerial;
            result["recipes"] = recipes;
            return result;
        };
        // The player makes a recipe of the open menu: "made", or "skill" or "items" for what is short; "" for a recipe
        // that is not in the menu
        api["craft"] = [](std::string_view recipe) { return scripts().craftRecipe(parseId(recipe)); };
        api["closeRecipes"] = []() { scripts().closeRecipes(); };

        // The levels the player reached since this was last called: the level, the skill points it gave and whether
        // it gave a perk
        api["takeLevelUps"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const FalloutScript::PlayerProgress::LevelUp& up : scripts().takeLevelUps())
            {
                sol::table row(lua, sol::create);
                row["level"] = up.mLevel;
                row["skillPoints"] = up.mSkillPoints;
                row["perk"] = up.mPerk;
                rows[index++] = row;
            }
            return rows;
        };
        // The screen of the last level that was handed out is done: the next level is handed out
        api["levelUpDone"] = []() { scripts().finishLevelUp(); };
        // The perks the player can pick with a perk point, and the points it can spend
        api["offeredPerks"] = [lua]() { return perkRows(lua, scripts().pipboyOfferedPerks()); };
        // Spends points on a skill; how many were spent
        api["spendSkillPoints"] = [](int skill, int points) { return scripts().spendSkillPoints(skill, points); };
        // Takes points back from a skill; how many were
        api["refundSkillPoints"] = [](int skill, int points) { return scripts().refundSkillPoints(skill, points); };
        api["choosePerk"] = [](std::string_view perk) { return scripts().choosePerk(parseId(perk)); };

        // Whether the player is dead, and the game asks to load a save when the screen of its death is over
        api["playerDead"] = []() { return scripts().actors().isDead(scripts().playerReference()); };
        api["askReload"] = []() { OFBase::Environment::get().getStateManager()->askLoadRecent(); };
        return LuaUtil::makeReadOnly(api);
    }
}
