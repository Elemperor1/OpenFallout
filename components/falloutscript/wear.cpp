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
        take("fDamageToWeaponGunMult", settings.mGunMult);
        take("fDamageToWeaponEnergyMult", settings.mEnergyMult);
        take("fDamageToWeaponLauncherMult", settings.mLauncherMult);
        take("fDamageToWeaponMeleeMult", settings.mMeleeMult);
        take("fArmorRatingBase", settings.mArmorBase);
        take("fArmorRatingMax", settings.mArmorMax);
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

    double armorFactor(const Settings& settings, double condition)
    {
        if (!settings.mFallout3)
            return armorFactor(condition);
        condition = std::clamp(condition, 0.0, 1.0);
        return settings.mArmorBase + (settings.mArmorMax - settings.mArmorBase) * condition;
    }

    double armorValue(const Settings& settings, double value, double condition)
    {
        const double worn = value * armorFactor(settings, condition);
        return settings.mFallout3 ? std::round(worn) : worn;
    }

    double armorShown(const Settings& settings, double value, double condition)
    {
        const double worn = value * armorFactor(settings, condition);
        return settings.mFallout3 ? std::round(worn) : std::trunc(worn);
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

    double shotPoints(const Settings& settings, const WeaponWear& weapon)
    {
        if (!settings.mFallout3)
            return shotPoints(settings);
        double factor = 1.0;
        if (weapon.mOverridden)
            factor = weapon.mOverride;
        else
            switch (weapon.mType)
            {
                case 0:
                case 1:
                case 2:
                    factor = settings.mMeleeMult;
                    break;
                case 3:
                case 5:
                case 6:
                    factor = settings.mGunMult;
                    break;
                case 4:
                case 7:
                    factor = settings.mEnergyMult;
                    break;
                case 8:
                case 9:
                    factor = settings.mLauncherMult;
                    break;
                default:
                    break;
            }
        return std::max(static_cast<double>(weapon.mDamage) * factor, 0.0);
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
