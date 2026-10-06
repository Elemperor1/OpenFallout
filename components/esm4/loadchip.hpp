#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCHIP_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCHIP_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "destruction.hpp"
#include "objectbounds.hpp"

namespace ESM4
{
    class Reader;

    /// A poker chip of New Vegas.
    struct PokerChip
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ObjectBounds mBounds; // OBND
        std::string mFullName; // FULL
        ESM::Path mModel; // MODL, the model
        std::string mIcon; // ICON
        ESM::FormId mPickUpSound; // YNAM
        ESM::FormId mDropSound; // ZNAM
        std::string mSmallIcon; // MICO
        float mBoundRadius = 0; // MODB
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        std::vector<std::uint8_t> mModelAlternateTextures; // MODS, not decoded
        std::uint8_t mModelFlags = 0; // MODD, FaceGen model flags
        Destruction mDestruction; // DEST, DSTD, DMDL, DMDT, DMDS and DSTF

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CHIP4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCHIP_H
