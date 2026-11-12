#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SOUNDDATA_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SOUNDDATA_H

#include <array>
#include <cstdint>

namespace FalloutScript
{
    /// What a sound record of Fallout 3 or New Vegas (`SOUN`) says about how its file is played: the part of its data
    /// (`SNDD`, 12 bytes of the settings and 24 of the curve) that decides how loud it is where the listener stands. The
    /// distances are in the units of the games: a byte of the minimum is five units, a byte of the maximum a hundred
    /// (the executables compare a distance with the maximum byte times 100: FalloutNV.exe 0x8384F0, 0x838608).
    struct SoundData
    {
        /// The lowest pitch that is played: the output does not play at a pitch of zero or less
        static constexpr float minimumPitch = 0.1f;

        enum Flags : std::uint16_t
        {
            RandomFrequencyShift = 0x0001,
            PlayAtRandom = 0x0002,
            EnvironmentIgnored = 0x0004,
            RandomLocation = 0x0008,
            Loop = 0x0010,
            MenuSound = 0x0020,
            TwoDimensional = 0x0040,
            ThreeSixtyLfe = 0x0080,
        };

        std::uint8_t mMinAttenuation = 0; // times 5 units
        std::uint8_t mMaxAttenuation = 0; // times 100 units
        std::int8_t mFrequencyAdjustment = 0; // percent
        std::uint16_t mFlags = 0;
        std::uint16_t mStaticAttenuation = 0; // hundredths of a decibel that the whole sound is made quieter by
        std::uint8_t mStopTime = 0; // of the day, in 1440/256 minutes
        std::uint8_t mStartTime = 0;
        /// The volume in percent at five distances evenly spread from the minimum to the maximum (the default curve of
        /// the editor, 100 50 20 5 0, is what most sounds have)
        std::array<std::int16_t, 5> mCurve = { 100, 50, 20, 5, 0 };
        std::int32_t mPriority = 0;

        bool isTwoDimensional() const { return (mFlags & (TwoDimensional | MenuSound)) != 0; }
        bool isLooping() const { return (mFlags & Loop) != 0; }
        bool hasDistance() const { return mMaxAttenuation != 0 && !isTwoDimensional(); }

        float minDistance() const { return static_cast<float>(mMinAttenuation) * 5.f; }
        float maxDistance() const { return static_cast<float>(mMaxAttenuation) * 100.f; }

        /// The linear gain of the static attenuation: 10 to the power of minus the decibels over twenty
        float staticGain() const;

        /// The gain at a distance from the sound: full up to the minimum distance, along the curve between the minimum
        /// and the maximum, none beyond the maximum. A sound with no maximum (or one that is not placed) has no
        /// distance, and a gain of one.
        float distanceGain(float distance) const;

        /// The pitch of one playing: one plus the frequency adjustment in percent, and with the flag of the random
        /// shift anywhere from minus that to plus that (`unit` is a number in [0, 1]); never under `minimumPitch`
        float pitch(float unit) const;
    };

    /// The data of a record's subrecord: `size` is 12 (the settings only) or more (the curve follows). A subrecord that
    /// is shorter than the settings gives the defaults.
    SoundData soundDataFromBytes(const unsigned char* bytes, std::size_t size);
}

#endif
