#include "restrictions.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Restrictions
{
    namespace
    {
        const char* refusalOfWait(const Facts& facts)
        {
            if (facts.mTrespassing)
                return "sNoWaitTrespass";
            if (facts.mAlarmSounding)
                return "sNoWaitWhileAlarmSounding";
            if (facts.mUnderwater)
                return "sNoWaitUnderWater";
            // (the executables spell this setting without the e of hostile)
            if (facts.mHostilesNear)
                return "sNoWaitHostilActorsNear";
            if (facts.mAirborne)
                return "sNoWaitInAir";
            if (facts.mNoWaitPlace || facts.mWaitSwitchedOff)
                return "sNoWaitInCell";
            if (facts.mInRadiation || facts.mRadiationProcess)
                return "sNoWaitInRadiation";
            if (facts.mTakingHealthDamage)
                return "sNoWaitTakingHealthDamage";
            return nullptr;
        }

        const char* refusalOfSleep(const Facts& facts)
        {
            if (facts.mOwnedBed)
                return "sNoSleepInOwnedBed";
            // A place that forbids waiting forbids sleeping too, and says it with the word of waiting
            if (facts.mNoWaitPlace)
                return "sNoWaitInCell";
            if (facts.mTrespassing)
                return "sNoSleepTrespass";
            if (facts.mHostilesNear)
                return "sNoSleepHostileActorsNear";
            if (facts.mInRadiation)
                return "sNoSleepInRadiation";
            if (facts.mTakingHealthDamage)
                return "sNoSleepTakingHealthDamage";
            return nullptr;
        }

        const char* refusalOfTravel(const Facts& facts)
        {
            if (facts.mHostilesNear)
                return "sNoFastTravelHostileActorsNear";
            if (facts.mAlarmSounding)
                return "sNoFastTravelAlarm";
            if (facts.mTakingHealthDamage)
                return "sFastTravelNoTravelHealthDamage";
            if (facts.mScriptBlocked)
                return "sNoFastTravelScriptBlock";
            if (facts.mOverencumbered && !facts.mTravelWhileOverencumbered)
                return "sNoFastTravelOverencumbered";
            if (facts.mSpent)
                return "sFastTravelNoTravelHealthDamage";
            if (facts.mAirborne)
                return "sNoFastTravelInAir";
            if (facts.mTravelForbidden)
                return "sNoFastTravelCell";
            if (facts.mUndiscovered)
                return "sNoFastTravelUndiscovered";
            return nullptr;
        }
    }

    const char* refusal(Action action, const Facts& facts)
    {
        switch (action)
        {
            case Action::Wait:
                return refusalOfWait(facts);
            case Action::Sleep:
                return refusalOfSleep(facts);
            case Action::FastTravel:
                return refusalOfTravel(facts);
        }
        return nullptr;
    }

    double hostileReach(bool interior, double exterior, double inside)
    {
        return interior ? inside : exterior;
    }

    bool hostileNear(const std::vector<Candidate>& candidates, double reach)
    {
        return std::any_of(candidates.begin(), candidates.end(), [reach](const Candidate& candidate) {
            return !candidate.mDead && !candidate.mDisabled && candidate.mHostile && candidate.mDistance <= reach;
        });
    }

    int travelHours(double pathLength, double speed, double timeScale)
    {
        if (!(pathLength > 0) || !(speed > 0) || !(timeScale > 0))
            return 0;
        // The executable divides by the speed and 3600 and multiplies by the time scale in single precision, and
        // truncates what comes out
        const double hours = pathLength / speed / 3600.0 * timeScale;
        return static_cast<int>(std::floor(hours));
    }

    double travelSpeed(double speedMultiplier, int crippledLegs, double baseSpeed, double runMultiplier,
        double oneLeg, double twoLegs)
    {
        const double legs = crippledLegs >= 2 ? twoLegs : crippledLegs == 1 ? oneLeg : 1.0;
        return std::max(0.0, speedMultiplier * 0.01 * baseSpeed * legs * runMultiplier);
    }
}
