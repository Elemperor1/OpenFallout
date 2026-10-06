#include "loadimad.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void ImageSpaceModifier::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "IMAD");
        // BNAM and a dozen other tracks have names of their own. The other 42 are a number or an at-sign or a letter,
        // followed by IAD, in pairs.
        const auto isTrack = [](std::uint32_t type) {
            switch (type)
            {
                case ESM::fourCC("BNAM"):
                case ESM::fourCC("VNAM"):
                case ESM::fourCC("TNAM"):
                case ESM::fourCC("NAM1"):
                case ESM::fourCC("NAM2"):
                case ESM::fourCC("NAM3"):
                case ESM::fourCC("NAM4"):
                case ESM::fourCC("RNAM"):
                case ESM::fourCC("SNAM"):
                case ESM::fourCC("UNAM"):
                case ESM::fourCC("WNAM"):
                case ESM::fourCC("XNAM"):
                case ESM::fourCC("YNAM"):
                    return true;
                default:
                    break;
            }
            const std::uint32_t first = type & 0xff;
            return (type >> 8) == (ESM::fourCC("\0IAD") >> 8) && (first <= 0x14 || (first >= 0x40 && first <= 0x54));
        };
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("DNAM"):
                    in.bytes(mData, { 188, 236, 240, 244 });
                    break;
                case ESM::fourCC("RDSD"):
                    in.formId(mRdsd);
                    break;
                case ESM::fourCC("RDSI"):
                    in.formId(mRdsi);
                    break;
                default:
                    if (!isTrack(in.type()))
                        in.unknown();
                    in.raw(mTracks.emplace_back());
            }
        }
        in.finish();
    }
}
