#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADADDN_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADADDN_H

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

    /// An addon node of Fallout 3 and New Vegas: a model that a particle system or an effect attaches to. Other records
    /// name it by its node index.
    struct AddonNode
    {
#pragma pack(push, 1)
        struct ParticleData
        {
            std::uint16_t mMasterParticleSystemCap = 0;
            std::uint16_t mUnknown = 0;
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ObjectBounds mBounds; // OBND
        ESM::Path mModel; // MODL, the model
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        std::int32_t mNodeIndex = 0; // DATA, the index other records use to name the node
        ESM::FormId mSound; // SNAM
        ParticleData mParticleData; // DNAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_ADDN4;
    };

    static_assert(sizeof(AddonNode::ParticleData) == 4);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADADDN_H
