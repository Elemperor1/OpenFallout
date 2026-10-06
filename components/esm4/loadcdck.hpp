#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCDCK_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCDCK_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A deck of Caravan cards of New Vegas.
    struct CaravanDeck
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::vector<ESM::FormId> mCards; // CARD, CCRD records
        std::uint32_t mData = 0; // DATA, not known

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CDCK4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCDCK_H
