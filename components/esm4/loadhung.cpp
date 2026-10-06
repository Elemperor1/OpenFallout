#include "loadhung.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void HungerStage::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "HUNG");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData, &Data::mActorEffect);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
