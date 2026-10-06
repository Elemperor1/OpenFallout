#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADMGEF_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADMGEF_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "alternatetexture.hpp"

namespace ESM4
{
    class Reader;

    /// A magic effect of Fallout 3 and New Vegas.
    struct MagicEffect
    {
#pragma pack(push, 1)
        struct Data
        {
            std::uint32_t mEffectFlags = 0;
            float mBaseCost = 0.f;
            ESM::FormId32 mAssociatedItem = 0;
            std::int32_t mMagicSchool = 0; // unused
            std::int32_t mResistValue = 0; // an actor value
            std::uint16_t mCounterEffectCount = 0;
            std::uint16_t mUnused = 0;
            ESM::FormId32 mLight = 0;
            float mProjectileSpeed = 0.f;
            ESM::FormId32 mEffectShader = 0;
            ESM::FormId32 mObjectDisplayShader = 0;
            ESM::FormId32 mEffectSound = 0;
            ESM::FormId32 mBoltSound = 0;
            ESM::FormId32 mHitSound = 0;
            ESM::FormId32 mAreaSound = 0;
            float mConstantEffectEnchantmentFactor = 0.f;
            float mConstantEffectBarterFactor = 0.f;
            std::uint32_t mArchetype = 0;
            std::int32_t mActorValue = 0;
        };
#pragma pack(pop)

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        std::string mFullName; // FULL
        std::string mDescription; // DESC
        ESM::Path mModel; // MODL, the model
        Data mData; // DATA
        std::string mIcon; // ICON
        std::string mSmallIcon; // MICO
        float mBoundRadius = 0; // MODB
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        std::vector<AlternateTexture> mModelAlternateTextures; // MODS
        std::uint8_t mModelFlags = 0; // MODD, FaceGen model flags

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_MGEF4;
    };

    static_assert(sizeof(MagicEffect::Data) == 72);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADMGEF_H
