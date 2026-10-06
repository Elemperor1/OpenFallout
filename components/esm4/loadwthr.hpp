#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADWTHR_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADWTHR_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

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

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
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
