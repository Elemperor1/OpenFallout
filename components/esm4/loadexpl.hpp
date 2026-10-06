#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADEXPL_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADEXPL_H

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

    /// An explosion of Fallout 3 and New Vegas.
    struct Explosion
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ObjectBounds mBounds; // OBND
        std::string mFullName; // FULL
        ESM::Path mModel; // MODL, the model
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        ESM::FormId mObjectEffect; // EITM
        ESM::FormId mImageSpaceModifier; // MNAM
        ESM::FormId mPlacedImpactObject; // INAM
        std::array<std::uint8_t, 52> mData{}; // DATA, force, damage, radius, light, sounds, flags; not decoded

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_EXPL4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADEXPL_H
