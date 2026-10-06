#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCSNO_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCSNO_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A casino of New Vegas. The block of numbers is kept as it is.
    struct Casino
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::array<std::uint8_t, 56> mData{}; // DATA
        std::vector<std::string> mModels; // MODL, eight of them
        std::string mModel2; // MOD2
        std::string mModel3; // MOD3
        std::string mModel4; // MOD4
        std::vector<std::string> mIcons; // ICON, seven of them
        std::vector<std::string> mIcons2; // ICO2, four of them

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CSNO4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCSNO_H
