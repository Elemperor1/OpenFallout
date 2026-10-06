#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCPTH_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCPTH_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

#include "script.hpp"

namespace ESM4
{
    class Reader;

    /// A camera path of Fallout 3 and New Vegas: a chain of camera shots that the game plays one after the other.
    struct CameraPath
    {
#pragma pack(push, 1)
        struct Related
        {
            ESM::FormId32 mParent = 0; // a CPTH
            ESM::FormId32 mPrevious = 0; // a CPTH
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::vector<TargetCondition> mConditions; // CTDA
        Related mRelated; // ANAM
        std::uint8_t mZoom = 0; // DATA
        std::vector<ESM::FormId> mCameraShots; // SNAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CPTH4;
    };

    static_assert(sizeof(CameraPath::Related) == 8);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCPTH_H
