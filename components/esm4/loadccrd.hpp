#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADCCRD_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADCCRD_H

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

    /// A card of the Caravan card game of New Vegas.
    struct CaravanCard
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ObjectBounds mBounds; // OBND
        std::string mFullName; // FULL
        ESM::Path mModel; // MODL, the model
        std::string mIcon; // ICON
        ESM::FormId mScript; // SCRI
        std::string mFrontFace; // TX00
        std::string mBackFace; // TX01
        std::vector<std::uint32_t> mIntegers; // INTV, two numbers for the card, not decoded
        std::uint32_t mValue = 0; // DATA
        std::string mSmallIcon; // MICO
        float mBoundRadius = 0; // MODB
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        std::vector<std::uint8_t> mModelAlternateTextures; // MODS, not decoded
        std::uint8_t mModelFlags = 0; // MODD, FaceGen model flags
        ESM::FormId mPickUpSound; // YNAM
        ESM::FormId mDropSound; // ZNAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CCRD4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADCCRD_H
