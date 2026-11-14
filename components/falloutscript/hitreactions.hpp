#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HITREACTIONS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_HITREACTIONS_H

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string_view>

#include "script.hpp"

namespace FalloutScript::HitReactions
{
    /// The settings that a blow and a crippled leg are worked out from. The values are the defaults of FalloutNV.exe
    /// (read in round 14 part two: the knockdown check at 0x646400, the knockback force at 0x646580 and its use at
    /// 0x89c8cf, the speed of a character at 0x647d10); a setting that a plugin does not have keeps them, and the plugins
    /// of both games do not override any of these.
    struct Settings
    {
        /// fMoveOneCrippledLegSpeedMult and fMoveTwoCrippledLegsSpeedMult: what a character with one or both legs
        /// crippled has left of its speed
        double mOneCrippledLeg = 0.85;
        double mTwoCrippledLegs = 0.75;

        /// A blow that does more than this share (in percent) of the base health knocks the character down for sure
        /// (fKnockdownBaseHealthThreshold), and one that does no more than this share of the current health never
        /// does (fKnockdownCurrentHealthThreshold)
        double mKnockdownBaseHealthThreshold = 75;
        double mKnockdownCurrentHealthThreshold = 25;
        /// The chance of a knockdown is (damage * mult + base) / (agility * 10 * mult + base), at most
        /// fKnockdownChance, and is rolled as a number of a thousand
        double mKnockdownDamageBase = 0;
        double mKnockdownDamageMult = 0.3;
        double mKnockdownAgilBase = 0;
        double mKnockdownAgilMult = 1;
        double mKnockdownChance = 0.25;

        /// The force that throws a character back is (agility * 10 * mult + base) * (damage * mult + base), at most
        /// fKnockbackForceMax, and it works for fKnockbackTime seconds
        double mKnockbackAgilBase = 1;
        double mKnockbackAgilMult = -0.008;
        double mKnockbackDamageBase = 50;
        double mKnockbackDamageMult = 10;
        double mKnockbackForceMax = 512;
        double mKnockbackTime = 1;

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The settings that the lookup (a GMST by its editor id) has
        static Settings fromGameSettings(const Lookup& lookup);
    };

    /// The number of crippled legs (0 to 2) of a character from the conditions of its left and right legs
    /// (LeftMobilityCondition and RightMobilityCondition): a leg is crippled when nothing is left of its condition
    /// (the executable's check is "above 0": 0x646800)
    int crippledLegs(double leftCondition, double rightCondition);

    /// What a character with that many legs crippled has left of its speed (1 for none, the settings' value for one or
    /// two)
    double legSpeedMultiplier(const Settings& settings, int crippled);

    /// Whether a blow of `damage` can knock a character down at all: not one that is dead or that the blow kills, and
    /// else one that does more than the base-health share for sure or more than the current-health share by chance
    enum class Knockdown
    {
        Never,
        Always,
        ByChance,
    };
    Knockdown knockdownKind(const Settings& settings, double damage, double health, double baseHealth);

    /// The chance of a knockdown that goes by chance, as a number of a thousand (the character is down when a roll of 0
    /// to 999 is below it): `agility` is the character's Agility
    int knockdownPerMille(const Settings& settings, double damage, double agility);

    /// The force that throws the character back (units a second for `mKnockbackTime` seconds, along the line of the
    /// blow)
    double knockbackForce(const Settings& settings, double damage, double agility);

    /// The knock state of a character: the byte at +0x13C of the process (MiddleHighProcess, read at FalloutNV.exe
    /// 0x8D8540, written at 0x8D8560), 0 to 6. GetKnockedState (0x5A08C0) answers 1 for 3 and 4 and 0 for every other
    /// state. The port has the one knock, the throw that a blow or PushActorAway gives for `mKnockbackTime` seconds,
    /// and calls it state 3 (**chosen**: the executable's names for the states were not read).
    namespace KnockState
    {
        constexpr int none = 0;
        constexpr int thrown = 3;
    }

    /// What GetKnockedState answers for a knock state
    bool knockedAnswer(int state);

    /// Who is being thrown and for how many seconds more. It can be used from any thread.
    class KnockTracker
    {
    public:
        /// Starts a knock (one that runs is replaced); a time of 0 or less is no knock
        void start(FormId actor, double seconds);
        /// Takes the seconds of a frame off every knock and ends those that have none left
        void advance(double seconds);
        /// The state of the actor's knock (KnockState)
        int stateOf(FormId actor) const;
        void clear();

    private:
        mutable std::mutex mMutex;
        std::map<FormId, double> mLeft;
    };
}

#endif
