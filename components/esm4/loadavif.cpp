#include "loadavif.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void ActorValueInfo::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "AVIF");
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
                case ESM::fourCC("DESC"):
                    in.string(mDescription);
                    break;
                case ESM::fourCC("ICON"):
                    in.string(mIcon);
                    break;
                case ESM::fourCC("ANAM"):
                    in.string(mShortName);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
