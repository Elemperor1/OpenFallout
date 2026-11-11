#include "falldamage.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::FallDamage
{
    Settings Settings::fromGameSettings(const Lookup& lookup)
    {
        Settings settings;
        const auto number = [&](std::string_view name, double& value) {
            if (const std::optional<double> found = lookup(name))
                value = *found;
        };
        number("fJumpFallHeightMin", settings.mMinHeight);
        number("fJumpFallHeightExponent", settings.mExponent);
        number("fJumpFallHeightMult", settings.mMultiplier);
        number("fFallLegDamageMult", settings.mLegMultiplier);
        number("fHardLandingDamageThreshold", settings.mHardLanding);
        if (const std::optional<double> chance = lookup("iFallLegDamageChance"))
            settings.mLegChance = static_cast<int>(*chance);
        return settings;
    }

    double damage(const Settings& settings, double heightLost)
    {
        if (!(heightLost > settings.mMinHeight))
            return 0;
        return settings.mMultiplier * std::pow(heightLost - settings.mMinHeight, settings.mExponent);
    }

    double damageToPlayerMultiplier(int difficulty)
    {
        constexpr double multipliers[] = { 0.5, 0.75, 1, 1.5, 2 };
        return multipliers[std::clamp(difficulty, 0, 4)];
    }

    double damageByPlayerMultiplier(int difficulty)
    {
        constexpr double multipliers[] = { 2, 1.5, 1, 0.75, 0.5 };
        return multipliers[std::clamp(difficulty, 0, 4)];
    }

    double blowMultiplier(int difficulty, bool attackerIsPlayer, bool victimIsPlayer)
    {
        if (victimIsPlayer && !attackerIsPlayer)
            return damageToPlayerMultiplier(difficulty);
        if (attackerIsPlayer && !victimIsPlayer)
            return damageByPlayerMultiplier(difficulty);
        return 1;
    }

    bool isHardLanding(const Settings& settings, double damage)
    {
        return damage > settings.mHardLanding;
    }

    bool legHurt(const Settings& settings, int roll)
    {
        return roll < settings.mLegChance;
    }
}
