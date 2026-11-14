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
        /// A condition of the copies, in tenths of the full worth that the prices count with (full is 10)
        double conditionTenths(int condition)
        {
            return std::clamp(condition, 0, int(FalloutScript::ActorState::fullCondition)) / 100.0;
        }

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

    int FalloutScripts::barterStock(FalloutScript::FormId vendor, FalloutScript::FormId item, int condition)
    {
        int total = 0;
        for (const FalloutScript::FormId holder : barterSources(vendor))
        {
            // A trader does not sell what it wears: the copies like the one on are not for sale
            const int worn = holder == vendor ? mEquipment.equippedCopy(vendor, item) : -1;
            for (const FalloutScript::ActorState::ConditionStack& stack : mActors.conditions(holder, item))
                if (stack.copy() != worn && (condition < 0 || stack.copy() == condition))
                    total += stack.mCount;
        }
        return total;
    }

    int FalloutScripts::barterMove(
        FalloutScript::FormId vendor, FalloutScript::FormId to, FalloutScript::FormId item, int count, int condition)
    {
        // Taken from the container first, then from what the vendor carries
        int moved = 0;
        for (const FalloutScript::FormId holder : barterSources(vendor))
        {
            const int worn = holder == vendor ? mEquipment.equippedCopy(vendor, item) : -1;
            for (const FalloutScript::ActorState::ConditionStack& stack : mActors.conditions(holder, item))
            {
                if (moved >= count)
                    break;
                if (stack.copy() == worn || (condition >= 0 && stack.copy() != condition))
                    continue;
                moved += transferItems(holder, to, item, count - moved, stack.copy());
            }
        }
        return moved;
    }

    FalloutScripts::BarterPiles FalloutScripts::barterNamed(const BarterPiles& pile, bool buying)
    {
        // A pick that names no condition takes the best copies, the next best for what is left; from here on every
        // pick says which copies it means
        BarterPiles result;
        std::map<std::pair<FalloutScript::FormId, int>, int> taken;
        const std::optional<BarterMenu> menu = barterMenu();
        for (const BarterPick& pick : pile)
        {
            if (pick.mCondition >= 0 || pick.mCount <= 0 || pick.mItem == 0)
            {
                result.push_back(pick);
                continue;
            }
            // The copies there are, by condition, best first
            std::vector<FalloutScript::ActorState::ConditionStack> stacks;
            const auto gather = [&stacks](const std::vector<FalloutScript::ActorState::ConditionStack>& more,
                                    int worn) {
                for (const FalloutScript::ActorState::ConditionStack& stack : more)
                {
                    if (stack.copy() == worn)
                        continue;
                    const auto same = std::find_if(stacks.begin(), stacks.end(),
                        [&stack](const FalloutScript::ActorState::ConditionStack& listed) {
                            return listed.copy() == stack.copy();
                        });
                    if (same != stacks.end())
                        same->mCount += stack.mCount;
                    else
                        stacks.push_back(stack);
                }
            };
            if (buying && menu)
            {
                for (const FalloutScript::FormId holder : barterSources(menu->mVendor))
                    gather(mActors.conditions(holder, pick.mItem),
                        holder == menu->mVendor ? mEquipment.equippedCopy(holder, pick.mItem) : -1);
            }
            else
                gather(mActors.conditions(mPlayer, pick.mItem), -1);
            std::stable_sort(stacks.begin(), stacks.end(),
                [](const FalloutScript::ActorState::ConditionStack& left,
                    const FalloutScript::ActorState::ConditionStack& right) {
                    // The best first; of one condition the copy with fewer mods
                    return left.mCondition != right.mCondition ? left.mCondition > right.mCondition
                                                               : left.mMods < right.mMods;
                });
            int left = pick.mCount;
            for (const FalloutScript::ActorState::ConditionStack& stack : stacks)
            {
                int& already = taken[{ pick.mItem, stack.copy() }];
                const int going = std::min(left, stack.mCount - already);
                if (going <= 0)
                    continue;
                already += going;
                left -= going;
                result.push_back({ pick.mItem, going, stack.copy() });
            }
            // More than there is: a pick that is no deal
            if (left > 0)
                result.push_back({ pick.mItem, left, FalloutScript::ActorState::fullCondition });
        }
        return result;
    }

    int FalloutScripts::itemValue(FalloutScript::FormId item)
    {
        // The value that a script gave the item (SetItemValue) is the one every trade uses
        if (int given = 0; mItemValues.find(item, given))
            return given;
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

    double FalloutScripts::barterPrice(const BarterMenu& menu, FalloutScript::FormId item, bool buying, int condition)
    {
        const int value = itemValue(item);
        if (value <= 0)
            return 0.0;
        const FalloutScript::Barter::Settings settings = barterSettings();
        const FalloutScript::Barter::Factors factors
            = FalloutScript::Barter::factors(settings, mActorValues.current(mPlayer, AV::barter));
        double base = FalloutScript::Barter::baseValue(value, conditionTenths(FalloutScript::copyCondition(condition)), settings);
        // Each mod installed in a weapon adds its own worth, and the sum is rounded up to a tenth (FalloutNV.exe 0x4BD400,
        // Mac round 12AX R3b)
        if (const int mods = FalloutScript::copyMods(condition); mods != 0)
            base = std::ceil((base + modsValue(item, mods)) * 10.0 - 1e-9) / 10.0;
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
                const auto same = std::find_if(items.begin(), items.end(), [&item](const PipboyItem& listed) {
                    return listed.mId == item.mId && listed.mCondition == item.mCondition && listed.mMods == item.mMods;
                });
                if (same != items.end())
                    same->mCount += item.mCount;
                else
                    items.push_back(std::move(item));
            }
        }
        // Copies with another condition are rows of their own, and a row says which copies it is when there is more
        // than one condition among the copies of the item
        for (PipboyItem& item : items)
        {
            if (!item.mHasCondition)
                continue;
            item.mStack = item.mCondition != FalloutScript::ActorState::fullCondition || item.mMods != 0
                || std::any_of(items.begin(), items.end(), [&item](const PipboyItem& other) {
                       return other.mId == item.mId
                           && (other.mCondition != item.mCondition || other.mMods != item.mMods);
                   });
        }
        for (PipboyItem& item : items)
        {
            // What has no worth is not traded, nor the caps (they are the money), nor what the player does not see
            // in its own things
            if (item.mId == caps || item.mValue <= 0 || (!vendorSide && (item.mKey || item.mHidden)))
                continue;
            BarterRow row;
            row.mPrice = barterPrice(*menu, item.mId, vendorSide, FalloutScript::copyOf(item.mCondition, item.mMods));
            row.mItem = std::move(item);
            rows.push_back(std::move(row));
        }
        return rows;
    }

    FalloutScripts::BarterQuote FalloutScripts::barterQuote(const BarterPiles& buyPicked, const BarterPiles& sellPicked)
    {
        const BarterPiles buy = barterNamed(buyPicked, true);
        const BarterPiles sell = barterNamed(sellPicked, false);
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
        // A pile that asks for more than the holder has (counting the lines of one thing together, a request from a
        // script can name a thing twice), or for a thing without a worth, is no deal
        const auto add = [&](const BarterPiles& pile, bool buying) {
            std::map<std::pair<FalloutScript::FormId, int>, int> asked;
            for (const BarterPick& pick : pile)
            {
                int have = 0;
                if (buying)
                    have = barterStock(vendor, pick.mItem, pick.mCondition);
                else
                    for (const FalloutScript::ActorState::ConditionStack& stack : mActors.conditions(mPlayer, pick.mItem))
                        if (stack.copy() == pick.mCondition)
                            have = stack.mCount;
                int& total = asked[{ pick.mItem, pick.mCondition }];
                if (pick.mCount <= 0 || itemValue(pick.mItem) <= 0 || have - total < pick.mCount)
                {
                    quote.mValid = false;
                    continue;
                }
                total += pick.mCount;
                lines.push_back({ barterPrice(*menu, pick.mItem, buying, pick.mCondition), pick.mCount, buying });
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

    FalloutScripts::BarterOutcome FalloutScripts::barterDeal(
        const BarterPiles& buyPicked, const BarterPiles& sellPicked)
    {
        const BarterPiles buy = barterNamed(buyPicked, true);
        const BarterPiles sell = barterNamed(sellPicked, false);
        BarterOutcome outcome;
        const std::optional<BarterMenu> menu = barterMenu();
        if (!menu)
        {
            outcome.mStatus = "closed";
            return outcome;
        }
        const BarterQuote quote = barterQuote(buyPicked, sellPicked);
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
        for (const BarterPick& pick : buy)
            barterMove(vendor, mPlayer, pick.mItem, pick.mCount, pick.mCondition);
        for (const BarterPick& pick : sell)
            transferItems(mPlayer, holder, pick.mItem, pick.mCount, pick.mCondition);
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
