#include "loadlscr.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void LoadScreen::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "LSCR");
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
                case ESM::fourCC("DESC"):
                    in.string(mDescription);
                    break;
                case ESM::fourCC("LNAM"):
                    in.value(mLocations.emplace_back(), &Location::mDirect, &Location::mIndirect);
                    break;
                case ESM::fourCC("WMI1"):
                    in.formId(mLoadScreenType);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
