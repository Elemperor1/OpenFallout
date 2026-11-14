#include "destruction.hpp"

#include "reader.hpp"
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

    bool readDestructionSubRecord(Reader& reader, Destruction& destruction)
    {
        const SubRecordHeader& header = reader.subRecordHeader();
        switch (header.typeId)
        {
            case ESM::fourCC("DEST"):
                if (header.dataSize != sizeof(DestructionHeader))
                    reader.skipSubRecordData();
                else
                {
                    destruction.mPresent = true;
                    reader.get(destruction.mHeader);
                }
                return true;
            case ESM::fourCC("DSTD"):
                if (header.dataSize != sizeof(DestructionStage))
                    reader.skipSubRecordData();
                else
                {
                    DestructionStage stage;
                    reader.get(stage);
                    reader.adjustFormId(stage.mExplosion);
                    reader.adjustFormId(stage.mDebris);
                    destruction.mStages.push_back(stage);
                    destruction.mStageModels.emplace_back();
                }
                return true;
            case ESM::fourCC("DMDL"):
                if (destruction.mStageModels.empty())
                    reader.skipSubRecordData();
                else
                    reader.getZString(destruction.mStageModels.back().mModel);
                return true;
            case ESM::fourCC("DMDT"):
            case ESM::fourCC("DMDS"):
            case ESM::fourCC("DSTF"):
                reader.skipSubRecordData();
                return true;
            default:
                return false;
        }
    }
}
