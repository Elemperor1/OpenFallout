#include "spawnrules.hpp"

#include <algorithm>

namespace FalloutScript
{
    int zoneLevel(int playerLevel, int minimumLevel, int zoneFlags, float levelScaling)
    {
        // the minimum is a byte, the level of the player a word
        minimumLevel = std::clamp(minimumLevel, 0, 255);
        playerLevel = std::clamp(playerLevel, 0, 65535);
        if (playerLevel < minimumLevel)
            return (zoneFlags & ZoneFlag::matchPlayerBelowMinimum) != 0 ? playerLevel : minimumLevel;
        const double level = minimumLevel + static_cast<double>(playerLevel - minimumLevel) * levelScaling;
        return static_cast<int>(level) & 0xFFFF;
    }

    int storedLevelMod(int levelMod)
    {
        return levelMod >= 0 && levelMod <= 3 ? levelMod : 0;
    }

    float levelModMultiplier(int levelMod, const SpawnSettings& settings)
    {
        if (levelMod < 0 || levelMod > 3)
            return 1.0f;
        return settings.mLevelModMultiplier[static_cast<std::size_t>(levelMod)];
    }

    int scaledSpawnLevel(int level, int levelMod, const SpawnSettings& settings)
    {
        const double scaled = static_cast<double>(level) * static_cast<double>(levelModMultiplier(levelMod, settings));
        return std::max(static_cast<int>(scaled), 1);
    }

    SpawnLevels::SpawnLevels(const SpawnSettings& settings)
        : mSettings(settings)
    {
    }

    void SpawnLevels::clear()
    {
        const std::lock_guard lock(mMutex);
        mCounter = 0;
        mPlacements.clear();
    }

    int SpawnLevels::placementLevel(int baseLevel, bool rotate)
    {
        int level = baseLevel;
        if (rotate)
        {
            const std::lock_guard lock(mMutex);
            ++mCounter;
            if (mCounter > mSettings.mAdjustUp)
                mCounter = -mSettings.mAdjustDown;
            level += mCounter;
        }
        return std::max(level, 1);
    }

    int SpawnLevels::counter() const
    {
        const std::lock_guard lock(mMutex);
        return mCounter;
    }

    bool SpawnLevels::find(FormId reference, int& level, int& levelMod) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mPlacements.find(reference);
        if (found == mPlacements.end())
            return false;
        level = found->second.mLevel;
        levelMod = found->second.mLevelMod;
        return true;
    }

    void SpawnLevels::set(FormId reference, int level, int levelMod)
    {
        if (reference == 0)
            return;
        const std::lock_guard lock(mMutex);
        mPlacements[reference] = Placement{ std::max(level, 1), levelMod };
    }

    void SpawnLevels::forget(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        mPlacements.erase(reference);
    }

    std::size_t SpawnLevels::size() const
    {
        const std::lock_guard lock(mMutex);
        return mPlacements.size();
    }

    void SpawnLevels::write(SaveWriter& out) const
    {
        const std::lock_guard lock(mMutex);
        out.i32(mCounter);
        out.count(mPlacements.size());
        for (const auto& [reference, placement] : mPlacements)
        {
            out.form(reference);
            out.i32(placement.mLevel);
            out.i32(placement.mLevelMod);
        }
    }

    void SpawnLevels::read(SaveReader& in, std::uint32_t)
    {
        const int counter = in.i32();
        std::map<FormId, Placement> all;
        const std::size_t count = in.count(12);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::optional<FormId> reference = in.form();
            Placement placement;
            placement.mLevel = std::max(in.i32(), 1);
            placement.mLevelMod = in.i32();
            if (reference)
                all[*reference] = placement;
        }
        const std::lock_guard lock(mMutex);
        mCounter = counter;
        mPlacements = std::move(all);
    }
}
