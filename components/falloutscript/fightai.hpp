#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FIGHTAI_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FIGHTAI_H

#include <cstdint>
#include <vector>

#include "script.hpp"

namespace FalloutScript::FightAI
{
    /// The values of the AIDT of a character (aggression, confidence), as the records of both games have them
    namespace Aggression
    {
        constexpr int unaggressive = 0;
        constexpr int aggressive = 1;
        constexpr int veryAggressive = 2;
        constexpr int frenzied = 3;
    }

    namespace Confidence
    {
        constexpr int cowardly = 0;
        constexpr int cautious = 1;
        constexpr int average = 2;
        constexpr int brave = 3;
        constexpr int foolhardy = 4;
    }

    /// How a character who looks at another one finds it, from the factions of the two
    enum class Standing : std::uint8_t
    {
        Neutral,
        Enemy,
        Ally,
        Friend,
    };

    /// Whether a character with this aggression attacks, without being hit first, a character it notices and stands
    /// so to: an unaggressive one never does, an aggressive one attacks its enemies, a very aggressive one its enemies
    /// and the neutral, a frenzied one anyone (the descriptions of the editor of the games)
    bool attacksOnSight(int aggression, Standing standing);

    /// The share of its health at which a character runs from a fight, by its confidence: a cowardly one always runs,
    /// a foolhardy one never. The shares are not known from the games (only the names of the levels are), so they are
    /// a guess.
    double fleeHealthShare(int confidence);

    /// Whether the character runs from the fight: its health share (0 to 1) is below that of its confidence
    bool runsAway(int confidence, double healthShare);

    /// How close a character wants to be to its target to attack: a blow reaches `reach` units, and a shot is made from
    /// half the range of the weapon at most, and from no further than a character can see and aim (the numbers are
    /// guesses)
    double engageDistance(bool melee, double reach);

    /// What the character does about the distance to the target
    enum class Move : std::uint8_t
    {
        Hold, // stays where it is and attacks (or waits for the weapon)
        Approach, // goes to the target
        Retreat, // goes away from the target
    };

    /// A character that runs away goes on until it is this far from the target (fCombatFleeNormalDistance of the games)
    constexpr double fleeDistance = 1400.0;

    /// The step in a fight for the distance to the target: `distance` is how far the target is, `engage` the distance
    /// to attack from, `visible` whether the character sees the target (it has to come nearer if not), `flee` whether
    /// it runs away
    Move decide(double distance, double engage, bool visible, bool flee);

    /// A weapon a character carries, for choosing the one to hold
    struct WeaponCandidate
    {
        FormId mWeapon = 0;
        double mDamage = 0;
        bool mMelee = false;
        /// Rounds the character has for it; a melee weapon has no need of them (-1)
        int mRounds = -1;
        /// Rounds one shot uses
        int mAmmoUse = 1;
    };

    /// The weapon to hold for a fight: of those that can be used (a melee weapon, or one the character has the rounds
    /// for), the one that does the most damage, with a shot counted for a little more than a blow, as the character
    /// can use it from further away. 0 when there is none (the character fights with its hands).
    FormId bestWeapon(const std::vector<WeaponCandidate>& candidates);

    /// The chance that an attack of a character hits what it aims at, from its skill with the weapon (0 to 100):
    /// skill over a hundred, between 0.3 and 0.95 (a guess: the games work it out from the spread of the weapon and
    /// the distance, and the skill)
    double hitChance(double skill);
}

#endif
