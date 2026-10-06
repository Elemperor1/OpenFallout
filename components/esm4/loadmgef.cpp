#include "loadmgef.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void MagicEffect::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "MGEF");
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
                case ESM::fourCC("DESC"):
                    in.string(mDescription);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData, &Data::mAssociatedItem, &Data::mLight, &Data::mEffectShader,
                        &Data::mObjectDisplayShader, &Data::mEffectSound, &Data::mBoltSound, &Data::mHitSound,
                        &Data::mAreaSound);
                    break;
                case ESM::fourCC("ICON"):
                    in.string(mIcon);
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
