#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_AMBIENT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_AMBIENT_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <vector>

namespace FalloutScript::Ambient
{
    /// The ambient sound of the places: the loop of an acoustic space (ASPC, named by the cell's XCAS), and the sounds of the
    /// region that the acoustic space names (REGN, its RDSD list). No sound is made here: the engine asks these rules what
    /// should play. The numbers that are the games' own are the defaults of the audio settings that FalloutNV.exe
    /// registers (0xF9DBEE, 0xF9DC7E, 0xFB6E35 and the ones after them).

    enum class TimeOfDay : std::uint8_t
    {
        Dawn,
        Day,
        Dusk,
        Night,
    };

    struct Settings
    {
        // The hours at which the periods start (fDawnTime, fDayTime, fDuskTime, fNightTime of the audio settings)
        float mDawn = 5.f;
        float mDay = 8.f;
        float mDusk = 18.f;
        float mNight = 20.f;
        // The seconds a new acoustic space waits before its loop starts, fades in and fades out (fASFadeStartTime,
        // fASFadeInTime, fASFadeOutTime)
        float mSpaceStart = 2.f;
        float mSpaceFadeIn = 3.f;
        float mSpaceFadeOut = 5.f;
        // The fades of the loops of a region (fRegionLoopFadeInTime, fRegionLoopFadeOutTime)
        float mRegionFadeIn = 0.6f;
        float mRegionFadeOut = 1.f;
    };

    /// The period of the day at the hour: dawn from the dawn hour to the day hour, day until dusk, dusk until night, and night
    /// until the next dawn (a night hour that comes before the dawn hour wraps over midnight)
    TimeOfDay timeOfDay(float hour, const Settings& settings = {});

    /// The loop of an acoustic space (0 none): an interior space plays its first sound; the others have one for each
    /// period of the day (slot 0 dawn, 1 day, 2 dusk, 3 night). A period without a sound takes the first slot that has one.
    std::uint32_t loopOfSpace(std::span<const std::uint32_t> slots, bool interior, TimeOfDay time);

    /// The weather that a region sound is for: the bits of its flags are pleasant (1), cloudy (2), rainy (4) and snowy
    /// (8), as the classification of a weather record has them
    bool weatherAllows(std::uint32_t flags, std::uint32_t classification);

    /// A sound of a region (RDSD)
    struct RegionSound
    {
        std::uint32_t mSound = 0;
        std::uint32_t mFlags = 0;
        std::uint32_t mChance = 0;
    };

    /// The chance of a region sound as a probability: its numbers are millionths (100000 is one in ten)
    float chanceOf(std::uint32_t chance);

    /// Which of a region's one-shot sounds to play. The numbers of the chances are the data's own; how often they are rolled
    /// is not in anything that was read, so the port gives each sound its own clock: every `interval` seconds the sound is
    /// rolled with its chance. The clocks start at random times in the first interval, so that the sounds of a region are
    /// spread out instead of coming together.
    class RegionRoller
    {
    public:
        explicit RegionRoller(float interval = 120.f)
            : mInterval(interval)
        {
        }

        /// Called every frame: the sounds to play now. `unit` gives numbers in [0, 1). Only a sound that is for the weather
        /// of the moment, has a chance and does not loop has a clock.
        std::vector<std::uint32_t> update(float seconds, std::span<const RegionSound> sounds,
            std::uint32_t classification, const std::function<double()>& unit,
            const std::function<bool(std::uint32_t)>& isLoop);
        void reset() { mLeft.clear(); }

    private:
        float mInterval;
        std::map<std::uint32_t, float> mLeft;
    };
}

#endif
