#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADMICN_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADMICN_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// A menu icon of Fallout 3 and New Vegas.
    struct MenuIcon
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mIcon; // ICON
        std::string mSmallIcon; // MICO, New Vegas only

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_MICN4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADMICN_H
