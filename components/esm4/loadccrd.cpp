#include "loadccrd.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void CaravanCard::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CCRD");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("OBND"):
                    in.value(mBounds);
                    break;
                case ESM::fourCC("FULL"):
                    in.string(mFullName);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("ICON"):
                    in.string(mIcon);
                    break;
                case ESM::fourCC("SCRI"):
                    in.formId(mScript);
                    break;
                case ESM::fourCC("TX00"):
                    in.string(mFrontFace);
                    break;
                case ESM::fourCC("TX01"):
                    in.string(mBackFace);
                    break;
                case ESM::fourCC("INTV"):
                    in.value(mIntegers.emplace_back());
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mValue);
                    break;
                case ESM::fourCC("MICO"):
                    in.string(mSmallIcon);
                    break;
                case ESM::fourCC("MODB"):
                    in.value(mBoundRadius);
                    break;
                case ESM::fourCC("MODT"):
                    in.bytes(mModelTextures);
                    break;
                case ESM::fourCC("MODS"):
                    in.alternateTextures(mModelAlternateTextures);
                    break;
                case ESM::fourCC("MODD"):
                    in.value(mModelFlags);
                    break;
                case ESM::fourCC("YNAM"):
                    in.formId(mPickUpSound);
                    break;
                case ESM::fourCC("ZNAM"):
                    in.formId(mDropSound);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
