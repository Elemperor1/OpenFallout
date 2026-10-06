#include "loadwthr.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Weather::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "WTHR");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("\000IAD"):
                    in.formId(mImageSpaceModifiers[0]);
                    break;
                case ESM::fourCC("\001IAD"):
                    in.formId(mImageSpaceModifiers[1]);
                    break;
                case ESM::fourCC("\002IAD"):
                    in.formId(mImageSpaceModifiers[2]);
                    break;
                case ESM::fourCC("\003IAD"):
                    in.formId(mImageSpaceModifiers[3]);
                    break;
                case ESM::fourCC("\004IAD"):
                    in.formId(mImageSpaceModifiers[4]);
                    break;
                case ESM::fourCC("\005IAD"):
                    in.formId(mImageSpaceModifiers[5]);
                    break;
                case ESM::fourCC("DNAM"):
                    in.string(mCloudTextures[0]);
                    break;
                case ESM::fourCC("CNAM"):
                    in.string(mCloudTextures[1]);
                    break;
                case ESM::fourCC("ANAM"):
                    in.string(mCloudTextures[2]);
                    break;
                case ESM::fourCC("BNAM"):
                    in.string(mCloudTextures[3]);
                    break;
                case ESM::fourCC("LNAM"):
                    in.value(mLnam);
                    break;
                case ESM::fourCC("ONAM"):
                    in.value(mCloudSpeeds);
                    break;
                case ESM::fourCC("PNAM"):
                    in.bytes(mCloudColours, { 64, 96 });
                    break;
                case ESM::fourCC("NAM0"):
                    in.bytes(mColours, { 160, 240 });
                    break;
                case ESM::fourCC("FNAM"):
                    in.value(mFog);
                    break;
                case ESM::fourCC("INAM"):
                    in.value(mInam);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData);
                    break;
                case ESM::fourCC("SNAM"):
                    in.value(mSounds.emplace_back(), &WeatherSound::mSound);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
