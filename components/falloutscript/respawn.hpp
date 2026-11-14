#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_RESPAWN_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_RESPAWN_H

#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>

#include "savestate.hpp"
#include "script.hpp"
#include "spawnrules.hpp"

namespace FalloutScript
{
    /// The settings that decide when the world resets (the executable's default, as no file of either game has the
    /// setting)
    struct RespawnSettings
    {
        /// iHoursToRespawnCell: the hours that pass before a cell that was left resets, and before a dead character
        /// comes back
        std::uint32_t mHoursToRespawn = 72;
        /// iMerchantRespawnDay1 and ...Day2: the days of the week (the whole days since the game began, modulo 7) on
        /// which a merchant gets new stock (0x886360; the two settings are in no file of either game)
        int mMerchantDay1 = 0;
        int mMerchantDay2 = 3;
    };

    /// The game hours that the clock of the respawn rules counts in: whole hours since the game began (the global
    /// GameDaysPassed counts days with the part of a day after the point)
    inline std::uint32_t gameHoursOf(double gameDaysPassed)
    {
        if (!(gameDaysPassed > 0))
            return 0;
        const double hours = std::floor(gameDaysPassed * 24.0);
        return hours >= 4294967295.0 ? 0xFFFFFFFFu : static_cast<std::uint32_t>(hours);
    }

    /// When cells were left, when characters died, which zones never reset and how often an inventory was rebuilt:
    /// what the rules of the world that bring things back need to remember. It can be used from any thread.
    ///
    /// A cell that loads again resets (its characters and containers are made new) when more than the settings' hours
    /// went by since it was left (0x54E0AA of the New Vegas executable: a difference of the whole hours, as unsigned
    /// numbers, above the setting) or when it was told to reset by ResetInterior (a detach time of -1). A zone that
    /// never resets keeps the cells that are in it as they are. A character that is dead comes back when more than
    /// the hours passed since it died and its zone does not forbid it (0x881C90); a living one is always up to be made
    /// new.
    class RespawnState
    {
    public:
        explicit RespawnState(const RespawnSettings& settings = RespawnSettings());

        /// Forgets everything, as for a new game
        void clear();
        const RespawnSettings& settings() const { return mSettings; }

        /// A cell was left (unloaded) at that game hour
        void cellDetached(FormId cell, std::uint32_t now);
        /// ResetInterior: the cell resets the next time it is loaded, however soon
        void forceReset(FormId cell);
        /// Whether the cell resets when it is loaded at that game hour, and the cell is attached from here: its time
        /// of leaving is forgotten. A cell that was never left does not reset (it is new). `zoneNeverResets` is whether
        /// the zone of the cell says it does not (a forced reset does not ask the zone).
        bool attachCell(FormId cell, std::uint32_t now, bool zoneNeverResets);
        /// The same without changing anything
        bool cellResets(FormId cell, std::uint32_t now, bool zoneNeverResets) const;
        /// Whether the cell was left and not loaded since
        bool wasDetached(FormId cell) const;

        /// An actor died at that game hour, or was brought back
        void died(FormId actor, std::uint32_t now);
        void revived(FormId actor);
        /// Whether the actor is up to be made new when its cell resets (0x881C90): an actor without a time of death
        /// (alive) always is; a dead one only when more than the setting's hours went by since it died and its zone
        /// does not forbid it.
        bool actorDue(FormId actor, std::uint32_t now, bool zoneNeverResets) const;
        /// The game hour an actor died at; false for one that did not die as far as this knows
        bool deathTime(FormId actor, std::uint32_t& when) const;

        /// SetZoneRespawns: whether the zone is told to never reset or to reset, whatever its record says
        void setZoneNeverResets(FormId zone, bool never);
        /// Whether the zone never resets: what a script told it, or else its flags
        bool zoneNeverResets(FormId zone, int recordFlags) const;

        /// Whether a merchant that is up gets new stock on that day (the whole days since the game began): the weekday
        /// is one of the two the settings name (0x886360). It says yes once for each day for each merchant.
        bool merchantRestocks(FormId merchant, std::int64_t day);

        /// How often the inventory of the holder was rebuilt (ResetInventory), which the host chooses the new roll of
        /// the levelled lists in it by
        int inventoryGeneration(FormId holder) const;
        void resetInventory(FormId holder);

        void write(SaveWriter& out) const;
        void read(SaveReader& in, std::uint32_t version);

    private:
        /// The detach time of a cell that has to reset when it loads
        static constexpr std::uint32_t forced = 0xFFFFFFFFu;

        bool cellResetsLocked(FormId cell, std::uint32_t now, bool zoneNeverResets) const;

        RespawnSettings mSettings;
        mutable std::mutex mMutex;
        std::map<FormId, std::uint32_t> mDetached;
        std::map<FormId, std::uint32_t> mDeaths;
        std::map<FormId, bool> mZones;
        std::map<FormId, int> mGenerations;
        std::map<FormId, std::int64_t> mMerchantDays;
    };
}

#endif
