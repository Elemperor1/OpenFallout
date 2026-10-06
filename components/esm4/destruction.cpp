#include "destruction.hpp"

#include "recordreader.hpp"

namespace ESM4
{
    bool readDestructionSubRecord(RecordReader& in, Destruction& destruction)
    {
        // The model of the stage that the last DSTD began
        const auto stageModel = [&]() -> DestructionStageModel& {
            if (destruction.mStageModels.empty())
                in.fail(std::string(ESM::printName(in.type())) + " comes before DSTD");
            return destruction.mStageModels.back();
        };

        switch (in.type())
        {
            case ESM::fourCC("DEST"):
                destruction.mPresent = true;
                in.value(destruction.mHeader);
                return true;
            case ESM::fourCC("DSTD"):
                in.value(destruction.mStages.emplace_back(), &DestructionStage::mExplosion, &DestructionStage::mDebris);
                destruction.mStageModels.emplace_back();
                return true;
            case ESM::fourCC("DMDL"):
                in.path(stageModel().mModel);
                return true;
            case ESM::fourCC("DMDT"):
                in.bytes(stageModel().mTextures);
                return true;
            case ESM::fourCC("DMDS"):
                in.alternateTextures(stageModel().mAlternateTextures);
                return true;
            case ESM::fourCC("DSTF"):
                in.expectSize(0);
                return true;
            default:
                return false;
        }
    }
}
