#include "loadlsct.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void LoadScreenType::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "LSCT");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
