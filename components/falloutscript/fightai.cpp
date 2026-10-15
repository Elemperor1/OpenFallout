#include "fightai.hpp"

#include <algorithm>

namespace FalloutScript::FightAI
{
    bool attacksOnSight(int aggression, Standing standing)
    {
        switch (aggression)
        {
            case Aggression::aggressive:
                return standing == Standing::Enemy;
            case Aggression::veryAggressive:
                return standing == Standing::Enemy || standing == Standing::Neutral;
            case Aggression::frenzied:
                return true;
            default:
                return false;
        }
    }

    double fleeHealthShare(int confidence)
    {
        switch (confidence)
        {
            case Confidence::cowardly:
                return 1.01;
            case Confidence::cautious:
                return 0.5;
            case Confidence::average:
                return 0.25;
            case Confidence::brave:
                return 0.1;
            default:
                return 0.0;
        }
    }

    bool runsAway(int confidence, double healthShare)
    {
        return healthShare < fleeHealthShare(confidence);
    }

    double engageDistance(bool melee, double reach)
    {
        if (melee)
            return std::max(reach * 0.7, 50.0);
        // A shot is made from the middle of its range, but a weapon of short range from as far as it reaches, and none
        // from beyond what a character can aim at
        return std::clamp(reach * 0.5, std::min(reach, 300.0), 1500.0);
    }

    Move decide(double distance, double engage, bool visible, bool flee)
    {
        if (flee)
            return distance < fleeDistance ? Move::Retreat : Move::Hold;
        if (!visible || distance > engage)
            return Move::Approach;
        return Move::Hold;
    }

    FormId bestWeapon(const std::vector<WeaponCandidate>& candidates)
    {
        FormId best = 0;
        double bestScore = 0;
        for (const WeaponCandidate& candidate : candidates)
        {
            if (candidate.mWeapon == 0)
                continue;
            if (!candidate.mMelee && candidate.mRounds < std::max(candidate.mAmmoUse, 1))
                continue;
            const double score = candidate.mDamage * (candidate.mMelee ? 1.0 : 1.25);
            if (score > bestScore)
            {
                bestScore = score;
                best = candidate.mWeapon;
            }
        }
        return best;
    }

    double hitChance(double skill)
    {
        return std::clamp(skill / 100.0, 0.3, 0.95);
    }
}
