#include "loadrcpe.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Recipe::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "RCPE");
        // The list that the last RCIL or RCOD went to, which the RCQY that follows belongs to.
        std::vector<Item>* last = nullptr;
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
                case ESM::fourCC("CTDA"):
                    in.condition(mConditions.emplace_back());
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData, &Data::mCategory, &Data::mSubCategory);
                    break;
                case ESM::fourCC("RCIL"):
                    in.formId(mIngredients.emplace_back().mItem);
                    last = &mIngredients;
                    break;
                case ESM::fourCC("RCOD"):
                    in.formId(mOutputs.emplace_back().mItem);
                    last = &mOutputs;
                    break;
                case ESM::fourCC("RCQY"):
                {
                    std::uint32_t count;
                    in.value(count);
                    if (last == nullptr)
                        in.fail("RCQY comes before RCIL or RCOD");
                    last->back().mCount = count;
                    break;
                }
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
