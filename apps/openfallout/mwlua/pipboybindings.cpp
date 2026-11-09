#include "pipboybindings.hpp"

#include <charconv>
#include <stdexcept>
#include <string>
#include <system_error>

#include <components/esm/refid.hpp>
#include <components/falloutscript/chargen.hpp>
#include <components/falloutscript/pipboy.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
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

        /// The id of a row of a list. Copies of a weapon or armor that differ in condition are rows of their own, and
        /// the row tells which: the form, then "~" and the number of the copy (the condition in tenths of a percent of
        /// the health, with the mask of the installed mods over its ten low bits). Every
        /// function that takes the id of a row takes either kind (a plain form names no copy: the best, or the one
        /// that is on).
        std::string rowId(const OFScript::FalloutScripts::PipboyItem& item)
        {
            std::string text = idText(item.mId);
            if (item.mStack)
                text += "~" + std::to_string(FalloutScript::copyOf(item.mCondition, item.mMods));
            return text;
        }

        struct RowKey
        {
            FormId mItem = 0;
            int mCondition = -1;
        };

        RowKey parseRowId(std::string_view text)
        {
            RowKey key;
            const std::size_t tilde = text.find('~');
            if (tilde != std::string_view::npos)
            {
                int condition = -1;
                const std::string_view digits = text.substr(tilde + 1);
                if (std::from_chars(digits.data(), digits.data() + digits.size(), condition).ec == std::errc())
                    key.mCondition = condition;
                text = text.substr(0, tilde);
            }
            key.mItem = parseId(text);
            return key;
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
            { "AVStrength", "Strength" },
            { "AVPerception", "Perception" },
            { "AVEndurance", "Endurance" },
            { "AVCharisma", "Charisma" },
            { "AVIntelligence", "Intelligence" },
            { "AVAgility", "Agility" },
            { "AVLuck", "Luck" },
        };

        sol::table itemRow(const sol::state_view& lua, const OFScript::FalloutScripts::PipboyItem& item)
        {
            sol::table row(lua, sol::create);
            row["id"] = rowId(item);
            // The form alone, for what compares with a form and not with a row
            row["form"] = idText(item.mId);
            row["name"] = item.mName;
            row["count"] = item.mCount;
            // Weapons and armor: the condition in tenths of a percent of the health (1000 is full)
            row["hasCondition"] = item.mHasCondition;
            row["condition"] = item.mCondition;
            // The mask of the mods installed in a weapon (1, 2 and 4 for the slots); the games add a plus to its name
            row["mods"] = item.mMods;
            row["weight"] = item.mWeight;
            row["value"] = item.mValue;
            row["category"] = FalloutScript::Pipboy::categoryName(item.mCategory);
            row["equipped"] = item.mEquipped;
            row["damage"] = item.mDamage;
            row["rating"] = item.mRating;
            row["threshold"] = item.mThreshold;
            row["key"] = item.mKey;
            row["book"] = item.mBook;
            row["hidden"] = item.mHidden;
            row["usable"] = item.mUsable;
            row["cardWeight"] = item.mCardWeight;
            row["cardDamage"] = item.mCardDamage;
            row["pellets"] = item.mPellets;
            row["hasAmmo"] = item.mHasAmmo;
            row["ammoName"] = item.mAmmoName;
            row["clip"] = item.mClip;
            row["loaded"] = item.mLoaded;
            row["reserve"] = item.mReserve;
            row["icon"] = item.mIcon;
            return row;
        }

        /// The things picked out for a deal, as a list of tables with the id of an item and how many of it
        OFScript::FalloutScripts::BarterPiles pilesOf(const sol::optional<sol::table>& table)
        {
            OFScript::FalloutScripts::BarterPiles piles;
            if (!table)
                return piles;
            for (const auto& entry : *table)
            {
                if (!entry.second.is<sol::table>())
                    continue;
                const sol::table pick = entry.second.as<sol::table>();
                const sol::optional<std::string> id = pick["id"];
                const sol::optional<int> count = pick["count"];
                if (id && count)
                {
                    const RowKey key = parseRowId(*id);
                    piles.push_back({ key.mItem, *count, key.mCondition });
                }
            }
            return piles;
        }

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
                row["icon"] = perk.mIcon;
                row["minLevel"] = perk.mMinimumLevel;
                row["rank"] = perk.mRank;
                row["ranks"] = perk.mRanks;
                row["available"] = perk.mAvailable;
                sol::table requirements(lua, sol::create);
                int requirement = 1;
                for (const OFScript::FalloutScripts::PipboyRequirement& entry : perk.mRequirements)
                {
                    sol::table line(lua, sol::create);
                    switch (entry.mKind)
                    {
                        case OFScript::FalloutScripts::PipboyRequirement::Kind::ActorValue:
                            line["kind"] = "value";
                            break;
                        case OFScript::FalloutScripts::PipboyRequirement::Kind::Perk:
                            line["kind"] = "perk";
                            break;
                        default:
                            line["kind"] = "other";
                            break;
                    }
                    line["actorValue"] = entry.mActorValue;
                    line["name"] = entry.mName;
                    line["comparison"] = entry.mComparison;
                    line["number"] = entry.mNumber;
                    line["or"] = entry.mOr;
                    requirements[requirement++] = line;
                }
                row["requirements"] = requirements;
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
            row["newVegas"] = stats.mNewVegas;
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
                row["icon"] = text.mIcon;
                row["levelUpSkip"] = text.mLevelUpSkip;
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
        // damage and for armor its rating and threshold. Also what the lists and cards of the Pip-Boy need: a key
        // (listed on the keyring), a book, armor the files mark as not for the player (hidden), whether the item
        // can be used now, the weight of one for the player, the damage of a shot and its pellets, and the
        // ammunition line (name, clip, rounds loaded and in reserve)
        api["items"] = [lua](sol::optional<std::string_view> holder) {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::PipboyItem& item : scripts().pipboyItems(holderOf(holder)))
                rows[index++] = itemRow(lua, item);
            return rows;
        };
        // The player puts on or takes off, eats or reads the item; what happened ("equipped", "unequipped", "used",
        // "read"), or "" when nothing did
        api["use"] = [](std::string_view item) {
            const RowKey key = parseRowId(item);
            return scripts().useItemFromList(key.mItem, key.mCondition);
        };
        // The pages of the help manual (the messages of the form list `HelpManual`, or `HelpManualXBox` for a pad):
        // a list of tables with the id of the message, its title and its words; empty when the data has none
        api["helpManual"] = [lua](sol::optional<bool> pad) {
            sol::table list(lua, sol::create);
            int index = 1;
            for (const auto& page : scripts().helpManual(pad.value_or(false)))
            {
                sol::table row(lua, sol::create);
                row["id"] = page.mId;
                row["title"] = page.mTitle;
                row["text"] = page.mText;
                list[index++] = row;
            }
            return list;
        };
        // One hint of the game by its editor id (`HelpCrafting`): the same table as a page of the manual; nothing when
        // there is none
        api["helpMessage"] = [lua](std::string_view editorId) -> sol::object {
            const auto page = scripts().helpMessage(editorId);
            if (!page)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["id"] = page->mId;
            row["title"] = page->mTitle;
            row["text"] = page->mText;
            return row;
        };
        // The most items one drop puts on the floor
        api["dropLimit"] = OFScript::FalloutScripts::sDropLimit;
        // The player drops items in front of it; how many
        api["drop"] = [](std::string_view item, sol::optional<int> count) {
            const RowKey key = parseRowId(item);
            return scripts().dropItem(key.mItem, count.value_or(1), key.mCondition);
        };
        // Items go from one holder to another (a container to the player and back); how many moved
        api["transfer"] = [](std::string_view from, std::string_view to, std::string_view item, int count) {
            const RowKey key = parseRowId(item);
            return scripts().transferItems(parseId(from), parseId(to), key.mItem, count, key.mCondition);
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

        // ---- Repair ----
        // Whether the Pip-Boy offers to repair the copy the row names: a weapon or armor below full condition with
        // something that can repair it
        api["canRepair"] = [](std::string_view item) {
            const RowKey key = parseRowId(item);
            return scripts().canRepairItem(key.mItem, key.mCondition);
        };
        // The repair screen of one copy: the skill of the player, the copy (id, name, icon, condition, worn, what it
        // does as `stat`) and the things that can repair it, one for each, the weakest first (id, name, condition, worn,
        // the condition the copy has after it, and what the copy does then). Nothing for an item that is not repaired.
        api["repairView"] = [lua](std::string_view item) -> sol::object {
            const RowKey key = parseRowId(item);
            const std::optional<OFScript::FalloutScripts::RepairView> view
                = scripts().repairView(key.mItem, key.mCondition);
            if (!view)
                return sol::nil;
            const auto statTable = [&lua](const OFScript::FalloutScripts::RepairStat& stat) {
                sol::table table(lua, sol::create);
                table["damage"] = stat.mDamage;
                table["rating"] = stat.mRating;
                table["threshold"] = stat.mThreshold;
                return table;
            };
            const auto idOf = [](FormId form, int condition, int mods) {
                return idText(form) + "~" + std::to_string(FalloutScript::copyOf(condition, mods));
            };
            sol::table result(lua, sol::create);
            result["skill"] = view->mSkill;
            sol::table copy(lua, sol::create);
            copy["id"] = idOf(view->mItem, view->mCondition, view->mMods);
            copy["form"] = idText(view->mItem);
            copy["name"] = view->mName;
            copy["icon"] = view->mIcon;
            copy["condition"] = view->mCondition;
            copy["worn"] = view->mWorn;
            copy["stat"] = statTable(view->mStat);
            result["item"] = copy;
            sol::table donors(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::RepairDonor& donor : view->mDonors)
            {
                sol::table row(lua, sol::create);
                row["id"] = idOf(donor.mItem, donor.mCondition, donor.mMods);
                row["form"] = idText(donor.mItem);
                row["name"] = donor.mName;
                row["condition"] = donor.mCondition;
                row["worn"] = donor.mWorn;
                row["result"] = donor.mResult;
                row["stat"] = statTable(donor.mStat);
                donors[index++] = row;
            }
            result["donors"] = donors;
            return result;
        };
        // The player repairs the copy of the first row with a thing of the second (used up); the condition the copy has
        // now, -1 when nothing was done
        api["repair"] = [](std::string_view item, std::string_view donor) {
            const RowKey key = parseRowId(item);
            const RowKey donorKey = parseRowId(donor);
            return scripts().repairItem(key.mItem, key.mCondition, donorKey.mItem, donorKey.mCondition);
        };

        // The repair services a script showed (a conversation that ends in ShowRepairMenu): the character that repairs,
        // its name, its Repair skill and a number that is another for each time a menu opens; nothing when none is open
        api["repairServices"] = [lua]() -> sol::object {
            const auto menu = scripts().repairServices();
            if (!menu)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["vendor"] = idText(menu->mVendor);
            row["name"] = menu->mName;
            row["skill"] = menu->mSkill;
            row["serial"] = menu->mSerial;
            return row;
        };
        // What the vendor repairs and asks: the caps of the player, the sum of every row, the condition the vendor
        // leaves things in (tenths of a percent) and the rows (id, name, condition, worn, cost, whether the player can
        // pay it, in the order the menu lists them)
        api["serviceRows"] = [lua]() -> sol::object {
            const auto view = scripts().serviceView();
            if (!view)
                return sol::nil;
            sol::table result(lua, sol::create);
            result["skill"] = view->mSkill;
            result["caps"] = view->mCaps;
            result["total"] = view->mTotal;
            result["cap"] = view->mCap;
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::ServiceRow& entry : view->mRows)
            {
                sol::table row(lua, sol::create);
                row["id"] = idText(entry.mItem) + "~" + std::to_string(FalloutScript::copyOf(entry.mCondition, entry.mMods));
                row["name"] = entry.mName;
                row["condition"] = entry.mCondition;
                row["worn"] = entry.mWorn;
                row["cost"] = entry.mCost;
                row["canPay"] = entry.mCanPay;
                row["icon"] = entry.mIcon;
                sol::table now(lua, sol::create);
                now["damage"] = entry.mStat.mDamage;
                now["rating"] = entry.mStat.mRating;
                now["threshold"] = entry.mStat.mThreshold;
                row["stat"] = now;
                sol::table atCap(lua, sol::create);
                atCap["damage"] = entry.mCapStat.mDamage;
                atCap["rating"] = entry.mCapStat.mRating;
                atCap["threshold"] = entry.mCapStat.mThreshold;
                row["capStat"] = atCap;
                rows[index++] = row;
            }
            result["rows"] = rows;
            return result;
        };
        // The vendor repairs the copy of the row (the worn one when `worn`) and the player pays, or every row that can be
        // paid for (`all`); how many copies were repaired
        api["serviceRepair"] = [](std::string_view item, sol::optional<bool> worn, sol::optional<bool> all) {
            const RowKey key = parseRowId(item);
            return scripts().repairService(key.mItem, key.mCondition, worn.value_or(false), all.value_or(false));
        };
        api["closeRepairServices"] = []() { scripts().closeRepairServices(); };

        // ---- Item mods ----
        // The screen of one weapon: the copy (id, name, icon, condition, mods, what a shot does) and the lines of the list
        // (the mod, its name, picture and description, whether it is installed in the weapon)
        const auto modTable = [lua](const OFScript::FalloutScripts::ModView& view) {
            sol::table result(lua, sol::create);
            sol::table copy(lua, sol::create);
            copy["id"] = idText(view.mItem) + "~" + std::to_string(FalloutScript::copyOf(view.mCondition, view.mMods));
            copy["form"] = idText(view.mItem);
            copy["name"] = view.mName;
            copy["icon"] = view.mIcon;
            copy["condition"] = view.mCondition;
            copy["mods"] = view.mMods;
            copy["damage"] = view.mDamage;
            result["item"] = copy;
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::ModRow& entry : view.mRows)
            {
                sol::table row(lua, sol::create);
                row["mod"] = idText(entry.mMod);
                row["name"] = entry.mName;
                row["icon"] = entry.mIcon;
                row["description"] = entry.mDescription;
                row["installed"] = entry.mInstalled;
                rows[index++] = row;
            }
            result["rows"] = rows;
            return result;
        };
        // The player opens the screen for a copy (the weapon that is on comes off until it closes); nothing for a thing
        // that is not a weapon of the player
        api["modOpen"] = [modTable](std::string_view item) -> sol::object {
            const RowKey key = parseRowId(item);
            const std::optional<OFScript::FalloutScripts::ModView> view = scripts().openItemMods(key.mItem, key.mCondition);
            return view ? sol::object(modTable(*view)) : sol::object(sol::nil);
        };
        // The same, as the copy is now
        api["modView"] = [modTable](std::string_view item) -> sol::object {
            const RowKey key = parseRowId(item);
            const std::optional<OFScript::FalloutScripts::ModView> view = scripts().modView(key.mItem, key.mCondition);
            return view ? sol::object(modTable(*view)) : sol::object(sol::nil);
        };
        // The mod is installed in the copy and one copy of the mod item is used up; the row id of the copy as it is now,
        // an empty text when nothing was done
        api["modInstall"] = [](std::string_view item, std::string_view mod) -> std::string {
            const RowKey key = parseRowId(item);
            const int copy = scripts().installMod(key.mItem, key.mCondition, parseId(mod));
            return copy < 0 ? std::string() : idText(key.mItem) + "~" + std::to_string(copy);
        };
        // The player is done with the screen: the weapon that came off when it opened is put on again
        api["modClose"] = [](std::string_view item) {
            const RowKey key = parseRowId(item);
            scripts().closeItemMods(key.mItem, key.mCondition);
        };

        // The barter menu a script showed (a conversation that ends in ShowBarterMenu): the character that trades, the
        // holder of what it sells, its name, the percent the script gave, and a number that is another for each time a
        // menu opens; nothing when none is open
        api["barter"] = [lua]() -> sol::object {
            const auto menu = scripts().barterMenu();
            if (!menu)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["vendor"] = idText(menu->mVendor);
            row["holder"] = idText(scripts().barterHolder(menu->mVendor));
            row["name"] = menu->mName;
            row["percent"] = menu->mPercent;
            row["serial"] = menu->mSerial;
            return row;
        };
        // What the menu lists on one side, "vendor" or "player": the things as `items` gives them, with the price of
        // one
        api["barterRows"] = [lua](std::string_view side) {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::BarterRow& entry : scripts().barterRows(side == "vendor"))
            {
                sol::table row = itemRow(lua, entry.mItem);
                row["price"] = entry.mPrice;
                rows[index++] = row;
            }
            return rows;
        };
        // What the two piles come to, each a list of {id, count} (what the player buys from the vendor, what it
        // sells): whether the piles are valid and empty, whether Accept works, the balance (plus when the vendor pays),
        // the caps that move, the caps of both sides
        api["barterQuote"] = [lua](sol::optional<sol::table> buy, sol::optional<sol::table> sell) {
            const OFScript::FalloutScripts::BarterQuote quote = scripts().barterQuote(pilesOf(buy), pilesOf(sell));
            sol::table row(lua, sol::create);
            row["valid"] = quote.mValid;
            row["empty"] = quote.mPilesEmpty;
            row["accept"] = quote.mAccept;
            row["balance"] = quote.mBalance;
            row["amount"] = quote.mAmount;
            row["playerCaps"] = quote.mPlayerCaps;
            row["vendorCaps"] = quote.mVendorCaps;
            return row;
        };
        // The player accepts the deal: {status = "done" | "empty" | "items" | "caps" | "closed", amount, received}
        api["barterDeal"] = [lua](sol::optional<sol::table> buy, sol::optional<sol::table> sell) {
            const OFScript::FalloutScripts::BarterOutcome outcome = scripts().barterDeal(pilesOf(buy), pilesOf(sell));
            sol::table row(lua, sol::create);
            row["status"] = outcome.mStatus;
            row["amount"] = outcome.mAmount;
            row["received"] = outcome.mReceived;
            return row;
        };
        api["closeBarter"] = []() { scripts().closeBarter(); };

        // The wheel of commands of a companion (the player activated a character that follows it): its id and name, a
        // number that is another for each time the wheel opens and what the companion's script said when it opened
        // (aggressive, staysFar, ranged, following, waiting) and whether a pad was in use; nothing when none is open
        api["companion"] = [lua]() -> sol::object {
            const auto wheel = scripts().companionWheel();
            if (!wheel)
                return sol::nil;
            sol::table row(lua, sol::create);
            row["actor"] = idText(wheel->mActor);
            row["name"] = wheel->mName;
            row["serial"] = wheel->mSerial;
            row["aggressive"] = wheel->mAggressive;
            row["staysFar"] = wheel->mStaysFar;
            row["ranged"] = wheel->mRanged;
            row["following"] = wheel->mFollowing;
            row["waiting"] = wheel->mWaiting;
            row["pad"] = wheel->mPad;
            return row;
        };
        // What the wheel tells about the companion and the player's stimpaks: stimpaks, health, maxHealth, carried,
        // capacity, weapon
        api["companionStatus"] = [lua]() {
            const OFScript::FalloutScripts::CompanionStatus status = scripts().companionStatus();
            sol::table row(lua, sol::create);
            row["stimpaks"] = status.mStimpaks;
            row["health"] = status.mHealth;
            row["maxHealth"] = status.mMaxHealth;
            row["carried"] = status.mCarried;
            row["capacity"] = status.mCapacity;
            row["weapon"] = status.mWeapon;
            return row;
        };
        // The companion says a topic of the commands, found by its editor id; false when there is nothing to say
        api["companionSay"] = [](std::string_view topic) { return scripts().companionSay(topic); };
        api["companionSetAggressive"] = [](bool aggressive) { scripts().companionSetAggressive(aggressive); };
        // A stimpak of the player's is used on the companion; false when the player has none
        api["companionStimpak"] = []() { return scripts().companionStimpak(); };
        // The wheel is over and the conversation with the companion begins
        api["companionTalk"] = []() { scripts().companionTalk(); };
        api["closeCompanion"] = []() { scripts().closeCompanionWheel(); };

        // The eight slots of the hot key wheel, in order: each a table with the slot (counted from 0), whether it takes
        // an item (the second is the ammunition swap), and what the player put there: the item's id, name, how many of
        // it the player carries, its picture and whether it is equipped (id is "" for an empty slot)
        api["hotKeys"] = [lua]() {
            sol::table rows(lua, sol::create);
            int index = 1;
            for (const OFScript::FalloutScripts::HotKeySlot& slot : scripts().hotKeySlots())
            {
                sol::table row(lua, sol::create);
                row["slot"] = slot.mSlot;
                row["assignable"] = slot.mAssignable;
                row["id"] = slot.mItem != 0 ? idText(slot.mItem) : std::string();
                row["name"] = slot.mName;
                row["count"] = slot.mCount;
                row["icon"] = slot.mIcon;
                row["equipped"] = slot.mEquipped;
                rows[index++] = row;
            }
            return rows;
        };
        // The player puts an item on the key of a slot (counted from 0): "assigned", "ammo" (ammunition goes on no
        // key), "cannot" (a thing that is neither held, worn nor used up) or "slot" (the ammunition swap, or none)
        api["assignHotKey"]
            = [](int slot, std::string_view item) { return scripts().assignHotKey(slot, parseRowId(item).mItem); };

        // Play the sound that a menu file or the code of the game names by its editor id ("UIMenuOK",
        // "ITMBottlecapsUp"), in 2D; false when the data has no such sound
        api["playSoundNamed"] = [](std::string_view name) { return scripts().playInterfaceSound(name); };

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

        // Whether the player is dead (by what the scripts did to it or by what the engine did: a fall, the breath), and
        // the game asks to load a save when the screen of its death is over
        api["playerDead"] = []() {
            return scripts().actors().isDead(scripts().playerReference())
                || OFBase::Environment::get().getStateManager()->playerDied();
        };
        api["askReload"] = []() { OFBase::Environment::get().getStateManager()->askLoadRecent(); };
        // The game's own box for the death has these: whether there is a save to reload, loading the most recent one,
        // and the main menu (which opens the menu that Escape does)
        api["hasRecentSave"] = []() { return OFBase::Environment::get().getStateManager()->hasRecentSave(); };
        api["reloadRecent"] = []() { OFBase::Environment::get().getStateManager()->loadRecent(); };
        api["openMainMenu"] = []() { OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_MainMenu); };
        return LuaUtil::makeReadOnly(api);
    }
}
