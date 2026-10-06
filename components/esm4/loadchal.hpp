#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCHAL_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCHAL_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A challenge of New Vegas, which the player completes for a reward.
    struct Challenge
    {
#pragma pack(push, 1)
        struct Data
        {
            std::uint32_t mType = 0; // 0 to 13: what is counted, from kills of a list to a scripted challenge
            std::uint32_t mThreshold = 0; // how much of it completes the challenge
            std::uint32_t mFlags = 0; // 1 = start disabled, 2 = recurring, 4 = show zero progress
            std::uint32_t mInterval = 0;
            std::array<std::uint8_t, 8> mTypeData{}; // two values of 2 bytes and one of 4, which the type decides
        };
#pragma pack(pop)
        static_assert(sizeof(Data) == 24);

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        ESM::FormId mScript; // SCRI
        std::string mDescription; // DESC
        Data mData; // DATA
        ESM::FormId mSnam; // SNAM, a form ID that the type decides
        ESM::FormId mXnam; // XNAM, a form ID that the type decides
        std::string mIcon; // ICON
        std::string mSmallIcon; // MICO

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CHAL4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCHAL_H
