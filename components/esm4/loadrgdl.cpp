#include "loadrgdl.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Ragdoll::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "RGDL");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("NVER"):
                    in.value(mVersion);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData);
                    break;
                case ESM::fourCC("XNAM"):
                    in.formId(mActorBase);
                    break;
                case ESM::fourCC("TNAM"):
                    in.formId(mBodyPartData);
                    break;
                case ESM::fourCC("RAFD"):
                    in.value(mFeedbackData);
                    break;
                case ESM::fourCC("RAFB"):
                    in.bytes(mFeedbackDynamicBones);
                    break;
                case ESM::fourCC("RAPS"):
                    in.value(mPoseMatching);
                    break;
                case ESM::fourCC("ANAM"):
                    in.string(mDeathPose);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
