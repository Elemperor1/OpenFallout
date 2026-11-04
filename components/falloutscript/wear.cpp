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
        if (settings.mFallout3)
            return std::round(value * armorFactor(settings, condition));
        // New Vegas: a piece gives its whole number times the factor, to the next whole number up (0x4BDF90 and 0x4BE0B0,
        // Mac round 12BA A; the small step keeps 17.000000000000004 from becoming 18)
        return std::ceil(std::trunc(value) * armorFactor(settings, condition) - 1e-9);
    }

    double armorShown(const Settings& settings, double value, double condition)
    {
        // What a card prints is what the piece gives (the card writes the next whole number up of the product, 0x70896B)
        return armorValue(settings, value, condition);
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

    double armorPoints(const Settings& settings, double damage, double resistance, double threshold, double remaining,
        double floorShare)
    {
        const double fromResistance = armorPoints(settings, damage, resistance);
        // Fallout 3 has no threshold, and so no second term
        if (settings.mFallout3)
            return fromResistance;
        // The threshold wears armor by the share of the smaller of itself and what is left of the blow above the floor
        // under which a blow cannot fall (0x9B62CD: the term can be negative, and then it takes from the first)
        const double floor = std::max(floorShare, 0.0) * std::max(damage, 0.0);
        return fromResistance + std::max(settings.mArmorShare, 0.0) * std::min(std::max(threshold, 0.0), remaining - floor);
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
