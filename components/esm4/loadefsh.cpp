#include "loadefsh.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void EffectShader::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "EFSH");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("ICON"):
                    in.string(mFillTexture);
                    break;
                case ESM::fourCC("ICO2"):
                    in.string(mParticleTexture);
                    break;
                case ESM::fourCC("NAM7"):
                    in.string(mHolesTexture);
                    break;
                case ESM::fourCC("DATA"):
                    in.bytes(mData, { 224, 244, 248, 284, 300, 308 });
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
