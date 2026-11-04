#include "falloutscripts.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loaddobj.hpp>
#include <components/esm4/loadflst.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/script.hpp>
#include <components/falloutscript/exesettings.hpp>
#include <components/falloutscript/levelling.hpp>
#include <components/falloutscript/perks.hpp>
#include <components/falloutscript/settingtext.hpp>
#include <components/files/conversion.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        namespace AV = FalloutScript::ActorValue;

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        /// The local number, in the first master, of the bottle caps, the money of both games
        constexpr FalloutScript::FormId capsNumber = 0x0000000F;
        /// The local number, in the first master, of the weapon of bare hands (the executables take it for a blow that
        /// has no weapon)
        constexpr FalloutScript::FormId fistsNumber = 0x1F4;

        /// The distance in front of the player that a dropped item lands at
        constexpr double dropDistance = 64;
    }

    std::string FalloutScripts::gameSettingText(std::string_view name)
    {
        const std::lock_guard lock(mDerivedSettingsMutex);
        if (!mGameSettingTexts)
        {
            std::map<std::string, std::string> texts;
            for (const ESM4::GameSetting& setting : mStore.get<ESM4::GameSetting>())
                if (const auto* text = std::get_if<std::string>(&setting.mData))
                    texts[Misc::StringUtils::lowerCase(setting.mEditorId)] = *text;
            mGameSettingTexts = std::move(texts);
        }
        const auto found = mGameSettingTexts->find(Misc::StringUtils::lowerCase(name));
        if (found != mGameSettingTexts->end() && !found->second.empty())
            return found->second;
        // The words of the labels are in the executables of the games, not in the plugins (unless a plugin changed
        // them): the executable of the player's own copy when it was found, else the port's own words
        const auto executable = mExecutableTexts.find(Misc::StringUtils::lowerCase(name));
        if (executable != mExecutableTexts.end() && !executable->second.empty())
            return executable->second;
        return std::string(FalloutScript::SettingText::standIn(name));
    }

    void FalloutScripts::loadExecutableTexts(
        const std::vector<std::filesystem::path>& dataDirs, const ToUTF8::StatelessUtf8Encoder* encoder)
    {
        // Tale of Two Wastelands has the data of both games: the executable of the one that is played comes first, and
        // the other one gives what the first has not
        const bool newVegas = mGame == FalloutScript::Game::NewVegas;
        const std::array<std::string_view, 2> executables
            = { newVegas ? "FalloutNV.exe" : "Fallout3.exe", newVegas ? "Fallout3.exe" : "FalloutNV.exe" };
        FalloutScript::ExeSettings::Texts texts;
        for (const std::string_view executable : executables)
        {
            const std::filesystem::path path = FalloutScript::ExeSettings::find(dataDirs, executable);
            if (path.empty())
                continue;
            FalloutScript::ExeSettings::Texts found = FalloutScript::ExeSettings::read(path, encoder);
            Log(Debug::Info) << "Settings texts: " << found.size() << " read from " << Files::pathToUnicodeString(path);
            for (auto& [name, text] : found)
                texts.emplace(name, std::move(text));
        }
        if (texts.empty())
            Log(Debug::Info)
                << "Settings texts: no executable of the game found next to its data, the port's own words "
                   "are used for the labels";
        const std::lock_guard lock(mDerivedSettingsMutex);
        mExecutableTexts = std::move(texts);
    }

    std::vector<FalloutScripts::PipboyItem> FalloutScripts::pipboyItems(FalloutScript::FormId holder)
    {
        std::vector<PipboyItem> rows;
        std::vector<std::pair<FalloutScript::FormId, int>> items;
        if (!mActors.items(holder, items))
            return rows;
        const bool player = holder == mPlayer;
        // What the card of an item tells depends on what the player holds and on its perks
        const FalloutScript::FormId heldId = player ? mEquipment.weapon(mPlayer) : 0;
        const ESM4::Weapon* held = heldId != 0 ? mStore.get<ESM4::Weapon>().search(refId(heldId)) : nullptr;
        const bool packRat = player && mGame == FalloutScript::Game::NewVegas
            && mPerks.apply(FalloutScript::EntryPoint::packRat, mPlayer, 0.0) > 0;
        for (const auto& [id, count] : items)
        {
            // The caps of the player are on the status line; those of a container or a corpse are there to take
            if (count <= 0 || (player && id == masterForm(capsNumber)))
                continue;
            const ESM::RefId record = refId(id);
            PipboyItem row;
            row.mId = id;
            row.mCount = count;
            if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(record))
            {
                row.mName = weapon->mFullName;
                row.mIcon = weapon->mIcon;
                row.mValue = static_cast<int>(weapon->mData.value);
                row.mWeight = weapon->mData.weight;
                row.mDamage = weapon->mData.damage;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Weapons;
                if (player)
                {
                    row.mCardDamage = weaponCardDamage(id);
                    row.mClip = weapon->mData.clipSize;
                    if (weapon->mHasGameData)
                        row.mPellets = std::max<int>(weapon->mGame.mProjectileCount, 1);
                    const FalloutScript::FormId ammo = ammoOfWeapon(*weapon, mPlayer);
                    if (const ESM4::Ammunition* shot
                        = ammo != 0 ? mStore.get<ESM4::Ammunition>().search(refId(ammo)) : nullptr)
                    {
                        row.mHasAmmo = true;
                        row.mAmmoName = shot->mFullName;
                        // The rounds in the weapon are those of its clip, the rest are in reserve
                        const int have = mActors.itemCount(mPlayer, ammo);
                        row.mLoaded = std::min(row.mClip, have);
                        row.mReserve = have - row.mLoaded;
                    }
                }
            }
            else if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(record))
            {
                row.mName = armor->mFullName;
                row.mIcon = !armor->mIconMale.empty() ? armor->mIconMale : armor->mIconFemale;
                row.mValue = static_cast<int>(armor->mData.value);
                row.mWeight = armor->mData.weight;
                // A piece rates as the executables read it: the whole number of the rating
                row.mRating = armor->mHasRating ? std::trunc(armor->mRating) : 0.0;
                row.mThreshold = armor->mDamageThreshold;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Apparel;
                row.mHidden = (armor->mGeneralFlags & ESM4::Armor::FO3_NonPlayable) != 0;
            }
            else if (const ESM4::Ammunition* ammo = mStore.get<ESM4::Ammunition>().search(record))
            {
                row.mName = ammo->mFullName;
                row.mIcon = ammo->mIcon;
                row.mValue = static_cast<int>(ammo->mData.mValue);
                row.mWeight = ammo->mData.mWeight;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Ammo;
                // Ammunition is for the weapon in hand, and for nothing else
                row.mUsable = !player || (held != nullptr && weaponTakesAmmo(*held, id));
            }
            else if (const ESM4::Potion* potion = mStore.get<ESM4::Potion>().search(record))
            {
                row.mName = potion->mFullName;
                row.mIcon = potion->mIcon;
                row.mValue = potion->mItem.value;
                row.mWeight = potion->mData.weight;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Aid;
            }
            else if (const ESM4::MiscItem* misc = mStore.get<ESM4::MiscItem>().search(record))
            {
                row.mName = misc->mFullName;
                row.mIcon = misc->mIcon;
                row.mValue = static_cast<int>(misc->mData.value);
                row.mWeight = misc->mData.weight;
                row.mUsable = false;
            }
            else if (const ESM4::Book* book = mStore.get<ESM4::Book>().search(record))
            {
                // A book is listed with the things to use
                row.mName = book->mFullName;
                row.mIcon = book->mIcon;
                row.mValue = static_cast<int>(book->mData.value);
                row.mWeight = book->mData.weight;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Aid;
                row.mBook = true;
            }
            else if (const ESM4::Ingredient* ingredient = mStore.get<ESM4::Ingredient>().search(record))
            {
                row.mName = ingredient->mFullName;
                row.mIcon = ingredient->mIcon;
                row.mValue = static_cast<int>(ingredient->mData.value);
                row.mWeight = ingredient->mData.weight;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Aid;
                // The executables let it be used, but its effects are not read yet, so a use would do nothing
                row.mUsable = false;
            }
            else if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(record))
            {
                row.mName = mod->mFullName;
                row.mIcon = mod->mIcon;
                row.mValue = static_cast<int>(mod->mData.mValue);
                row.mWeight = mod->mData.mWeight;
                row.mUsable = false;
            }
            else if (const ESM4::Key* key = mStore.get<ESM4::Key>().search(record))
            {
                // A key is on the keyring and not in the lists of the tabs
                row.mName = key->mFullName;
                row.mIcon = key->mIcon;
                row.mValue = static_cast<int>(key->mData.value);
                row.mWeight = key->mData.weight;
                row.mKey = true;
                row.mUsable = false;
            }
            else
                continue;
            if (row.mName.empty())
                row.mName = editorId(id);
            row.mCardWeight = carriedWeight(row.mWeight, packRat);
            row.mEquipped = mEquipment.isEquipped(holder, id);
            row.mHasCondition
                = row.mCategory == FalloutScript::Pipboy::ItemCategory::Weapons
                || row.mCategory == FalloutScript::Pipboy::ItemCategory::Apparel;
            if (row.mHasCondition)
            {
                // Copies with another condition are a row of their own (the best first)
                const std::vector<FalloutScript::ActorState::ConditionStack> stacks = mActors.conditions(holder, id);
                if (stacks.size() > 1
                    || (stacks.size() == 1
                        && (stacks.front().mCondition != FalloutScript::ActorState::fullCondition
                            || stacks.front().mMods != 0)))
                {
                    for (const FalloutScript::ActorState::ConditionStack& stack : stacks)
                    {
                        PipboyItem copy = row;
                        copy.mCount = stack.mCount;
                        copy.mCondition = stack.mCondition;
                        copy.mMods = stack.mMods;
                        copy.mStack = true;
                        // What the card says is what the copy does at its condition and with its mods
                        if (copy.mCategory == FalloutScript::Pipboy::ItemCategory::Weapons && player)
                            copy.mCardDamage = weaponCardDamage(id, stack.mCondition, stack.mMods);
                        else if (copy.mCategory == FalloutScript::Pipboy::ItemCategory::Apparel)
                        {
                            const double factor = FalloutScript::Wear::armorFactor(
                                static_cast<double>(stack.mCondition) / FalloutScript::ActorState::fullCondition);
                            copy.mRating = std::trunc(row.mRating * factor);
                            // The executable cuts the threshold of the record to a whole number and the product again (0x646360), as the
                            // repair screen does
                            copy.mThreshold = std::trunc(std::trunc(row.mThreshold) * factor);
                        }
                        // What the mods installed in a weapon do to what its card tells (FalloutNV.exe 0x4BE380, 0x4FE160)
                        if (stack.mMods != 0 && copy.mCategory == FalloutScript::Pipboy::ItemCategory::Weapons)
                        {
                            FalloutScript::ItemMods::Slots slots;
                            if (modSlotsOf(id, slots))
                            {
                                if (const std::optional<double> less = FalloutScript::ItemMods::installedValue(
                                        stack.mMods, slots, FalloutScript::ItemMods::Weight))
                                {
                                    copy.mWeight = std::max(row.mWeight - *less, 0.0);
                                    copy.mCardWeight = carriedWeight(copy.mWeight, packRat);
                                }
                                if (const std::optional<double> more = FalloutScript::ItemMods::installedValue(
                                        stack.mMods, slots, FalloutScript::ItemMods::ClipSize))
                                {
                                    copy.mClip = row.mClip + static_cast<int>(*more);
                                    const int have = row.mLoaded + row.mReserve;
                                    copy.mLoaded = std::min(copy.mClip, have);
                                    copy.mReserve = have - copy.mLoaded;
                                }
                            }
                        }
                        copy.mEquipped = mEquipment.isEquippedCopy(holder, id, stack.copy());
                        rows.push_back(std::move(copy));
                    }
                    continue;
                }
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }

    FalloutScripts::PipboyStats FalloutScripts::pipboyStats()
    {
        ensureLevellingRules();
        PipboyStats stats;
        const FalloutScript::LevellingRules rules = mProgress.rules();
        stats.mLevel = mProgress.level();
        stats.mExperience = mProgress.experience();
        stats.mLevelStart = FalloutScript::Levelling::experienceForLevel(rules, stats.mLevel);
        stats.mNextLevel = mProgress.experienceForNextLevel();
        stats.mKarma = mProgress.karma();
        const auto lookup = [this](std::string_view name) { return gameSetting(name); };
        const FalloutScript::Pipboy::KarmaClass karma
            = FalloutScript::Pipboy::karmaClass(FalloutScript::Pipboy::karmaRules(lookup), stats.mKarma);
        stats.mKarmaTitle = gameSettingText(FalloutScript::Pipboy::karmaTitleSetting(stats.mLevel, karma));
        stats.mHealth = mActorValues.current(mPlayer, AV::health);
        stats.mMaxHealth = mActorValues.permanent(mPlayer, AV::health);
        stats.mActionPoints = mActorValues.current(mPlayer, AV::actionPoints);
        stats.mMaxActionPoints = mActorValues.permanent(mPlayer, AV::actionPoints);
        stats.mNewVegas = mGame == FalloutScript::Game::NewVegas;
        // What is carried weighs what its cards say (a perk can lighten the small things)
        std::vector<std::pair<FalloutScript::FormId, int>> held;
        if (mActors.items(mPlayer, held))
        {
            const bool packRat = stats.mNewVegas && mPerks.apply(FalloutScript::EntryPoint::packRat, mPlayer, 0.0) > 0;
            for (const auto& [item, count] : held)
            {
                FalloutScript::ItemInfo info;
                if (itemInfo(item, info))
                {
                    stats.mWeight += carriedWeight(info.mWeight, packRat) * count;
                    // A mod of the effect 4 makes the weapon it is installed in lighter
                    if (info.mKind == FalloutScript::ItemKind::Weapon)
                    {
                        FalloutScript::ItemMods::Slots slots;
                        if (modSlotsOf(item, slots))
                            for (const FalloutScript::ActorState::ConditionStack& stack : mActors.conditions(mPlayer, item))
                                if (const std::optional<double> less = stack.mMods != 0
                                        ? FalloutScript::ItemMods::installedValue(
                                            stack.mMods, slots, FalloutScript::ItemMods::Weight)
                                        : std::nullopt)
                                    stats.mWeight -= (carriedWeight(info.mWeight, packRat)
                                                         - carriedWeight(std::max(info.mWeight - *less, 0.0), packRat))
                                        * stack.mCount;
                    }
                }
            }
        }
        stats.mMaxWeight = mActorValues.permanent(mPlayer, AV::carryWeight);
        stats.mCaps = mActors.itemCount(mPlayer, masterForm(capsNumber));
        stats.mSkillPoints = mProgress.skillPoints();
        stats.mPerkPoints = mProgress.perkPoints();
        // The resistance is a whole number and goes no higher than the most that armor takes away
        stats.mArmorRating
            = std::min(std::trunc(mEquipment.armorRating(mPlayer)), std::trunc(damageSettings().mMaxResistance));
        if (stats.mNewVegas)
        {
            // The threshold of what is worn and of the perks, against the weapon in hand (the fists for none)
            FalloutScript::FormId weapon = mEquipment.weapon(mPlayer);
            if (weapon == 0)
                weapon = masterForm(fistsNumber);
            const FalloutScript::PerkEngine::Context context{ 0, weapon };
            stats.mThreshold = mEquipment.damageThreshold(mPlayer)
                + mPerks.apply(FalloutScript::EntryPoint::thresholdDefender, mPlayer, 0.0, context);
        }
        return stats;
    }

    std::vector<FalloutScripts::PipboyQuest> FalloutScripts::pipboyQuests()
    {
        std::vector<PipboyQuest> result;
        mQuests.forEachState([&result](FalloutScript::FormId id, const FalloutScript::QuestState& state) {
            if (!state.listed())
                return;
            PipboyQuest quest;
            quest.mId = id;
            quest.mName = state.mRecord->mQuestName;
            quest.mCompleted = state.mCompleted || state.mFailed;
            for (const ESM4::QuestObjective& objective : state.mRecord->mObjectives)
            {
                const auto found = state.mObjectives.find(objective.mIndex);
                if (found == state.mObjectives.end() || !found->second.mDisplayed || objective.mText.empty())
                    continue;
                quest.mObjectives.push_back({ objective.mText, found->second.mCompleted });
            }
            // A quest with a name and something to do is in the list; so is one that is done, to tell it
            if (!quest.mName.empty() && (!quest.mObjectives.empty() || quest.mCompleted))
                result.push_back(std::move(quest));
        });
        std::stable_sort(result.begin(), result.end(), [](const PipboyQuest& left, const PipboyQuest& right) {
            if (left.mCompleted != right.mCompleted)
                return !left.mCompleted;
            return left.mName < right.mName;
        });
        return result;
    }

    std::string FalloutScripts::actorValueName(int number) const
    {
        // The attributes are the actor values 5 to 11 (the records are named after them with AV in front)
        constexpr std::array<std::pair<const char*, const char*>, FalloutScript::Chargen::attributeCount> attributes
            = { {
                { "AVStrength", "Strength" },
                { "AVPerception", "Perception" },
                { "AVEndurance", "Endurance" },
                { "AVCharisma", "Charisma" },
                { "AVIntelligence", "Intelligence" },
                { "AVAgility", "Agility" },
                { "AVLuck", "Luck" },
            } };
        const int attribute = number - FalloutScript::Chargen::firstAttribute;
        if (attribute >= 0 && attribute < FalloutScript::Chargen::attributeCount)
            return actorValueText(attributes[attribute].first, attributes[attribute].second).mName;
        for (const FalloutScript::ChargenSkill& skill : mChargen.skills())
            if (skill.mNumber == number)
                return actorValueText(skill.mEditorId, skill.mName).mName;
        return {};
    }

    FalloutScripts::PipboyPerk FalloutScripts::pipboyPerkRow(const FalloutScript::PerkInfo& info, int rank)
    {
        PipboyPerk row;
        row.mId = info.mId;
        row.mName = info.mName;
        row.mDescription = info.mDescription;
        row.mIcon = info.mIcon;
        row.mMinimumLevel = info.mMinimumLevel;
        row.mRank = rank;
        row.mRanks = info.mRanks;
        // The level-up screen writes a line for the conditions that ask for an actor value or for another perk, and
        // nothing for the rest (the level, the sex, quests), which still count where the list is joined
        for (const ESM4::TargetCondition& condition : info.mRequirements)
        {
            PipboyRequirement requirement;
            requirement.mComparison = static_cast<int>((condition.condition >> 5) & 7);
            requirement.mOr = (condition.condition & ESM4::CTF_Combine) != 0;
            if (condition.functionIndex == ESM4::FUN_GetPermanentActorValue)
            {
                requirement.mKind = PipboyRequirement::Kind::ActorValue;
                requirement.mActorValue = static_cast<int>(condition.param1);
                requirement.mName = actorValueName(requirement.mActorValue);
                // The number of the condition is a float; the screen writes its whole part
                requirement.mNumber = static_cast<int>(condition.comparison);
            }
            else if (condition.functionIndex == ESM4::FUN_HasPerk)
            {
                requirement.mKind = PipboyRequirement::Kind::Perk;
                FalloutScript::PerkInfo needed;
                if (mPerks.info(static_cast<FalloutScript::FormId>(condition.param1), needed))
                    requirement.mName = needed.mName;
                requirement.mNumber = static_cast<int>(condition.param2);
            }
            row.mRequirements.push_back(std::move(requirement));
        }
        return row;
    }

    std::vector<FalloutScripts::PipboyPerk> FalloutScripts::pipboyPerks()
    {
        std::vector<PipboyPerk> result;
        for (const auto& [id, rank] : mProgress.perksOf(mPlayer))
        {
            FalloutScript::PerkInfo info;
            if (mPerks.info(id, info) && !info.mName.empty())
                result.push_back(pipboyPerkRow(info, rank));
        }
        std::stable_sort(result.begin(), result.end(),
            [](const PipboyPerk& left, const PipboyPerk& right) { return left.mName < right.mName; });
        return result;
    }

    std::vector<FalloutScripts::PipboyPerk> FalloutScripts::pipboyOfferedPerks()
    {
        std::vector<PipboyPerk> result;
        for (const FalloutScript::PerkEngine::Offer& offer : mPerks.levelUpList(mPlayer, mProgress.perkLevel()))
        {
            FalloutScript::PerkInfo info;
            if (mPerks.info(offer.mPerk, info) && !info.mName.empty())
            {
                PipboyPerk row = pipboyPerkRow(info, mProgress.perkRank(mPlayer, offer.mPerk));
                row.mAvailable = offer.mAvailable;
                result.push_back(std::move(row));
            }
        }
        return result;
    }

    int FalloutScripts::spendSkillPoints(int skill, int points)
    {
        if (skill < AV::firstSkill || skill > AV::lastSkill || points <= 0)
            return 0;
        // The room is the room under 100 of the value, as on the screen of the level up: a bonus takes room away, a
        // penalty makes some
        points = std::min(points, FalloutScript::Levelling::skillRoom(mActorValues.current(mPlayer, skill)));
        if (points <= 0 || !mProgress.spendSkillPoints(points))
            return 0;
        mActorValues.setBase(mPlayer, skill, mActorValues.base(mPlayer, skill) + points);
        return points;
    }

    int FalloutScripts::refundSkillPoints(int skill, int points)
    {
        if (skill < AV::firstSkill || skill > AV::lastSkill || points <= 0)
            return 0;
        const double base = mActorValues.base(mPlayer, skill);
        points = std::min(points, static_cast<int>(std::floor(std::max(base, 0.0))));
        if (points <= 0)
            return 0;
        mActorValues.setBase(mPlayer, skill, base - points);
        mProgress.addSkillPoints(points);
        return points;
    }

    bool FalloutScripts::choosePerk(FalloutScript::FormId perk)
    {
        if (mProgress.perkPoints() <= 0)
            return false;
        const std::vector<FalloutScript::FormId> offered = mPerks.offered(mPlayer, mProgress.perkLevel());
        if (std::find(offered.begin(), offered.end(), perk) == offered.end() || !mProgress.spendPerkPoint())
            return false;
        mProgress.addPerk(mPlayer, perk);
        mPerks.refresh(mPlayer);
        FalloutScript::PerkInfo info;
        if (mPerks.info(perk, info))
            log("Perk chosen: " + info.mName + ", rank " + std::to_string(mProgress.perkRank(mPlayer, perk)));
        return true;
    }

    bool FalloutScripts::atFullHealthFor(FalloutScript::FormId item)
    {
        // The stimpak mends the health, the doctor's bag of New Vegas the health and the limbs: neither is used up on
        // a player who has nothing to mend (the objects are those the default object record names)
        for (const ESM4::DefaultObj& objects : mStore.get<ESM4::DefaultObj>())
        {
            const auto health = [this] {
                return mActorValues.current(mPlayer, AV::health) >= mActorValues.permanent(mPlayer, AV::health);
            };
            if (!objects.mData.stimpack.isZeroOrUnset() && objects.mData.stimpack.toUint32() == item)
                return health();
            if (mGame == FalloutScript::Game::NewVegas && !objects.mData.doctorsBag.isZeroOrUnset()
                && objects.mData.doctorsBag.toUint32() == item)
            {
                // The conditions the bag restores (the executables name the six of the body; the player has no more,
                // the seventh is the brain of a robot)
                for (int limb = AV::firstCondition; limb <= AV::lastCondition; ++limb)
                    if (mActorValues.current(mPlayer, limb) < mActorValues.permanent(mPlayer, limb))
                        return false;
                return true;
            }
        }
        return false;
    }

    std::string FalloutScripts::useItemFromList(FalloutScript::FormId item, int condition)
    {
        if (mActors.itemCount(mPlayer, item) <= 0)
            return std::string();
        FalloutScript::ItemInfo info;
        const bool known = itemInfo(item, info);
        if (known
            && (info.mKind == FalloutScript::ItemKind::Weapon || info.mKind == FalloutScript::ItemKind::Armor
                || info.mKind == FalloutScript::ItemKind::Ammo))
        {
            // No condition asked (a hot key names the form): the copy that is on, else the best one
            if (condition < 0)
            {
                condition = mEquipment.equippedCopy(mPlayer, item);
                if (condition < 0)
                {
                    const std::vector<FalloutScript::ActorState::ConditionStack> stacks
                        = mActors.conditions(mPlayer, item);
                    condition = stacks.empty() ? int(FalloutScript::ActorState::fullCondition) : stacks.front().copy();
                }
            }
            if (mEquipment.isEquippedCopy(mPlayer, item, condition))
            {
                mEquipment.unequip(mPlayer, item);
                return "unequipped";
            }
            // Ammunition is put on only while the weapon in hand takes it
            if (info.mKind == FalloutScript::ItemKind::Ammo)
            {
                const FalloutScript::FormId heldId = mEquipment.weapon(mPlayer);
                const ESM4::Weapon* held = heldId != 0 ? mStore.get<ESM4::Weapon>().search(refId(heldId)) : nullptr;
                if (held == nullptr || !weaponTakesAmmo(*held, item))
                    return std::string();
            }
            const FalloutScript::Equipment::Outcome outcome = mEquipment.equipCopy(mPlayer, item, condition);
            if (outcome == FalloutScript::Equipment::Outcome::Broken)
            {
                // A weapon or armor with no health left does not go on
                const std::string text = gameSettingText("sCantEquipBrokenItem");
                if (!text.empty())
                    tellPlayer(text);
            }
            return outcome == FalloutScript::Equipment::Outcome::Equipped ? "equipped" : std::string();
        }
        if (known && info.mKind == FalloutScript::ItemKind::Aid && atFullHealthFor(item))
        {
            const std::string text = gameSettingText("sFullHealth");
            tellPlayer(text.empty() ? "You are already at full health." : text);
            return std::string();
        }
        const FalloutScript::ItemUse::Result result = useItem(mPlayer, item);
        switch (result.mOutcome)
        {
            case FalloutScript::ItemUse::Outcome::Consumed:
                return "used";
            case FalloutScript::ItemUse::Outcome::Read:
                return "read";
            default:
                break;
        }
        return std::string();
    }

    int FalloutScripts::dropItem(FalloutScript::FormId item, int count, int condition)
    {
        const std::vector<FalloutScript::ActorState::ConditionStack> stacks = mActors.conditions(mPlayer, item);
        if (stacks.empty())
            return 0;
        // No condition asked: the best copies go
        condition = condition < 0 ? stacks.front().copy() : condition;
        int have = 0;
        for (const FalloutScript::ActorState::ConditionStack& stack : stacks)
            if (stack.copy() == condition)
                have = stack.mCount;
        count = std::min({ count, sDropLimit, have });
        if (count <= 0)
            return 0;
        // The copy that is on goes off when the last copy like it leaves
        if (mEquipment.isEquippedCopy(mPlayer, item, condition) && have <= count)
            mEquipment.unequip(mPlayer, item);
        const int removed = mActors.removeCopy(mPlayer, item, count, condition);
        if (removed > 0)
        {
            // Each copy is an object of its own in the world and keeps how worn it is
            const FalloutScript::FormId first = placeAtMe(mPlayer, item, removed, dropDistance, 0);
            if (first != 0 && condition != FalloutScript::ActorState::fullCondition)
                for (int i = 0; i < removed; ++i)
                    mReferenceState.setCondition(
                        first + i, FalloutScript::copyCondition(condition), FalloutScript::copyMods(condition));
        }
        return removed;
    }

    int FalloutScripts::transferItems(FalloutScript::FormId from, FalloutScript::FormId to, FalloutScript::FormId item,
        int count, int condition)
    {
        if (from == 0 || to == 0 || from == to || count <= 0)
            return 0;
        // The copies that go, and the condition each has
        std::vector<FalloutScript::ActorState::ConditionStack> going;
        int left = count;
        for (const FalloutScript::ActorState::ConditionStack& stack : mActors.conditions(from, item))
        {
            if (left <= 0)
                break;
            if (condition >= 0 && stack.copy() != condition)
                continue;
            const int taking = std::min(left, stack.mCount);
            going.push_back({ stack.mCondition, taking, stack.mMods });
            left -= taking;
        }
        int moved = 0;
        for (const FalloutScript::ActorState::ConditionStack& stack : going)
        {
            // The copy that is on goes off when the last copy like it leaves
            if (mEquipment.isEquippedCopy(from, item, stack.copy()))
            {
                int here = 0;
                for (const FalloutScript::ActorState::ConditionStack& held : mActors.conditions(from, item))
                    if (held.copy() == stack.copy())
                        here = held.mCount;
                if (here <= stack.mCount)
                    mEquipment.unequip(from, item);
            }
            const int taken = mActors.removeCopy(from, item, stack.mCount, stack.copy());
            if (taken > 0)
            {
                mActors.addCopy(to, item, taken, stack.copy());
                moved += taken;
            }
        }
        return moved;
    }

    void FalloutScripts::containerOpened(FalloutScript::FormId container, bool opened)
    {
        if (container == 0)
            return;
        runOnMainThread([this, container, opened] {
            mObjects.trigger(
                container, opened ? FalloutScript::BlockType::OnOpen : FalloutScript::BlockType::OnClose, mPlayer);
        });
    }

    bool FalloutScripts::openContainer(const OFWorld::Ptr& object, const OFWorld::Ptr& actor)
    {
        if (object.isEmpty() || actor != OFBase::Environment::get().getWorld()->getPlayerPtr())
            return false;
        // A container, or a character that is dead; the things of anything else are not for taking this way
        if (object.getType() != ESM4::Container::sRecordId && !isDead(object))
            return false;
        const FalloutScript::FormId reference = referenceOf(object);
        if (reference == 0)
            return false;
        if (mReferenceState.isLocked(reference))
        {
            const std::string locked = gameSettingText("sLocked");
            tellPlayer(locked.empty() ? "It is locked." : locked);
            return false;
        }
        std::string name(object.getClass().getName(object));
        {
            const std::lock_guard lock(mLootMutex);
            mLoot = Loot{ reference, std::move(name), ++mLootSerial };
        }
        containerOpened(reference, true);
        return true;
    }

    bool FalloutScripts::openTeammateContainer(FalloutScript::FormId teammate)
    {
        if (teammate == 0 || teammate == mPlayer)
            return false;
        const OFWorld::Ptr object = findReference(teammate);
        if (object.isEmpty())
            return false;
        std::string name(object.getClass().getName(object));
        {
            const std::lock_guard lock(mLootMutex);
            mLoot = Loot{ teammate, std::move(name), ++mLootSerial };
        }
        containerOpened(teammate, true);
        return true;
    }

    std::optional<FalloutScripts::Loot> FalloutScripts::loot()
    {
        const std::lock_guard lock(mLootMutex);
        return mLoot;
    }

    void FalloutScripts::closeLoot()
    {
        FalloutScript::FormId holder = 0;
        {
            const std::lock_guard lock(mLootMutex);
            if (mLoot)
                holder = mLoot->mHolder;
            mLoot.reset();
        }
        if (holder != 0)
            containerOpened(holder, false);
    }
}
