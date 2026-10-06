#include "loadipct.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void ImpactData::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "IPCT");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("MODT"):
                    in.bytes(mModelTextures);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData);
                    break;
                case ESM::fourCC("DODT"):
                    in.value(mDecalData);
                    break;
                case ESM::fourCC("DNAM"):
                    in.formId(mTextureSet);
                    break;
                case ESM::fourCC("SNAM"):
                    in.formId(mSound1);
                    break;
                case ESM::fourCC("NAM1"):
                    in.formId(mSound2);
                    break;
                case ESM::fourCC("MODB"):
                    in.value(mBoundRadius);
                    break;
                case ESM::fourCC("MODS"):
                    in.bytes(mModelAlternateTextures);
                    break;
                case ESM::fourCC("MODD"):
                    in.value(mModelFlags);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
