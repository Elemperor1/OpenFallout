#include "dialoguecensus.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <ostream>
#include <set>
#include <string>

#include "censusreading.hpp"
#include "common.hpp"
#include "dialogue.hpp"
#include "loaddial.hpp"
#include "loadinfo.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    using namespace CensusReading;

    namespace
    {
        // The run on of a condition that is the player
        constexpr std::uint32_t runOnTarget = 1;
        constexpr std::uint32_t runOnReference = 2;

        std::size_t capped(std::size_t count)
        {
            return std::min(count, DialogueCensus::maxCounted);
        }
    }

    std::string DialogueCensus::topicTypeName(int type)
    {
        static const char* const names[]
            = { "Topic", "Conversation", "Combat", "Persuasion", "Detection", "Service", "Miscellaneous", "Radio" };
        if (type >= 0 && static_cast<std::size_t>(type) < std::size(names))
            return names[type];
        return "type " + std::to_string(type);
    }

    std::string DialogueCensus::infoFlagName(int bit)
    {
        static const char* const names[] = { "Goodbye", "Random", "SayOnce", "RunImmediately", "InfoRefusal",
            "RandomEnd", "RunForRumors", "SpeechChallenge", "SayOnceADay", "AlwaysDarken", "bit 10", "bit 11",
            "LowIntelligence", "HighIntelligence" };
        if (bit >= 0 && static_cast<std::size_t>(bit) < std::size(names))
            return names[bit];
        return "bit " + std::to_string(bit);
    }

    std::string DialogueCensus::nextSpeakerName(int speaker)
    {
        static const char* const names[] = { "Target", "Self", "Either" };
        if (speaker >= 0 && static_cast<std::size_t>(speaker) < std::size(names))
            return names[speaker];
        return "speaker " + std::to_string(speaker);
    }

    void DialogueCensus::collect(Reader& reader)
    {
        const auto addError = [this](const std::string& type, const std::exception& e) {
            ++mUnreadable;
            if (mReadErrors.size() < maxReadErrors)
                mReadErrors.push_back(type + ": " + firstLine(e.what()));
        };

        auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            if (type != REC_DIAL && type != REC_INFO && type != REC_QUST)
                return false;

            const ESM::FormId id = r.getFormIdFromHeader();
            const std::uint32_t flags = r.hdr().record.flags;
            const ReaderContext recordStart = r.getContext();
            if (type == REC_QUST)
            {
                // Only that the quest exists matters here
                mQuests.erase(id);
                if ((flags & Rec_Deleted) == 0)
                    mQuests.insert(id);
                return false;
            }

            mTopics.erase(id);
            mResponses.erase(id);

            if ((flags & Rec_Deleted) == 0)
            {
                try
                {
                    r.getRecordData();
                    if (type == REC_DIAL)
                    {
                        Dialogue record;
                        record.load(r);
                        Topic& topic = mTopics[id];
                        topic.mType = record.mDialType;
                        topic.mFlags = record.mDialFlags;
                        topic.mQuests = record.mQuests.size();
                        topic.mSharedInfos = record.mSharedInfos.size();
                        topic.mNamed = !record.mTopicName.empty();
                        topic.mPriority = record.mPriority;
                    }
                    else
                    {
                        DialogInfo record;
                        record.load(r);
                        Response& response = mResponses[id];
                        response.mTopic = record.mTopic;
                        response.mQuest = record.mQuest;
                        response.mPrevious = record.mPreviousInfo;
                        response.mType = record.mDialType;
                        response.mNextSpeaker = record.mNextSpeaker;
                        response.mFlags = record.mInfoFlags;
                        response.mLines = record.mResponses.size();
                        for (const DialogResponse& line : record.mResponses)
                        {
                            response.mLinesNoText += line.mText.empty() ? 1 : 0;
                            response.mLinesWithSound += line.mSound.isZeroOrUnset() ? 0 : 1;
                            response.mLinesWithAnimation
                                += (line.mSpeakerAnimation.isZeroOrUnset() && line.mListenerAnimation.isZeroOrUnset())
                                ? 0
                                : 1;
                            response.mEmotions.push_back(line.mEmotionType);
                        }
                        response.mConditions = record.mConditions.size();
                        for (const TargetCondition& condition : record.mConditions)
                        {
                            response.mFunctions.push_back(condition.functionIndex);
                            std::uint32_t runOn = condition.runOn;
                            if ((condition.condition & CTF_RunOnTarget) != 0)
                                runOn = runOnTarget;
                            response.mRunOn.push_back(runOn);
                            response.mOr += (condition.condition & CTF_Combine) != 0 ? 1 : 0;
                            response.mGlobals += (condition.condition & CTF_UseGlobal) != 0 ? 1 : 0;
                        }
                        response.mChoices = record.mChoices;
                        response.mAddTopics = record.mAddTopics;
                        response.mLinkedFrom = record.mLinkedFrom.size();
                        response.mFollowUps = record.mFollowUps.size();
                        response.mPrompt = !record.mPrompt.empty();
                        response.mSpeaker = !record.mSpeaker.isZeroOrUnset();
                        response.mChallenge = (record.mInfoFlags & INFO_SpeechChallenge) != 0 || record.mChallenge != 0;
                        response.mChallengeLevel = record.mChallenge;
                        const auto hasScript = [](const ScriptDefinition& script) {
                            return !script.compiledScript.empty() || !script.scriptSource.empty()
                                || script.scriptHeader.compiledSize != 0;
                        };
                        response.mBeginScript = hasScript(record.mScript);
                        response.mEndScript = hasScript(record.mEndScript);
                        response.mScriptsWithSource = (record.mScript.scriptSource.empty() ? 0 : 1)
                            + (record.mEndScript.scriptSource.empty() ? 0 : 1);
                    }
                }
                catch (const std::exception& e)
                {
                    // The record cannot be read, so it counts for nothing, not even as the one an earlier file had.
                    mTopics.erase(id);
                    mResponses.erase(id);
                    addError(type == REC_DIAL ? "DIAL" : "INFO", e);
                }
            }
            r.skipFailedRecord(recordStart);
            return true;
        };

        try
        {
            ReaderUtils::readAll(reader, visitRecord, [](Reader&) {});
        }
        catch (const std::exception& e)
        {
            // Raised by the reader while it walks record and group headers, so it holds offsets and sizes only.
            mFatalErrors.push_back(firstLine(e.what()));
        }
    }

    DialogueCensus::Summary DialogueCensus::summarize() const
    {
        Summary summary;

        std::unordered_map<ESM::FormId, std::size_t> responsesPerTopic;
        std::unordered_map<ESM::FormId, std::size_t> firstPerTopic;

        for (const auto& [id, topic] : mTopics)
        {
            ++summary.mTopics;
            ++summary.mTopicTypes[topic.mType];
            summary.mTopicsRumors += (topic.mFlags & Dialogue::Flag_Rumors) != 0 ? 1 : 0;
            if ((topic.mFlags & Dialogue::Flag_TopLevel) != 0)
            {
                ++summary.mTopicsTopLevel;
                ++summary.mTopLevelTypes[topic.mType];
            }
            summary.mTopicsNoQuest += topic.mQuests == 0 ? 1 : 0;
            ++summary.mTopicQuests[capped(topic.mQuests)];
            summary.mTopicsSharingInfos += topic.mSharedInfos != 0 ? 1 : 0;
            summary.mSharedInfos += topic.mSharedInfos;
            summary.mTopicsNamed += topic.mNamed ? 1 : 0;
            if ((topic.mFlags & Dialogue::Flag_TopLevel) != 0)
                ++summary.mPriorities[static_cast<std::int32_t>(std::floor(topic.mPriority))];
        }

        for (const auto& [id, response] : mResponses)
        {
            ++summary.mInfos;
            ++summary.mInfoTypes[response.mType];
            ++summary.mNextSpeakers[response.mNextSpeaker];
            for (std::size_t bit = 0; bit < infoFlagBits; ++bit)
                summary.mFlags[bit] += (response.mFlags & (1u << bit)) != 0 ? 1 : 0;

            if (response.mTopic.isZeroOrUnset())
                ++summary.mNoTopic;
            else
            {
                ++responsesPerTopic[response.mTopic];
                if (mTopics.find(response.mTopic) == mTopics.end())
                    ++summary.mTopicNotRead;
                if (response.mPrevious.isZeroOrUnset())
                    ++firstPerTopic[response.mTopic];
            }
            if (response.mQuest.isZeroOrUnset())
                ++summary.mNoQuest;
            else if (mQuests.find(response.mQuest) == mQuests.end())
                ++summary.mQuestNotRead;

            ++summary.mResponses[capped(response.mLines)];
            summary.mLines += response.mLines;
            summary.mLinesNoText += response.mLinesNoText;
            summary.mLinesWithSound += response.mLinesWithSound;
            summary.mLinesWithAnimation += response.mLinesWithAnimation;
            for (const std::uint32_t emotion : response.mEmotions)
                ++summary.mEmotions[static_cast<int>(emotion)];

            ++summary.mConditions[capped(response.mConditions)];
            ++summary.mChoices[capped(response.mChoices.size())];
            summary.mWithLinkedFrom += response.mLinkedFrom != 0 ? 1 : 0;
            summary.mWithAddTopics += response.mAddTopics.empty() ? 0 : 1;
            summary.mWithFollowUps += response.mFollowUps != 0 ? 1 : 0;
            summary.mWithPrompt += response.mPrompt ? 1 : 0;
            summary.mWithSpeaker += response.mSpeaker ? 1 : 0;
            summary.mWithChallenge += response.mChallenge ? 1 : 0;
            if (response.mChallenge)
                ++summary.mChallenges[response.mChallengeLevel];
            summary.mWithBeginScript += response.mBeginScript ? 1 : 0;
            summary.mWithEndScript += response.mEndScript ? 1 : 0;
            summary.mScriptsWithSource += response.mScriptsWithSource;

            if (!response.mPrevious.isZeroOrUnset())
            {
                ++summary.mPreviousSet;
                const auto previous = mResponses.find(response.mPrevious);
                if (previous == mResponses.end())
                    ++summary.mPreviousNotRead;
                else if (previous->second.mTopic != response.mTopic)
                    ++summary.mPreviousOtherTopic;
            }

            for (const ESM::FormId choice : response.mChoices)
                if (mTopics.find(choice) == mTopics.end())
                    ++summary.mChoicesNotTopics;
            for (const ESM::FormId added : response.mAddTopics)
                if (mTopics.find(added) == mTopics.end())
                    ++summary.mAddTopicsNotTopics;

            summary.mOrConditions += response.mOr;
            summary.mConditionsWithGlobal += response.mGlobals;
            for (std::size_t i = 0; i < response.mFunctions.size(); ++i)
            {
                Summary::FunctionUse& use = summary.mFunctions[response.mFunctions[i]];
                ++use.mUses;
                const std::uint32_t runOn = response.mRunOn[i];
                if (runOn == runOnTarget)
                    ++use.mOnTarget;
                else if (runOn == runOnReference)
                    ++use.mOnReference;
                else if (runOn != 0)
                    ++use.mOnOther;
            }
        }

        for (const auto& [id, topic] : mTopics)
        {
            const auto found = responsesPerTopic.find(id);
            if (found == responsesPerTopic.end())
                ++summary.mTopicsNoResponses;
            const auto first = firstPerTopic.find(id);
            ++summary.mFirstInTopic[capped(first == firstPerTopic.end() ? 0 : first->second)];
        }

        return summary;
    }

    void DialogueCensus::write(std::ostream& stream) const
    {
        const Summary summary = summarize();
        constexpr int nameWidth = 22;
        constexpr int countWidth = 9;

        const auto line = [&](const std::string& name, std::size_t number) {
            stream << "  " << std::left << std::setw(nameWidth) << name << std::right << std::setw(countWidth) << number
                   << '\n';
        };
        const auto writeCapped = [&](const std::array<std::size_t, maxCounted + 1>& counts) {
            for (std::size_t number = 0; number <= maxCounted; ++number)
                line(std::to_string(number) + (number == maxCounted ? " or more" : ""), counts[number]);
        };

        stream << "Topic records (DIAL): " << summary.mTopics << '\n';
        stream << "  rumours: " << summary.mTopicsRumors << '\n';
        stream << "  top-level: " << summary.mTopicsTopLevel << '\n';
        stream << "  with a name the player sees: " << summary.mTopicsNamed << '\n';
        stream << "  with no response read in them: " << summary.mTopicsNoResponses << '\n';
        stream << "  that show responses of other topics: " << summary.mTopicsSharingInfos << " ("
               << summary.mSharedInfos << " responses)\n";
        stream << "\nTopics by type\n";
        for (const auto& [type, number] : summary.mTopicTypes)
            line(topicTypeName(type), number);
        stream << "\nTop-level topics by type\n";
        for (const auto& [type, number] : summary.mTopLevelTypes)
            line(topicTypeName(type), number);
        stream << "\nTopics by how many quests they list\n";
        writeCapped(summary.mTopicQuests);
        stream << "\nTop-level topics by priority (rounded down)\n";
        for (const auto& [priority, number] : summary.mPriorities)
            line(std::to_string(priority), number);
        stream << "\nTopics by how many of their responses have no previous response in the topic (PNAM)\n";
        writeCapped(summary.mFirstInTopic);

        stream << "\nResponse records (INFO): " << summary.mInfos << '\n';
        stream << "  not in a topic group: " << summary.mNoTopic << '\n';
        stream << "  in a topic that no DIAL record read has: " << summary.mTopicNotRead << '\n';
        stream << "  with no quest: " << summary.mNoQuest << '\n';
        stream << "  with a quest that no QUST record read has: " << summary.mQuestNotRead << '\n';
        stream << "  with a previous response (PNAM): " << summary.mPreviousSet << ", of which not read "
               << summary.mPreviousNotRead << ", in another topic " << summary.mPreviousOtherTopic << '\n';
        stream << "  with link-from topics (TCLF): " << summary.mWithLinkedFrom << '\n';
        stream << "  that add topics (NAME): " << summary.mWithAddTopics << ", naming a record that is not a topic "
               << summary.mAddTopicsNotTopics << '\n';
        stream << "  with follow-ups (TCFU): " << summary.mWithFollowUps << '\n';
        stream << "  with a prompt that replaces the topic name (RNAM): " << summary.mWithPrompt << '\n';
        stream << "  for one speaker only (ANAM): " << summary.mWithSpeaker << '\n';
        stream << "  with a speech challenge: " << summary.mWithChallenge << '\n';
        stream << "  with a script when it starts: " << summary.mWithBeginScript << '\n';
        stream << "  with a script when it ends: " << summary.mWithEndScript << '\n';
        stream << "  scripts that keep their source text: " << summary.mScriptsWithSource << '\n';

        stream << "\nResponses by the type of their topic\n";
        for (const auto& [type, number] : summary.mInfoTypes)
            line(topicTypeName(type), number);
        stream << "\nResponses by who speaks next\n";
        for (const auto& [speaker, number] : summary.mNextSpeakers)
            line(nextSpeakerName(speaker), number);
        stream << "\nResponses by the flags they have\n";
        for (std::size_t bit = 0; bit < infoFlagBits; ++bit)
            if (summary.mFlags[bit] != 0)
                line(infoFlagName(static_cast<int>(bit)), summary.mFlags[bit]);
        stream << "\nSpeech challenges by difficulty\n";
        for (const auto& [level, number] : summary.mChallenges)
            line(std::to_string(level), number);

        stream << "\nResponses by how many lines they say\n";
        writeCapped(summary.mResponses);
        stream << "  lines: " << summary.mLines << ", with no text " << summary.mLinesNoText << ", with a sound record "
               << summary.mLinesWithSound << ", with an idle " << summary.mLinesWithAnimation << '\n';
        stream << "\nLines by emotion\n";
        for (const auto& [emotion, number] : summary.mEmotions)
            line(std::to_string(emotion), number);

        stream << "\nResponses by how many conditions they have\n";
        writeCapped(summary.mConditions);
        stream << "  conditions joined with OR: " << summary.mOrConditions << '\n';
        stream << "  conditions with a global variable for the value: " << summary.mConditionsWithGlobal << '\n';
        stream << "\nResponses by how many topics the player can pick after them (TCLT), of which not topics: "
               << summary.mChoicesNotTopics << '\n';
        writeCapped(summary.mChoices);

        stream << "\nFunctions that the conditions call (the number of the command less 0x1000)\n";
        for (const auto& [function, use] : summary.mFunctions)
        {
            const std::string name = mFunctionNames ? mFunctionNames(function) : std::string();
            stream << "  " << std::left << std::setw(6) << function << std::setw(28) << (name.empty() ? "-" : name)
                   << std::right << std::setw(countWidth) << use.mUses << "  on the player " << use.mOnTarget
                   << ", on a reference " << use.mOnReference << ", other " << use.mOnOther << '\n';
        }

        if (mUnreadable != 0)
        {
            stream << "\nRecords that the loaders could not read: " << mUnreadable << '\n';
            for (const std::string& error : mReadErrors)
                stream << "  " << error << '\n';
        }

        if (!mFatalErrors.empty())
        {
            stream << "\nReading stopped early\n";
            for (const std::string& error : mFatalErrors)
                stream << "  " << error << '\n';
        }
    }
}
