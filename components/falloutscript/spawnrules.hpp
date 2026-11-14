#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SPAWNRULES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SPAWNRULES_H

#include <array>
#include <cstdint>
#include <map>
#include <mutex>

#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript
{
    /// The settings that decide at what level a character or creature comes into the world, with the values of the
    /// executables for what no file of either game sets (the files of both games have fLevelScalingMult and the
    /// fLeveledActorMult settings with the same values as below; the other two are in no file)
    struct SpawnSettings
    {
        float mLevelScaling = 1.0f; // fLevelScalingMult: how much of the level above the minimum of a zone counts
        int mAdjustUp = 1; // iCalcLevelAdjustUp: the highest the level of a placement is made higher
        int mAdjustDown = 1; // iCalcLevelAdjustDown: the highest it is made lower
        /// fLeveledActorMultEasy, ...Medium, ...Hard and ...Boss: what the level is multiplied with for the level
        /// modifier 0 to 3 of PlaceLeveledActorAtMe
        std::array<float, 4> mLevelModMultiplier{ 0.5f, 0.75f, 1.25f, 2.0f };
    };

    /// The level modifier that a placement has when its command gives none (PlaceLeveledActorAtMe has 4 as default)
    constexpr int noLevelMod = 4;

    /// The flags of an encounter zone (ECZN)
    namespace ZoneFlag
    {
        constexpr int neverResets = 0x01;
        constexpr int matchPlayerBelowMinimum = 0x02;
    }

    /// The level of an encounter zone for a player of that level (0x526190 of the New Vegas executable): a player below
    /// the minimum level of the zone gets the minimum, or his own level when the zone matches the player below its
    /// minimum; a player above it gets the minimum and the part of the difference that the scaling setting says,
    /// with the fraction cut off.
    int zoneLevel(int playerLevel, int minimumLevel, int zoneFlags, float levelScaling);

    /// The level modifier as the executable stores it with a placement: 0 to 3, or none for anything else. It is what
    /// the levelled lists are told (the modifier of the placement, 0 for one without).
    int storedLevelMod(int levelMod);

    /// What the level of a placement is multiplied with: the setting of its modifier, 1 for none (0x4214F0)
    float levelModMultiplier(int levelMod, const SpawnSettings& settings);

    /// The level that the levelled lists of a placement are rolled for: the level of the placement times its
    /// multiplier with the fraction cut off, and not below 1 (0x488350)
    int scaledSpawnLevel(int level, int levelMod, const SpawnSettings& settings);

    /// The levels at which things were put into the world, and the rotation that makes every level a little different:
    /// each placement is made one level higher, one lower or at the level itself, in turn (the counter that 0x567E10
    /// keeps goes up by one for every placement, and starts again below zero by the setting for the largest
    /// difference once it is above the setting that says how much higher a level may be made). It can be used from any
    /// thread.
    class SpawnLevels
    {
    public:
        explicit SpawnLevels(const SpawnSettings& settings = SpawnSettings());

        /// Forgets everything, as for a new game
        void clear();
        const SpawnSettings& settings() const { return mSettings; }

        /// The level of a placement: the level of its zone or the player, made one higher, lower or not, in turn when
        /// `rotate`; never below 1 (0x567E10)
        int placementLevel(int baseLevel, bool rotate);
        /// Where the counter of the rotation stands
        int counter() const;

        /// What a reference was put into the world at (and the modifier of its placement); false for a reference
        /// that was not
        bool find(FormId reference, int& level, int& levelMod) const;
        void set(FormId reference, int level, int levelMod = noLevelMod);
        void forget(FormId reference);
        std::size_t size() const;

        void write(SaveWriter& out) const;
        void read(SaveReader& in, std::uint32_t version);

    private:
        struct Placement
        {
            int mLevel = 1;
            int mLevelMod = noLevelMod;
        };

        SpawnSettings mSettings;
        mutable std::mutex mMutex;
        int mCounter = 0;
        std::map<FormId, Placement> mPlacements;
    };
}

#endif
