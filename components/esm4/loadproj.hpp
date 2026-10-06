#ifndef OPENFALLOUT_COMPONENTS_ESM4_LOADPROJ_H
#define OPENFALLOUT_COMPONENTS_ESM4_LOADPROJ_H

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

        ESM::FormId mId; // from the header
        std::uint32_t mFlags = 0; // from the header, see enum type RecordFlag for details

        std::string mEditorId; // EDID
        ObjectBounds mBounds; // OBND
        std::string mFullName; // FULL
        ESM::Path mModel; // MODL, the model
        std::vector<std::uint8_t> mModelTextures; // MODT, texture hashes of the model, not decoded
        Destruction mDestruction; // DEST
        std::vector<DestructionStage> mStages; // DSTD
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
