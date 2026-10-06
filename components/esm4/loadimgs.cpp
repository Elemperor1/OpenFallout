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
                    in.bytes(mData, { 132, 136, 140, 144, 148, 149, 152 });
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
