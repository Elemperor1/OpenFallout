#include "radio.hpp"

#include <algorithm>

namespace FalloutScript::Radio
{
    Signal signalOf(Reach reach, float radius, float staticPercent, const Where& where, const Settings& settings)
    {
        Signal signal;
        switch (reach)
        {
            case Reach::Everywhere:
                signal.mInRange = true;
                signal.mStrength = 1.f;
                return signal;
            case Reach::Worldspace:
                signal.mInRange = where.mSameWorldspace || where.mSameCell;
                signal.mStrength = signal.mInRange ? 1.f : 0.f;
                return signal;
            case Reach::LinkedInteriors:
                signal.mInRange = where.mSameCell || (!where.mExterior && where.mLinkedInterior);
                signal.mStrength = signal.mInRange ? 1.f : 0.f;
                return signal;
            case Reach::CurrentCell:
                signal.mInRange = where.mSameCell;
                signal.mStrength = signal.mInRange ? 1.f : 0.f;
                return signal;
            case Reach::Radius:
                break;
        }
        // The radius is measured in exteriors, from where the station is
        if (!where.mExterior || where.mDistance < 0.f || radius <= 0.f || where.mDistance > radius)
            return signal;
        signal.mInRange = true;
        const float far = std::clamp(where.mDistance / radius, 0.f, 1.f);
        signal.mStrength = 1.f - far;
        const float atOuter = staticPercent > 0.f ? std::min(staticPercent, 100.f) / 100.f : settings.mStaticAtOuterRadius;
        signal.mStatic = atOuter * far;
        return signal;
    }

    float gainOf(const Signal& signal, bool dialogue, const Settings& settings)
    {
        if (!signal.mInRange)
            return 0.f;
        const float gain = settings.mVolume * (1.f - std::clamp(signal.mStatic, 0.f, 1.f));
        return dialogue ? gain * settings.mDialogMute : gain;
    }

    std::vector<Event> Receiver::update(float seconds, std::span<const Sighting> sightings)
    {
        std::vector<Event> events;
        std::set<std::uint32_t> seen;
        for (const Sighting& sighting : sightings)
        {
            if (!sighting.mSignal.mInRange)
                continue;
            seen.insert(sighting.mStation);
            const auto [held, isNew] = mReach.try_emplace(sighting.mStation);
            held->second.mSignal = sighting.mSignal;
            held->second.mOut = 0.f;
            if (isNew || held->second.mLost)
            {
                held->second.mLost = false;
                const bool first = mDiscovered.insert(sighting.mStation).second;
                events.push_back(
                    { first ? Event::Kind::Discovered : Event::Kind::Found, sighting.mStation });
            }
        }
        for (auto it = mReach.begin(); it != mReach.end();)
        {
            if (seen.count(it->first) != 0)
            {
                ++it;
                continue;
            }
            it->second.mOut += seconds;
            if (it->second.mOut < mSettings.mStationTimeout)
            {
                ++it;
                continue;
            }
            events.push_back({ Event::Kind::Lost, it->first });
            if (mTuned == it->first)
                mTuned = 0;
            it = mReach.erase(it);
        }
        return events;
    }

    bool Receiver::tune(std::uint32_t station)
    {
        if (station == 0 || mReach.count(station) == 0)
            return false;
        mTuned = station;
        return true;
    }

    Signal Receiver::signal(std::uint32_t station) const
    {
        const auto found = mReach.find(station);
        return found != mReach.end() ? found->second.mSignal : Signal();
    }

    std::vector<std::uint32_t> Receiver::stations() const
    {
        std::vector<std::uint32_t> result;
        for (const auto& [station, held] : mReach)
            result.push_back(station);
        return result;
    }

    void Receiver::reset()
    {
        mReach.clear();
        mDiscovered.clear();
        mTuned = 0;
    }
}
