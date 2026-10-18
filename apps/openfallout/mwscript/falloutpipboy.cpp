#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/levelling.hpp>
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

        /// The most objects one drop makes (each is an object of its own in the world)
        constexpr int dropLimit = 10;
        /// The distance in front of the player that a dropped item lands at
        constexpr double dropDistance = 64;

        /// What a skill can be raised to by points
        constexpr double skillLimit = 100;
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
        return found != mGameSettingTexts->end() ? found->second : std::string();
    }

    std::vector<FalloutScripts::PipboyItem> FalloutScripts::pipboyItems(FalloutScript::FormId holder)
    {
        std::vector<PipboyItem> rows;
        std::vector<std::pair<FalloutScript::FormId, int>> items;
        if (!mActors.items(holder, items))
            return rows;
        for (const auto& [id, count] : items)
        {
            if (count <= 0 || id == masterForm(capsNumber))
                continue;
            const ESM::RefId record = refId(id);
            PipboyItem row;
            row.mId = id;
            row.mCount = count;
            if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(record))
            {
                row.mName = weapon->mFullName;
                row.mValue = static_cast<int>(weapon->mData.value);
                row.mWeight = weapon->mData.weight;
                row.mDamage = weapon->mData.damage;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Weapons;
            }
            else if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(record))
            {
                row.mName = armor->mFullName;
                row.mValue = static_cast<int>(armor->mData.value);
                row.mWeight = armor->mData.weight;
                row.mRating = armor->mHasRating ? armor->mRating : 0.0;
                row.mThreshold = armor->mDamageThreshold;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Apparel;
            }
            else if (const ESM4::Ammunition* ammo = mStore.get<ESM4::Ammunition>().search(record))
            {
                row.mName = ammo->mFullName;
                row.mValue = static_cast<int>(ammo->mData.mValue);
                row.mWeight = ammo->mData.mWeight;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Ammo;
            }
            else if (const ESM4::Potion* potion = mStore.get<ESM4::Potion>().search(record))
            {
                row.mName = potion->mFullName;
                row.mValue = potion->mItem.value;
                row.mWeight = potion->mData.weight;
                row.mCategory = FalloutScript::Pipboy::ItemCategory::Aid;
            }
            else if (const ESM4::MiscItem* misc = mStore.get<ESM4::MiscItem>().search(record))
            {
                row.mName = misc->mFullName;
                row.mValue = static_cast<int>(misc->mData.value);
                row.mWeight = misc->mData.weight;
            }
            else if (const ESM4::Book* book = mStore.get<ESM4::Book>().search(record))
            {
                row.mName = book->mFullName;
                row.mValue = static_cast<int>(book->mData.value);
                row.mWeight = book->mData.weight;
            }
            else if (const ESM4::Ingredient* ingredient = mStore.get<ESM4::Ingredient>().search(record))
            {
                row.mName = ingredient->mFullName;
                row.mValue = static_cast<int>(ingredient->mData.value);
                row.mWeight = ingredient->mData.weight;
            }
            else if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(record))
            {
                row.mName = mod->mFullName;
                row.mValue = static_cast<int>(mod->mData.mValue);
                row.mWeight = mod->mData.mWeight;
            }
            else if (const ESM4::Key* key = mStore.get<ESM4::Key>().search(record))
            {
                row.mName = key->mFullName;
                row.mValue = static_cast<int>(key->mData.value);
                row.mWeight = key->mData.weight;
            }
            else
                continue;
            if (row.mName.empty())
                row.mName = editorId(id);
            row.mEquipped = mEquipment.isEquipped(holder, id);
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
        mEquipment.inventoryWeight(mPlayer, stats.mWeight);
        stats.mMaxWeight = mActorValues.permanent(mPlayer, AV::carryWeight);
        stats.mCaps = mActors.itemCount(mPlayer, masterForm(capsNumber));
        stats.mSkillPoints = mProgress.skillPoints();
        stats.mPerkPoints = mProgress.perkPoints();
        stats.mArmorRating = mEquipment.armorRating(mPlayer);
        stats.mThreshold = mEquipment.damageThreshold(mPlayer);
        return stats;
    }

    std::vector<FalloutScripts::PipboyQuest> FalloutScripts::pipboyQuests()
    {
        std::vector<PipboyQuest> result;
        mQuests.forEachState([&result](FalloutScript::FormId id, const FalloutScript::QuestState& state) {
            if (state.mRecord == nullptr || !state.mRunning)
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

    namespace
    {
        FalloutScripts::PipboyPerk perkRow(const FalloutScript::PerkInfo& info, int rank)
        {
            return FalloutScripts::PipboyPerk{ info.mId, info.mName, info.mDescription, rank, info.mRanks, true };
        }
    }

    std::vector<FalloutScripts::PipboyPerk> FalloutScripts::pipboyPerks()
    {
        std::vector<PipboyPerk> result;
        for (const auto& [id, rank] : mProgress.perksOf(mPlayer))
        {
            FalloutScript::PerkInfo info;
            if (mPerks.info(id, info) && !info.mName.empty())
                result.push_back(perkRow(info, rank));
        }
        std::stable_sort(result.begin(), result.end(),
            [](const PipboyPerk& left, const PipboyPerk& right) { return left.mName < right.mName; });
        return result;
    }

    std::vector<FalloutScripts::PipboyPerk> FalloutScripts::pipboyOfferedPerks()
    {
        std::vector<PipboyPerk> result;
        for (const FalloutScript::PerkEngine::Offer& offer : mPerks.levelUpList(mPlayer, mProgress.level()))
        {
            FalloutScript::PerkInfo info;
            if (mPerks.info(offer.mPerk, info) && !info.mName.empty())
            {
                PipboyPerk row = perkRow(info, mProgress.perkRank(mPlayer, offer.mPerk));
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
        const double room = skillLimit - mActorValues.base(mPlayer, skill);
        points = std::min(points, static_cast<int>(std::floor(std::max(room, 0.0))));
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
        const std::vector<FalloutScript::FormId> offered = mPerks.offered(mPlayer, mProgress.level());
        if (std::find(offered.begin(), offered.end(), perk) == offered.end() || !mProgress.spendPerkPoint())
            return false;
        mProgress.addPerk(mPlayer, perk);
        mPerks.refresh(mPlayer);
        FalloutScript::PerkInfo info;
        if (mPerks.info(perk, info))
            log("Perk chosen: " + info.mName + ", rank " + std::to_string(mProgress.perkRank(mPlayer, perk)));
        return true;
    }

    std::string FalloutScripts::useItemFromList(FalloutScript::FormId item)
    {
        if (mActors.itemCount(mPlayer, item) <= 0)
            return std::string();
        FalloutScript::ItemInfo info;
        const bool known = itemInfo(item, info);
        if (known
            && (info.mKind == FalloutScript::ItemKind::Weapon || info.mKind == FalloutScript::ItemKind::Armor
                || info.mKind == FalloutScript::ItemKind::Ammo))
        {
            if (mEquipment.isEquipped(mPlayer, item))
            {
                mEquipment.unequip(mPlayer, item);
                return "unequipped";
            }
            return mEquipment.equip(mPlayer, item) == FalloutScript::Equipment::Outcome::Equipped ? "equipped"
                                                                                                  : std::string();
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

    int FalloutScripts::dropItem(FalloutScript::FormId item, int count)
    {
        count = std::min({ count, dropLimit, mActors.itemCount(mPlayer, item) });
        if (count <= 0)
            return 0;
        if (mEquipment.isEquipped(mPlayer, item) && mActors.itemCount(mPlayer, item) <= count)
            mEquipment.unequip(mPlayer, item);
        const int removed = mActors.removeItem(mPlayer, item, count);
        if (removed > 0)
            placeAtMe(mPlayer, item, removed, dropDistance, 0);
        return removed;
    }

    int FalloutScripts::transferItems(
        FalloutScript::FormId from, FalloutScript::FormId to, FalloutScript::FormId item, int count)
    {
        if (from == 0 || to == 0 || from == to || count <= 0)
            return 0;
        if (mEquipment.isEquipped(from, item) && mActors.itemCount(from, item) <= count)
            mEquipment.unequip(from, item);
        const int moved = mActors.removeItem(from, item, count);
        if (moved > 0)
            mActors.addItem(to, item, moved);
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
