#include "loadperk.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    namespace
    {
#pragma pack(push, 1)
        // DATA of an entry that sets a quest stage.
        struct QuestStageData
        {
            ESM::FormId32 mQuest = 0;
            std::uint16_t mStage = 0;
            std::array<std::uint8_t, 2> mUnused{};
        };
#pragma pack(pop)
        static_assert(sizeof(QuestStageData) == 8);
    }

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
                        break;
                    }
                    Entry& entry = mEntries.back();
                    switch (entry.mType)
                    {
                        case 0:
                        {
                            QuestStageData data;
                            in.value(data, &QuestStageData::mQuest);
                            entry.mQuest = ESM::FormId::fromUint32(data.mQuest);
                            entry.mQuestStage = data.mStage;
                            break;
                        }
                        case 1:
                            in.formId(entry.mAbility);
                            break;
                        case 2:
                        {
                            std::array<std::uint8_t, 3> data;
                            in.value(data);
                            entry.mEntryPoint = data[0];
                            entry.mFunction = data[1];
                            entry.mTabCount = data[2];
                            break;
                        }
                        default:
                            in.bytes(entry.mData, { 3, 4, 8 });
                    }
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
                    std::int8_t runOn;
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
                    if (mEntries.empty())
                        in.fail("EPFD comes before PRKE");
                    Entry& entry = mEntries.back();
                    // EPFT comes before EPFD and says what it holds. A type that the format reference does not list
                    // can hold anything.
                    if (entry.mFunctionType == 3)
                    {
                        in.formId(entry.mLeveledItem);
                        break;
                    }
                    switch (entry.mFunctionType)
                    {
                        case 1:
                            in.expectSize(4);
                            break;
                        case 2:
                        case 5:
                            in.expectSize(8);
                            break;
                        case 4:
                            in.expectSize(0);
                            break;
                        default:
                            break;
                    }
                    in.bytes(entry.mFunctionData);
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
                case ESM::fourCC("MICO"):
                    in.string(mSmallIcon);
                    break;
                default:
                    // SCHR, SCDA, SCTX, SLSD, SCVR, SCRO and SCRV, a script that an entry runs.
                    if (!(mEntries.empty() ? mScript : mEntries.back().mScript).loadSubRecord(reader))
                        in.unknown();
            }
        }
        in.finish();
    }
}
