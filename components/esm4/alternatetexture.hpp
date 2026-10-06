#ifndef OPENFALLOUT_COMPONENTS_ESM4_ALTERNATETEXTURE_H
#define OPENFALLOUT_COMPONENTS_ESM4_ALTERNATETEXTURE_H

#include <cstdint>
#include <string>

#include <components/esm/formid.hpp>

namespace ESM4
{
    /// One entry of an alternate texture sub-record (MODS, DMDS and the like): the texture set that a part of a model
    /// shows instead of its own.
    struct AlternateTexture
    {
        std::string mName; // the name of the part in the model, "3D Name"
        ESM::FormId mTexture; // a TXST
        std::int32_t mIndex = 0; // "3D Index"
    };
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_ALTERNATETEXTURE_H
