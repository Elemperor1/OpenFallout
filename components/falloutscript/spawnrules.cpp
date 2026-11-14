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

    FormId placementZone(FormId own, FormId cell, FormId world)
    {
        if (own != 0)
            return own;
        return cell != 0 ? cell : world;
    }

    bool SpawnLevels::placedLevel(
        FormId reference, int baseLevel, int levelMod, bool leveledTemplate, int& level, int& storedMod)
    {
        const std::lock_guard lock(mMutex);
        if (const auto found = mPlacements.find(reference); reference != 0 && found != mPlacements.end())
        {
            level = found->second.mLevel;
            storedMod = found->second.mLevelMod;
            return false;
        }
        level = std::max(baseLevel, 1);
        storedMod = noLevelMod;
        if (leveledTemplate)
        {
            // The counter of 0x567E10 goes up for every template that is resolved
            ++mCounter;
            if (mCounter > mSettings.mAdjustUp)
                mCounter = -mSettings.mAdjustDown;
            level = std::max(baseLevel + mCounter, 1);
            // The multiplier of the modifier is that of 0x4214F0; the result is not below 1 (0x488350)
            level = scaledSpawnLevel(level, levelMod, mSettings);
            storedMod = levelMod >= 0 && levelMod <= 3 ? levelMod : noLevelMod;
        }
        if (reference != 0)
            mPlacements[reference] = Placement{ level, storedMod };
        return true;
    }

    void SpawnLevels::clear()
    {
        const std::lock_guard lock(mMutex);
        mCounter = 0;
        mPlacements.clear();
        mZoneLevels.clear();
    }

    int SpawnLevels::lockedZoneLevel(FormId zone) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mZoneLevels.find(zone);
        return found == mZoneLevels.end() ? 0 : found->second;
    }

    void SpawnLevels::lockZoneLevel(FormId zone, int level)
    {
        if (zone == 0 || level < 1)
            return;
        const std::lock_guard lock(mMutex);
        mZoneLevels.emplace(zone, level);
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
        // The levels the zones fixed (the section has this from its version 2)
        out.count(mZoneLevels.size());
        for (const auto& [zone, level] : mZoneLevels)
        {
            out.form(zone);
            out.i32(level);
        }
    }

    void SpawnLevels::read(SaveReader& in, std::uint32_t version)
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
        std::map<FormId, int> zones;
        if (version >= 2)
        {
            const std::size_t zoneCount = in.count(8);
            for (std::size_t i = 0; i < zoneCount; ++i)
            {
                const std::optional<FormId> zone = in.form();
                const int level = in.i32();
                if (zone && level >= 1)
                    zones[*zone] = level;
            }
        }
        const std::lock_guard lock(mMutex);
        mCounter = counter;
        mPlacements = std::move(all);
        mZoneLevels = std::move(zones);
    }
}
