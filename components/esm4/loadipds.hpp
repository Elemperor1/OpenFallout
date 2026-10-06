#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADIPDS_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADIPDS_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// An impact data set of Fallout 3 and New Vegas: which impact to use for each kind of material.
    struct ImpactDataSet
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::vector<ESM::FormId> mImpacts; // DATA, IPCT records, one for each kind of material

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_IPDS4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADIPDS_H
