#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DERIVEDSTATS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DERIVEDSTATS_H

#include <functional>
#include <optional>
#include <string_view>

namespace FalloutScript
{
    /// The settings of the games (the GMST records) that say how the actor values that depend on others are worked out.
    /// A setting that the files do not have keeps the value of New Vegas; Fallout 3 differs in the health a level gives
    /// (10) and the action points an agility point gives (2), and its files say so. The offsets are only known by their
    /// names in the GECK (0 until the files say otherwise): they are added to the attribute before its multiplier.
    struct DerivedStatSettings
    {
        double mHealthEnduranceMult = 20; // fAVDHealthEnduranceMult, the player
        double mHealthEnduranceOffset = 0; // fAVDHealthEnduranceOffset: added to the endurance before the multiplier
        double mHealthLevelMult = 5; // fAVDHealthLevelMult, the player
        double mNpcHealthEnduranceMult = 5; // fAVDNPCHealthEnduranceMult, everyone else
        double mNpcHealthEnduranceOffset = 0; // fAVDNPCHealthEnduranceOffset
        double mNpcHealthLevelMult = 5; // fAVDNPCHealthLevelMult
        double mActionPointsBase = 65; // fAVDActionPointsBase
        double mActionPointsMult = 3; // fAVDActionPointsMult
        double mCarryWeightBase = 150; // fAVDCarryWeightsBase
        double mCarryWeightMult = 10; // fAVDCarryWeightMult
        double mMeleeDamageStrengthMult = 0.5; // fAVDMeleeDamageStrengthMult
        double mMeleeDamageStrengthOffset = 0; // fAVDMeleeDamageStrengthOffset: added to the strength
        double mCritLuckBase = 0; // fAVDCritLuckBase
        double mCritLuckMult = 1; // fAVDCritLuckMult

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The settings that the lookup (a GMST by its editor id) has
        static DerivedStatSettings fromGameSettings(const Lookup& lookup);
    };

    /// What the games work out from other actor values, in the formulas the settings of their files support: the base
    /// health is the one that the record of the character gives
    namespace DerivedStats
    {
        /// The player: the base health of the record, a share of its endurance and one for each level
        double playerHealth(const DerivedStatSettings& settings, double baseHealth, double endurance, int level);
        /// Everyone else (characters and creatures)
        double npcHealth(const DerivedStatSettings& settings, double baseHealth, double endurance, int level);
        double actionPoints(const DerivedStatSettings& settings, double agility);
        double carryWeight(const DerivedStatSettings& settings, double strength);
        /// What a hit with a melee weapon (and the hands) adds to its damage
        double meleeDamage(const DerivedStatSettings& settings, double strength);
        /// The chance of a critical hit in percent
        double criticalChance(const DerivedStatSettings& settings, double luck);
    }
}

#endif
