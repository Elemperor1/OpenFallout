#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADECZN_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADECZN_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// An encounter zone of Fallout 3 and New Vegas: who owns the cell and how strong the actors that appear in it are.
    struct EncounterZone
    {
#pragma pack(push, 1)
        struct Data
        {
            ESM::FormId32 mOwner = 0; // a FACT or an NPC_
            std::int8_t mRank = 0;
            std::int8_t mMinimumLevel = 0;
            std::uint8_t mFlags = 0;
            std::uint8_t mUnused = 0;
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        Data mData; // DATA

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_ECZN4;
    };

    static_assert(sizeof(EncounterZone::Data) == 8);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADECZN_H
