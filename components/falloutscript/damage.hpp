#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DAMAGE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DAMAGE_H

#include <functional>
#include <optional>
#include <string_view>

namespace FalloutScript
{
    /// The settings of the games (GMST records) that say how much of a blow a character takes. A setting that the
    /// files do not have keeps the value given here, which is what both games have for it (read from their GMST
    /// records), and the rule of New Vegas for armor (a flat threshold, then a share). Fallout 3 has the share only
    /// and says so by having no threshold on its armor. fMinDamMultiplier is in New Vegas only.
    struct DamageSettings
    {
        /// The most of a blow that armor can take away in percent (fMaxArmorRating)
        double mMaxResistance = 85;
        /// The share of a blow that always gets through the threshold of armor (fMinDamMultiplier, a fifth)
        double mMinimumThroughThreshold = 0.2;
        /// A character who hits with a weapon of its hands or of melee adds its strength bonus (the derived melee
        /// damage)
        bool mMeleeStrengthBonus = true;
        /// What the skill with the weapon makes of a blow: base + mult * skill / 100 (fDamageSkillBase,
        /// fDamageSkillMult)
        double mSkillBase = 0.5;
        double mSkillMult = 0.5;
        /// What the condition of the weapon makes of a blow: base + mult * condition, for guns and for melee weapons
        /// (fDamageGunWeapCondBase, fDamageGunWeapCondMult, fDamageMeleeWeapCondBase, fDamageMeleeWeapCondMult)
        double mGunConditionBase = 0.66;
        double mGunConditionMult = 0.34;
        double mMeleeConditionBase = 0.5;
        double mMeleeConditionMult = 0.5;
        /// How long a blow from a friend counts for GetFriendHit, and the least time between two that count
        /// (fFriendHitTimer, fFriendMinimumLastHitTime)
        double mFriendHitSeconds = 10;
        double mFriendHitSpacingSeconds = 0.5;
        /// How many blows of the player a character lets pass before it turns on the player, by how it stands to the
        /// victim's faction (a friend or an ally) and whether the character is in a fight (iFriendHitCombatAllowed,
        /// iFriendHitNonCombatAllowed, iAllyHitCombatAllowed, iAllyHitNonCombatAllowed). 0 or less lets none pass.
        int mFriendHitCombatAllowed = 4;
        int mFriendHitNonCombatAllowed = 0;
        int mAllyHitCombatAllowed = 1000;
        int mAllyHitNonCombatAllowed = 3;

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The settings that the lookup (a GMST by its editor id) has
        static DamageSettings fromGameSettings(const Lookup& lookup);
    };

    /// What a weapon does with one hit
    struct WeaponDamage
    {
        double mDamage = 0;
        /// The bonus that a critical hit adds, and the chance multiplier of the weapon (1 for the usual weapon)
        double mCriticalDamage = 0;
        double mCriticalChanceMultiplier = 1;
        /// Hands and melee weapons (the animation types 0 to 2) add the strength of the attacker
        bool mMelee = false;
        /// 1 for a weapon in full condition
        double mCondition = 1;
        /// The skill of the attacker with the weapon (0 to 100); 100 does the damage of the record
        double mSkill = 100;
    };

    /// What a character has between itself and a blow
    struct Defense
    {
        /// What armor takes away: a number of points (the threshold) and a share (the resistance, in percent)
        double mThreshold = 0;
        double mResistance = 0;
    };

    struct HitResult
    {
        /// What the character loses: the blow and the bonus of a critical hit, after the armor
        double mDamage = 0;
        bool mCritical = false;
    };

    namespace Damage
    {
        /// What a blow does to the character that takes it. `roll` is a number from 0 to 100 that decides whether the
        /// blow is critical (it is when the roll is below the chance), `attackerChance` the chance in percent of the
        /// attacker, `meleeBonus` what its strength adds to a melee blow.
        HitResult resolve(const DamageSettings& settings, const WeaponDamage& weapon, double attackerChance,
            double meleeBonus, const Defense& defense, double roll);

        /// The chance of a critical hit with the weapon in percent, for an attacker with that chance
        double criticalChance(const WeaponDamage& weapon, double attackerChance);
    }
}

#endif
