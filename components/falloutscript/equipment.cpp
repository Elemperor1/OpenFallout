#include "equipment.hpp"

#include <algorithm>

#include "actors.hpp"
#include "arguments.hpp"

namespace FalloutScript
{
    Equipment::Equipment(ItemRecords& records, ActorState& state)
        : mRecords(records)
        , mState(state)
    {
    }

    void Equipment::clear()
    {
        const std::lock_guard lock(mMutex);
        mWorn.clear();
        mChanges.clear();
    }

    void Equipment::noteChange(FormId actor, FormId item, bool worn)
    {
        constexpr std::size_t mostChanges = 1024;
        if (mChanges.size() < mostChanges)
            mChanges.push_back({ actor, item, worn });
    }

    std::vector<Equipment::WornChange> Equipment::takeChanges()
    {
        const std::lock_guard lock(mMutex);
        std::vector<WornChange> changes;
        changes.swap(mChanges);
        return changes;
    }

    void Equipment::tidy(FormId actor, Worn& worn)
    {
        if (worn.mWeapon != 0 && mState.itemCount(actor, worn.mWeapon) <= 0)
            worn.mWeapon = 0;
        if (worn.mAmmo != 0 && mState.itemCount(actor, worn.mAmmo) <= 0)
            worn.mAmmo = 0;
        worn.mArmor.erase(std::remove_if(worn.mArmor.begin(), worn.mArmor.end(),
                              [&](FormId item) { return mState.itemCount(actor, item) <= 0; }),
            worn.mArmor.end());
    }

    Equipment::Outcome Equipment::equip(FormId actor, FormId item)
    {
        ItemInfo info;
        if (actor == 0 || item == 0 || !mRecords.itemInfo(item, info))
            return Outcome::NotEquippable;
        if (info.mKind != ItemKind::Weapon && info.mKind != ItemKind::Armor && info.mKind != ItemKind::Ammo)
            return Outcome::NotEquippable;
        if (mState.itemCount(actor, item) <= 0)
            return Outcome::NotCarried;

        const std::lock_guard lock(mMutex);
        Worn& worn = mWorn[actor];
        tidy(actor, worn);
        switch (info.mKind)
        {
            case ItemKind::Weapon:
                if (worn.mWeapon == item)
                    return Outcome::AlreadyEquipped;
                if (worn.mWeapon != 0)
                    noteChange(actor, worn.mWeapon, false);
                worn.mWeapon = item;
                break;
            case ItemKind::Ammo:
                if (worn.mAmmo == item)
                    return Outcome::AlreadyEquipped;
                if (worn.mAmmo != 0)
                    noteChange(actor, worn.mAmmo, false);
                worn.mAmmo = item;
                break;
            case ItemKind::Armor:
            {
                if (std::find(worn.mArmor.begin(), worn.mArmor.end(), item) != worn.mArmor.end())
                    return Outcome::AlreadyEquipped;
                // What covers any of the same parts of the body comes off
                const auto covers = [&](FormId other) {
                    ItemInfo otherInfo;
                    return !mRecords.itemInfo(other, otherInfo) || (otherInfo.mSlots & info.mSlots) != 0;
                };
                for (const FormId other : worn.mArmor)
                    if (covers(other))
                        noteChange(actor, other, false);
                worn.mArmor.erase(std::remove_if(worn.mArmor.begin(), worn.mArmor.end(), covers), worn.mArmor.end());
                worn.mArmor.push_back(item);
                break;
            }
            default:
                break;
        }
        noteChange(actor, item, true);
        return Outcome::Equipped;
    }

    bool Equipment::unequip(FormId actor, FormId item)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mWorn.find(actor);
        if (found == mWorn.end())
            return false;
        Worn& worn = found->second;
        tidy(actor, worn);
        if (worn.mWeapon == item)
        {
            worn.mWeapon = 0;
            worn.mDrawn = false;
            noteChange(actor, item, false);
            return true;
        }
        if (worn.mAmmo == item)
        {
            worn.mAmmo = 0;
            noteChange(actor, item, false);
            return true;
        }
        const auto armor = std::find(worn.mArmor.begin(), worn.mArmor.end(), item);
        if (armor == worn.mArmor.end())
            return false;
        worn.mArmor.erase(armor);
        noteChange(actor, item, false);
        return true;
    }

    bool Equipment::isEquipped(FormId actor, FormId item)
    {
        if (item == 0)
            return false;
        const std::lock_guard lock(mMutex);
        const auto found = mWorn.find(actor);
        if (found == mWorn.end())
            return false;
        Worn& worn = found->second;
        tidy(actor, worn);
        return worn.mWeapon == item || worn.mAmmo == item
            || std::find(worn.mArmor.begin(), worn.mArmor.end(), item) != worn.mArmor.end();
    }

    FormId Equipment::weapon(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mWorn.find(actor);
        if (found == mWorn.end())
            return 0;
        tidy(actor, found->second);
        return found->second.mWeapon;
    }

    FormId Equipment::ammo(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mWorn.find(actor);
        if (found == mWorn.end())
            return 0;
        tidy(actor, found->second);
        return found->second.mAmmo;
    }

    std::vector<FormId> Equipment::armor(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mWorn.find(actor);
        if (found == mWorn.end())
            return {};
        tidy(actor, found->second);
        return found->second.mArmor;
    }

    std::uint32_t Equipment::weaponAnimationType(FormId actor)
    {
        const FormId held = weapon(actor);
        ItemInfo info;
        return held != 0 && mRecords.itemInfo(held, info) ? info.mAnimationType : 0;
    }

    namespace
    {
        template <class Pick>
        double sumOfWornArmor(ItemRecords& records, const std::vector<FormId>& armor, Pick pick)
        {
            double total = 0;
            for (const FormId item : armor)
            {
                ItemInfo info;
                if (records.itemInfo(item, info))
                    total += pick(info);
            }
            return total;
        }
    }

    double Equipment::armorRating(FormId actor)
    {
        return sumOfWornArmor(mRecords, armor(actor), [](const ItemInfo& info) { return info.mRating; });
    }

    double Equipment::damageThreshold(FormId actor)
    {
        return sumOfWornArmor(mRecords, armor(actor), [](const ItemInfo& info) { return info.mThreshold; });
    }

    double Equipment::upperBodyRating(FormId actor)
    {
        return sumOfWornArmor(mRecords, armor(actor),
            [](const ItemInfo& info) { return (info.mSlots & BipedSlot::upperBody) != 0 ? info.mRating : 0.0; });
    }

    bool Equipment::inventoryWeight(FormId actor, double& weight)
    {
        std::vector<std::pair<FormId, int>> held;
        if (actor == 0 || !mState.items(actor, held))
            return false;
        weight = 0;
        for (const auto& [item, count] : held)
        {
            ItemInfo info;
            if (mRecords.itemInfo(item, info))
                weight += info.mWeight * count;
        }
        return true;
    }

    bool Equipment::weaponDrawn(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mWorn.find(actor);
        return found != mWorn.end() && found->second.mDrawn && found->second.mWeapon != 0;
    }

    void Equipment::setWeaponDrawn(FormId actor, bool drawn)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        mWorn[actor].mDrawn = drawn;
    }

    void addEquipmentCommands(Interpreter& interpreter, Equipment& equipment)
    {
        interpreter.setHandler("EquipItem", [&equipment](CallContext& call) {
            equipment.equip(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("UnequipItem", [&equipment](CallContext& call) {
            equipment.unequip(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("GetEquipped", [&equipment](CallContext& call) {
            return call.mReference != 0 && equipment.isEquipped(call.mReference, formArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsWeaponOut", [&equipment](CallContext& call) {
            return call.mReference != 0 && equipment.weaponDrawn(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetWeaponAnimType", [&equipment](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(equipment.weaponAnimationType(call.mReference)) : 0.0;
        });
        interpreter.setHandler("GetArmorRating", [&equipment](CallContext& call) {
            return call.mReference != 0 ? equipment.armorRating(call.mReference) : 0.0;
        });
        interpreter.setHandler("GetArmorRatingUpperBody", [&equipment](CallContext& call) {
            return call.mReference != 0 ? equipment.upperBodyRating(call.mReference) : 0.0;
        });
    }
}
