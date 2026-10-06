#include "loadperk.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    void Perk::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "PERK");
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
                case ESM::fourCC("ICON"):
                    in.string(mIcon);
                    break;
                case ESM::fourCC("CTDA"):
                {
                    // The conditions of the perk come before its first entry, the ones after it belong to a group of
                    // the entry.
                    TargetCondition condition;
                    in.condition(condition);
                    if (mEntries.empty())
                        mConditions.push_back(condition);
                    else
                    {
                        Entry& entry = mEntries.back();
                        if (entry.mConditionGroups.empty())
                            entry.mConditionGroups.emplace_back();
                        entry.mConditionGroups.back().mConditions.push_back(condition);
                    }
                    break;
                }
                case ESM::fourCC("DATA"):
                {
                    // The data of the perk come before its first entry and have 3 to 5 bytes, the ones after it are the
                    // data of that entry.
                    if (mEntries.empty())
                    {
                        std::vector<std::uint8_t> data;
                        in.bytes(data, { 3, 4, 5 });
                        mData.mTrait = data[0];
                        mData.mMinimumLevel = data[1];
                        mData.mRanks = data[2];
                        mData.mPlayable = data.size() > 3 ? data[3] : 0;
                        mData.mHidden = data.size() > 4 ? data[4] : 0;
                    }
                    else
                        in.bytes(mEntries.back().mData, { 3, 4, 8 });
                    break;
                }
                case ESM::fourCC("PRKE"):
                {
                    std::array<std::uint8_t, 3> header;
                    in.value(header);
                    Entry& entry = mEntries.emplace_back();
                    entry.mType = header[0];
                    entry.mRank = header[1];
                    entry.mPriority = header[2];
                    break;
                }
                case ESM::fourCC("PRKC"):
                {
                    std::uint8_t runOn;
                    in.value(runOn);
                    if (mEntries.empty())
                        in.fail("PRKC comes before PRKE");
                    ConditionGroup& group = mEntries.back().mConditionGroups.emplace_back();
                    group.mHasRunOn = true;
                    group.mRunOn = runOn;
                    break;
                }
                case ESM::fourCC("PRKF"):
                    in.expectSize(0);
                    break;
                case ESM::fourCC("EPFT"):
                {
                    std::uint8_t type;
                    in.value(type);
                    if (mEntries.empty())
                        in.fail("EPFT comes before PRKE");
                    mEntries.back().mFunctionType = type;
                    break;
                }
                case ESM::fourCC("EPFD"):
                {
                    std::array<std::uint8_t, 4> data;
                    in.value(data);
                    if (mEntries.empty())
                        in.fail("EPFD comes before PRKE");
                    mEntries.back().mFunctionData = data;
                    break;
                }
                case ESM::fourCC("EPF2"):
                {
                    std::string text;
                    in.string(text);
                    if (mEntries.empty())
                        in.fail("EPF2 comes before PRKE");
                    mEntries.back().mFunctionText = std::move(text);
                    break;
                }
                case ESM::fourCC("EPF3"):
                {
                    std::array<std::uint8_t, 2> flags;
                    in.value(flags);
                    if (mEntries.empty())
                        in.fail("EPF3 comes before PRKE");
                    mEntries.back().mButtonFlags = flags;
                    break;
                }
                default:
                    // SCHR, SCDA, SCTX, SLSD, SCVR, SCRO and SCRV, a script that an entry runs.
                    if (!(mEntries.empty() ? mScript : mEntries.back().mScript).loadSubRecord(reader))
                        in.unknown();
            }
        }
        in.finish();
    }
}
