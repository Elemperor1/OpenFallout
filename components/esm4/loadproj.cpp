#include "loadproj.hpp"

#include <cstring>

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    namespace
    {
        template <class T>
        T field(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            T value{};
            if (offset + sizeof(T) <= bytes.size())
                std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return value;
        }

        ESM::FormId formId(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            return ESM::FormId::fromUint32(field<std::uint32_t>(bytes, offset));
        }
    }

    Projectile::GameData Projectile::gameData() const
    {
        GameData data;
        data.mFlags = field<std::uint16_t>(mData, 0);
        data.mType = field<std::uint16_t>(mData, 2);
        data.mGravity = field<float>(mData, 4);
        data.mSpeed = field<float>(mData, 8);
        data.mRange = field<float>(mData, 12);
        data.mLight = formId(mData, 16);
        data.mMuzzleFlashLight = formId(mData, 20);
        data.mTracerChance = field<float>(mData, 24);
        data.mProximityFuse = field<float>(mData, 28);
        data.mFuseTimer = field<float>(mData, 32);
        data.mExplosion = formId(mData, 36);
        data.mSound = formId(mData, 40);
        data.mMuzzleFlashDuration = field<float>(mData, 44);
        data.mFadeDuration = field<float>(mData, 48);
        data.mImpactForce = field<float>(mData, 52);
        data.mCountdownSound = formId(mData, 56);
        data.mDisableSound = formId(mData, 60);
        data.mDefaultWeaponSource = formId(mData, 64);
        return data;
    }

    void Projectile::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "PROJ");
        // The model of the stage that the last DSTD began
        const auto stageModel = [&]() -> StageModel& {
            if (mStageModels.empty())
                in.fail(std::string(ESM::printName(in.type())) + " comes before DSTD");
            return mStageModels.back();
        };
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("OBND"):
                    in.value(mBounds);
                    break;
                case ESM::fourCC("FULL"):
                    in.string(mFullName);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("MODT"):
                    in.bytes(mModelTextures);
                    break;
                case ESM::fourCC("DEST"):
                    in.value(mDestruction);
                    break;
                case ESM::fourCC("DSTD"):
                    in.value(mStages.emplace_back(), &DestructionStage::mExplosion, &DestructionStage::mDebris);
                    mStageModels.emplace_back();
                    break;
                case ESM::fourCC("DMDL"):
                    in.path(stageModel().mModel);
                    break;
                case ESM::fourCC("DMDT"):
                    in.bytes(stageModel().mTextures);
                    break;
                case ESM::fourCC("DMDS"):
                    in.alternateTextures(stageModel().mAlternateTextures);
                    break;
                case ESM::fourCC("DSTF"):
                    in.expectSize(0);
                    break;
                case ESM::fourCC("DATA"):
                    in.bytes(mData, { 68, 80, 84 });
                    // The lights, the explosion, the sounds and the default weapon source.
                    in.adjustFormIds(mData, { 16, 20, 36, 40, 56, 60, 64 });
                    break;
                case ESM::fourCC("NAM1"):
                    in.string(mMuzzleFlashModel);
                    break;
                case ESM::fourCC("NAM2"):
                    in.bytes(mMuzzleFlashTextures);
                    break;
                case ESM::fourCC("VNAM"):
                    in.value(mSoundLevel);
                    break;
                case ESM::fourCC("MODB"):
                    in.value(mBoundRadius);
                    break;
                case ESM::fourCC("MODS"):
                    in.alternateTextures(mModelAlternateTextures);
                    break;
                case ESM::fourCC("MODD"):
                    in.value(mModelFlags);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
