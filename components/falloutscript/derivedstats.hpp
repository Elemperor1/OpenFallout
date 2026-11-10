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
        double mSwimBreathBase = 10; // fActorSwimBreathBase: the seconds that a character holds its breath under water
        double mSwimBreathMult = 0.5; // fActorSwimBreathMult: what each point of the stat adds to them
        double mSwimBreathDamage = 0.2; // fActorSwimBreathDamage: the share of its health that it loses for each second
        /// The heal rate that an endurance of 6 to 10 gives (fAVDHealRateEndurance6Bonus to
        /// fAVDHealRateEndurance10Bonus; a lower or higher endurance gives none), which the breath is worked out from
        double mHealRateEnduranceBonus[5] = { 5, 5, 5, 10, 10 };

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The settings that the lookup (a GMST by its editor id) has
        static DerivedStatSettings fromGameSettings(const Lookup& lookup);
    };

    /// What the games work out from other actor values, in the formulas the settings of their files support: the base
    /// health is the one that the record of the character gives (100 for the player in both games, so that it has
    /// 200 health at the first level with 5 endurance)
    namespace DerivedStats
    {
        /// The player: the base health of the record, a share of its endurance and one for each level after the first
        double playerHealth(const DerivedStatSettings& settings, double baseHealth, double endurance, int level);
        /// Everyone else (characters and creatures)
        double npcHealth(const DerivedStatSettings& settings, double baseHealth, double endurance, int level);
        double actionPoints(const DerivedStatSettings& settings, double agility);
        double carryWeight(const DerivedStatSettings& settings, double strength);
        /// What a hit with a melee weapon (and the hands) adds to its damage
        double meleeDamage(const DerivedStatSettings& settings, double strength);
        /// The chance of a critical hit in percent
        double criticalChance(const DerivedStatSettings& settings, double luck);

        /// The heal rate that the endurance gives (FalloutNV.exe 0x643A70, Mac round 12BM): the games make the breath
        /// of a character last by it
        double healRate(const DerivedStatSettings& settings, double endurance);

        /// The health that a character gets back by its heal rate in the simulated seconds that pass: the games give
        /// the heal rate over 3600 for each second of the simulation (FalloutNV.exe 0x648B00 and 0x88B510, item 17 of
        /// m13-exe-reads.md), so a game hour is 3600 over the time scale of those seconds. A negative rate gives none.
        double regeneration(double healRate, double seconds);

        /// How far under water a character has to be for the breath to run out, as a share of its height: the head is
        /// under the water when the point at that share of the height is (FalloutNV.exe 0x889930, Mac rounds 12BI B and
        /// 12BM A)
        constexpr double swimSubmersion = 0.875;
        /// The seconds that a character holds its breath: the base and a share of the heal rate (whole points of it)
        double swimBreath(const DerivedStatSettings& settings, double healRate);

        /// What a frame under or over the water does to the breath
        struct SwimBreathStep
        {
            /// The seconds of breath that are left
            double mRemaining = 0;
            /// The health that the frame took (the breath ran out)
            double mDamage = 0;
            /// The breath is out and the health is under one point: the health counts in whole points, so the frame
            /// takes none of it and the character would stay alive for ever. The port ends it (the executable's end
            /// of the drowning, a call after the damage, was not traced).
            bool mFatal = false;
        };
        /// The breath after a frame of `seconds`. With the head under the water it runs down, unless the character can
        /// breathe there; out of the breath the character takes the share of its health that the settings say for each
        /// second (whole points of the health, and a character with less than one point dies), and the breath stays at
        /// 0. Above the water it is full again at once.
        SwimBreathStep swimBreathStep(const DerivedStatSettings& settings, double remaining, double longest,
            bool headUnder, bool breathesWater, double health, double seconds);

        /// The ticks of the air meter that show: 16 when the breath is full, one less for each sixteenth that is gone
        /// (the meter's sequence hides tick k at k/30 seconds and the sequence time is a sixteenth-thirtieths of the
        /// share that is gone)
        constexpr int airMeterTicks = 16;
        int airMeterVisibleTicks(double remaining, double longest);
    }
}

#endif
