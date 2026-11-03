#include "lockpick.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Lockpick
{
    namespace
    {
        int clampClass(int lockClass)
        {
            return std::clamp(lockClass, 0, 4);
        }
    }

    bool mayTry(double skill, int lockClass, const Rules& rules)
    {
        return skill >= rules.mSkillGate[static_cast<size_t>(std::clamp(lockClass, 0, 5))];
    }

    SweetSpot sweetSpot(int lockClass, double meterWidth, double random, const Rules& rules)
    {
        const size_t index = static_cast<size_t>(clampClass(lockClass));
        SweetSpot spot;
        spot.mLength = rules.mSweetSpotLength[index];
        spot.mConcentric = rules.mConcentricLength[index];
        const double outer = spot.outer();
        const double wanted = std::clamp(random, 0.0, 1.0) * meterWidth;
        // A meter too narrow for the whole spot has it in the middle
        spot.mCenter = meterWidth > 2 * outer ? std::clamp(wanted, outer, meterWidth - outer) : meterWidth / 2;
        return spot;
    }

    double maxTurn(double pick, const SweetSpot& spot, double meterWidth)
    {
        const double distance = std::abs(pick - spot.mCenter);
        const double outer = spot.outer();
        if (distance <= outer)
        {
            for (int band = 0; band < 6; ++band)
                if (distance <= spot.halfWidth(band))
                    return (1.0 - band / 6.0) * 90.0;
            return 15.0;
        }
        const double span = 2 * outer - meterWidth;
        if (span == 0)
            return 0;
        return std::clamp(15.0 * (outer + distance - meterWidth) / span, 0.0, 15.0);
    }

    double pickHealth(double skill, const Rules& rules)
    {
        return 1.0 + skill * rules.mBonusHealth;
    }

    double forceChance(double skill, int lockClass, const Rules& rules)
    {
        return std::clamp(
            skill - rules.mSkillGate[static_cast<size_t>(std::clamp(lockClass, 0, 5))] + rules.mForceBase, 0.0, 100.0);
    }
}
