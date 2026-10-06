#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADREPU_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADREPU_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A reputation of New Vegas: how a faction or a town feels about the player.
    struct Reputation
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::string mIcon; // ICON
        std::array<std::uint8_t, 4> mData{}; // DATA, not known

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_REPU4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADREPU_H
