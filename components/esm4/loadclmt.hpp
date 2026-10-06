#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCLMT_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCLMT_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

namespace ESM4
{
    class Reader;

    /// A climate of Fallout 3 and New Vegas: which weathers a region can have and how the sky looks at what time of
    /// day.
    struct Climate
    {
#pragma pack(push, 1)
        struct WeatherEntry
        {
            ESM::FormId32 mWeather = 0; // a WTHR
            std::int32_t mChance = 0;
            ESM::FormId32 mGlobal = 0; // a GLOB that must be set for the weather
        };

        struct Timing
        {
            std::uint8_t mSunriseBegin = 0; // in units of 10 minutes
            std::uint8_t mSunriseEnd = 0;
            std::uint8_t mSunsetBegin = 0;
            std::uint8_t mSunsetEnd = 0;
            std::uint8_t mVolatility = 0;
            std::uint8_t mMoonPhaseLength = 0; // with the moons
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::vector<WeatherEntry> mWeathers; // WLST
        std::string mSunTexture; // FNAM
        std::string mSunGlareTexture; // GNAM
        ESM::Path mModel; // MODL, the model
        float mBoundRadius = 0; // MODB
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        std::vector<std::uint8_t> mModelAlternateTextures; // MODS, not decoded
        std::uint8_t mModelFlags = 0; // MODD, FaceGen model flags
        Timing mTiming; // TNAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CLMT4;
    };

    static_assert(sizeof(Climate::WeatherEntry) == 12);
    static_assert(sizeof(Climate::Timing) == 6);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCLMT_H
