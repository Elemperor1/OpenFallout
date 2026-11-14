#include "respawn.hpp"

namespace FalloutScript
{
    RespawnState::RespawnState(const RespawnSettings& settings)
        : mSettings(settings)
    {
    }

    void RespawnState::clear()
    {
        const std::lock_guard lock(mMutex);
        mDetached.clear();
        mDeaths.clear();
        mZones.clear();
        mGenerations.clear();
        mMerchantDays.clear();
    }

    void RespawnState::cellDetached(FormId cell, std::uint32_t now)
    {
        if (cell == 0)
            return;
        const std::lock_guard lock(mMutex);
        // A cell that was told to reset keeps that until it loads
        const auto found = mDetached.find(cell);
        if (found != mDetached.end() && found->second == forced)
            return;
        mDetached[cell] = now == forced ? forced - 1 : now;
    }

    void RespawnState::forceReset(FormId cell)
    {
        if (cell == 0)
            return;
        const std::lock_guard lock(mMutex);
        mDetached[cell] = forced;
    }

    bool RespawnState::cellResetsLocked(FormId cell, std::uint32_t now, bool zoneNeverResets) const
    {
        const auto found = mDetached.find(cell);
        if (found == mDetached.end())
            return false;
        if (found->second == forced)
            return true;
        const std::uint32_t elapsed = now - found->second;
        return elapsed > mSettings.mHoursToRespawn && !zoneNeverResets;
    }

    bool RespawnState::attachCell(FormId cell, std::uint32_t now, bool zoneNeverResets)
    {
        const std::lock_guard lock(mMutex);
        const bool resets = cellResetsLocked(cell, now, zoneNeverResets);
        mDetached.erase(cell);
        return resets;
    }

    bool RespawnState::cellResets(FormId cell, std::uint32_t now, bool zoneNeverResets) const
    {
        const std::lock_guard lock(mMutex);
        return cellResetsLocked(cell, now, zoneNeverResets);
    }

    bool RespawnState::wasDetached(FormId cell) const
    {
        const std::lock_guard lock(mMutex);
        return mDetached.find(cell) != mDetached.end();
    }

    void RespawnState::died(FormId actor, std::uint32_t now)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        // 0 is what the executable keeps for a character that did not die
        mDeaths[actor] = now == 0 ? 1 : now;
    }

    void RespawnState::revived(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        mDeaths.erase(actor);
    }

    bool RespawnState::actorDue(FormId actor, std::uint32_t now, bool zoneNeverResets) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mDeaths.find(actor);
        if (found == mDeaths.end())
            return true;
        if (now <= found->second)
            return false;
        return now - found->second > mSettings.mHoursToRespawn && !zoneNeverResets;
    }

    bool RespawnState::deathTime(FormId actor, std::uint32_t& when) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mDeaths.find(actor);
        if (found == mDeaths.end())
            return false;
        when = found->second;
        return true;
    }

    void RespawnState::setZoneNeverResets(FormId zone, bool never)
    {
        if (zone == 0)
            return;
        const std::lock_guard lock(mMutex);
        mZones[zone] = never;
    }

    bool RespawnState::zoneNeverResets(FormId zone, int recordFlags) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mZones.find(zone);
        if (found != mZones.end())
            return found->second;
        return (recordFlags & ZoneFlag::neverResets) != 0;
    }

    bool RespawnState::merchantRestocks(FormId merchant, std::int64_t day)
    {
        if (day < 0)
            return false;
        const int weekday = static_cast<int>(day % 7);
        if (weekday != mSettings.mMerchantDay1 && weekday != mSettings.mMerchantDay2)
            return false;
        const std::lock_guard lock(mMutex);
        const auto found = mMerchantDays.find(merchant);
        if (found != mMerchantDays.end() && found->second == day)
            return false;
        mMerchantDays[merchant] = day;
        return true;
    }

    int RespawnState::inventoryGeneration(FormId holder) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mGenerations.find(holder);
        return found == mGenerations.end() ? 0 : found->second;
    }

    void RespawnState::resetInventory(FormId holder)
    {
        if (holder == 0)
            return;
        const std::lock_guard lock(mMutex);
        ++mGenerations[holder];
    }

    void RespawnState::write(SaveWriter& out) const
    {
        const std::lock_guard lock(mMutex);
        out.count(mDetached.size());
        for (const auto& [cell, when] : mDetached)
        {
            out.form(cell);
            out.u32(when);
        }
        out.count(mDeaths.size());
        for (const auto& [actor, when] : mDeaths)
        {
            out.form(actor);
            out.u32(when);
        }
        out.count(mZones.size());
        for (const auto& [zone, never] : mZones)
        {
            out.form(zone);
            out.boolean(never);
        }
        out.count(mGenerations.size());
        for (const auto& [holder, generation] : mGenerations)
        {
            out.form(holder);
            out.i32(generation);
        }
    }

    void RespawnState::read(SaveReader& in, std::uint32_t)
    {
        std::map<FormId, std::uint32_t> detached, deaths;
        std::map<FormId, bool> zones;
        std::map<FormId, int> generations;
        for (std::size_t i = 0, n = in.count(8); i < n; ++i)
        {
            const std::optional<FormId> cell = in.form();
            const std::uint32_t when = in.u32();
            if (cell)
                detached[*cell] = when;
        }
        for (std::size_t i = 0, n = in.count(8); i < n; ++i)
        {
            const std::optional<FormId> actor = in.form();
            const std::uint32_t when = in.u32();
            if (actor)
                deaths[*actor] = when;
        }
        for (std::size_t i = 0, n = in.count(5); i < n; ++i)
        {
            const std::optional<FormId> zone = in.form();
            const bool never = in.boolean();
            if (zone)
                zones[*zone] = never;
        }
        for (std::size_t i = 0, n = in.count(8); i < n; ++i)
        {
            const std::optional<FormId> holder = in.form();
            const int generation = in.i32();
            if (holder)
                generations[*holder] = generation;
        }
        const std::lock_guard lock(mMutex);
        mDetached = std::move(detached);
        mDeaths = std::move(deaths);
        mZones = std::move(zones);
        mGenerations = std::move(generations);
    }
}
