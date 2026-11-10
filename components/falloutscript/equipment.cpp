#include "equipment.hpp"

#include <algorithm>
#include <cmath>

#include "actors.hpp"
#include "arguments.hpp"
#include "wear.hpp"

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
        ++mRevision;
    }

    void Equipment::noteChange(FormId actor, FormId item, bool worn)
    {
        ItemInfo info;
        if (mRecords.itemInfo(item, info) && info.mKind == ItemKind::Armor)
            ++mArmorRevisions[actor];
        constexpr std::size_t mostChanges = 1024;
        if (mChanges.size() < mostChanges)
            mChanges.push_back({ actor, item, worn });
    }

    std::uint32_t Equipment::revision(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mArmorRevisions.find(actor);
        return mRevision + (found != mArmorRevisions.end() ? found->second : 0);
    }

    std::vector<Equipment::WornChange> Equipment::takeChanges()
    {
        const std::lock_guard lock(mMutex);
        // What a script took away since nothing asked about the character is off it now, so its item hears of it
        for (auto& [actor, worn] : mWorn)
            tidy(actor, worn);
        std::vector<WornChange> changes;
        changes.swap(mChanges);
        return changes;
    }

    Equipment::Worn& Equipment::wornOf(FormId actor)
    {
        Worn& worn = mWorn[actor];
        if (worn.mSeeded || actor == 0)
            return worn;
        // The host may not be able to tell yet (a character made while the game runs is known to the thread of the
        // frame only); it is asked again the next time
        std::vector<FormId> armor;
        if (!mRecords.startingArmor(actor, armor))
            return worn;
        worn.mSeeded = true;
        // What was put on before the host could tell stays on, over what the character started in
        std::uint32_t covered = 0;
        for (const FormId item : worn.mArmor)
        {
            ItemInfo info;
            if (mRecords.itemInfo(item, info))
                covered |= info.mSlots;
        }
        std::vector<FormId> starting;
        for (const FormId item : armor)
        {
            ItemInfo info;
            if (!mRecords.itemInfo(item, info) || info.mKind != ItemKind::Armor || mState.itemCount(actor, item) <= 0)
                continue;
            // The first piece to cover a part of the body is the one worn, as in the model the character is shown in
            if ((info.mSlots & covered) != 0)
                continue;
            covered |= info.mSlots;
            starting.push_back(item);
        }
        worn.mArmor.insert(worn.mArmor.begin(), starting.begin(), starting.end());
        return worn;
    }

    void Equipment::tidy(FormId actor, Worn& worn)
    {
        // What a script took away (RemoveItem, RemoveAllItems) or gave away is off the character, which its item
        // hears as it does when the character takes it off
        if (worn.mWeapon != 0 && mState.itemCount(actor, worn.mWeapon) <= 0)
        {
            noteChange(actor, worn.mWeapon, false);
            worn.mWeapon = 0;
            worn.mWeaponMods = 0;
            worn.mDrawn = false;
        }
        if (worn.mAmmo != 0 && mState.itemCount(actor, worn.mAmmo) <= 0)
        {
            noteChange(actor, worn.mAmmo, false);
            worn.mAmmo = 0;
        }
        worn.mArmor.erase(std::remove_if(worn.mArmor.begin(), worn.mArmor.end(),
                              [&](FormId item) {
                                  if (mState.itemCount(actor, item) > 0)
                                      return false;
                                  noteChange(actor, item, false);
                                  worn.mArmorConditions.erase(item);
                                  return true;
                              }),
            worn.mArmor.end());
        // The copy that is on is gone when the character has no copy like it (a script took it away and left others):
        // another copy of the item is on then
        if (worn.mWeapon != 0)
            worn.mWeaponCondition
                = chooseCopy(actor, worn.mWeapon, worn.mWeaponCondition, worn.mWeaponMods, worn.mWeaponMods);
        for (const FormId item : worn.mArmor)
        {
            const auto set = worn.mArmorConditions.find(item);
            int noMods = 0;
            const int condition = chooseCopy(
                actor, item, set == worn.mArmorConditions.end() ? ActorState::fullCondition : set->second, 0, noMods);
            if (condition >= ActorState::fullCondition)
                worn.mArmorConditions.erase(item);
            else
                worn.mArmorConditions[item] = condition;
        }
    }

    int Equipment::chooseCopy(FormId actor, FormId item, int asked, int askedMods, int& mods)
    {
        const std::vector<ActorState::ConditionStack> stacks = mState.conditions(actor, item);
        if (stacks.empty())
        {
            mods = 0;
            return ActorState::fullCondition;
        }
        if (asked >= 0)
            for (const ActorState::ConditionStack& stack : stacks)
                if (stack.mCondition == asked && stack.mMods == askedMods)
                {
                    mods = askedMods;
                    return asked;
                }
        mods = stacks.front().mMods;
        return stacks.front().mCondition;
    }

    int Equipment::conditionOn(const Worn& worn, FormId item) const
    {
        if (worn.mWeapon == item)
            return worn.mWeaponCondition;
        const auto set = worn.mArmorConditions.find(item);
        return set == worn.mArmorConditions.end() ? ActorState::fullCondition : set->second;
    }

    void Equipment::wearArmor(FormId actor, Worn& worn, FormId item, const ItemInfo& info, bool note, int condition)
    {
        // What covers any of the same parts of the body comes off
        const auto covers = [&](FormId other) {
            ItemInfo otherInfo;
            return !mRecords.itemInfo(other, otherInfo) || (otherInfo.mSlots & info.mSlots) != 0;
        };
        if (note)
            for (const FormId other : worn.mArmor)
                if (covers(other))
                    noteChange(actor, other, false);
        for (const FormId other : worn.mArmor)
            if (covers(other))
                worn.mArmorConditions.erase(other);
        worn.mArmor.erase(std::remove_if(worn.mArmor.begin(), worn.mArmor.end(), covers), worn.mArmor.end());
        worn.mArmor.push_back(item);
        if (condition < ActorState::fullCondition)
            worn.mArmorConditions[item] = condition;
        else
            worn.mArmorConditions.erase(item);
    }

    Equipment::Outcome Equipment::equip(FormId actor, FormId item, int condition, int mods)
    {
        ItemInfo info;
        if (actor == 0 || item == 0 || !mRecords.itemInfo(item, info))
            return Outcome::NotEquippable;
        if (info.mKind != ItemKind::Weapon && info.mKind != ItemKind::Armor && info.mKind != ItemKind::Ammo)
            return Outcome::NotEquippable;
        if (mState.itemCount(actor, item) <= 0)
            return Outcome::NotCarried;

        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        // The copy that goes on: the one that was asked for, else the one that is on already, else the best
        const bool on = (info.mKind == ItemKind::Weapon && worn.mWeapon == item)
            || (info.mKind == ItemKind::Armor
                && std::find(worn.mArmor.begin(), worn.mArmor.end(), item) != worn.mArmor.end());
        const bool weaponOn = info.mKind == ItemKind::Weapon && worn.mWeapon == item;
        int copyMods = 0;
        const int copy = chooseCopy(actor, item, condition >= 0 ? condition : (on ? conditionOn(worn, item) : -1),
            condition >= 0 ? mods : (weaponOn ? worn.mWeaponMods : 0), copyMods);
        if (condition >= 0 && (copy != condition || copyMods != mods))
            return Outcome::NotCarried;
        // A weapon or a piece of armor with no health left does not go on (one that is on and breaks stays on)
        if ((info.mKind == ItemKind::Weapon || info.mKind == ItemKind::Armor) && copy <= 0
            && !(on && conditionOn(worn, item) == copy))
            return Outcome::Broken;
        switch (info.mKind)
        {
            case ItemKind::Weapon:
                if (worn.mWeapon == item)
                {
                    if (copy == worn.mWeaponCondition && copyMods == worn.mWeaponMods)
                        return Outcome::AlreadyEquipped;
                    // Another copy of the weapon in the hands: nothing is put on or taken off
                    worn.mWeaponCondition = copy;
                    worn.mWeaponMods = copyMods;
                    return Outcome::Equipped;
                }
                if (worn.mWeapon != 0)
                    noteChange(actor, worn.mWeapon, false);
                worn.mWeapon = item;
                worn.mWeaponCondition = copy;
                worn.mWeaponMods = copyMods;
                break;
            case ItemKind::Ammo:
                if (worn.mAmmo == item)
                    return Outcome::AlreadyEquipped;
                if (worn.mAmmo != 0)
                    noteChange(actor, worn.mAmmo, false);
                worn.mAmmo = item;
                break;
            case ItemKind::Armor:
                if (on)
                {
                    if (copy == conditionOn(worn, item))
                        return Outcome::AlreadyEquipped;
                    if (copy < ActorState::fullCondition)
                        worn.mArmorConditions[item] = copy;
                    else
                        worn.mArmorConditions.erase(item);
                    return Outcome::Equipped;
                }
                wearArmor(actor, worn, item, info, true, copy);
                break;
            default:
                break;
        }
        noteChange(actor, item, true);
        return Outcome::Equipped;
    }

    bool Equipment::unequip(FormId actor, FormId item)
    {
        if (actor == 0)
            return false;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        if (worn.mWeapon == item)
        {
            worn.mWeapon = 0;
            worn.mWeaponMods = 0;
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
        worn.mArmorConditions.erase(item);
        noteChange(actor, item, false);
        return true;
    }

    bool Equipment::isEquipped(FormId actor, FormId item, int condition, int mods)
    {
        return equippedCondition(actor, item) == std::clamp(condition, 0, int(ActorState::fullCondition))
            && equippedMods(actor, item) == mods;
    }

    int Equipment::equippedMods(FormId actor, FormId item)
    {
        if (item == 0 || actor == 0)
            return 0;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        return worn.mWeapon == item ? worn.mWeaponMods : 0;
    }

    int Equipment::equippedCondition(FormId actor, FormId item)
    {
        if (item == 0 || actor == 0)
            return -1;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        if (worn.mAmmo == item)
            return ActorState::fullCondition;
        if (worn.mWeapon != item && std::find(worn.mArmor.begin(), worn.mArmor.end(), item) == worn.mArmor.end())
            return -1;
        return conditionOn(worn, item);
    }

    bool Equipment::setEquippedCondition(FormId actor, FormId item, int condition)
    {
        if (item == 0 || actor == 0)
            return false;
        condition = std::clamp(condition, 0, int(ActorState::fullCondition));
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        const bool weapon = worn.mWeapon == item;
        if (!weapon && std::find(worn.mArmor.begin(), worn.mArmor.end(), item) == worn.mArmor.end())
            return false;
        const int before = conditionOn(worn, item);
        if (before == condition)
            return true;
        // The copy that is on is the one that changes
        if (mState.setCondition(actor, item, before, condition, 1, weapon ? worn.mWeaponMods : 0) < 1)
            return false;
        if (weapon)
            worn.mWeaponCondition = condition;
        else if (condition < ActorState::fullCondition)
            worn.mArmorConditions[item] = condition;
        else
            worn.mArmorConditions.erase(item);
        return true;
    }

    int Equipment::wear(FormId actor, FormId item, double points)
    {
        ItemInfo info;
        if (actor == 0 || item == 0 || points <= 0 || !mRecords.itemInfo(item, info) || info.mHealth <= 0)
            return -1;
        const int before = equippedCondition(actor, item);
        if (before < 0)
            return -1;
        if (before == 0)
            return 0;
        int lost = 0;
        WearListener listener;
        {
            const std::lock_guard lock(mMutex);
            lost = Wear::tenthsLost(points, info.mHealth, mWearRemainders[{ actor, item }]);
            listener = mWearListener;
        }
        if (lost <= 0)
            return before;
        const int after = std::max(before - lost, 0);
        if (!setEquippedCondition(actor, item, after))
            return before;
        if (listener)
            listener(actor, item, before, after);
        return after;
    }

    void Equipment::setWearListener(WearListener listener)
    {
        const std::lock_guard lock(mMutex);
        mWearListener = std::move(listener);
    }

    void Equipment::addWithHealth(FormId actor, FormId item, int count, double fraction)
    {
        if (actor == 0 || item == 0 || count <= 0)
            return;
        ItemInfo info;
        int condition = ActorState::fullCondition;
        if (mRecords.itemInfo(item, info) && info.mHealth > 0 && fraction < 1.0)
            condition = std::clamp(static_cast<int>(std::lround(fraction * ActorState::fullCondition)), 0,
                static_cast<int>(ActorState::fullCondition));
        mState.addItem(actor, item, count, condition);
    }

    double Equipment::weaponPercent(FormId actor)
    {
        const FormId held = weapon(actor);
        const int condition = held != 0 ? equippedCondition(actor, held) : -1;
        return condition >= 0 ? static_cast<double>(condition) * 100.0 / ActorState::fullCondition : 100.0;
    }

    bool Equipment::setWeaponPercent(FormId actor, double percent)
    {
        const FormId held = weapon(actor);
        ItemInfo info;
        if (held == 0 || !mRecords.itemInfo(held, info))
            return false;
        const int now = equippedCondition(actor, held);
        if (now < 0)
            return false;
        const double target = std::max(percent, 0.0) / 100.0 * ActorState::fullCondition;
        if (target <= now)
            wear(actor, held, (now - target) / ActorState::fullCondition * info.mHealth);
        else if (info.mHealth > 0)
        {
            // The executable stops a point short of the most the weapon has (the cap is the health less one)
            const double cap = (info.mHealth - 1.0) / info.mHealth * ActorState::fullCondition;
            setEquippedCondition(actor, held, static_cast<int>(std::min(target, cap)));
        }
        return true;
    }

    bool Equipment::modWeaponPercent(FormId actor, double percent)
    {
        const FormId held = weapon(actor);
        const int now = held != 0 ? equippedCondition(actor, held) : -1;
        if (now < 0)
            return false;
        const double wanted = std::max(static_cast<double>(now) * 100.0 / ActorState::fullCondition + percent, 0.0);
        ItemInfo info;
        if (!mRecords.itemInfo(held, info))
            return false;
        const double target = wanted / 100.0 * ActorState::fullCondition;
        if (target <= now)
            wear(actor, held, (now - target) / ActorState::fullCondition * info.mHealth);
        else
            setEquippedCondition(actor, held, static_cast<int>(std::min(target, double(ActorState::fullCondition))));
        return true;
    }

    bool Equipment::isEquipped(FormId actor, FormId item)
    {
        if (item == 0 || actor == 0)
            return false;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        return worn.mWeapon == item || worn.mAmmo == item
            || std::find(worn.mArmor.begin(), worn.mArmor.end(), item) != worn.mArmor.end();
    }

    FormId Equipment::weapon(FormId actor)
    {
        if (actor == 0)
            return 0;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        return worn.mWeapon;
    }

    FormId Equipment::ammo(FormId actor)
    {
        if (actor == 0)
            return 0;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        return worn.mAmmo;
    }

    std::vector<FormId> Equipment::armor(FormId actor)
    {
        if (actor == 0)
            return {};
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        return worn.mArmor;
    }

    std::uint32_t Equipment::weaponAnimationType(FormId actor)
    {
        const FormId held = weapon(actor);
        ItemInfo info;
        return held != 0 && mRecords.itemInfo(held, info) ? info.mAnimationType : 0;
    }

    std::vector<Equipment::WornPiece> Equipment::wornArmor(FormId actor)
    {
        std::vector<WornPiece> pieces;
        if (actor == 0)
            return pieces;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        for (const FormId item : worn.mArmor)
            pieces.push_back({ item, conditionOn(worn, item) });
        return pieces;
    }

    void Equipment::setWearSettings(WearSettings settings)
    {
        const std::lock_guard lock(mMutex);
        mWearSettings = std::move(settings);
    }

    template <class Pick>
    double Equipment::sumOfWornArmor(FormId actor, Pick pick)
    {
        WearSettings provider;
        {
            const std::lock_guard lock(mMutex);
            provider = mWearSettings;
        }
        const Wear::Settings newVegas;
        double total = 0;
        for (const WornPiece& piece : wornArmor(actor))
        {
            ItemInfo info;
            if (mRecords.itemInfo(piece.mItem, info))
                total += Wear::armorValue(provider ? provider() : newVegas, pick(info),
                    static_cast<double>(piece.mCondition) / ActorState::fullCondition);
        }
        return total;
    }

    double Equipment::armorRating(FormId actor)
    {
        return sumOfWornArmor(actor, [](const ItemInfo& info) { return info.mRating; });
    }

    double Equipment::damageThreshold(FormId actor)
    {
        return sumOfWornArmor(actor, [](const ItemInfo& info) { return info.mThreshold; });
    }

    double Equipment::upperBodyRating(FormId actor)
    {
        return sumOfWornArmor(
            actor, [](const ItemInfo& info) { return (info.mSlots & BipedSlot::upperBody) != 0 ? info.mRating : 0.0; });
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

    bool Equipment::weaponDrawn(FormId actor)
    {
        if (actor == 0)
            return false;
        const std::lock_guard lock(mMutex);
        Worn& worn = wornOf(actor);
        tidy(actor, worn);
        return worn.mDrawn && worn.mWeapon != 0;
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
        // The health that a command gives an item is a fraction of the most it has (the name says percent; the executable
        // reads 0 to 1, Mac round 12AW S12). A copy that is not whole is a copy of its own.
        interpreter.setHandler("AddItemHealthPercent", [&equipment](CallContext& call) {
            const FormId item = formArgument(call, 0);
            const double health = call.mArguments.size() > 2 ? argument(call, 2) : 1.0;
            equipment.addWithHealth(call.mReference, item, intArgument(call, 1), std::isnan(health) ? 1.0 : health);
            return 0.0;
        });
        interpreter.setHandler("GetWeaponHealthPerc", [&equipment](CallContext& call) {
            return call.mReference != 0 ? equipment.weaponPercent(call.mReference) : 0.0;
        });
        interpreter.setHandler("SetWeaponHealthPerc", [&equipment](CallContext& call) {
            const double percent = call.mArguments.empty() ? 1.0 : argument(call, 0);
            if (call.mReference != 0 && !std::isnan(percent))
                equipment.setWeaponPercent(call.mReference, percent);
            return 0.0;
        });
        interpreter.setHandler("ModWeaponHealthPerc", [&equipment](CallContext& call) {
            const double percent = call.mArguments.empty() ? 0.0 : argument(call, 0);
            if (call.mReference != 0 && !std::isnan(percent))
                equipment.modWeaponPercent(call.mReference, percent);
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
