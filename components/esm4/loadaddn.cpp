#include "loadaddn.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void AddonNode::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "ADDN");
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
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("MODT"):
                    in.bytes(mModelTextures);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mNodeIndex);
                    break;
                case ESM::fourCC("SNAM"):
                    in.formId(mSound);
                    break;
                case ESM::fourCC("DNAM"):
                    in.value(mParticleData);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
