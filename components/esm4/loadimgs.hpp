#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADIMGS_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADIMGS_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// An image space of Fallout 3 and New Vegas: the colour grading and the bloom of a cell or a weather. The block of
    /// settings is kept as it is.
    struct ImageSpace
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::vector<std::uint8_t> mData; // DNAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_IMGS4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADIMGS_H
