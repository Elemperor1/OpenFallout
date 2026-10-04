#ifndef OPENFALLOUT_MECHANICS_OBSTACLE_H
#define OPENFALLOUT_MECHANICS_OBSTACLE_H

#include "apps/openfallout/mwworld/movementdirection.hpp"

#include <osg/Vec3f>

namespace OFWorld
{
    class Ptr;
    class ConstPtr;
}

namespace OFMechanics
{
    struct Movement;

    /// tests actor's proximity to a closed door by default
    bool proximityToDoor(const OFWorld::Ptr& actor, float minDist);

    /// Returns door pointer within range. No guarantee is given as to which one
    /** \return Pointer to the door, or empty pointer if none exists **/
    const OFWorld::Ptr getNearbyDoor(const OFWorld::Ptr& actor, float minDist);

    class ObstacleCheck
    {
    public:
        ObstacleCheck();

        // Clear the timers and set the state machine to default
        void clear();

        bool isEvading() const;

        // Updates internal state, call each frame for moving actor
        void update(const OFWorld::Ptr& actor, const osg::Vec3f& destination, float duration,
            OFWorld::MovementDirectionFlags supportedMovementDirection);

        // change direction to try to fix "stuck" actor
        void takeEvasiveAction(Movement& actorMovement) const;

    private:
        enum class WalkState
        {
            Initial,
            Norm,
            CheckStuck,
            Evade,
        };

        WalkState mWalkState = WalkState::Initial;
        float mStateDuration = 0;
        float mInitialDistance = 0;
        std::size_t mEvadeDirectionIndex;
        osg::Vec3f mPrev;
        osg::Vec3f mDestination;
    };
}

#endif
