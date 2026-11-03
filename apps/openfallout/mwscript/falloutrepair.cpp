#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadflst.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/perks.hpp>
#include <components/falloutscript/wear.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        namespace AV = FalloutScript::ActorValue;

        constexpr FalloutScript::FormId capsNumber = 0x0000000F;
        /// The statistic that counts the items repaired (FalloutNV.exe 0x7B5F5F adds to the one of the number 0x22)
        constexpr int itemsRepairedStat = 0x22;
        /// The record flag of an item that belongs to a quest
        constexpr std::uint32_t questItemFlag = 0x400;

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        /// A copy whose health in percent rounds up to the whole: nothing left to repair in the list of copies that
        /// repair an item of the same kind (FalloutNV.exe 0x7B6C00)
        bool nearlyWhole(int condition)
        {
            return (condition + 9) / 10 >= 100;
        }
    }

    const FalloutScript::Repair::Settings& FalloutScripts::repairSettings()
    {
        const std::lock_guard lock(mDerivedSettingsMutex);
        if (!mRepairSettings)
        {
            const std::map<std::string, double>& settings = gameSettingValues();
            mRepairSettings = FalloutScript::Repair::Settings::fromGameSettings(
                [&settings](std::string_view name) -> std::optional<double> {
                    const auto found = settings.find(Misc::StringUtils::lowerCase(name));
                    return found != settings.end() ? std::optional<double>(found->second) : std::nullopt;
                });
        }
        return *mRepairSettings;
    }

    FalloutScripts::RepairStat FalloutScripts::repairStatAt(FalloutScript::FormId item, int condition)
    {
        RepairStat stat;
        FalloutScript::ItemInfo info;
        if (!itemInfo(item, info))
            return stat;
        if (info.mKind == FalloutScript::ItemKind::Weapon)
        {
            stat.mDamage = static_cast<int>(std::ceil(weaponCardDamage(item, condition) - 1e-9));
        }
        else if (info.mKind == FalloutScript::ItemKind::Armor)
        {
            // The executable works the whole number of the threshold and of the rating with the factor of the condition,
            // and shows the whole number of that (0x646360)
            const double factor = FalloutScript::Wear::armorFactor(
                static_cast<double>(condition) / FalloutScript::ActorState::fullCondition);
            stat.mThreshold = static_cast<int>(std::trunc(std::trunc(info.mThreshold) * factor));
            stat.mRating = static_cast<int>(std::trunc(info.mRating * factor));
        }
        return stat;
    }

    bool FalloutScripts::isQuestItem(FalloutScript::FormId item)
    {
        const ESM::RefId record = refId(item);
        if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(record))
            return (weapon->mFlags & questItemFlag) != 0;
        if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(record))
            return (armor->mFlags & questItemFlag) != 0;
        return false;
    }

    bool FalloutScripts::repairsWith(FalloutScript::FormId item, FalloutScript::FormId other)
    {
        const ESM::RefId record = refId(item);
        const ESM::RefId otherRecord = refId(other);
        const auto listHas = [this, other](const ESM::FormId& list) {
            if (list.isZeroOrUnset())
                return false;
            const ESM4::FormIdList* found = mStore.get<ESM4::FormIdList>().search(list);
            return found != nullptr
                && std::any_of(found->mObjects.begin(), found->mObjects.end(),
                    [other](const ESM::FormId& object) { return object.toUint32() == other; });
        };
        if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(record))
        {
            if (listHas(weapon->mRepairList))
                return true;
            // The perk that lets one weapon repair another of the same kind: a weapon for the player with the same skill
            // and the same way of being held (the exe's 0x47BB50 for the entry point 0x30)
            const ESM4::Weapon* candidate = mStore.get<ESM4::Weapon>().search(otherRecord);
            return candidate != nullptr && weapon->mHasGameData && candidate->mHasGameData
                && mPerks.apply(FalloutScript::EntryPoint::repairWithSimilar, mPlayer, 0.0) > 0
                && (candidate->mGame.mFlags1 & ESM4::Weapon::GameData::NonPlayable) == 0
                && candidate->mGame.mSkill == weapon->mGame.mSkill
                && candidate->mGame.mAnimationType == weapon->mGame.mAnimationType;
        }
        if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(record))
            return listHas(armor->mRepairList);
        return false;
    }

    std::optional<FalloutScripts::RepairView> FalloutScripts::repairView(FalloutScript::FormId item, int condition)
    {
        FalloutScript::ItemInfo info;
        if (item == 0 || !itemInfo(item, info)
            || (info.mKind != FalloutScript::ItemKind::Weapon && info.mKind != FalloutScript::ItemKind::Armor))
            return std::nullopt;
        const std::vector<FalloutScript::ActorState::ConditionStack> stacks = mActors.conditions(mPlayer, item);
        if (stacks.empty())
            return std::nullopt;
        // No copy named: the one that is on, else the best
        if (condition < 0)
        {
            condition = mEquipment.equippedCondition(mPlayer, item);
            if (condition < 0)
                condition = stacks.front().mCondition;
        }
        if (std::none_of(stacks.begin(), stacks.end(),
                [condition](const FalloutScript::ActorState::ConditionStack& stack) {
                    return stack.mCondition == condition;
                }))
            return std::nullopt;

        RepairView view;
        view.mItem = item;
        view.mCondition = condition;
        view.mWorn = mEquipment.isEquipped(mPlayer, item, condition);
        view.mSkill = static_cast<int>(mActorValues.current(mPlayer, AV::repair));
        view.mStat = repairStatAt(item, condition);
        const FalloutScript::Repair::Settings settings = repairSettings();

        // What the copy does at each condition a donor could leave it in (a stack of alike donors asks once)
        std::map<int, RepairStat> statsAt;
        std::vector<PipboyItem> rows = pipboyItems(mPlayer);
        for (const PipboyItem& row : rows)
            if (row.mId == item)
            {
                view.mName = row.mName;
                view.mIcon = row.mIcon;
                break;
            }
        for (const PipboyItem& row : rows)
        {
            if (!row.mHasCondition || row.mCount <= 0 || row.mHidden)
                continue;
            int units = row.mCount;
            bool wornUnit = row.mEquipped;
            if (row.mId == item)
            {
                // A copy of the same item that is whole or nearly is no use; the copy that is repaired is not its own
                // donor, and the copy that is on is not used up
                if (nearlyWhole(row.mCondition))
                    continue;
                if (row.mCondition == condition)
                {
                    units -= 1;
                    wornUnit = false;
                }
                else if (wornUnit)
                {
                    units -= 1;
                    wornUnit = false;
                }
            }
            else
            {
                if (isQuestItem(row.mId) || !repairsWith(item, row.mId))
                    continue;
            }
            for (int unit = 0; unit < units; ++unit)
            {
                RepairDonor donor;
                donor.mItem = row.mId;
                donor.mCondition = row.mCondition;
                donor.mName = row.mName;
                donor.mWorn = wornUnit && unit == 0;
                donor.mResult = FalloutScript::Repair::repairedCondition(view.mSkill, condition, row.mCondition, settings);
                const auto known = statsAt.find(donor.mResult);
                donor.mStat = known != statsAt.end() ? known->second
                                                     : statsAt.emplace(donor.mResult, repairStatAt(item, donor.mResult))
                                                           .first->second;
                view.mDonors.push_back(std::move(donor));
            }
        }
        // The weakest donor first (the one that is worth least to the player), the one that is on first of two alike
        std::stable_sort(view.mDonors.begin(), view.mDonors.end(), [](const RepairDonor& a, const RepairDonor& b) {
            if (a.mCondition != b.mCondition)
                return a.mCondition < b.mCondition;
            return a.mWorn && !b.mWorn;
        });
        return view;
    }

    bool FalloutScripts::canRepairItem(FalloutScript::FormId item, int condition)
    {
        const std::optional<RepairView> view = repairView(item, condition);
        // A copy whose health in percent rounds up to the whole needs none; any other has a thing to repair it with
        return view && !nearlyWhole(view->mCondition) && !view->mDonors.empty();
    }

    int FalloutScripts::repairItem(
        FalloutScript::FormId item, int condition, FalloutScript::FormId donorItem, int donorCondition)
    {
        const std::optional<RepairView> view = repairView(item, condition);
        if (!view || nearlyWhole(view->mCondition))
            return -1;
        const auto donor = std::find_if(view->mDonors.begin(), view->mDonors.end(), [&](const RepairDonor& candidate) {
            return candidate.mItem == donorItem && candidate.mCondition == donorCondition;
        });
        if (donor == view->mDonors.end())
            return -1;
        const int result = donor->mResult;
        // The donor is used up, one copy of it; the copy of another kind that is on comes off with its last copy
        // (only the copies that are alike count: another copy of the donor with its own condition stays in the pack)
        if (donorItem != item && mEquipment.isEquipped(mPlayer, donorItem, donorCondition))
        {
            int alike = 0;
            for (const FalloutScript::ActorState::ConditionStack& held : mActors.conditions(mPlayer, donorItem))
                if (held.mCondition == donorCondition)
                    alike = held.mCount;
            if (alike <= 1)
                mEquipment.unequip(mPlayer, donorItem);
        }
        if (mActors.removeItem(mPlayer, donorItem, 1, donorCondition) <= 0)
            return -1;
        // The copy that is repaired stays on if it was on (the executable changes its list in place, and a list that is
        // worn is not merged with another)
        if (view->mWorn && mEquipment.isEquipped(mPlayer, item, view->mCondition))
            mEquipment.setEquippedCondition(mPlayer, item, result);
        else
            mActors.setCondition(mPlayer, item, view->mCondition, result, 1);
        mProgress.modifyMiscStat(itemsRepairedStat, 1);
        return result;
    }

    // ---- The repair services of a vendor ----

    bool FalloutScripts::openRepairServices(FalloutScript::FormId vendor)
    {
        // The command is run on a character that repairs for the player; the player's own call is nothing
        if (vendor == 0 || vendor == mPlayer)
            return false;
        const std::lock_guard lock(mRepairMutex);
        mRepairServices = RepairServices{ vendor, std::string(), ++mRepairSerial, -1 };
        return true;
    }

    std::optional<FalloutScripts::RepairServices> FalloutScripts::repairServices()
    {
        std::optional<RepairServices> menu;
        {
            const std::lock_guard lock(mRepairMutex);
            menu = mRepairServices;
        }
        if (!menu)
            return menu;
        // The skill of the vendor is read once, when the menu opens, and never again (FalloutNV.exe 0x7B76EE); the name
        // is the one of its reference, which only the thread of the frame may look up
        if (menu->mSkill < 0 || menu->mName.empty())
        {
            if (menu->mSkill < 0)
                menu->mSkill = static_cast<int>(mActorValues.current(menu->mVendor, AV::repair));
            const OFWorld::Ptr ptr = findReference(menu->mVendor);
            if (!ptr.isEmpty())
                menu->mName = std::string(ptr.getClass().getName(ptr));
            const std::lock_guard lock(mRepairMutex);
            if (mRepairServices && mRepairServices->mSerial == menu->mSerial)
            {
                mRepairServices->mSkill = std::max(menu->mSkill, 0);
                mRepairServices->mName = menu->mName;
            }
            menu->mSkill = std::max(menu->mSkill, 0);
        }
        return menu;
    }

    void FalloutScripts::closeRepairServices()
    {
        FalloutScript::FormId vendor = 0;
        {
            const std::lock_guard lock(mRepairMutex);
            if (mRepairServices)
                vendor = mRepairServices->mVendor;
            mRepairServices.reset();
        }
        // The vendor says a line of the topic of subtype RepairExit when a conversation is open (FalloutNV.exe 0x7B78E0 and
        // 0x7640A0)
        if (vendor != 0)
        {
            FalloutScript::FormId topic = 0;
            {
                const std::lock_guard lock(mDialogueMutex);
                if (mDialogue.active())
                    topic = mDialogue.findTopic("RepairExit");
            }
            if (topic != 0)
                sayTopic(vendor, topic, 0, false, false);
        }
    }

    std::optional<FalloutScripts::ServiceView> FalloutScripts::serviceView()
    {
        const std::optional<RepairServices> menu = repairServices();
        if (!menu)
            return std::nullopt;
        const FalloutScript::Repair::Settings settings = repairSettings();
        ServiceView view;
        view.mSkill = menu->mSkill;
        view.mCap = FalloutScript::Repair::serviceCondition(menu->mSkill, settings);
        view.mCaps = mActors.itemCount(mPlayer, masterForm(capsNumber));
        // A copy is listed while its health in percent is below the most the settings let a repair reach, times ten (the
        // shop does not leave an item in a better condition than that): 90 in the executable, 100 in the data
        const int listedBelow = static_cast<int>(settings.mSkillMax * 100.0);
        for (const PipboyItem& row : pipboyItems(mPlayer))
        {
            if (!row.mHasCondition || row.mCount <= 0 || row.mHidden || row.mCondition >= listedBelow
                || isQuestItem(row.mId))
                continue;
            FalloutScript::ItemInfo info;
            if (!itemInfo(row.mId, info))
                continue;
            // Armor with neither a rating nor a threshold is not listed
            if (info.mKind == FalloutScript::ItemKind::Armor && !(info.mRating > 0) && !(info.mThreshold > 0))
                continue;
            if (info.mKind == FalloutScript::ItemKind::Weapon)
                if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(refId(row.mId));
                    weapon != nullptr && weapon->mHasGameData
                    && (weapon->mGame.mFlags1 & ESM4::Weapon::GameData::NonPlayable) != 0)
                    continue;
            const int cost = FalloutScript::Repair::serviceCost(menu->mSkill, row.mCondition, itemValue(row.mId), settings);
            const RepairStat stat = repairStatAt(row.mId, row.mCondition);
            const RepairStat capStat = repairStatAt(row.mId, view.mCap);
            for (int unit = 0; unit < row.mCount; ++unit)
            {
                ServiceRow listed;
                listed.mItem = row.mId;
                listed.mCondition = row.mCondition;
                listed.mName = row.mName;
                listed.mIcon = row.mIcon;
                listed.mWorn = row.mEquipped && unit == 0;
                listed.mCost = cost;
                listed.mCanPay = cost > 0 && cost <= view.mCaps;
                listed.mHealth = info.mHealth * row.mCondition / FalloutScript::ActorState::fullCondition;
                listed.mStat = stat;
                listed.mCapStat = capStat;
                view.mTotal += cost;
                view.mRows.push_back(std::move(listed));
            }
        }
        // The rows that can be paid for come first, the healthiest of them first and the one that is on before another
        // alike; the others come after, the weakest first and the one that is on last (the executable's 0x7B7C40)
        std::stable_sort(view.mRows.begin(), view.mRows.end(), [](const ServiceRow& a, const ServiceRow& b) {
            if (a.mCanPay != b.mCanPay)
                return a.mCanPay;
            if (a.mHealth != b.mHealth)
                return a.mCanPay ? a.mHealth > b.mHealth : a.mHealth < b.mHealth;
            if (a.mWorn != b.mWorn)
                return a.mCanPay ? a.mWorn : b.mWorn;
            return false;
        });
        return view;
    }

    int FalloutScripts::repairService(FalloutScript::FormId item, int condition, bool worn, bool all)
    {
        const std::optional<ServiceView> view = serviceView();
        if (!view)
            return 0;
        std::vector<const ServiceRow*> chosen;
        int total = 0;
        if (all)
        {
            // The button is on only while the whole sum can be paid; every row that asks something is repaired
            if (!(view->mTotal > 0 && view->mTotal <= view->mCaps))
                return 0;
            for (const ServiceRow& row : view->mRows)
                if (row.mCanPay)
                    chosen.push_back(&row);
            total = view->mTotal;
        }
        else
        {
            const auto row = std::find_if(view->mRows.begin(), view->mRows.end(), [&](const ServiceRow& candidate) {
                return candidate.mItem == item && candidate.mCondition == condition && candidate.mWorn == worn;
            });
            const auto any = row != view->mRows.end() ? row
                                                      : std::find_if(view->mRows.begin(), view->mRows.end(),
                                                          [&](const ServiceRow& candidate) {
                                                              return candidate.mItem == item && candidate.mCondition == condition;
                                                          });
            if (any == view->mRows.end() || !any->mCanPay)
                return 0;
            chosen.push_back(&*any);
            total = any->mCost;
        }
        int done = 0;
        for (const ServiceRow* row : chosen)
        {
            // The copy goes to the condition the vendor leaves things in, and stays on if it was on
            if (row->mWorn && mEquipment.isEquipped(mPlayer, row->mItem, row->mCondition))
                mEquipment.setEquippedCondition(mPlayer, row->mItem, view->mCap);
            else
                mActors.setCondition(mPlayer, row->mItem, row->mCondition, view->mCap, 1);
            ++done;
        }
        // One payment: the caps go from the player to the vendor
        const std::optional<RepairServices> menu = repairServices();
        if (done > 0 && menu)
            transferItems(mPlayer, menu->mVendor, masterForm(capsNumber), total);
        return done;
    }
}
