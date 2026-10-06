#include "loaddebr.hpp"

#include <algorithm>

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Debris::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "DEBR");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DATA"):
                {
                    // A percentage, the path of the model with the zero that ends it, and a byte of flags.
                    std::vector<std::uint8_t> data;
                    in.bytes(data);
                    if (data.size() < 3 || data[data.size() - 2] != 0
                        || std::find(data.begin() + 1, data.end() - 2, 0) != data.end() - 2)
                        in.fail("DATA has an unexpected layout");
                    Model& model = mModels.emplace_back();
                    model.mPercentage = data.front();
                    model.mModel = std::string(data.begin() + 1, data.end() - 2);
                    model.mFlags = data.back();
                    break;
                }
                case ESM::fourCC("MODT"):
                    if (mModels.empty())
                        in.fail("MODT comes before DATA");
                    in.bytes(mModels.back().mTextures);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
