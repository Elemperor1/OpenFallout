#include "ambient.hpp"

#include <algorithm>

namespace FalloutScript::Ambient
{
    TimeOfDay timeOfDay(float hour, const Settings& settings)
    {
        if (hour >= settings.mDawn && hour < settings.mDay)
            return TimeOfDay::Dawn;
        if (hour >= settings.mDay && hour < settings.mDusk)
            return TimeOfDay::Day;
        if (hour >= settings.mDusk && hour < settings.mNight)
            return TimeOfDay::Dusk;
        return TimeOfDay::Night;
    }

    std::uint32_t loopOfSpace(std::span<const std::uint32_t> slots, bool interior, TimeOfDay time)
    {
        if (slots.empty())
            return 0;
        if (interior)
            return slots[0];
        const std::size_t slot = static_cast<std::size_t>(time);
        if (slot < slots.size() && slots[slot] != 0)
            return slots[slot];
        for (const std::uint32_t id : slots)
            if (id != 0)
                return id;
        return 0;
    }

    bool weatherAllows(std::uint32_t flags, std::uint32_t classification)
    {
        // A weather that has no class is taken as pleasant
        return (flags & (classification == 0 ? 1u : classification)) != 0;
    }

    float chanceOf(std::uint32_t chance)
    {
        return std::clamp(static_cast<float>(chance) / 1000000.f, 0.f, 1.f);
    }

    std::vector<std::uint32_t> RegionRoller::update(float seconds, std::span<const RegionSound> sounds,
        std::uint32_t classification, const std::function<double()>& unit,
        const std::function<bool(std::uint32_t)>& isLoop)
    {
        std::vector<std::uint32_t> result;
        for (const RegionSound& sound : sounds)
        {
            if (sound.mSound == 0 || sound.mChance == 0 || !weatherAllows(sound.mFlags, classification)
                || isLoop(sound.mSound))
                continue;
            auto it = mLeft.find(sound.mSound);
            if (it == mLeft.end())
                it = mLeft.emplace(sound.mSound, static_cast<float>(unit()) * mInterval).first;
            it->second -= seconds;
            if (it->second > 0.f)
                continue;
            it->second += mInterval;
            if (it->second <= 0.f)
                it->second = mInterval;
            if (unit() < static_cast<double>(chanceOf(sound.mChance)))
                result.push_back(sound.mSound);
        }
        return result;
    }
}
