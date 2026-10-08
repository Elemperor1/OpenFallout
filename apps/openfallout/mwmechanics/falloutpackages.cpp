#include "falloutpackages.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include <osg/Math>

#include <components/misc/mathutil.hpp>

namespace OFMechanics
{
    namespace
    {
        // A number from 0 up to but not including 1 from the dice
        float unit(ESM4::LevelledRandom& random)
        {
            constexpr std::size_t steps = 1 << 24;
            return static_cast<float>(random.below(steps)) / static_cast<float>(steps);
        }
    }

    float falloutGoalRadius(std::int32_t locationRadius, ESM4::PackageBehaviour behaviour)
    {
        if (behaviour == ESM4::PackageBehaviour::Roam)
            return locationRadius <= 0
                ? falloutDefaultRoam
                : std::clamp(static_cast<float>(locationRadius), falloutSmallestRoam, falloutLargestRoam);
        return std::clamp(
            static_cast<float>(std::max(locationRadius, 0)), falloutSmallestArrival, falloutLargestArrival);
    }

    osg::Vec3f falloutRoamPoint(const FalloutGoal& goal, ESM4::LevelledRandom& random)
    {
        // The square root makes the points as likely near the edge, where there is more room, as near the center
        const float distance = goal.mRadius * std::sqrt(unit(random));
        const float angle = 2.f * osg::PIf * unit(random);
        return goal.mCenter + osg::Vec3f(std::sin(angle) * distance, std::cos(angle) * distance, 0.f);
    }

    float falloutRoamPause(ESM4::LevelledRandom& random)
    {
        constexpr float shortest = 3.f;
        constexpr float longest = 12.f;
        return shortest + (longest - shortest) * unit(random);
    }

    float falloutFollowDistance(std::int32_t packageDistance)
    {
        return falloutScriptFollowDistance(static_cast<float>(packageDistance));
    }

    float falloutScriptFollowDistance(float distance)
    {
        // The comparison is false for a number that is not one
        if (!(distance > 0.f))
            return falloutDefaultFollow;
        return std::clamp(distance, falloutSmallestFollow, falloutLargestFollow);
    }

    bool falloutFollowMoves(float away, float distance, bool walking)
    {
        return away > (walking ? distance : distance + falloutFollowSlack);
    }

    bool falloutFollowRuns(float away, float distance)
    {
        return away > distance + falloutFollowRunBeyond;
    }

    int falloutFirstPlugin(const std::vector<std::string>& contentFiles)
    {
        for (std::size_t i = 0; i < contentFiles.size(); ++i)
        {
            const std::string& name = contentFiles[i];
            const std::size_t dot = name.rfind('.');
            if (dot == std::string::npos)
                continue;
            std::string extension = name.substr(dot + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == "esm" || extension == "esp" || extension == "esl")
                return static_cast<int>(i);
        }
        return -1;
    }

    ESM4::PackageClock falloutClock(float hour, int daysPassed)
    {
        ESM4::PackageClock clock;
        clock.mHour = std::clamp(hour, 0.f, 24.f);
        clock.mDayOfWeek = ((daysPassed + 1) % 7 + 7) % 7;
        return clock;
    }

    float falloutTurnToward(float angle, float target, float change)
    {
        const float difference = static_cast<float>(Misc::normalizeAngle(static_cast<double>(target) - angle));
        if (std::abs(difference) <= change)
            return static_cast<float>(Misc::normalizeAngle(target));
        return static_cast<float>(Misc::normalizeAngle(angle + std::copysign(change, difference)));
    }

    osg::Vec3f falloutStepToward(const osg::Vec3f& from, const osg::Vec3f& to, float distance)
    {
        const osg::Vec3f way = to - from;
        const float length = way.length();
        if (length <= distance || length == 0.f)
            return to;
        return from + way * (distance / length);
    }
}
