#include "regionline.hpp"

namespace FalloutScript::RegionLine
{
    std::optional<std::size_t> pick(const std::vector<Candidate>& covering)
    {
        if (covering.empty())
            return std::nullopt;
        std::size_t chosen = 0;
        for (std::size_t i = 1; i < covering.size(); ++i)
        {
            const Candidate& later = covering[i];
            const Candidate& current = covering[chosen];
            if (!later.mHasWeather)
                continue;
            if (!current.mHasWeather || (later.mOverride && !current.mOverride)
                || (later.mOverride == current.mOverride && later.mPriority > current.mPriority))
                chosen = i;
        }
        return chosen;
    }

    std::optional<std::string> Announcer::enter(bool exterior, const std::vector<Candidate>& covering)
    {
        // In a building the region stays as it was, and it is the one that was shown last: nothing is shown
        if (!exterior)
            return std::nullopt;
        // The region is made again from the cell's list; a place that none covers has none
        std::string name;
        mRegion = 0;
        if (const std::optional<std::size_t> chosen = pick(covering))
        {
            mRegion = covering[*chosen].mRegion;
            name = covering[*chosen].mName;
        }
        if (mRegion == mLast)
            return std::nullopt;
        mLast = mRegion;
        if (name.empty())
            return std::nullopt;
        return name;
    }

    void Announcer::reset()
    {
        mRegion = 0;
        mLast = 0;
    }
}
