#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADEXPL_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADEXPL_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "alternatetexture.hpp"
#include "objectbounds.hpp"

namespace ESM4
{
    class Reader;

    /// An explosion of Fallout 3 and New Vegas.
    struct Explosion
    {
        enum Flags : std::uint32_t
        {
            AlwaysUsesWorldOrientation = 0x00000001,
            KnockDownAlways = 0x00000002,
            KnockDownByFormula = 0x00000004,
            IgnoreLosCheck = 0x00000008,
            PushExplosionSourceRefOnly = 0x00000010,
            IgnoreImageSpaceSwap = 0x00000020,
        };

        /// DATA, decoded
        struct GameData
        {
            float mForce = 0.f;
            float mDamage = 0.f;
            float mRadius = 0.f; // units
            ESM::FormId mLight;
            ESM::FormId mSound1;
            std::uint32_t mFlags = 0;
            float mImageSpaceRadius = 0.f;
            ESM::FormId mImpactDataSet;
            ESM::FormId mSound2;
            float mRadiationLevel = 0.f;
            float mRadiationDissipationTime = 0.f;
            float mRadiationRadius = 0.f;
            std::uint32_t mSoundLevel = 0;
        };

        GameData gameData() const;

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ObjectBounds mBounds; // OBND
        std::string mFullName; // FULL
        ESM::Path mModel; // MODL, the model
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        float mBoundRadius = 0; // MODB
        std::vector<AlternateTexture> mModelAlternateTextures; // MODS
        std::uint8_t mModelFlags = 0; // MODD, FaceGen model flags
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
