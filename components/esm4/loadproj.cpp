#include "loadproj.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
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
                    in.bytes(mData, { 68, 84 });
                    break;
                case ESM::fourCC("NAM1"):
                    in.string(mMuzzleFlashModel);
                    break;
                case ESM::fourCC("NAM2"):
                    in.bytes(mMuzzleFlashTextures, { 48, 72, 96 });
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
