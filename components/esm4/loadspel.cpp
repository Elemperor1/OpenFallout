#include "loadspel.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Spell::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "SPEL");
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
                case ESM::fourCC("SPIT"):
                    in.value(mData);
                    break;
                default:
                    if (!readEffectSubRecord(in, mEffects, mConditions))
                        in.unknown();
            }
        }
        in.finish();
    }
}
