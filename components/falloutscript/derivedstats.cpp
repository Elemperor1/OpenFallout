#include "derivedstats.hpp"

#include <algorithm>

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
}
