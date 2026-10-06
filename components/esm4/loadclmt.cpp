#include "loadclmt.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Climate::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "CLMT");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("WLST"):
                    in.values(mWeathers, &WeatherEntry::mWeather, &WeatherEntry::mGlobal);
                    break;
                case ESM::fourCC("FNAM"):
                    in.string(mSunTexture);
                    break;
                case ESM::fourCC("GNAM"):
                    in.string(mSunGlareTexture);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("TNAM"):
                    in.value(mTiming);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
