#include "loadmesg.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Message::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "MESG");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DESC"):
                    in.string(mDescription);
                    break;
                case ESM::fourCC("FULL"):
                    in.string(mFullName);
                    break;
                case ESM::fourCC("INAM"):
                    in.formId(mIcon);
                    break;
                case ESM::fourCC("NAM0"):
                case ESM::fourCC("NAM1"):
                case ESM::fourCC("NAM2"):
                case ESM::fourCC("NAM3"):
                case ESM::fourCC("NAM4"):
                case ESM::fourCC("NAM5"):
                case ESM::fourCC("NAM6"):
                case ESM::fourCC("NAM7"):
                case ESM::fourCC("NAM8"):
                case ESM::fourCC("NAM9"):
                    in.raw(mNams.emplace_back());
                    break;
                case ESM::fourCC("DNAM"):
                    in.value(mMessageFlags);
                    break;
                case ESM::fourCC("TNAM"):
                    in.value(mDisplayTime);
                    break;
                case ESM::fourCC("ITXT"):
                    in.string(mButtons.emplace_back().mText);
                    break;
                case ESM::fourCC("CTDA"):
                {
                    TargetCondition condition;
                    in.condition(condition);
                    (mButtons.empty() ? mConditions : mButtons.back().mConditions).push_back(condition);
                    break;
                }
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
