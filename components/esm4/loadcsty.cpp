#include "loadcsty.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void CombatStyle::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CSTY");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("CSTD"):
                    in.value(mStandard);
                    break;
                case ESM::fourCC("CSAD"):
                    in.value(mAdvanced);
                    break;
                case ESM::fourCC("CSSD"):
                    in.value(mSimple);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
