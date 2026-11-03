#include "falloutscripts.hpp"

#include <string>
#include <utility>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadweap.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }
    }

    std::string FalloutScripts::itemIcon(FalloutScript::FormId item)
    {
        const ESM::RefId record = refId(item);
        if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(record))
            return weapon->mIcon;
        if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(record))
            return !armor->mIconMale.empty() ? armor->mIconMale : armor->mIconFemale;
        if (const ESM4::Ammunition* ammo = mStore.get<ESM4::Ammunition>().search(record))
            return ammo->mIcon;
        if (const ESM4::Potion* potion = mStore.get<ESM4::Potion>().search(record))
            return potion->mIcon;
        if (const ESM4::MiscItem* misc = mStore.get<ESM4::MiscItem>().search(record))
            return misc->mIcon;
        if (const ESM4::Book* book = mStore.get<ESM4::Book>().search(record))
            return book->mIcon;
        if (const ESM4::Ingredient* ingredient = mStore.get<ESM4::Ingredient>().search(record))
            return ingredient->mIcon;
        if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(record))
            return mod->mIcon;
        if (const ESM4::Key* key = mStore.get<ESM4::Key>().search(record))
            return key->mIcon;
        return std::string();
    }

    std::vector<FalloutScripts::HotKeySlot> FalloutScripts::hotKeySlots()
    {
        std::vector<HotKeySlot> slots;
        for (int slot = 0; slot < FalloutScript::HotKeys::slotCount; ++slot)
        {
            HotKeySlot row;
            row.mSlot = slot;
            row.mAssignable = FalloutScript::HotKeys::assignable(slot);
            FalloutScript::FormId item = mHotKeys.item(slot);
            if (item != 0 && mPlayer != 0 && mActors.itemCount(mPlayer, item) <= 0)
            {
                // The key is kept by the entry of the inventory, and goes when the entry does
                mHotKeys.clearItem(item);
                item = 0;
            }
            if (item != 0)
            {
                row.mItem = item;
                row.mCount = mActors.itemCount(mPlayer, item);
                row.mName = itemName(item);
                row.mIcon = itemIcon(item);
                row.mEquipped = mEquipment.isEquipped(mPlayer, item);
            }
            slots.push_back(std::move(row));
        }
        return slots;
    }

    std::string FalloutScripts::assignHotKey(int slot, FalloutScript::FormId item)
    {
        if (!FalloutScript::HotKeys::assignable(slot))
            return "slot";
        FalloutScript::ItemInfo info;
        if (item == 0 || !itemInfo(item, info) || mActors.itemCount(mPlayer, item) <= 0)
            return "cannot";
        // Ammunition is never put on a key (the executable does nothing for it), and neither is what is neither held,
        // worn nor used up
        if (info.mKind == FalloutScript::ItemKind::Ammo)
            return "ammo";
        if (info.mKind == FalloutScript::ItemKind::Other || mStore.get<ESM4::Book>().search(refId(item)) != nullptr)
            return "cannot";
        if (!mHotKeys.assign(slot, item))
            return "slot";
        Log(Debug::Info) << "Hot keys: slot " << (slot + 1) << " holds " << std::hex << item << std::dec;
        return "assigned";
    }

}
