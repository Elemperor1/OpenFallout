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
        /// The size of a part of the body that a shot can land on, in units: a small part is about this wide
        constexpr double smallestPart = 5;
        constexpr double largestPart = 25;
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

    double hitChance(
        const Rules& rules, const Weapon& weapon, double partToHit, double distance, double skill, bool hidden)
    {
        partToHit = std::clamp(partToHit, 0.0, 100.0);
        distance = std::max(distance, 0.0);
        // How much the skill counts: none of it gives half the chance, all of it the whole
        const double skillTerm
            = std::clamp(1.0 - rules.mSkillFactor * 0.5 * (1.0 - std::clamp(skill, 0.0, 100.0) / 100.0), 0.0, 1.0);
        double chance;
        if (isMelee(weapon.mAnimationType))
        {
            if (distance > rules.mMeleeMaxDistance)
                return 0.0;
            chance = 100.0 * (partToHit / 100.0) * skillTerm;
        }
        else
        {
            if (weapon.mRange > 0 && distance > weapon.mRange)
                return 0.0;
            // The shots spread in a circle at the distance of the target; the part is a circle too, the bigger the more
            // the part is worth. The part is hit by the share of the circle of the spread that it covers.
            const double partRadius = smallestPart + (largestPart - smallestPart) * partToHit / 100.0;
            const double spreadRadius = distance * std::tan(std::clamp(weapon.mSpread, 0.0, 45.0) * pi / 180.0);
            const double covered
                = spreadRadius <= partRadius ? 1.0 : (partRadius * partRadius) / (spreadRadius * spreadRadius);
            const double own = weapon.mBaseChance > 0 ? weapon.mBaseChance / 100.0 : 1.0;
            chance = 100.0 * covered * own * (partToHit / 100.0) * skillTerm;
        }
        chance *= rules.mHitChanceMult;
        if (hidden)
            chance *= rules.mStealthMult;
        return std::clamp(chance, 0.0, rules.mMaxChance);
    }

    double restored(const Rules& rules, double maxActionPoints, double seconds)
    {
        const double perSecond = rules.mRestoreShare > 0 ? rules.mRestoreShare * maxActionPoints : rules.mRestoreRate;
        return std::max(perSecond * seconds, 0.0);
    }
}
