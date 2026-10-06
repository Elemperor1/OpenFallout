#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADIPCT_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADIPCT_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

namespace ESM4
{
    class Reader;

    /// An impact of Fallout 3 and New Vegas: what a bullet or a blow leaves behind on what it hits.
    struct ImpactData
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ESM::Path mModel; // MODL, the model
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        std::array<std::uint8_t, 24>
            mData{}; // DATA, duration, orientation, angle threshold, placement radius, sound level, flags
        std::array<std::uint8_t, 36> mDecalData{}; // DODT
        ESM::FormId mTextureSet; // DNAM
        ESM::FormId mSound1; // SNAM
        ESM::FormId mSound2; // NAM1

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_IPCT4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADIPCT_H
