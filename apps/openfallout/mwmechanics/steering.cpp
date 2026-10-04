#include "steering.hpp"

#include <components/misc/mathutil.hpp>
#include <components/settings/values.hpp>

#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwbase/environment.hpp"

#include "movement.hpp"

namespace OFMechanics
{

    bool smoothTurn(const OFWorld::Ptr& actor, float targetAngleRadians, int axis, float epsilonRadians)
    {
        OFMechanics::Movement& movement = actor.getClass().getMovementSettings(actor);
        float diff
            = static_cast<float>(Misc::normalizeAngle(targetAngleRadians - actor.getRefData().getPosition().rot[axis]));
        float absDiff = std::abs(diff);

        // The turning animation actually moves you slightly, so the angle will be wrong again.
        // Use epsilon to prevent jerkiness.
        if (absDiff < epsilonRadians)
            return true;

        float limit
            = getAngularVelocity(actor.getClass().getMaxSpeed(actor)) * OFBase::Environment::get().getFrameDuration();
        if (Settings::game().mSmoothMovement)
            limit *= std::min(absDiff / osg::PIf + 0.1f, 0.5f);

        if (absDiff > limit)
            diff = osg::sign(diff) * limit;

        movement.mRotation[axis] = diff;
        return false;
    }

    bool zTurn(const OFWorld::Ptr& actor, float targetAngleRadians, float epsilonRadians)
    {
        return smoothTurn(actor, targetAngleRadians, 2, epsilonRadians);
    }

}
