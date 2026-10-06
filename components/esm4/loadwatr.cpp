#include "loadwatr.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Water::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "WATR");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("FULL"):
                    in.string(mFullName);
                    break;
                case ESM::fourCC("NNAM"):
                    in.string(mNoiseTexture);
                    break;
                case ESM::fourCC("ANAM"):
                    in.value(mOpacity);
                    break;
                case ESM::fourCC("FNAM"):
                    in.value(mWaterFlags);
                    break;
                case ESM::fourCC("MNAM"):
                    in.value(mMnam);
                    break;
                case ESM::fourCC("SNAM"):
                    in.formId(mSound);
                    break;
                case ESM::fourCC("XNAM"):
                    in.formId(mActorEffect);
                    break;
                case ESM::fourCC("DATA"):
                    in.bytes(mData, { 2, 186 });
                    break;
                case ESM::fourCC("DNAM"):
                    in.bytes(mVisualData, { 184, 196 });
                    break;
                case ESM::fourCC("GNAM"):
                    in.value(mRelatedWaters, &RelatedWaters::mDaytime, &RelatedWaters::mNighttime,
                        &RelatedWaters::mUnderwater);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
