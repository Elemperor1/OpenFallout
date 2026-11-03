#include "barter.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Barter
{
    namespace
    {
        // The executable works in single-precision floats: a value that is a hair over a whole number is not above it
        constexpr double tolerance = 1e-6;
    }

    Factors factors(const Settings& settings, double barterSkill)
    {
        Factors result;
        result.mBuy = settings.mBuyBase + barterSkill * settings.mBuyMult / 100.0;
        result.mSell = settings.mSellBase + barterSkill * settings.mSellMult / 100.0;
        return result;
    }

    double roundUp(double value, double step)
    {
        if (step <= 0)
            return value;
        return std::ceil(value / step - tolerance) * step;
    }

    double baseValue(double value, double conditionTenths, const Settings& settings)
    {
        const double worth = value * settings.mConditionBase
            + settings.mConditionMult * (1.0 - settings.mConditionBase) * value
                * std::pow(conditionTenths, settings.mConditionExp);
        return std::floor(worth) > 1.0 ? roundUp(worth, 1.0) : roundUp(worth, 0.1);
    }

    double unitPrice(
        double base, bool buying, const Factors& factors, int percent, const std::function<double(double)>& perk)
    {
        double value = base * (buying ? factors.mBuy : factors.mSell);
        const double fraction = std::abs(percent) / 100.0;
        // A positive percent is the vendor's kindness: less to pay for what is bought, more for what is sold
        if (percent != 0)
            value *= (buying == (percent > 0)) ? 1.0 - fraction : 1.0 + fraction;
        if (perk)
            value = perk(value);
        value = std::floor(value) > 1.0 ? roundUp(value, 1.0) : roundUp(value, 0.1);
        return buying ? std::max(value, base) : std::min(value, base);
    }

    double balance(const std::vector<Line>& lines)
    {
        double sum = 0;
        for (const Line& line : lines)
            sum += (line.mBuying ? -1.0 : 1.0) * line.mCount * line.mPrice;
        return sum;
    }

    int amount(double balance, int vendorCaps)
    {
        const int rounded = static_cast<int>(std::lround(std::abs(balance)));
        if (balance > 0)
            return std::min(rounded, std::max(vendorCaps, 0));
        return rounded;
    }

    bool canAccept(bool pilesEmpty, double balance, int playerCaps)
    {
        if (pilesEmpty)
            return false;
        if (balance > 0)
            return true;
        return -balance <= playerCaps;
    }
}
