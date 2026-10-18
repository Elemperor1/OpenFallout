#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VATS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VATS_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace FalloutScript
{
    /// V.A.T.S., the targeting that stops the game and spends action points on attacks at parts of the body. The
    /// numbers come from the settings of the files and the formulas from the executables of both games (New Vegas,
    /// and Fallout 3 where it differs); what is only a stand-in is marked.
    namespace Vats
    {
        using Lookup = std::function<std::optional<double>(std::string_view)>;

        struct Rules
        {
            /// The most any shot has (fVATSMaxChance), in percent
            double mMaxChance = 95;
            /// How far a blow of the hands or a melee weapon reaches in VATS (fVATSMeleeMaxDistance)
            double mMeleeMaxDistance = 300;
            /// What the chance is multiplied by against a target that hides from the attacker (fVATSStealthMult)
            double mStealthMult = 0.5;
            /// fVATSHitChanceMult
            double mHitChanceMult = 1;
            /// The skill term of a thrown weapon (fVATSSkillFactor)
            double mSkillFactor = 1;
            /// The shots: fVATSSpreadMult, the most the area of the target counts against the area of the spread
            /// (fVATSRangeSpreadMax), and, in Fallout 3 only, fVATSRangeSpreadUncertainty and fVATSDistanceFactor
            double mSpreadMult = 1;
            double mRangeSpreadMax = 2;
            double mRangeSpreadUncertainty = 0.2;
            double mDistanceFactor = 2;
            /// Whether the formula of the shots is that of New Vegas (the radius of the spread is squared) or of
            /// Fallout 3 (it gets the uncertainty and the factor of distance)
            bool mNewVegas = true;
            /// The thrown weapons: fVATSThrownWeaponRangeMult, fVATSGrenadeRangeMult, fVATSGrenadeRangeMin,
            /// fVATSGrenadeSkillFactor, fVATSGrenadeChanceMult
            double mThrownRangeMult = 2;
            double mGrenadeRangeMult = 2;
            double mGrenadeRangeMin = 128;
            double mGrenadeSkillFactor = 0.4;
            double mGrenadeChanceMult = 1;
            /// What a shooter's own aim adds to the spread: the penalties of crippled arms for a weapon of one hand
            /// and of two (fCrippledArm1HSpreadPenalty, fCrippledArm2HSpreadPenalty, fCrippledArms1HSpreadPenalty,
            /// fCrippledArms2HSpreadPenalty), fWobbleToSkillConversion, fMinGunSpreadValue, and the angle the
            /// penalties make in degrees (fNPCMaxGunWobbleAngle)
            double mCrippledArm1H = 0.2;
            double mCrippledArm2H = 0.4;
            double mCrippledArms1H = 0.4;
            double mCrippledArms2H = 0.6;
            double mWobbleToSkill = 0.5;
            double mMinSpread = 0.01;
            double mMaxWobbleAngle = 5;
            /// What each point of strength or skill that the weapon wants more of adds (fWeapStrengthReqPenalty)
            double mWantedPenalty = 0.1;
            /// The action points an attack costs by the kind of weapon (fActionPointsAttackUnarmed and the like)
            double mUnarmed = 22;
            double mOneHand = 15;
            double mTwoHand = 30;
            double mPistol = 20;
            double mRifle = 25;
            double mHandle = 20;
            double mLauncher = 40;
            double mGrenade = 35;
            double mMine = 10;
            /// What an attack in VATS costs more for a blow of the hands or a weapon of the hand
            /// (fActionPointsVATSMeleeMult)
            double mMeleeMult = 1;
            /// The action points that come back in a second outside VATS (fActionPointsRestoreRate); New Vegas has a
            /// share of the most instead (0.06) and a perk can change it
            double mRestoreRate = 4;
            double mRestoreShare = 0;
        };

        Rules rules(const Lookup& setting);

        /// What the weapon says that VATS needs (a WEAP record)
        struct Weapon
        {
            /// The animation type of the record: the hands, a melee weapon, a pistol, a rifle ... a mine
            std::uint32_t mAnimationType = 0;
            /// The spread of the shots in degrees
            double mSpread = 0;
            /// The action points the record names in place of the kind of weapon's, negative for none
            double mActionPoints = -1;
            /// How far the weapon reaches
            double mRange = 0;
            /// That the range of the weapon is a limit in VATS too (the flag "range fixed" of the record)
            bool mRangeFixed = false;
        };

        /// Whether the weapon strikes with the hands or a weapon of the hand (it reaches no further than a blow)
        bool isMelee(std::uint32_t animationType);

        /// Whether the weapon shoots (a pistol to a launcher); what is neither is thrown
        bool isGun(std::uint32_t animationType);

        /// The action points one attack with the weapon costs
        double attackCost(const Rules& rules, const Weapon& weapon);

        /// What the shooter and the target are like when an attack is aimed
        struct Shot
        {
            /// How far the target is, in units
            double distance = 0;
            /// The size of the target: twice the largest half of its box, 128 for a target without a size
            double bound = 128;
            /// The part's own chance to be hit, in percent (the byte of the body part data)
            double partToHit = 100;
            /// The skill with the weapon, 0 to 100 (more is counted as 100)
            double skill = 100;
            /// What the shooter's aim adds to the spread of the weapon, in degrees (see spreadDegrees)
            double extraSpread = 0;
            /// The target hides from the attacker (chameleon or invisibility)
            bool hidden = false;
            /// The attacker can use its arms to throw (the arms are not crippled)
            bool arms = true;
        };

        /// The farthest a target can be for the weapon to have any chance: the reach of a blow, twice the range of a
        /// thrown weapon, the range of a shot only if the record fixes it (else there is no limit, only the chance
        /// falls).
        double reach(const Rules& rules, const Weapon& weapon);

        /// The chance in percent that an attack at a part hits, 0 to the most the files allow. A blow of the hands or
        /// a melee weapon is sure within its reach; a thrown weapon has the chance of its range and the skill; a
        /// shot has the share of the spread that falls on the target, times the part's own chance.
        double hitChance(const Rules& rules, const Weapon& weapon, const Shot& shot);

        /// What a shooter's aim adds to the spread before the perks change it: the penalties of crippled arms
        /// (`arms` is how many are, 0 to 2), then of a strength or a skill that the weapon wants more of, made smaller
        /// by the skill, and not below the least. `strengthShort` and `skillShort` are the points the character lacks
        /// (0 or more). A weapon of two hands is any rifle, handle or launcher (an estimate).
        double spreadFactor(
            const Rules& rules, const Weapon& weapon, int arms, double strengthShort, double skillShort, double skill);

        /// The spread in degrees that VATS adds to the weapon's: the factor (after the perks) times the largest angle
        double spreadDegrees(const Rules& rules, double factor);

        /// The action points that come back in `seconds` outside VATS
        double restored(const Rules& rules, double maxActionPoints, double seconds);
    }
}

#endif
