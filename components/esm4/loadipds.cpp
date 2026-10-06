#include "loadipds.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void ImpactDataSet::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "IPDS");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DATA"):
                    in.formIds(mImpacts);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
