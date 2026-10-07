#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADWTHR_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADWTHR_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "alternatetexture.hpp"

namespace ESM4
{
    class Reader;

    /// A weather of Fallout 3 and New Vegas.
    struct Weather
    {
#pragma pack(push, 1)
        struct Fog
        {
            float mDayNear = 0.f;
            float mDayFar = 0.f;
            float mNightNear = 0.f;
            float mNightFar = 0.f;
            float mDayPower = 0.f;
            float mNightPower = 0.f;
        };

        struct Data
        {
            std::uint8_t mWindSpeed = 0;
            std::uint8_t mCloudSpeedLower = 0;
            std::uint8_t mCloudSpeedUpper = 0;
            std::uint8_t mTransDelta = 0;
            std::uint8_t mSunGlare = 0;
            std::uint8_t mSunDamage = 0;
            std::uint8_t mPrecipitationBeginFadeIn = 0;
            std::uint8_t mPrecipitationEndFadeOut = 0;
            std::uint8_t mThunderBeginFadeIn = 0;
            std::uint8_t mThunderEndFadeOut = 0;
            std::uint8_t mThunderFrequency = 0;
            std::uint8_t mClassification = 0;
            std::uint8_t mLightningRed = 0;
            std::uint8_t mLightningGreen = 0;
            std::uint8_t mLightningBlue = 0;
        };

        struct WeatherSound
        {
            ESM::FormId32 mSound = 0; // a SOUN
            std::uint32_t mType = 0;
        };
#pragma pack(pop)

        /// What a colour of NAM0 colours: the sky overhead and near the horizon, the fog, the light of the sun, and so
        /// on.
        enum class ColourType : std::size_t
        {
            SkyUpper,
            Fog,
            CloudsLower,
            Ambient,
            Sunlight,
            Sun,
            Stars,
            SkyLower,
            Horizon,
            CloudsUpper,
        };

        /// The times of day that NAM0 gives a colour for. A weather of Fallout 3 has the first four and one of New
        /// Vegas has all six.
        enum class TimeOfDay : std::size_t
        {
            Sunrise,
            Day,
            Sunset,
            Night,
            HighNoon,
            Midnight,
        };

        struct Colour
        {
            std::uint8_t mRed = 0;
            std::uint8_t mGreen = 0;
            std::uint8_t mBlue = 0;
        };

        static constexpr std::size_t sColourTypeCount = 10;

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ESM::Path mModel; // MODL, the model
        float mBoundRadius = 0; // MODB
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        std::vector<AlternateTexture> mModelAlternateTextures; // MODS
        std::uint8_t mModelFlags = 0; // MODD, FaceGen model flags
        std::array<ESM::FormId, 6> mImageSpaceModifiers; // \x00IAD and others, IMAD records for sunrise, day, sunset,
                                                         // night, high noon and midnight
        std::array<std::string, 4> mCloudTextures; // DNAM and others, the four layers of clouds
        std::uint32_t mLnam = 0; // LNAM, not known
        std::array<std::uint8_t, 4> mCloudSpeeds{}; // ONAM, one byte for each layer of clouds
        std::vector<std::uint8_t> mCloudColours; // PNAM, not decoded
        std::vector<std::uint8_t> mColours; // NAM0, not decoded
        Fog mFog; // FNAM
        std::array<std::uint8_t, 304> mInam{}; // INAM, not known
        Data mData; // DATA
        std::vector<WeatherSound> mSounds; // SNAM

        /// How many times of day NAM0 gives colours for: 4 or 6, and 0 when the record has no NAM0.
        std::size_t colourTimeCount() const { return mColours.size() / (sColourTypeCount * 4); }

        /// The colour of a type at a time of day, or nothing when the record has none for that time. The alpha byte of
        /// NAM0 is not read, no game uses it.
        std::optional<Colour> colour(ColourType type, TimeOfDay time) const;

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_WTHR4;
    };

    static_assert(sizeof(Weather::Fog) == 24);
    static_assert(sizeof(Weather::Data) == 15);
    static_assert(sizeof(Weather::WeatherSound) == 8);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADWTHR_H
