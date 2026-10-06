#ifndef OPENFALLOUT_COMPONENTS_ESM4_DESTRUCTION_H
#define OPENFALLOUT_COMPONENTS_ESM4_DESTRUCTION_H

#include <cstdint>
#include <vector>

#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

#include "alternatetexture.hpp"

namespace ESM4
{
    class RecordReader;

#pragma pack(push, 1)
    /// DEST: how much damage a destructible object takes, and how many stages it has.
    struct DestructionHeader
    {
        std::int32_t mHealth = 0;
        std::uint8_t mStageCount = 0;
        std::uint8_t mFlags = 0;
        std::uint8_t mUnused1 = 0;
        std::uint8_t mUnused2 = 0;
    };

    /// DSTD: what happens when the object has taken enough damage for a stage.
    struct DestructionStage
    {
        std::uint8_t mHealthPercent = 0;
        std::uint8_t mIndex = 0;
        std::uint8_t mDamageStage = 0;
        std::uint8_t mFlags = 0;
        std::int32_t mSelfDamagePerSecond = 0;
        ESM::FormId32 mExplosion = 0; // an EXPL
        ESM::FormId32 mDebris = 0; // a DEBR
        std::int32_t mDebrisCount = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(DestructionHeader) == 8);
    static_assert(sizeof(DestructionStage) == 20);

    /// The model that a destruction stage shows.
    struct DestructionStageModel
    {
        ESM::Path mModel; // DMDL
        std::vector<std::uint8_t> mTextures; // DMDT, texture hashes, not decoded
        std::vector<AlternateTexture> mAlternateTextures; // DMDS
    };

    /// The destruction data of a record: DEST, then for each stage DSTD with its optional models and DSTF.
    struct Destruction
    {
        bool mPresent = false; // DEST was read
        DestructionHeader mHeader;
        std::vector<DestructionStage> mStages;
        std::vector<DestructionStageModel> mStageModels; // one for each of mStages, empty when it has none
    };

    /// Reads the current sub-record if it is DEST, DSTD, DMDL, DMDT, DMDS or DSTF and returns true. Returns false and
    /// reads nothing for any other sub-record. A model before the first DSTD throws.
    bool readDestructionSubRecord(RecordReader& in, Destruction& destruction);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_DESTRUCTION_H
