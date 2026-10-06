#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADDEBR_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADDEBR_H

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

    /// The debris of Fallout 3 and New Vegas: the models that fly off when something is destroyed.
    struct Debris
    {
        struct Model
        {
            std::uint8_t mPercentage = 0; // how much of the debris is this model
            ESM::Path mModel;
            std::uint8_t mFlags = 0; // 0x01 = has collision
            std::vector<std::uint8_t> mTextures; // MODT, texture hashes, not decoded
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::vector<Model> mModels; // DATA/MODT, DATA starts a model, the MODT after it belongs to it

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_DEBR4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADDEBR_H
