#ifndef OPENFALLOUT_MWMECHANICS_FALLOUTPACKAGES_H
#define OPENFALLOUT_MWMECHANICS_FALLOUTPACKAGES_H

#include <cstdint>

#include <osg/Vec3f>

#include <components/esm4/levelled.hpp>
#include <components/esm4/packageschedule.hpp>

namespace OFMechanics
{
    // The parts of what a character of Fallout 3 or New Vegas does for its AI package that need nothing of the world:
    // where the clock of the game stands for the schedules of the packages, how far a character may be from a place
    // that a package names, where it goes about and for how long it stands still, and how it moves along its way.

    /// Where a package sends a character and what it does there
    struct FalloutGoal
    {
        ESM4::PackageBehaviour mBehaviour = ESM4::PackageBehaviour::None;
        osg::Vec3f mCenter;
        /// The distance from the center at which a character that goes there has arrived (Stay) or the radius of the
        /// circle in which it goes about (Roam)
        float mRadius = 0.f;
    };

    /// The smallest and largest distances from the center of the place, in game units, at which a character has
    /// arrived, and the radii in which it goes about, whatever radius the package names (many name none, and some
    /// name a cell)
    constexpr float falloutSmallestArrival = 48.f;
    constexpr float falloutLargestArrival = 200.f;
    constexpr float falloutSmallestRoam = 128.f;
    constexpr float falloutLargestRoam = 2048.f;
    constexpr float falloutDefaultRoam = 512.f;

    /// The radius of a goal from the radius that the location of the package (PLDT) names
    float falloutGoalRadius(std::int32_t locationRadius, ESM4::PackageBehaviour behaviour);

    /// A place in the circle of the goal, at the height of its center, that every place of the circle is as likely to
    /// be chosen
    osg::Vec3f falloutRoamPoint(const FalloutGoal& goal, ESM4::LevelledRandom& random);

    /// How long, in seconds, a character stands still where it has gone to before it goes on
    float falloutRoamPause(ESM4::LevelledRandom& random);

    /// The clock for the schedules from the time of day (0 to 24) and the number of days that have passed since the
    /// game began. The files of the games do not say what day the game begins on; it is a Monday here.
    ESM4::PackageClock falloutClock(float hour, int daysPassed);

    /// An angle (radians, as the games turn an object around the vertical axis) that is at most `change` closer to the
    /// target than the angle is, by the shorter way round
    float falloutTurnToward(float angle, float target, float change);

    /// The place `distance` from `from` toward `to`, or `to` if it is that close
    osg::Vec3f falloutStepToward(const osg::Vec3f& from, const osg::Vec3f& to, float distance);
}

#endif
