#include "repair.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Repair
{
    namespace
    {
        constexpr int fullCondition = 1000;

        /// The health as the formulas count it: tenths of the full health, 0 to 10
        double tenths(int condition)
        {
            return std::clamp(condition, 0, fullCondition) / 100.0;
        }
    }

    int repairedCondition(double skill, int brokenCondition, int spareCondition, const Settings& settings)
    {
        const double a = tenths(brokenCondition);
        const double b = tenths(spareCondition);
        const double c = std::max(a, b) + settings.mMin + (settings.mMax - settings.mMin) * skill / 100.0
            + std::min(a, b) * settings.mScavengeMult;
        // c is in tenths of the full health
        return std::clamp(static_cast<int>(c * 100.0), 0, fullCondition);
    }

    int serviceCondition(double vendorSkill, const Settings& settings)
    {
        const double factor = settings.mSkillBase + (settings.mSkillMax - settings.mSkillBase) * vendorSkill / 100.0;
        return std::clamp(static_cast<int>(factor * 100.0), 0, fullCondition);
    }

    int serviceCost(double vendorSkill, int condition, int value, const Settings& settings)
    {
        const double factor = settings.mSkillBase + (settings.mSkillMax - settings.mSkillBase) * vendorSkill / 100.0;
        const double cost = settings.mCostMult * value * (factor - tenths(condition)) / 10.0;
        if (cost <= 0.5)
            return 0;
        return std::max(1, static_cast<int>(cost));
    }
}
