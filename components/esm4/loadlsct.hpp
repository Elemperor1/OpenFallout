#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADLSCT_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADLSCT_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A load screen type of New Vegas: how a load screen is laid out. The block of settings is kept as it is.
    struct LoadScreenType
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::array<std::uint8_t, 88> mData{}; // DATA

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_LSCT4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADLSCT_H
