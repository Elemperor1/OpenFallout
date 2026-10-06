#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCMNY_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCMNY_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "objectbounds.hpp"

namespace ESM4
{
    class Reader;

    /// A kind of money of a casino of New Vegas.
    struct CaravanMoney
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ObjectBounds mBounds; // OBND
        std::string mFullName; // FULL
        ESM::Path mModel; // MODL, the model
        std::string mIcon; // ICON
        std::string mSmallIcon; // MICO
        std::uint32_t mAbsoluteValue = 0; // DATA

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CMNY4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCMNY_H
