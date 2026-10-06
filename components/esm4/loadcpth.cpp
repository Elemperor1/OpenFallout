#include "loadcpth.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void CameraPath::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CPTH");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("CTDA"):
                    in.condition(mConditions.emplace_back());
                    break;
                case ESM::fourCC("ANAM"):
                    in.value(mRelated, &Related::mParent, &Related::mPrevious);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mZoom);
                    break;
                case ESM::fourCC("SNAM"):
                    in.formId(mCameraShots.emplace_back());
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
