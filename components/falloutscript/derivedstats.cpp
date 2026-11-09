#include "derivedstats.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace FalloutScript
{
    namespace
    {
        void take(const DerivedStatSettings::Lookup& lookup, std::string_view name, double& setting)
        {
            if (const std::optional<double> value = lookup(name))
                setting = *value;
        }

        int atLeastOne(int level)
        {
            return std::max(level, 1);
        }
    }

    DerivedStatSettings DerivedStatSettings::fromGameSettings(const Lookup& lookup)
    {
        DerivedStatSettings settings;
        if (!lookup)
            return settings;
        take(lookup, "fAVDHealthEnduranceMult", settings.mHealthEnduranceMult);
        take(lookup, "fAVDHealthEnduranceOffset", settings.mHealthEnduranceOffset);
        take(lookup, "fAVDHealthLevelMult", settings.mHealthLevelMult);
        take(lookup, "fAVDNPCHealthEnduranceMult", settings.mNpcHealthEnduranceMult);
        take(lookup, "fAVDNPCHealthEnduranceOffset", settings.mNpcHealthEnduranceOffset);
        take(lookup, "fAVDNPCHealthLevelMult", settings.mNpcHealthLevelMult);
        take(lookup, "fAVDActionPointsBase", settings.mActionPointsBase);
        take(lookup, "fAVDActionPointsMult", settings.mActionPointsMult);
        take(lookup, "fAVDCarryWeightsBase", settings.mCarryWeightBase);
        take(lookup, "fAVDCarryWeightMult", settings.mCarryWeightMult);
        take(lookup, "fAVDMeleeDamageStrengthMult", settings.mMeleeDamageStrengthMult);
        take(lookup, "fAVDMeleeDamageStrengthOffset", settings.mMeleeDamageStrengthOffset);
        take(lookup, "fAVDCritLuckBase", settings.mCritLuckBase);
        take(lookup, "fAVDCritLuckMult", settings.mCritLuckMult);
        take(lookup, "fActorSwimBreathBase", settings.mSwimBreathBase);
        take(lookup, "fActorSwimBreathMult", settings.mSwimBreathMult);
        take(lookup, "fActorSwimBreathDamage", settings.mSwimBreathDamage);
        static constexpr std::string_view healRateNames[5]
            = { "fAVDHealRateEndurance6Bonus", "fAVDHealRateEndurance7Bonus", "fAVDHealRateEndurance8Bonus",
                  "fAVDHealRateEndurance9Bonus", "fAVDHealRateEndurance10Bonus" };
        for (std::size_t i = 0; i < 5; ++i)
            take(lookup, healRateNames[i], settings.mHealRateEnduranceBonus[i]);
        return settings;
    }

    double DerivedStats::playerHealth(
        const DerivedStatSettings& settings, double baseHealth, double endurance, int level)
    {
        return std::max(baseHealth + (endurance + settings.mHealthEnduranceOffset) * settings.mHealthEnduranceMult
                + (atLeastOne(level) - 1) * settings.mHealthLevelMult,
            1.0);
    }

    double DerivedStats::npcHealth(const DerivedStatSettings& settings, double baseHealth, double endurance, int level)
    {
        return std::max(baseHealth + (endurance + settings.mNpcHealthEnduranceOffset) * settings.mNpcHealthEnduranceMult
                + (atLeastOne(level) - 1) * settings.mNpcHealthLevelMult,
            1.0);
    }

    double DerivedStats::actionPoints(const DerivedStatSettings& settings, double agility)
    {
        return std::max(settings.mActionPointsBase + agility * settings.mActionPointsMult, 0.0);
    }

    double DerivedStats::carryWeight(const DerivedStatSettings& settings, double strength)
    {
        return std::max(settings.mCarryWeightBase + strength * settings.mCarryWeightMult, 0.0);
    }

    double DerivedStats::meleeDamage(const DerivedStatSettings& settings, double strength)
    {
        return std::max((strength + settings.mMeleeDamageStrengthOffset) * settings.mMeleeDamageStrengthMult, 0.0);
    }

    double DerivedStats::criticalChance(const DerivedStatSettings& settings, double luck)
    {
        return std::clamp(settings.mCritLuckBase + luck * settings.mCritLuckMult, 0.0, 100.0);
    }

    double DerivedStats::healRate(const DerivedStatSettings& settings, double endurance)
    {
        const int points = static_cast<int>(std::trunc(endurance));
        if (points < 6 || points > 10)
            return 0;
        return settings.mHealRateEnduranceBonus[points - 6];
    }

    double DerivedStats::swimBreath(const DerivedStatSettings& settings, double healRate)
    {
        return std::max(settings.mSwimBreathBase + settings.mSwimBreathMult * std::trunc(healRate), 0.0);
    }

    DerivedStats::SwimBreathStep DerivedStats::swimBreathStep(const DerivedStatSettings& settings, double remaining,
        double longest, bool headUnder, bool breathesWater, double health, double seconds)
    {
        SwimBreathStep step;
        step.mRemaining = remaining;
        if (!headUnder)
        {
            step.mRemaining = longest;
            return step;
        }
        if (breathesWater)
            return step;
        step.mRemaining = remaining - seconds;
        if (step.mRemaining < 0)
        {
            step.mDamage = settings.mSwimBreathDamage * std::trunc(health) * seconds;
            step.mRemaining = 0;
        }
        return step;
    }

    int DerivedStats::airMeterVisibleTicks(double remaining, double longest)
    {
        if (!(longest > 0))
            return airMeterTicks;
        const double share = std::clamp(remaining / longest, 0.0, 1.0);
        // The share is rounded to ten thousandths first: ticks that a full breath rounds off by a hair do not go
        return std::clamp(
            static_cast<int>(std::ceil(std::round(share * airMeterTicks * 10000.0) / 10000.0)), 0, airMeterTicks);
    }
}
