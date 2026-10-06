#include "loadcams.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void CameraShot::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CAMS");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("MODB"):
                    in.value(mBoundRadius);
                    break;
                case ESM::fourCC("DATA"):
                    in.bytes(mData, { 36, 40 });
                    break;
                case ESM::fourCC("MNAM"):
                    in.formId(mImageSpaceModifier);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
