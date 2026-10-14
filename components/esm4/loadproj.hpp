#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADPROJ_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADPROJ_H

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

    /// A projectile of Fallout 3 and New Vegas: what a weapon or a trap fires.
    struct Projectile
    {
#pragma pack(push, 1)
        struct Destruction
        {
            std::int32_t mHealth = 0;
            std::uint8_t mStageCount = 0;
            std::uint8_t mFlags = 0;
            std::uint8_t mUnused1 = 0;
            std::uint8_t mUnused2 = 0;
        };

        struct DestructionStage
        {
            std::uint8_t mHealthPercent = 0;
            std::uint8_t mIndex = 0;
            std::uint8_t mDamageStage = 0;
            std::uint8_t mFlags = 0;
            std::int32_t mSelfDamagePerSecond = 0;
            ESM::FormId32 mExplosion = 0;
            ESM::FormId32 mDebris = 0;
            std::int32_t mDebrisCount = 0;
        };
#pragma pack(pop)

        /// The model that a destruction stage shows.
        struct StageModel
        {
            ESM::Path mModel; // DMDL
            std::vector<std::uint8_t> mTextures; // DMDT, texture hashes, not decoded
            std::vector<AlternateTexture> mAlternateTextures; // DMDS
        };

        enum class Type : std::uint16_t
        {
            Missile = 1,
            Lobber = 2,
            Beam = 4,
            Flame = 8,
            ContinuousBeam = 16,
        };

        enum Flags : std::uint16_t
        {
            Hitscan = 0x0001,
            Explosion = 0x0002,
            AltTrigger = 0x0004,
            MuzzleFlash = 0x0008,
            CanBeDisabled = 0x0020,
            CanBePickedUp = 0x0040,
            Supersonic = 0x0080,
            PinsLimbs = 0x0100,
            PassThroughSmallTransparent = 0x0200,
        };

        /// DATA, decoded (the form ids already adjusted); what its bytes do not reach is the default
        struct GameData
        {
            std::uint16_t mFlags = 0;
            std::uint16_t mType = 0;
            float mGravity = 0.f;
            float mSpeed = 0.f; // units a second
            float mRange = 0.f;
            ESM::FormId mLight;
            ESM::FormId mMuzzleFlashLight;
            float mTracerChance = 0.f;
            float mProximityFuse = 0.f; // the explosion goes off when a target is this near (AltTrigger)
            float mFuseTimer = 0.f; // or after this long
            ESM::FormId mExplosion;
            ESM::FormId mSound;
            float mMuzzleFlashDuration = 0.f;
            float mFadeDuration = 0.f;
            float mImpactForce = 0.f;
            ESM::FormId mCountdownSound;
            ESM::FormId mDisableSound;
            ESM::FormId mDefaultWeaponSource;
        };

        /// The decoded DATA; the form ids in it are read from the bytes of the record, which the loader adjusted
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
        Destruction mDestruction; // DEST
        std::vector<DestructionStage> mStages; // DSTD
        std::vector<StageModel> mStageModels; // DMDL, DMDT and DMDS, one for each of mStages, empty when it has none
        std::vector<std::uint8_t> mData; // DATA, type, speed, range, lights, sounds; not decoded
        std::string mMuzzleFlashModel; // NAM1
        std::vector<std::uint8_t> mMuzzleFlashTextures; // NAM2, texture hashes, not decoded
        std::uint32_t mSoundLevel = 0; // VNAM

        /// Throws on unknown sub-records, on sizes that no known version of the record has, and on bytes of
        /// the record that no sub-record accounts for.
        void load(ESM4::Reader& reader);

        static constexpr ESM::RecNameInts sRecordId = ESM::REC_PROJ4;
    };

    static_assert(sizeof(Projectile::Destruction) == 8);
    static_assert(sizeof(Projectile::DestructionStage) == 20);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_LOADPROJ_H
