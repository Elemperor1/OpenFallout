#include "loadmicn.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void MenuIcon::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "MICN");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
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
