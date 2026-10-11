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
#include "loadinfo.hpp"

#include <cstring>
#include <stdexcept>

#include <components/esm/refid.hpp>

#include "grouptype.hpp"
#include "reader.hpp"
#include "recordreader.hpp"
// #include "writer.hpp"

namespace
{
    // TRDT of Fallout 3 and New Vegas: the emotion and its value, four unused bytes, the number of the response and
    // three unused bytes, then the sound (not in 2230 of the 38023 responses of Fallout 3, which keep the form of
    // Oblivion), and in New Vegas and most Fallout 3 records the flags with three unused bytes.
    constexpr std::size_t responseSoundOffset = 16;
    constexpr std::uint32_t responseSizeWithoutSound = 16;
    constexpr std::uint32_t responseSizeWithoutFlags = 20;
    constexpr std::uint32_t responseSizeWithFlags = 24;
}

void ESM4::DialogInfo::loadFallout(ESM4::Reader& reader)
{
    // A NEXT sub-record ends the first script and starts the second one.
    bool inEndScript = false;

    RecordReader in(reader, "INFO");
    while (in.next())
    {
        if ((inEndScript ? mEndScript : mScript).loadSubRecord(reader))
            continue;

        // The sub-records of a response follow its TRDT; one that comes before any is the first response's
        const auto currentResponse = [this]() -> DialogResponse& {
            if (mResponses.empty())
                mResponses.emplace_back();
            return mResponses.back();
        };

        switch (in.type())
        {
            case ESM::fourCC("QSTI"):
                in.formId(mQuest);
                break;
            case ESM::fourCC("TPIC"):
            {
                // The group the response is in says which topic it is for; TPIC is for a record outside of one
                ESM::FormId topic;
                in.formId(topic);
                if (mTopic.isZeroOrUnset())
                    mTopic = topic;
                break;
            }
            case ESM::fourCC("PNAM"):
                in.formId(mPreviousInfo);
                break;
            case ESM::fourCC("NAME"):
                in.formId(mAddTopics.emplace_back());
                break;
            case ESM::fourCC("SNDD"):
                in.formId(mSound);
                break;
            case ESM::fourCC("TRDT"):
            {
                std::vector<std::uint8_t> data;
                in.bytes(data, { responseSizeWithoutSound, responseSizeWithoutFlags, responseSizeWithFlags });
                DialogResponse& response = mResponses.emplace_back();
                std::memcpy(&response.mEmotionType, data.data(), sizeof(response.mEmotionType));
                std::memcpy(&response.mEmotionValue, data.data() + 4, sizeof(response.mEmotionValue));
                response.mNumber = data[12];
                if (data.size() >= responseSizeWithoutFlags)
                {
                    in.adjustFormIds(data, { responseSoundOffset });
                    std::uint32_t sound = 0;
                    std::memcpy(&sound, data.data() + responseSoundOffset, sizeof(sound));
                    response.mSound = ESM::FormId::fromUint32(sound);
                }
                if (data.size() >= responseSizeWithFlags)
                    response.mFlags = data[20];
                break;
            }
            case ESM::fourCC("NAM1"):
                in.string(currentResponse().mText);
                break;
            case ESM::fourCC("NAM2"):
                in.string(currentResponse().mNotes);
                break;
            case ESM::fourCC("NAM3"):
                in.string(currentResponse().mEdits);
                break;
            case ESM::fourCC("SNAM"):
                in.formId(currentResponse().mSpeakerAnimation);
                break;
            case ESM::fourCC("LNAM"):
                in.formId(currentResponse().mListenerAnimation);
                break;
            case ESM::fourCC("CTDA"):
                in.condition(mConditions.emplace_back());
                break;
            case ESM::fourCC("TCLT"):
                in.formId(mChoices.emplace_back());
                break;
            case ESM::fourCC("TCLF"):
                in.formId(mLinkedFrom.emplace_back());
                break;
            case ESM::fourCC("TCFU"):
                in.formId(mFollowUps.emplace_back());
                break;
            case ESM::fourCC("NEXT"):
                in.expectSize(0);
                inEndScript = true;
                break;
            case ESM::fourCC("RNAM"):
                in.string(mPrompt);
                break;
            case ESM::fourCC("ANAM"):
                in.formId(mSpeaker);
                break;
            case ESM::fourCC("KNAM"):
                in.formId(mChallengeStat);
                break;
            case ESM::fourCC("DNAM"):
                in.value(mChallenge);
                break;
            case ESM::fourCC("DATA"):
            {
                std::vector<std::uint8_t> data;
                // Some Fallout 3 responses have one byte of flags, the others two
                in.bytes(data, { 3, 4 });
                mDialType = data[0];
                mNextSpeaker = data[1];
                mInfoFlags = data[2];
                if (data.size() > 3)
                    mInfoFlags = static_cast<std::uint16_t>(mInfoFlags | (data[3] << 8));
                break;
            }
            default:
                in.unknown();
        }
    }
    in.finish();
}

/// Load the current INFO record, retaining separate response begin and end scripts.
/// Throws on unknown subrecords or script loading errors.
void ESM4::DialogInfo::load(ESM4::Reader& reader)
{
    mId = reader.getFormIdFromHeader();
    mFlags = reader.hdr().record.flags;

    mEditorId = ESM::RefId(mId).serializeText(); // FIXME: quick workaround to use existing code

    // The topic is the one whose group the response is in
    if (reader.stackSize() > 0 && reader.grp().type == Grp_TopicChild)
    {
        ESM::FormId topic = ESM::FormId::fromUint32(reader.grp().label.value);
        mTopic = topic;
    }

    if (reader.isFalloutFile())
    {
        loadFallout(reader);
        if (!mResponses.empty())
        {
            const DialogResponse& first = mResponses.front();
            mResponse = first.mText;
            mNotes = first.mNotes;
            mEdits = first.mEdits;
            mResponseData.emoType = first.mEmotionType;
            mResponseData.emoValue = first.mEmotionValue;
            mResponseData.responseNo = first.mNumber;
            mResponseData.sound = first.mSound.toUint32();
            mResponseData.flags = first.mFlags;
        }
        return;
    }

    // A NEXT sub-record ends the first script and starts the second one.
    bool inEndScript = false;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        if ((inEndScript ? mEndScript : mScript).loadSubRecord(reader))
            continue;

        switch (subHdr.typeId)
        {
            case ESM::fourCC("QSTI"):
                reader.getFormId(mQuest);
                break; // FormId quest id
            case ESM::fourCC("SNDD"):
                reader.getFormId(mSound);
                break; // FO3 (not used in FONV?)
            case ESM::fourCC("TRDT"):
            {
                if (subHdr.dataSize == 16) // TES4
                    reader.get(&mResponseData, 16);
                else if (subHdr.dataSize == 20) // FO3
                    reader.get(&mResponseData, 20);
                else // FO3/FONV
                {
                    reader.get(mResponseData);
                    if (mResponseData.sound)
                        reader.adjustFormId(mResponseData.sound);
                }

                break;
            }
            case ESM::fourCC("NAM1"):
                reader.getLocalizedString(mResponse);
                break; // response text
            case ESM::fourCC("NAM2"):
                reader.getZString(mNotes);
                break; // actor notes
            case ESM::fourCC("NAM3"):
                reader.getZString(mEdits);
                break; // not in TES4
            case ESM::fourCC("CTDA"): // FIXME: how to detect if 1st/2nd param is a formid?
            {
                TargetCondition& condition = mConditions.emplace_back();
                if (subHdr.dataSize == 24) // TES4
                    reader.get(&condition, 24);
                else if (subHdr.dataSize == 20) // FO3
                    reader.get(&condition, 20);
                else if (subHdr.dataSize == 28)
                {
                    reader.get(condition); // FO3/FONV
                    if (condition.reference)
                        reader.adjustFormId(condition.reference);
                }
                else // TES5
                {
                    reader.get(&condition, 20);
                    if (subHdr.dataSize == 36)
                        reader.getFormId(mParam3);
                    reader.get(condition.runOn);
                    reader.get(condition.reference);
                    if (condition.reference)
                        reader.adjustFormId(condition.reference);
                    reader.skipSubRecordData(4); // unknown
                }

                break;
            }
            case ESM::fourCC("NEXT"): // FO3/FONV marker for next script header
            {
                inEndScript = true;

                break;
            }
            case ESM::fourCC("DATA"): // always 3 for TES4 ?
            {
                if (subHdr.dataSize == 4) // FO3/FONV
                {
                    reader.get(mDialType);
                    reader.get(mNextSpeaker);
                    reader.get(mInfoFlags);
                }
                else
                    reader.skipSubRecordData(); // FIXME
                break;
            }
            case ESM::fourCC("NAME"): // FormId add topic (not always present)
            case ESM::fourCC("CTDT"): // older version of CTDA? 20 bytes
            case ESM::fourCC("SCHD"): // 28 bytes
            case ESM::fourCC("TCLT"): // FormId choice
            case ESM::fourCC("TCLF"): // FormId
            case ESM::fourCC("PNAM"): // TES4 DLC
            case ESM::fourCC("TPIC"): // TES4 DLC
            case ESM::fourCC("ANAM"): // FO3 speaker formid
            case ESM::fourCC("DNAM"): // FO3 speech challenge
            case ESM::fourCC("KNAM"): // FO3 formid
            case ESM::fourCC("LNAM"): // FONV
            case ESM::fourCC("TCFU"): // FONV
            case ESM::fourCC("TIFC"): // TES5
            case ESM::fourCC("TWAT"): // TES5
            case ESM::fourCC("CIS1"): // TES5
            case ESM::fourCC("CIS2"): // TES5
            case ESM::fourCC("CNAM"): // TES5
            case ESM::fourCC("ENAM"): // TES5
            case ESM::fourCC("EDID"): // TES5
            case ESM::fourCC("VMAD"): // TES5
            case ESM::fourCC("BNAM"): // TES5
            case ESM::fourCC("SNAM"): // TES5
            case ESM::fourCC("ONAM"): // TES5
            case ESM::fourCC("QNAM"): // TES5 for mScript
            case ESM::fourCC("RNAM"): // TES5
            case ESM::fourCC("ALFA"): // FO4
            case ESM::fourCC("GNAM"): // FO4
            case ESM::fourCC("GREE"): // FO4
            case ESM::fourCC("INAM"): // FO4
            case ESM::fourCC("INCC"): // FO4
            case ESM::fourCC("INTV"): // FO4
            case ESM::fourCC("IOVR"): // FO4
            case ESM::fourCC("MODQ"): // FO4
            case ESM::fourCC("NAM0"): // FO4
            case ESM::fourCC("NAM4"): // FO4
            case ESM::fourCC("NAM9"): // FO4
            case ESM::fourCC("SRAF"): // FO4
            case ESM::fourCC("TIQS"): // FO4
            case ESM::fourCC("TNAM"): // FO4
            case ESM::fourCC("TRDA"): // FO4
            case ESM::fourCC("TSCE"): // FO4
            case ESM::fourCC("WZMD"): // FO4
                reader.skipSubRecordData();
                break;
            default:
                throw std::runtime_error("ESM4::INFO::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }

    // A record of another game says one thing
    if (!mResponse.empty())
    {
        DialogResponse& response = mResponses.emplace_back();
        response.mEmotionType = mResponseData.emoType;
        response.mEmotionValue = mResponseData.emoValue;
        response.mNumber = static_cast<std::uint8_t>(mResponseData.responseNo);
        response.mSound = ESM::FormId::fromUint32(mResponseData.sound);
        response.mFlags = static_cast<std::uint8_t>(mResponseData.flags);
        response.mText = mResponse;
        response.mNotes = mNotes;
        response.mEdits = mEdits;
    }
}

// void ESM4::DialogInfo::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::DialogInfo::blank()
//{
// }
