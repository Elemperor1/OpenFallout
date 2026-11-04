#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadweap.hpp>

#include "../mwbase/environment.hpp"
#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        /// The statistic that counts the weapons modified (FalloutNV.exe 0x783EB0 adds to the one of the number 0x21)
        constexpr int weaponsModifiedStat = 0x21;

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }
    }

    bool FalloutScripts::modSlotsOf(FalloutScript::FormId weapon, FalloutScript::ItemMods::Slots& slots)
    {
        const ESM4::Weapon* record = weapon != 0 ? mStore.get<ESM4::Weapon>().search(refId(weapon)) : nullptr;
        if (record == nullptr)
            return false;
        slots = FalloutScript::ItemMods::Slots();
        bool any = false;
        for (int slot = 0; slot < FalloutScript::ItemMods::slotCount; ++slot)
        {
            slots.mMod[slot] = numberOf(record->mModForm[slot]);
            if (record->mHasGameData)
            {
                slots.mEffect[slot] = record->mGame.mModEffect[slot];
                slots.mValue1[slot] = record->mGame.mModValue1[slot];
                slots.mValue2[slot] = record->mGame.mModValue2[slot];
            }
            any = any || slots.mMod[slot] != 0;
        }
        return any;
    }

    int FalloutScripts::modsValue(FalloutScript::FormId weapon, int mods)
    {
        FalloutScript::ItemMods::Slots slots;
        if (mods == 0 || !modSlotsOf(weapon, slots))
            return 0;
        int value = 0;
        for (const int slot : FalloutScript::ItemMods::installedSlots(mods))
            if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(refId(slots.mMod[slot])))
                value += static_cast<int>(mod->mData.mValue);
        return value;
    }

    int FalloutScripts::copyToUse(FalloutScript::FormId item, int copy)
    {
        if (copy >= 0)
            return copy;
        // No copy named: the one that is on, else the best
        if (const int worn = mEquipment.equippedCopy(mPlayer, item); worn >= 0)
            return worn;
        const std::vector<FalloutScript::ActorState::ConditionStack> stacks = mActors.conditions(mPlayer, item);
        return stacks.empty() ? -1 : stacks.front().copy();
    }

    std::optional<FalloutScripts::ModView> FalloutScripts::modView(FalloutScript::FormId item, int copy)
    {
        copy = copyToUse(item, copy);
        FalloutScript::ItemInfo info;
        if (item == 0 || !itemInfo(item, info) || info.mKind != FalloutScript::ItemKind::Weapon)
            return std::nullopt;
        const std::vector<FalloutScript::ActorState::ConditionStack> stacks = mActors.conditions(mPlayer, item);
        if (std::none_of(stacks.begin(), stacks.end(),
                [copy](const FalloutScript::ActorState::ConditionStack& stack) { return stack.copy() == copy; }))
            return std::nullopt;
        ModView view;
        view.mItem = item;
        view.mCondition = FalloutScript::copyCondition(copy);
        view.mMods = FalloutScript::copyMods(copy);
        view.mDamage = repairStatAt(item, view.mCondition, view.mMods).mDamage;
        for (const PipboyItem& row : pipboyItems(mPlayer))
            if (row.mId == item)
            {
                view.mName = row.mName;
                view.mIcon = row.mIcon;
                break;
            }
        FalloutScript::ItemMods::Slots slots;
        if (!modSlotsOf(item, slots))
            return view;
        std::map<FalloutScript::FormId, int> held;
        for (const FalloutScript::FormId mod : slots.mMod)
            if (mod != 0)
                held[mod] = mActors.itemCount(mPlayer, mod);
        for (const FalloutScript::ItemMods::Row& line : FalloutScript::ItemMods::listRows(view.mMods, slots, held))
        {
            ModRow row;
            row.mMod = line.mMod;
            row.mInstalled = line.mInstalled;
            if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(refId(line.mMod)))
            {
                row.mName = mod->mFullName;
                row.mIcon = mod->mIcon;
                row.mDescription = mod->mText;
            }
            if (row.mName.empty())
                row.mName = gameSettingText("sMissingName");
            view.mRows.push_back(std::move(row));
        }
        return view;
    }

    std::optional<FalloutScripts::ModView> FalloutScripts::openItemMods(FalloutScript::FormId item, int copy)
    {
        copy = copyToUse(item, copy);
        std::optional<ModView> view = modView(item, copy);
        if (!view)
            return view;
        // A weapon that is on comes off while the screen is open and goes on again when it closes (FalloutNV.exe
        // 0x784710, 0x7838A0)
        const bool wasOn = mEquipment.isEquippedCopy(mPlayer, item, copy);
        if (wasOn)
            mEquipment.unequip(mPlayer, item);
        {
            const std::lock_guard lock(mRepairMutex);
            mModsWeaponWasOn = wasOn;
        }
        return view;
    }

    int FalloutScripts::installMod(FalloutScript::FormId item, int copy, FalloutScript::FormId mod)
    {
        FalloutScript::ItemInfo info;
        FalloutScript::ItemMods::Slots slots;
        copy = copyToUse(item, copy);
        if (item == 0 || mod == 0 || !itemInfo(item, info) || info.mKind != FalloutScript::ItemKind::Weapon
            || !modSlotsOf(item, slots))
            return -1;
        const std::vector<FalloutScript::ActorState::ConditionStack> stacks = mActors.conditions(mPlayer, item);
        if (std::none_of(stacks.begin(), stacks.end(),
                [copy](const FalloutScript::ActorState::ConditionStack& stack) { return stack.copy() == copy; }))
            return -1;
        const int condition = FalloutScript::copyCondition(copy);
        const int mods = FalloutScript::copyMods(copy);
        const int slot = FalloutScript::ItemMods::chooseSlot(mods, slots, mod);
        if (slot < 0 || mActors.itemCount(mPlayer, mod) <= 0)
            return -1;
        const int after = FalloutScript::ItemMods::conditionAfterInstalling(info.mHealth, condition, mods, slots, slot);
        const int newMods = mods | (1 << slot);
        if (mActors.setCopy(mPlayer, item, condition, mods, after, newMods, 1) <= 0)
            return -1;
        // The cost is one copy of the mod item; nothing else (no caps, no skill, no experience)
        mActors.removeItem(mPlayer, mod, 1);
        mProgress.modifyMiscStat(weaponsModifiedStat, 1);
        return FalloutScript::copyOf(after, newMods);
    }

    void FalloutScripts::closeItemMods(FalloutScript::FormId item, int copy)
    {
        bool wasOn = false;
        {
            const std::lock_guard lock(mRepairMutex);
            wasOn = mModsWeaponWasOn;
            mModsWeaponWasOn = false;
        }
        if (wasOn && item != 0)
            mEquipment.equipCopy(mPlayer, item, copyToUse(item, copy));
    }
}
