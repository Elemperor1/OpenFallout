#include "nearwater.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::NearWater
{
    namespace
    {
        // The smallest length of a direction that the executable normalises (0x1017CF8); a shorter one becomes nothing
        constexpr float tinyLength = 1.0e-6f;

        float nearness(float waterLevel, float playerZ, float tolerance)
        {
            const float distance = std::abs(waterLevel - playerZ);
            if (!(distance < tolerance))
                return 0.f;
            return (tolerance - distance) / tolerance;
        }
    }

    std::vector<Point> samplePoints(Point player, const Settings& settings)
    {
        std::vector<Point> points;
        const int count = std::max(settings.mPoints, 0);
        points.reserve(static_cast<std::size_t>(count) * static_cast<std::size_t>(count));
        const float radius = static_cast<float>(settings.mRadius);
        const float step = count > 1 ? radius * 2.f / static_cast<float>(count - 1) : 0.f;
        for (int x = 0; x < count; ++x)
            for (int y = 0; y < count; ++y)
                points.push_back(
                    { player.mX - radius + static_cast<float>(x) * step, player.mY - radius + static_cast<float>(y) * step });
        return points;
    }

    float interiorStrength(float waterLevel, float playerZ, bool listenerUnderwater, const Settings& settings)
    {
        if (listenerUnderwater)
            return 0.f;
        return std::min(nearness(waterLevel, playerZ, settings.mIndoorTolerance), 1.f);
    }

    float exteriorStrength(
        const Wet& wet, float waterLevel, float playerZ, bool listenerUnderwater, const Settings& settings)
    {
        const int points = std::max(settings.mPoints, 0);
        if (listenerUnderwater || points == 0)
            return 0.f;
        const float share = static_cast<float>(wet.mCount) * 2.f / static_cast<float>(points * points);
        return std::min(share * nearness(waterLevel, playerZ, settings.mOutdoorTolerance), 1.f);
    }

    Point soundPosition(Point player, const Wet& wet, float strength, bool interior, const Settings& settings)
    {
        if (interior || wet.mCount <= 0)
            return player;
        const float count = static_cast<float>(wet.mCount);
        const float dx = wet.mSumX / count - player.mX;
        const float dy = wet.mSumY / count - player.mY;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (!(length > tinyLength))
            return player;
        const float reach = static_cast<float>(settings.mRadius) * (1.f - std::clamp(strength, 0.f, 1.f));
        return { player.mX + dx / length * reach, player.mY + dy / length * reach };
    }
}
