#include "damage.hpp"

#include <algorithm>

namespace FalloutScript
{
    DamageSettings DamageSettings::fromGameSettings(const Lookup& lookup)
    {
        DamageSettings settings;
        if (!lookup)
            return settings;
        const auto take = [&lookup](std::string_view name, double& setting) {
            if (const std::optional<double> value = lookup(name))
                setting = *value;
        };
        take("fMaxArmorRating", settings.mMaxResistance);
        take("fMinDamMultiplier", settings.mMinimumThroughThreshold);
        take("fDamageSkillBase", settings.mSkillBase);
        take("fDamageSkillMult", settings.mSkillMult);
        take("fDamageGunWeapCondBase", settings.mGunConditionBase);
        take("fDamageGunWeapCondMult", settings.mGunConditionMult);
        take("fDamageMeleeWeapCondBase", settings.mMeleeConditionBase);
        take("fDamageMeleeWeapCondMult", settings.mMeleeConditionMult);
        return settings;
    }

    double Damage::criticalChance(const WeaponDamage& weapon, double attackerChance)
    {
        return std::clamp(attackerChance * weapon.mCriticalChanceMultiplier, 0.0, 100.0);
    }

    HitResult Damage::resolve(const DamageSettings& settings, const WeaponDamage& weapon, double attackerChance,
        double meleeBonus, const Defense& defense, double roll)
    {
        HitResult result;
        result.mCritical = roll < criticalChance(weapon, attackerChance);

        double blow = std::max(weapon.mDamage, 0.0);
        const double condition = std::clamp(weapon.mCondition, 0.0, 1.0);
        const double skill = std::clamp(weapon.mSkill, 0.0, 100.0) / 100.0;
        if (weapon.mMelee)
            blow *= settings.mMeleeConditionBase + settings.mMeleeConditionMult * condition;
        else
            blow *= settings.mGunConditionBase + settings.mGunConditionMult * condition;
        blow *= settings.mSkillBase + settings.mSkillMult * skill;
        if (weapon.mMelee && settings.mMeleeStrengthBonus)
            blow += std::max(meleeBonus, 0.0);

        // The bonus of a critical blow is part of what the armor works on
        if (result.mCritical)
            blow += std::max(weapon.mCriticalDamage, 0.0);

        // The threshold takes away points, but never more than what leaves a share of the blow
        const double floor = blow * std::clamp(settings.mMinimumThroughThreshold, 0.0, 1.0);
        double through = blow;
        if (defense.mThreshold > 0)
            through = std::max(blow - defense.mThreshold, floor);
        // Then the resistance takes a share
        const double resisted = std::clamp(defense.mResistance, 0.0, settings.mMaxResistance) / 100.0;
        blow = through * (1.0 - resisted);
        result.mDamage = std::max(blow, 0.0);
        return result;
    }
}
