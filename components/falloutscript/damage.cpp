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
        take("fFriendHitTimer", settings.mFriendHitSeconds);
        take("fFriendMinimumLastHitTime", settings.mFriendHitSpacingSeconds);
        const auto takeCount = [&lookup](std::string_view name, int& setting) {
            if (const std::optional<double> value = lookup(name))
                setting = static_cast<int>(*value);
        };
        takeCount("iFriendHitCombatAllowed", settings.mFriendHitCombatAllowed);
        takeCount("iFriendHitNonCombatAllowed", settings.mFriendHitNonCombatAllowed);
        takeCount("iAllyHitCombatAllowed", settings.mAllyHitCombatAllowed);
        takeCount("iAllyHitNonCombatAllowed", settings.mAllyHitNonCombatAllowed);
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

        // The executables work the armor in this order: the resistance takes its share (no more than the most armor
        // can), the threshold takes points, and the blow does not fall below a share of what it was when it met the
        // armor (New Vegas; Fallout 3 has no such floor)
        const double entering = blow;
        const double resisted = std::clamp(defense.mResistance, 0.0, settings.mMaxResistance) / 100.0;
        blow *= 1.0 - resisted;
        blow -= std::max(defense.mThreshold, 0.0);
        if (settings.mMinimumDamage)
            blow = std::max(blow, entering * std::clamp(settings.mMinimumThroughThreshold, 0.0, 1.0));
        result.mDamage = std::max(blow, 0.0);
        return result;
    }
}
