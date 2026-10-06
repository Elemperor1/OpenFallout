#include "loadimgs.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void ImageSpace::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "IMGS");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DNAM"):
                    in.bytesBetween(mData, 132, 152);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
