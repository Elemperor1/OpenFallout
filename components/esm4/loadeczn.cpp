#include "loadeczn.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void EncounterZone::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "ECZN");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData, &Data::mOwner);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
