#include "hitreactions.hpp"

#include <algorithm>

namespace FalloutScript::HitReactions
{
    Settings Settings::fromGameSettings(const Lookup& lookup)
    {
        Settings settings;
        const auto number = [&](std::string_view name, double& value) {
            if (const std::optional<double> found = lookup(name))
                value = *found;
        };
        number("fMoveOneCrippledLegSpeedMult", settings.mOneCrippledLeg);
        number("fMoveTwoCrippledLegsSpeedMult", settings.mTwoCrippledLegs);
        number("fKnockdownBaseHealthThreshold", settings.mKnockdownBaseHealthThreshold);
        number("fKnockdownCurrentHealthThreshold", settings.mKnockdownCurrentHealthThreshold);
        number("fKnockdownDamageBase", settings.mKnockdownDamageBase);
        number("fKnockdownDamageMult", settings.mKnockdownDamageMult);
        number("fKnockdownAgilBase", settings.mKnockdownAgilBase);
        number("fKnockdownAgilMult", settings.mKnockdownAgilMult);
        number("fKnockdownChance", settings.mKnockdownChance);
        number("fKnockbackAgilBase", settings.mKnockbackAgilBase);
        number("fKnockbackAgilMult", settings.mKnockbackAgilMult);
        number("fKnockbackDamageBase", settings.mKnockbackDamageBase);
        number("fKnockbackDamageMult", settings.mKnockbackDamageMult);
        number("fKnockbackForceMax", settings.mKnockbackForceMax);
        number("fKnockbackTime", settings.mKnockbackTime);
        return settings;
    }

    int crippledLegs(double leftCondition, double rightCondition)
    {
        return (leftCondition <= 0 ? 1 : 0) + (rightCondition <= 0 ? 1 : 0);
    }

    double legSpeedMultiplier(const Settings& settings, int crippled)
    {
        if (crippled == 1)
            return settings.mOneCrippledLeg;
        if (crippled >= 2)
            return settings.mTwoCrippledLegs;
        return 1;
    }

    Knockdown knockdownKind(const Settings& settings, double damage, double health, double baseHealth)
    {
        // Nobody that is dead already, or that the blow kills, falls: it dies
        if (!(health > 0) || !(health - damage > 0))
            return Knockdown::Never;
        if (baseHealth > 0 && damage / baseHealth * 100 > settings.mKnockdownBaseHealthThreshold)
            return Knockdown::Always;
        if (!(damage / health * 100 > settings.mKnockdownCurrentHealthThreshold))
            return Knockdown::Never;
        return Knockdown::ByChance;
    }

    int knockdownPerMille(const Settings& settings, double damage, double agility)
    {
        const double divisor = settings.mKnockdownAgilMult * (agility * 10) + settings.mKnockdownAgilBase;
        const double wanted = settings.mKnockdownDamageMult * damage + settings.mKnockdownDamageBase;
        // A divisor that is not above 0 (an agility of 0 with no base) is the largest chance there is
        const double chance = divisor > 0 ? std::min(wanted / divisor, settings.mKnockdownChance) : settings.mKnockdownChance;
        return std::max(0, static_cast<int>(chance * 1000));
    }

    double knockbackForce(const Settings& settings, double damage, double agility)
    {
        const double agile = settings.mKnockbackAgilMult * (agility * 10) + settings.mKnockbackAgilBase;
        const double force = agile * (settings.mKnockbackDamageMult * damage + settings.mKnockbackDamageBase);
        return std::min(force, settings.mKnockbackForceMax);
    }
}
