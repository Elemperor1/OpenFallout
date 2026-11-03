#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_BARTER_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_BARTER_H

#include <functional>
#include <vector>

namespace FalloutScript::Barter
{
    /// The prices of the barter menu of Fallout 3 and New Vegas, as FalloutNV.exe works them out (BarterMenu::Create
    /// 0x72d250 for the factors, 0x72ed00 for the price of one thing, 0x72f6f0 for the checks of the Accept button and
    /// 0x72fd10 for the deal; Mac round 12AG, A4). Nothing here knows of the menu or of the world: the host gives the
    /// numbers.

    /// The game settings the prices go by, with the defaults of the executables (the plugins of both games give 1.55,
    /// -0.45, 0.45 and 0.45 for the four of the factors, and 0 and 0.0316 for the two of the condition)
    struct Settings
    {
        double mBuyBase = 1.5;
        double mBuyMult = -0.5;
        double mSellBase = 0.5;
        double mSellMult = 0.5;
        double mConditionBase = 0.0;
        double mConditionMult = 0.0316;
        double mConditionExp = 1.5;
    };

    /// What the vendor's prices are multiplied by, from the Barter skill of the player (0 to 100), worked out once when
    /// the menu opens: the base plus the skill times the multiplier over 100
    struct Factors
    {
        double mBuy = 1.0;
        double mSell = 1.0;
    };
    Factors factors(const Settings& settings, double barterSkill);

    /// A number rounded up to a multiple of `step` (the executable's 0x4bd510); a little less than a step above is not
    /// counted as above (the numbers of the executable are floats)
    double roundUp(double value, double step);

    /// The worth of one thing with its condition: `value * base + mult * (1 - base) * value * c ^ exp`, with c the
    /// condition over ten (a thing with no condition, which is what the port has for every thing, has c = 10). A result
    /// above 1 is made a whole number, one of 1 or less goes up in tenths.
    double baseValue(double value, double conditionTenths, const Settings& settings);

    /// The price of one thing. `buying` is the player buying it from the vendor. The base value times the factor of
    /// its side, then the percent that the script that opened the menu gave (a buyer gets that much off with a
    /// positive percent, a seller that much more), then `perk` (the entry point of the perks that work on a price),
    /// then the rounding: a result above 1 goes up to a whole number and one of 1 or less goes up in tenths, and
    /// the price of buying is never less than the base value nor the price of selling more than it.
    double unitPrice(
        double base, bool buying, const Factors& factors, int percent, const std::function<double(double)>& perk);

    /// One line of the offer: `count` things at the price of one of them, bought from the vendor or sold to it
    struct Line
    {
        double mPrice = 0;
        int mCount = 0;
        bool mBuying = false;
    };

    /// What the vendor owes the player for the offer: what the player sells is plus and what the player buys is minus
    /// (the sum is of floats and rounded only at the end, by `amount`)
    double balance(const std::vector<Line>& lines);

    /// The caps that move for a balance: the balance rounded, and no more than the vendor has when the vendor pays
    /// (the vendor's gold is not asked for before, the payout is cut at the deal)
    int amount(double balance, int vendorCaps);

    /// Whether the Accept button is enabled: not while both piles are empty; a vendor that owes (a balance above 0)
    /// always, and else only when the player has the caps to pay
    bool canAccept(bool pilesEmpty, double balance, int playerCaps);
}

#endif
