#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADWATR_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADWATR_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A kind of water of Fallout 3 and New Vegas. The blocks of settings are kept as they are.
    struct Water
    {
#pragma pack(push, 1)
        struct RelatedWaters
        {
            ESM::FormId32 mDaytime = 0;
            ESM::FormId32 mNighttime = 0;
            ESM::FormId32 mUnderwater = 0;
        };
#pragma pack(pop)

        struct Colour
        {
            std::uint8_t mRed = 0;
            std::uint8_t mGreen = 0;
            std::uint8_t mBlue = 0;
        };

        /// How a water looks, from the first 52 bytes of its settings: the colour of shallow water and of deep water,
        /// the colour of what it reflects, and how much it reflects. The alpha byte of a colour is not read, no game
        /// uses it.
        struct Appearance
        {
            Colour mShallow;
            Colour mDeep;
            Colour mReflection;
            float mSunPower = 0.f;
            float mReflectivity = 0.f; // 0.6 for the clean water of New Vegas
            float mFresnel = 0.f;
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::string mNoiseTexture; // NNAM
        std::uint8_t mOpacity = 0; // ANAM
        std::uint8_t mWaterFlags = 0; // FNAM
        std::vector<std::uint8_t> mMnam; // MNAM, one byte in the game files, a string in the format reference
        ESM::FormId mSound; // SNAM
        ESM::FormId mActorEffect; // XNAM
        std::vector<std::uint8_t> mData; // DATA, the damage in the shorter form, all settings in the longer
        std::vector<std::uint8_t> mVisualData; // DNAM
        RelatedWaters mRelatedWaters; // GNAM, unused by the game

        /// The appearance given by the settings of the water: the DNAM sub-record, or the DATA sub-record of a water
        /// that has all its settings in it (8 of the water types of New Vegas have no DNAM). Nothing when the record
        /// has neither or when one of the numbers is not finite.
        std::optional<Appearance> appearance() const;

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_WATR4;
    };

    static_assert(sizeof(Water::RelatedWaters) == 12);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADWATR_H
