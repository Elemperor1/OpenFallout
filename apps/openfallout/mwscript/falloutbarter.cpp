#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/barter.hpp>
#include <components/falloutscript/perks.hpp>

#include "../mwbase/environment.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        namespace AV = FalloutScript::ActorValue;

        /// The form of the caps in the file the player is in (the games keep them as form 0xF)
        constexpr FalloutScript::FormId capsNumber = 0x0000000F;
        /// The misc stat "Barter Amount Traded" (FalloutNV.exe 0x72fd10 adds the caps of a deal to it)
        constexpr int barterAmountStat = 0x27;
        /// The condition of a thing that the port does not keep a condition for, in tenths of its full worth
        constexpr double fullCondition = 10.0;

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

    bool FalloutScripts::openBarter(FalloutScript::FormId vendor, int percent)
    {
        // The menu is for the player to trade with someone else; the call comes from the script of the conversation
        if (vendor == 0 || vendor == mPlayer)
            return false;
        const std::lock_guard lock(mBarterMutex);
        mBarter = BarterMenu{ vendor, std::string(), std::clamp(percent, -100, 100), ++mBarterSerial };
        return true;
    }

    std::optional<FalloutScripts::BarterMenu> FalloutScripts::barterMenu()
    {
        std::optional<BarterMenu> menu;
        {
            const std::lock_guard lock(mBarterMutex);
            menu = mBarter;
        }
        // The name is the one of the vendor's reference, which only the thread of the frame may look up
        if (menu && menu->mName.empty())
        {
            const OFWorld::Ptr ptr = findReference(menu->mVendor);
            if (!ptr.isEmpty())
                menu->mName = std::string(ptr.getClass().getName(ptr));
            const std::lock_guard lock(mBarterMutex);
            if (mBarter && mBarter->mSerial == menu->mSerial)
                mBarter->mName = menu->mName;
        }
        return menu;
    }

    void FalloutScripts::closeBarter()
    {
        FalloutScript::FormId vendor = 0;
        bool dealt = false;
        {
            const std::lock_guard lock(mBarterMutex);
            if (mBarter)
            {
                vendor = mBarter->mVendor;
                dealt = mBarter->mDealt;
            }
            mBarter.reset();
        }
        // After a deal the vendor says a line of the topic of subtype BarterExit, when a conversation is open
        // (FalloutNV.exe 0x72d6d0 and 0x7640a0, round 12AH A5); without one nothing is said
        if (dealt && vendor != 0)
        {
            FalloutScript::FormId topic = 0;
            {
                const std::lock_guard lock(mDialogueMutex);
                if (mDialogue.active())
                    topic = mDialogue.findTopic("BarterExit");
            }
            if (topic != 0)
                sayTopic(vendor, topic, 0, false, false);
        }
    }

    FalloutScript::FormId FalloutScripts::merchantContainer(FalloutScript::FormId vendor)
    {
        // What a script gave (SetMerchantContainer, or none with RemoveMerchantContainer) wins over the record
        {
            const std::lock_guard lock(mBarterMutex);
            if (const auto given = mMerchantContainers.find(vendor); given != mMerchantContainers.end())
                return given->second;
        }
        // The XMRC of the placed reference (FalloutNV.exe 0x421400 reads the extra data of the reference that the
        // loader fills from it)
        const ESM::FormId form = ESM::FormId::fromUint32(vendor);
        if (const ESM4::ActorCharacter* character = mStore.get<ESM4::ActorCharacter>().search(form))
            return numberOf(character->mMerchantContainer);
        if (const ESM4::ActorCreature* creature = mStore.get<ESM4::ActorCreature>().search(form))
            return numberOf(creature->mMerchantContainer);
        return 0;
    }

    void FalloutScripts::setMerchantContainer(FalloutScript::FormId vendor, FalloutScript::FormId container)
    {
        if (vendor == 0)
            return;
        const std::lock_guard lock(mBarterMutex);
        mMerchantContainers[vendor] = container;
    }

    FalloutScript::FormId FalloutScripts::barterHolder(FalloutScript::FormId vendor)
    {
        // What the player sells and pays goes to the container of the vendor, or to the vendor without one
        const FalloutScript::FormId container = merchantContainer(vendor);
        return container != 0 ? container : vendor;
    }

    std::vector<FalloutScript::FormId> FalloutScripts::barterSources(FalloutScript::FormId vendor)
    {
        // What the vendor sells is its own goods and those of the container together, the counts added by form
        // (FalloutNV.exe 0x46f310, round 12AH A1)
        const FalloutScript::FormId container = merchantContainer(vendor);
        if (container != 0)
            return { container, vendor };
        return { vendor };
    }

    int FalloutScripts::barterStock(FalloutScript::FormId vendor, FalloutScript::FormId item)
    {
        int total = 0;
        for (const FalloutScript::FormId holder : barterSources(vendor))
        {
            // A trader does not sell what it wears
            if (holder == vendor && mEquipment.isEquipped(vendor, item))
                continue;
            total += mActors.itemCount(holder, item);
        }
        return total;
    }

    int FalloutScripts::barterMove(
        FalloutScript::FormId vendor, FalloutScript::FormId to, FalloutScript::FormId item, int count)
    {
        // Taken from the container first, then from what the vendor carries
        int moved = 0;
        for (const FalloutScript::FormId holder : barterSources(vendor))
        {
            if (moved >= count)
                break;
            if (holder == vendor && mEquipment.isEquipped(vendor, item))
                continue;
            moved += transferItems(holder, to, item, count - moved);
        }
        return moved;
    }

    int FalloutScripts::itemValue(FalloutScript::FormId item)
    {
        const ESM::RefId record = refId(item);
        if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(record))
            return static_cast<int>(weapon->mData.value);
        if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(record))
            return static_cast<int>(armor->mData.value);
        if (const ESM4::Ammunition* ammo = mStore.get<ESM4::Ammunition>().search(record))
            return static_cast<int>(ammo->mData.mValue);
        if (const ESM4::Potion* potion = mStore.get<ESM4::Potion>().search(record))
            return potion->mItem.value;
        if (const ESM4::MiscItem* misc = mStore.get<ESM4::MiscItem>().search(record))
            return static_cast<int>(misc->mData.value);
        if (const ESM4::Book* book = mStore.get<ESM4::Book>().search(record))
            return static_cast<int>(book->mData.value);
        if (const ESM4::Ingredient* ingredient = mStore.get<ESM4::Ingredient>().search(record))
            return static_cast<int>(ingredient->mData.value);
        if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(record))
            return static_cast<int>(mod->mData.mValue);
        if (const ESM4::Key* key = mStore.get<ESM4::Key>().search(record))
            return static_cast<int>(key->mData.value);
        return 0;
    }

    FalloutScript::Barter::Settings FalloutScripts::barterSettings()
    {
        FalloutScript::Barter::Settings settings;
        const auto read = [this](std::string_view name, double& value) {
            if (const std::optional<double> found = gameSetting(name))
                value = *found;
        };
        read("fBarterBuyBase", settings.mBuyBase);
        read("fBarterBuyMult", settings.mBuyMult);
        read("fBarterSellBase", settings.mSellBase);
        read("fBarterSellMult", settings.mSellMult);
        read("fItemConditionValueBase", settings.mConditionBase);
        read("fItemConditionValueMult", settings.mConditionMult);
        read("fItemConditionValueExp", settings.mConditionExp);
        return settings;
    }

    double FalloutScripts::barterPrice(const BarterMenu& menu, FalloutScript::FormId item, bool buying)
    {
        const int value = itemValue(item);
        if (value <= 0)
            return 0.0;
        const FalloutScript::Barter::Settings settings = barterSettings();
        const FalloutScript::Barter::Factors factors
            = FalloutScript::Barter::factors(settings, mActorValues.current(mPlayer, AV::barter));
        const double base = FalloutScript::Barter::baseValue(value, fullCondition, settings);
        const auto perk = [this, &menu, item](double price) {
            return mPerks.apply(FalloutScript::EntryPoint::buyPrice, mPlayer, price,
                FalloutScript::PerkEngine::Context{ menu.mVendor, item });
        };
        return FalloutScript::Barter::unitPrice(base, buying, factors, menu.mPercent, perk);
    }

    std::vector<FalloutScripts::BarterRow> FalloutScripts::barterRows(bool vendorSide)
    {
        std::vector<BarterRow> rows;
        const std::optional<BarterMenu> menu = barterMenu();
        if (!menu)
            return rows;
        const FalloutScript::FormId caps = masterForm(capsNumber);
        // The things of the holders in one list: the vendor's two holders are one, the counts added
        std::vector<PipboyItem> items;
        const std::vector<FalloutScript::FormId> holders
            = vendorSide ? barterSources(menu->mVendor) : std::vector<FalloutScript::FormId>{ mPlayer };
        for (const FalloutScript::FormId holder : holders)
        {
            for (PipboyItem& item : pipboyItems(holder))
            {
                // A trader does not sell what it wears
                if (vendorSide && holder == menu->mVendor && item.mEquipped)
                    continue;
                const auto same = std::find_if(
                    items.begin(), items.end(), [&item](const PipboyItem& listed) { return listed.mId == item.mId; });
                if (same != items.end())
                    same->mCount += item.mCount;
                else
                    items.push_back(std::move(item));
            }
        }
        for (PipboyItem& item : items)
        {
            // What has no worth is not traded, nor the caps (they are the money), nor what the player does not see
            // in its own things
            if (item.mId == caps || item.mValue <= 0 || (!vendorSide && (item.mKey || item.mHidden)))
                continue;
            BarterRow row;
            row.mPrice = barterPrice(*menu, item.mId, vendorSide);
            row.mItem = std::move(item);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    FalloutScripts::BarterQuote FalloutScripts::barterQuote(const BarterPiles& buy, const BarterPiles& sell)
    {
        BarterQuote quote;
        const std::optional<BarterMenu> menu = barterMenu();
        if (!menu)
            return quote;
        const FalloutScript::FormId vendor = menu->mVendor;
        const FalloutScript::FormId caps = masterForm(capsNumber);
        quote.mHolder = barterHolder(vendor);
        quote.mPlayerCaps = mActors.itemCount(mPlayer, caps);
        // The vendor's money is the caps of its container and of itself
        for (const FalloutScript::FormId holder : barterSources(vendor))
            quote.mVendorCaps += mActors.itemCount(holder, caps);
        std::vector<FalloutScript::Barter::Line> lines;
        // A pile that asks for more than the holder has, or for a thing without a worth, is no deal
        const auto add = [&](const BarterPiles& pile, bool buying) {
            for (const auto& [item, count] : pile)
            {
                const int have = buying ? barterStock(vendor, item) : mActors.itemCount(mPlayer, item);
                if (count <= 0 || itemValue(item) <= 0 || have < count)
                {
                    quote.mValid = false;
                    continue;
                }
                lines.push_back({ barterPrice(*menu, item, buying), count, buying });
            }
        };
        quote.mValid = true;
        add(buy, true);
        add(sell, false);
        quote.mPilesEmpty = buy.empty() && sell.empty();
        quote.mBalance = FalloutScript::Barter::balance(lines);
        quote.mAmount = FalloutScript::Barter::amount(quote.mBalance, quote.mVendorCaps);
        quote.mAccept
            = quote.mValid && FalloutScript::Barter::canAccept(quote.mPilesEmpty, quote.mBalance, quote.mPlayerCaps);
        return quote;
    }

    FalloutScripts::BarterOutcome FalloutScripts::barterDeal(const BarterPiles& buy, const BarterPiles& sell)
    {
        BarterOutcome outcome;
        const std::optional<BarterMenu> menu = barterMenu();
        if (!menu)
        {
            outcome.mStatus = "closed";
            return outcome;
        }
        const BarterQuote quote = barterQuote(buy, sell);
        if (quote.mPilesEmpty)
        {
            outcome.mStatus = "empty";
            return outcome;
        }
        if (!quote.mValid)
        {
            outcome.mStatus = "items";
            return outcome;
        }
        if (!quote.mAccept)
        {
            outcome.mStatus = "caps";
            return outcome;
        }
        const FalloutScript::FormId vendor = menu->mVendor;
        const FalloutScript::FormId holder = quote.mHolder;
        const FalloutScript::FormId caps = masterForm(capsNumber);
        // What the player buys goes from the vendor to the player and what it sells the other way (each is taken off
        // first when it is worn)
        for (const auto& [item, count] : buy)
            barterMove(vendor, mPlayer, item, count);
        for (const auto& [item, count] : sell)
            transferItems(mPlayer, holder, item, count);
        // The caps: the vendor pays no more than it has, the player pays all of what is asked
        if (quote.mAmount > 0)
        {
            if (quote.mBalance > 0)
                barterMove(vendor, mPlayer, caps, quote.mAmount);
            else
                transferItems(mPlayer, holder, caps, quote.mAmount);
            mProgress.modifyMiscStat(barterAmountStat, quote.mAmount);
        }
        {
            const std::lock_guard lock(mBarterMutex);
            if (mBarter && mBarter->mSerial == menu->mSerial)
                mBarter->mDealt = true;
        }
        outcome.mStatus = "done";
        outcome.mAmount = quote.mAmount;
        outcome.mReceived = quote.mBalance > 0;
        return outcome;
    }
}
