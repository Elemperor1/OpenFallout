#include "loadcsno.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Casino::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CSNO");
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
                case ESM::fourCC("DATA"):
                    in.value(mData, &Data::mCurrency, &Data::mWinningsQuest);
                    break;
                case ESM::fourCC("MODL"):
                    in.string(mModels.emplace_back());
                    break;
                case ESM::fourCC("MOD2"):
                    in.string(mModel2);
                    break;
                case ESM::fourCC("MOD3"):
                    in.string(mModel3);
                    break;
                case ESM::fourCC("MOD4"):
                    in.string(mModel4);
                    break;
                case ESM::fourCC("ICON"):
                    in.string(mIcons.emplace_back());
                    break;
                case ESM::fourCC("ICO2"):
                    in.string(mIcons2.emplace_back());
                    break;
                case ESM::fourCC("MODB"):
                case ESM::fourCC("MODT"):
                case ESM::fourCC("MODD"):
                case ESM::fourCC("MO2T"):
                case ESM::fourCC("MO3T"):
                case ESM::fourCC("MOSD"):
                case ESM::fourCC("MO4T"):
                    in.raw(mModelData.emplace_back());
                    break;
                case ESM::fourCC("MODS"):
                case ESM::fourCC("MO2S"):
                case ESM::fourCC("MO3S"):
                case ESM::fourCC("MO4S"):
                {
                    ModelTextures& model = mModelTextures.emplace_back();
                    model.mType = in.type();
                    in.alternateTextures(model.mTextures);
                    break;
                }
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
