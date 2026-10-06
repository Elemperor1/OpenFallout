#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCSNO_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCSNO_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

#include "recordreader.hpp"

namespace ESM4
{
    class Reader;

    /// A casino of New Vegas.
    struct Casino
    {
#pragma pack(push, 1)
        struct Data
        {
            float mDecksPercentBeforeShuffle = 0;
            float mBlackjackPayoutRatio = 0;
            std::array<std::uint32_t, 7> mSlotReelStops{}; // symbols 1 to 6 and W
            std::uint32_t mNumberOfDecks = 0;
            std::uint32_t mMaxWinnings = 0;
            ESM::FormId32 mCurrency = 0; // a CHIP
            ESM::FormId32 mWinningsQuest = 0; // a QUST
            std::uint32_t mFlags = 0; // 1 = the dealer stays on soft 17
        };
#pragma pack(pop)
        static_assert(sizeof(Data) == 56);

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        Data mData; // DATA
        std::vector<std::string> mModels; // MODL, eight of them
        std::string mModel2; // MOD2
        std::string mModel3; // MOD3
        std::string mModel4; // MOD4
        std::vector<std::string> mIcons; // ICON, seven of them
        std::vector<std::string> mIcons2; // ICO2, four of them
        // MODB, MODT, MODS and MODD, and MO2T, MO2S, MO3T, MO3S, MOSD, MO4T and MO4S: the format reference does not
        // list them for a casino, which has eight models, so they cannot be told apart by what they follow. Kept with
        // their codes and not decoded.
        std::vector<RawSubRecord> mModelData;

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CSNO4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCSNO_H
