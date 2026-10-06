#include "loadcdck.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void CaravanDeck::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CDCK");
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
                case ESM::fourCC("CARD"):
                    in.formId(mCards.emplace_back());
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
