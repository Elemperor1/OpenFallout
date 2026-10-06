#include "loadchal.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Challenge::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CHAL");
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
                case ESM::fourCC("SCRI"):
                    in.formId(mScript);
                    break;
                case ESM::fourCC("DESC"):
                    in.string(mDescription);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData);
                    break;
                case ESM::fourCC("SNAM"):
                    in.formId(mSnam);
                    break;
                case ESM::fourCC("XNAM"):
                    in.formId(mXnam);
                    break;
                case ESM::fourCC("ICON"):
                    in.string(mIcon);
                    break;
                case ESM::fourCC("MICO"):
                    in.string(mSmallIcon);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
