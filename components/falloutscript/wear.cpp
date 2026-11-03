#include "wear.hpp"

#include <algorithm>
#include <cmath>

#include "actors.hpp"

namespace FalloutScript::Wear
{
    Settings Settings::fromGameSettings(const Lookup& lookup)
    {
        Settings settings;
        if (!lookup)
            return settings;
        const auto take = [&lookup](std::string_view name, double& setting) {
            if (const std::optional<double> value = lookup(name))
                setting = *value;
        };
        take("fDamageToWeaponValue", settings.mWeaponShot);
        take("fDamageToArmorPercentage", settings.mArmorShare);
        take("fMaxArmorRating", settings.mMaxRating);
        return settings;
    }

    double weaponDamageFactor(double condition)
    {
        condition = std::clamp(condition, 0.0, 1.0);
        if (condition > 0.75)
            return 1.0;
        return 1.0 - (0.75 - condition) * 0.67;
    }

    double armorFactor(double condition)
    {
        condition = std::clamp(condition, 0.0, 1.0);
        if (condition > 0.5)
            return 1.0;
        return 0.5 + condition;
    }

    double applyAmmoEffect(double value, std::uint32_t operation, double amount)
    {
        switch (operation)
        {
            case 0:
                return value + amount;
            case 1:
                return value * amount;
            case 2:
                return value - amount;
            default:
                return value;
        }
    }

    double shotPoints(const Settings& settings)
    {
        return std::max(settings.mWeaponShot, 0.0);
    }

    double armorPoints(const Settings& settings, double damage, double resistance)
    {
        const double taken = std::clamp(resistance, 0.0, std::max(settings.mMaxRating, 0.0)) / 100.0;
        return std::max(settings.mArmorShare, 0.0) * std::max(damage, 0.0) * taken;
    }

    int tenthsLost(double points, double maxHealth, double& remainder)
    {
        if (points <= 0 || maxHealth <= 0)
            return 0;
        const double tenths = points / maxHealth * ActorState::fullCondition + remainder;
        const double whole = std::floor(tenths + 1e-9);
        remainder = std::max(tenths - whole, 0.0);
        return static_cast<int>(whole);
    }

    bool crossedLow(int before, int after)
    {
        const double threshold = lowPercent * ActorState::fullCondition / 100.0;
        return before >= threshold && after < threshold;
    }
}
