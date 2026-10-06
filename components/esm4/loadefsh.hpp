#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADEFSH_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADEFSH_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    /// An effect shader of Fallout 3 and New Vegas, which colours and textures a magic or weapon effect. The block of
    /// settings is kept as it is.
    struct EffectShader
    {
        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFillTexture; // ICON
        std::string mParticleTexture; // ICO2
        std::string mHolesTexture; // NAM7
        std::vector<std::uint8_t> mData; // DATA, fills, particles, colours and times; not decoded

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_EFSH4;
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADEFSH_H
