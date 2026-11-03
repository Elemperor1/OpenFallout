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
        const std::lock_guard lock(mBarterMutex);
        mBarter.reset();
    }

    FalloutScript::FormId FalloutScripts::barterHolder(FalloutScript::FormId vendor)
    {
        // A character that trades sells what a container says (the XMRC of its placed reference); with none it sells
        // what it carries itself
        const ESM::FormId form = ESM::FormId::fromUint32(vendor);
        FalloutScript::FormId container = 0;
        if (const ESM4::ActorCharacter* character = mStore.get<ESM4::ActorCharacter>().search(form))
            container = numberOf(character->mMerchantContainer);
        else if (const ESM4::ActorCreature* creature = mStore.get<ESM4::ActorCreature>().search(form))
            container = numberOf(creature->mMerchantContainer);
        return container != 0 ? container : vendor;
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
        const FalloutScript::FormId holder = vendorSide ? barterHolder(menu->mVendor) : mPlayer;
        const FalloutScript::FormId caps = masterForm(capsNumber);
        for (PipboyItem& item : pipboyItems(holder))
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
        const FalloutScript::FormId holder = barterHolder(menu->mVendor);
        const FalloutScript::FormId caps = masterForm(capsNumber);
        quote.mHolder = holder;
        quote.mPlayerCaps = mActors.itemCount(mPlayer, caps);
        quote.mVendorCaps = mActors.itemCount(holder, caps);
        std::vector<FalloutScript::Barter::Line> lines;
        // A pile that asks for more than the holder has, or for a thing without a worth, is no deal
        const auto add = [&](const BarterPiles& pile, FalloutScript::FormId from, bool buying) {
            for (const auto& [item, count] : pile)
            {
                if (count <= 0 || itemValue(item) <= 0 || mActors.itemCount(from, item) < count)
                {
                    quote.mValid = false;
                    continue;
                }
                lines.push_back({ barterPrice(*menu, item, buying), count, buying });
            }
        };
        quote.mValid = true;
        add(buy, holder, true);
        add(sell, mPlayer, false);
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
        const FalloutScript::FormId holder = quote.mHolder;
        const FalloutScript::FormId caps = masterForm(capsNumber);
        // What the player buys goes from the vendor to the player and what it sells the other way (each is taken off
        // first when it is worn)
        for (const auto& [item, count] : buy)
            transferItems(holder, mPlayer, item, count);
        for (const auto& [item, count] : sell)
            transferItems(mPlayer, holder, item, count);
        // The caps: the vendor pays no more than it has, the player pays all of what is asked
        if (quote.mAmount > 0)
        {
            if (quote.mBalance > 0)
                transferItems(holder, mPlayer, caps, quote.mAmount);
            else
                transferItems(mPlayer, holder, caps, quote.mAmount);
            mProgress.modifyMiscStat(barterAmountStat, quote.mAmount);
        }
        outcome.mStatus = "done";
        outcome.mAmount = quote.mAmount;
        outcome.mReceived = quote.mBalance > 0;
        return outcome;
    }
}
