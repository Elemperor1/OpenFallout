#include "vats.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Vats
{
    namespace
    {
        void take(const Lookup& lookup, std::string_view name, double& setting)
        {
            if (const std::optional<double> value = lookup(name))
                setting = *value;
        }

        constexpr double pi = 3.14159265358979323846;
    }

    Rules rules(const Lookup& lookup)
    {
        Rules result;
        if (!lookup)
            return result;
        take(lookup, "fVATSMaxChance", result.mMaxChance);
        take(lookup, "fVATSMeleeMaxDistance", result.mMeleeMaxDistance);
        take(lookup, "fVATSStealthMult", result.mStealthMult);
        take(lookup, "fVATSHitChanceMult", result.mHitChanceMult);
        take(lookup, "fVATSSkillFactor", result.mSkillFactor);
        take(lookup, "fVATSSpreadMult", result.mSpreadMult);
        take(lookup, "fVATSRangeSpreadMax", result.mRangeSpreadMax);
        take(lookup, "fVATSRangeSpreadUncertainty", result.mRangeSpreadUncertainty);
        take(lookup, "fVATSDistanceFactor", result.mDistanceFactor);
        take(lookup, "fVATSThrownWeaponRangeMult", result.mThrownRangeMult);
        take(lookup, "fVATSGrenadeRangeMult", result.mGrenadeRangeMult);
        take(lookup, "fVATSGrenadeRangeMin", result.mGrenadeRangeMin);
        take(lookup, "fVATSGrenadeSkillFactor", result.mGrenadeSkillFactor);
        take(lookup, "fVATSGrenadeChanceMult", result.mGrenadeChanceMult);
        take(lookup, "fCrippledArm1HSpreadPenalty", result.mCrippledArm1H);
        take(lookup, "fCrippledArm2HSpreadPenalty", result.mCrippledArm2H);
        take(lookup, "fCrippledArms1HSpreadPenalty", result.mCrippledArms1H);
        take(lookup, "fCrippledArms2HSpreadPenalty", result.mCrippledArms2H);
        take(lookup, "fWobbleToSkillConversion", result.mWobbleToSkill);
        take(lookup, "fMinGunSpreadValue", result.mMinSpread);
        take(lookup, "fNPCMaxGunWobbleAngle", result.mMaxWobbleAngle);
        take(lookup, "fWeapStrengthReqPenalty", result.mWantedPenalty);
        take(lookup, "fActionPointsAttackUnarmed", result.mUnarmed);
        take(lookup, "fActionPointsAttackOneHand", result.mOneHand);
        take(lookup, "fActionPointsAttackTwoHand", result.mTwoHand);
        take(lookup, "fActionPointsAttackPistol", result.mPistol);
        take(lookup, "fActionPointsAttackRifle", result.mRifle);
        take(lookup, "fActionPointsAttackHandle", result.mHandle);
        take(lookup, "fActionPointsAttackLauncher", result.mLauncher);
        take(lookup, "fActionPointsAttackGrenade", result.mGrenade);
        take(lookup, "fActionPointsAttackMine", result.mMine);
        take(lookup, "fActionPointsVATSMeleeMult", result.mMeleeMult);
        take(lookup, "fActionPointsRestoreRate", result.mRestoreRate);
        return result;
    }

    bool isMelee(std::uint32_t animationType)
    {
        return animationType <= 2;
    }

    bool isGun(std::uint32_t animationType)
    {
        return animationType >= 3 && animationType <= 9;
    }

    double attackCost(const Rules& rules, const Weapon& weapon)
    {
        if (weapon.mActionPoints >= 0)
            return weapon.mActionPoints;
        double cost = 0;
        switch (weapon.mAnimationType)
        {
            case 0:
                cost = rules.mUnarmed;
                break;
            case 1:
                cost = rules.mOneHand;
                break;
            case 2:
                cost = rules.mTwoHand;
                break;
            case 3:
            case 4:
                cost = rules.mPistol;
                break;
            case 5:
            case 6:
            case 7:
                cost = rules.mRifle;
                break;
            case 8:
                cost = rules.mHandle;
                break;
            case 9:
                cost = rules.mLauncher;
                break;
            case 10:
                cost = rules.mGrenade;
                break;
            case 11:
            case 12:
                cost = rules.mMine;
                break;
            default:
                break;
        }
        return isMelee(weapon.mAnimationType) ? cost * rules.mMeleeMult : cost;
    }

    double spreadFactor(
        const Rules& rules, const Weapon& weapon, int arms, double strengthShort, double skillShort, double skill)
    {
        // The weapons held in both hands: the rifles, the handle weapons and the launchers
        const bool twoHands = weapon.mAnimationType >= 5 && weapon.mAnimationType <= 9;
        double sum = 0;
        if (arms == 1)
            sum += twoHands ? rules.mCrippledArm2H : rules.mCrippledArm1H;
        else if (arms >= 2)
            sum += twoHands ? rules.mCrippledArms2H : rules.mCrippledArms1H;
        sum += std::clamp(strengthShort, 0.0, 10.0) * rules.mWantedPenalty;
        sum += std::clamp(std::round(skillShort), 0.0, 10.0) * rules.mWantedPenalty;
        sum *= 1.0 - rules.mWobbleToSkill * std::clamp(skill, 0.0, 100.0) * 0.01;
        return std::max(sum, rules.mMinSpread);
    }

    double spreadDegrees(const Rules& rules, double factor)
    {
        return std::max(factor, 0.0) * rules.mMaxWobbleAngle;
    }

    double hitChance(const Rules& rules, const Weapon& weapon, const Shot& shot)
    {
        const double distance = std::max(shot.distance, 0.0);
        const double skill = std::clamp(shot.skill, 0.0, 100.0);
        double chance = 0;
        if (isMelee(weapon.mAnimationType))
        {
            // Within reach a blow is sure, and the part does not count
            chance = distance <= rules.mMeleeMaxDistance ? 100.0 : 0.0;
        }
        else if (!isGun(weapon.mAnimationType))
        {
            // A thrown weapon: sure near, less and less the further it has to fly, up to twice its range
            const double range = weapon.mRange;
            if (distance > range * rules.mThrownRangeMult)
                return 0.0;
            const double gap = range * rules.mGrenadeRangeMult - rules.mGrenadeRangeMin;
            const double rangeOdds = gap > 0 ? 1.0 - std::max(distance - rules.mGrenadeRangeMin, 0.0) / gap : 0.0;
            const double armFactor = shot.arms ? 1.0 : 1.0 - rules.mCrippledArm1H;
            const double skillCalc
                = ((rules.mSkillFactor - 1.0) + rules.mGrenadeSkillFactor * skill / 100.0) * armFactor;
            chance = 100.0 * (rangeOdds + skillCalc) * rules.mGrenadeChanceMult;
        }
        else
        {
            if (weapon.mRangeFixed && distance > weapon.mRange)
                return 0.0;
            // The shots fall in a circle of the spread at the distance of the target; the target is as big as its
            // size says, and as much of its area as is hit by the circle counts
            const double spread = std::max(weapon.mSpread + shot.extraSpread, 0.0);
            double radius;
            double spreadArea;
            if (rules.mNewVegas)
            {
                radius = std::tan(std::min(spread * rules.mSpreadMult, 89.0) * pi / 180.0) * distance;
                spreadArea = pi * radius * radius;
            }
            else
            {
                radius
                    = std::tan(std::min(spread * rules.mSpreadMult + rules.mRangeSpreadUncertainty, 89.0) * pi / 180.0)
                    * distance;
                spreadArea = pi * std::pow(radius, rules.mDistanceFactor);
            }
            const double bound = shot.bound >= 1.0 ? shot.bound : 128.0;
            double targetArea = bound * bound / 3.0;
            if (2.0 * radius < bound)
                targetArea *= 2.0 * radius / bound;
            const double rangeSpread = spreadArea <= 0 ? 0.0 : std::min(targetArea / spreadArea, rules.mRangeSpreadMax);
            chance = rules.mHitChanceMult * rangeSpread * std::clamp(shot.partToHit, 0.0, 255.0);
        }
        if (shot.hidden)
            chance *= rules.mStealthMult;
        return std::clamp(chance, 0.0, rules.mMaxChance);
    }

    double restored(const Rules& rules, double maxActionPoints, double seconds)
    {
        const double perSecond = rules.mRestoreShare > 0 ? rules.mRestoreShare * maxActionPoints : rules.mRestoreRate;
        return std::max(perSecond * seconds, 0.0);
    }
}
