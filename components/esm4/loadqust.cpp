/*
  Copyright (C) 2020-2021 cc9cii

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  cc9cii cc9c@iinet.net.au

  Much of the information on the data structures are based on the information
  from Tes4Mod:Mod_File_Format and Tes5Mod:File_Formats but also refined by
  trial & error.  See http://en.uesp.net/wiki for details.

*/
#include "loadqust.hpp"

#include <cstring>
#include <stdexcept>

#include "conditionparams.hpp"
#include "reader.hpp"
// #include "writer.hpp"

/// Load the current QUST record with stages and log entries, and the objectives of Fallout 3 and New Vegas, keeping
/// their conditions separate. Scripts outside stage log entries are skipped.
/// Throws on unknown subrecords or script loading errors.
void ESM4::Quest::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;

    // The sub-records of a quest do not say which part of it they belong to: INDX starts a stage, QSDT starts a log
    // entry in it, and the conditions, text and script that follow belong to that entry. QOBJ starts an objective,
    // and QSTA a target in it, which the conditions that follow belong to.
    enum class Section
    {
        Header,
        Stage,
        Objective
    };
    Section section = Section::Header;

    // The log entry that is being read, if there is one. A QSDT that cannot be read ends the entry before it, so
    // the sub-records that follow are not added to it.
    bool inLogEntry = false;
    auto currentLogEntry = [&]() -> QuestLogEntry* {
        if (!inLogEntry || section != Section::Stage || mStages.empty() || mStages.back().mLogEntries.empty())
            return nullptr;
        return &mStages.back().mLogEntries.back();
    };

    // The objective and the target that are being read, if there are any. A QOBJ or QSTA that is not in the form of
    // Fallout 3 and New Vegas (Skyrim has others of the same names) does not start one.
    bool inObjective = false;
    bool inTarget = false;
    auto currentObjective = [&]() -> QuestObjective* {
        if (!inObjective || section != Section::Objective || mObjectives.empty())
            return nullptr;
        return &mObjectives.back();
    };
    auto currentTarget = [&]() -> QuestTarget* {
        QuestObjective* objective = currentObjective();
        if (objective == nullptr || !inTarget || objective->mTargets.empty())
            return nullptr;
        return &objective->mTargets.back();
    };

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        QuestLogEntry* logEntry = currentLogEntry();
        if (logEntry != nullptr && logEntry->mScript.loadSubRecord(reader))
            continue;

        switch (subHdr.typeId)
        {
            case ESM::fourCC("EDID"):
                reader.getZString(mEditorId);
                break;
            case ESM::fourCC("FULL"):
                reader.getZString(mQuestName);
                break;
            case ESM::fourCC("ICON"):
                reader.getZString(mFileName);
                break; // TES4 (none in FO3/FONV)
            case ESM::fourCC("DATA"):
            {
                if (subHdr.dataSize == 2) // TES4
                {
                    reader.get(&mData, 2);
                    mData.questDelay = 0.f; // unused in TES4 but keep it clean

                    // if ((mData.flags & Flag_StartGameEnabled) != 0)
                    // std::cout << "start quest " << mEditorId << std::endl;
                }
                else
                    reader.get(mData); // FO3

                break;
            }
            case ESM::fourCC("SCRI"):
                reader.getFormId(mQuestScript);
                break;
            case ESM::fourCC("CTDA"): // FIXME: how to detect if 1st/2nd param is a formid?
            {
                std::vector<TargetCondition>* conditions = &mTargetConditions;
                if (section == Section::Stage)
                    conditions = logEntry != nullptr ? &logEntry->mTargetConditions : nullptr;
                else if (section == Section::Objective)
                {
                    QuestTarget* target = currentTarget();
                    conditions = target != nullptr ? &target->mTargetConditions : nullptr;
                }

                if (conditions == nullptr)
                    reader.skipSubRecordData();
                else if (subHdr.dataSize == 24) // TES4
                {
                    TargetCondition cond;
                    reader.get(&cond, 24);
                    cond.reference = 0; // unused in TES4 but keep it clean
                    conditions->push_back(cond);
                }
                else if (subHdr.dataSize == 28)
                {
                    TargetCondition cond;
                    reader.get(cond); // FO3/FONV
                    if (cond.reference)
                        reader.adjustFormId(cond.reference);
                    adjustConditionParameters(reader, cond);
                    adjustConditionComparison(reader, cond);
                    conditions->push_back(cond);
                }
                else
                {
                    // one record with size 20: EDID GenericSupMutBehemoth
                    reader.skipSubRecordData(); // FIXME
                }
                // FIXME: support TES5

                break;
            }
            case ESM::fourCC("INDX"):
            {
                section = Section::Stage;
                inLogEntry = false;
                QuestStage& stage = mStages.emplace_back();
                if (subHdr.dataSize >= sizeof(stage.mIndex))
                {
                    reader.get(stage.mIndex);
                    reader.skipSubRecordData(subHdr.dataSize - sizeof(stage.mIndex)); // TES5 has more
                }
                else
                    reader.skipSubRecordData();

                break;
            }
            case ESM::fourCC("QSDT"):
            {
                inLogEntry
                    = section == Section::Stage && !mStages.empty() && subHdr.dataSize == sizeof(QuestLogEntry::mFlags);
                if (inLogEntry)
                    reader.get(mStages.back().mLogEntries.emplace_back().mFlags);
                else
                    reader.skipSubRecordData();

                break;
            }
            case ESM::fourCC("CNAM"):
                if (logEntry != nullptr)
                    reader.getLocalizedString(logEntry->mText);
                else
                    reader.skipSubRecordData();

                break;
            case ESM::fourCC("NAM0"): // FO3
                if (logEntry != nullptr && subHdr.dataSize == sizeof(ESM::FormId32))
                    reader.getFormId(logEntry->mNextQuest);
                else
                    reader.skipSubRecordData();

                break;
            case ESM::fourCC("QOBJ"):
            {
                section = Section::Objective;
                inTarget = false;
                inObjective = subHdr.dataSize == sizeof(QuestObjective::mIndex);
                if (inObjective)
                    reader.get(mObjectives.emplace_back().mIndex);
                else
                    reader.skipSubRecordData();

                break;
            }
            case ESM::fourCC("NNAM"): // FO3
            {
                QuestObjective* objective = currentObjective();
                if (objective != nullptr)
                    reader.getLocalizedString(objective->mText);
                else
                    reader.skipSubRecordData();

                break;
            }
            case ESM::fourCC("QSTA"):
            {
                QuestObjective* objective = currentObjective();
                inTarget = objective != nullptr && subHdr.dataSize == sizeof(ESM::FormId32) + 4;
                if (inTarget)
                {
                    QuestTarget& target = objective->mTargets.emplace_back();
                    reader.getFormId(target.mTarget);
                    reader.get(target.mFlags);
                    reader.skipSubRecordData(3); // unused
                }
                else
                    reader.skipSubRecordData();

                break;
            }
            // The script of a stage is read above. Any other script sub-record has no log entry to go to.
            case ESM::fourCC("SCHR"):
            case ESM::fourCC("SCDA"):
            case ESM::fourCC("SCTX"):
            case ESM::fourCC("SCRO"):
            case ESM::fourCC("SLSD"):
            case ESM::fourCC("SCVR"):
            case ESM::fourCC("SCRV"):
            case ESM::fourCC("MICO"):
            case ESM::fourCC("ANAM"): // TES5
            case ESM::fourCC("DNAM"): // TES5
            case ESM::fourCC("ENAM"): // TES5
            case ESM::fourCC("FNAM"): // TES5
            case ESM::fourCC("NEXT"): // TES5
            case ESM::fourCC("ALCA"): // TES5
            case ESM::fourCC("ALCL"): // TES5
            case ESM::fourCC("ALCO"): // TES5
            case ESM::fourCC("ALDN"): // TES5
            case ESM::fourCC("ALEA"): // TES5
            case ESM::fourCC("ALED"): // TES5
            case ESM::fourCC("ALEQ"): // TES5
            case ESM::fourCC("ALFA"): // TES5
            case ESM::fourCC("ALFC"): // TES5
            case ESM::fourCC("ALFD"): // TES5
            case ESM::fourCC("ALFE"): // TES5
            case ESM::fourCC("ALFI"): // TES5
            case ESM::fourCC("ALFL"): // TES5
            case ESM::fourCC("ALFR"): // TES5
            case ESM::fourCC("ALID"): // TES5
            case ESM::fourCC("ALLS"): // TES5
            case ESM::fourCC("ALNA"): // TES5
            case ESM::fourCC("ALNT"): // TES5
            case ESM::fourCC("ALPC"): // TES5
            case ESM::fourCC("ALRT"): // TES5
            case ESM::fourCC("ALSP"): // TES5
            case ESM::fourCC("ALST"): // TES5
            case ESM::fourCC("ALUA"): // TES5
            case ESM::fourCC("CIS1"): // TES5
            case ESM::fourCC("CIS2"): // TES5
            case ESM::fourCC("CNTO"): // TES5
            case ESM::fourCC("COCT"): // TES5
            case ESM::fourCC("ECOR"): // TES5
            case ESM::fourCC("FLTR"): // TES5
            case ESM::fourCC("KNAM"): // TES5
            case ESM::fourCC("KSIZ"): // TES5
            case ESM::fourCC("KWDA"): // TES5
            case ESM::fourCC("QNAM"): // TES5
            case ESM::fourCC("QTGL"): // TES5
            case ESM::fourCC("SPOR"): // TES5
            case ESM::fourCC("VMAD"): // TES5
            case ESM::fourCC("VTCK"): // TES5
            case ESM::fourCC("ALCC"): // FO4
            case ESM::fourCC("ALCS"): // FO4
            case ESM::fourCC("ALDI"): // FO4
            case ESM::fourCC("ALFV"): // FO4
            case ESM::fourCC("ALLA"): // FO4
            case ESM::fourCC("ALMI"): // FO4
            case ESM::fourCC("GNAM"): // FO4
            case ESM::fourCC("GWOR"): // FO4
            case ESM::fourCC("LNAM"): // FO4
            case ESM::fourCC("NAM2"): // FO4
            case ESM::fourCC("OCOR"): // FO4
            case ESM::fourCC("SNAM"): // FO4
            case ESM::fourCC("XNAM"): // FO4
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::QUST::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
    // if (mEditorId == "DAConversations")
    // std::cout << mEditorId << std::endl;
}

// void ESM4::Quest::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::Quest::blank()
//{
// }
