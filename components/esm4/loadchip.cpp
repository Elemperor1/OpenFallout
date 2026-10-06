#include "loadchip.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void PokerChip::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CHIP");
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
                case ESM::fourCC("ICON"):
                    in.string(mIcon);
                    break;
                case ESM::fourCC("YNAM"):
                    in.formId(mPickUpSound);
                    break;
                case ESM::fourCC("ZNAM"):
                    in.formId(mDropSound);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
