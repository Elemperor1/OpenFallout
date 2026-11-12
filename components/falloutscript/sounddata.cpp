#include "sounddata.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace FalloutScript
{
    float SoundData::staticGain() const
    {
        return std::pow(10.f, -static_cast<float>(mStaticAttenuation) / 2000.f);
    }

    float SoundData::distanceGain(float distance) const
    {
        if (!hasDistance())
            return 1.f;
        const float min = minDistance();
        const float max = maxDistance();
        if (distance <= min)
            return 1.f;
        if (distance >= max)
            return 0.f;
        // Five points from the minimum to the maximum: four steps
        const float position = (distance - min) / (max - min) * 4.f;
        const int step = std::min(static_cast<int>(position), 3);
        const float fraction = position - static_cast<float>(step);
        const float from = static_cast<float>(mCurve[static_cast<std::size_t>(step)]);
        const float to = static_cast<float>(mCurve[static_cast<std::size_t>(step) + 1]);
        return std::clamp((from + (to - from) * fraction) / 100.f, 0.f, 1.f);
    }

    float SoundData::pitch(float unit) const
    {
        const float adjust = static_cast<float>(mFrequencyAdjustment) / 100.f;
        if ((mFlags & RandomFrequencyShift) != 0)
            return 1.f + std::abs(adjust) * (std::clamp(unit, 0.f, 1.f) * 2.f - 1.f);
        return 1.f + adjust;
    }

    SoundData soundDataFromBytes(const unsigned char* bytes, std::size_t size)
    {
        SoundData data;
        if (size < 12)
            return data;
        data.mMinAttenuation = bytes[0];
        data.mMaxAttenuation = bytes[1];
        data.mFrequencyAdjustment = static_cast<std::int8_t>(bytes[2]);
        std::memcpy(&data.mFlags, bytes + 4, 2);
        std::memcpy(&data.mStaticAttenuation, bytes + 8, 2);
        data.mStopTime = bytes[10];
        data.mStartTime = bytes[11];
        if (size >= 36)
        {
            for (std::size_t i = 0; i < 5; ++i)
                std::memcpy(&data.mCurve[i], bytes + 12 + 2 * i, 2);
            std::memcpy(&data.mPriority, bytes + 24, 4);
        }
        return data;
    }
}
