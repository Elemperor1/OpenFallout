#ifndef OPENFALLOUT_COMPONENTS_ESM4_DIALOGUECENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_DIALOGUECENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    // Counts the dialogue of one or more Fallout 3 and New Vegas plugins: the topics (DIAL) by their kind, and the
    // responses in them (INFO) by what they have: conditions, several lines, choices, links, topics they add,
    // prompts, challenges and scripts, and which functions their conditions call. It is for choosing which rules the
    // dialogue needs and for checking the loaders against the real files, so what it keeps is record types, the codes
    // of the kinds, the numbers of the functions and counts, never what anyone says (no names, texts or notes).
    //
    // It reads each record with the loader of the game, so a record that the loader rejects shows up as a record that
    // cannot be read, with the first line of the error (offsets and sizes only) for the first few.
    //
    // The files must be read in load order, each with its mod index and the indices of its masters set on the reader
    // (Reader::setModIndex and Reader::updateModIndices). A record that a later file defines again counts once, as the
    // later file has it, and one that a later file deletes, or that cannot be read, does not count.
    class DialogueCensus
    {
    public:
        // The names that the games use for the kinds of topic, the bits of the flags of a response and the speakers
        static std::string topicTypeName(int type);
        static std::string infoFlagName(int bit);
        static std::string nextSpeakerName(int speaker);
        static constexpr std::size_t infoFlagBits = 16;

        // A response with more than this many of something counts as having this many.
        static constexpr std::size_t maxCounted = 8;
        static constexpr std::size_t maxReadErrors = 5;

        // Names the function a condition calls by its number (the code of the command less 0x1000), or gives "".
        using FunctionNames = std::function<std::string(std::uint32_t function)>;
        void setFunctionNames(FunctionNames names) { mFunctionNames = std::move(names); }

        struct Summary
        {
            // The DIAL records
            std::size_t mTopics = 0;
            std::map<int, std::size_t> mTopicTypes;
            std::size_t mTopicsRumors = 0;
            std::size_t mTopicsTopLevel = 0;
            std::map<int, std::size_t> mTopLevelTypes; // the top-level topics by type
            std::size_t mTopicsNoQuest = 0;
            std::array<std::size_t, maxCounted + 1> mTopicQuests{}; // topics with n quests
            std::size_t mTopicsSharingInfos = 0; // have INFC/INFX pairs
            std::size_t mSharedInfos = 0;
            std::size_t mTopicsNoResponses = 0; // no INFO record that was read is in the topic
            std::size_t mTopicsNamed = 0; // with a name the player can see
            std::map<std::int32_t, std::size_t> mPriorities; // of the top-level topics, by the value rounded down

            // The INFO records
            std::size_t mInfos = 0;
            std::map<int, std::size_t> mInfoTypes; // by the topic type in DATA
            std::map<int, std::size_t> mNextSpeakers;
            std::array<std::size_t, infoFlagBits> mFlags{}; // responses that have the bit of the flags set
            std::size_t mNoTopic = 0; // not in a topic group and no TPIC
            std::size_t mTopicNotRead = 0; // the topic has no DIAL record that was read
            std::size_t mQuestNotRead = 0; // QSTI names a quest that no file has a record for
            std::size_t mNoQuest = 0;
            std::array<std::size_t, maxCounted + 1> mResponses{}; // responses (lines) per record
            std::size_t mLines = 0;
            std::size_t mLinesNoText = 0;
            std::size_t mLinesWithSound = 0; // a SOUN replaces the voice file
            std::size_t mLinesWithAnimation = 0; // an idle for the speaker or the listener
            std::map<int, std::size_t> mEmotions; // lines by emotion
            std::array<std::size_t, maxCounted + 1> mConditions{}; // records with n conditions
            std::array<std::size_t, maxCounted + 1> mChoices{};
            std::size_t mWithLinkedFrom = 0;
            std::size_t mWithAddTopics = 0;
            std::size_t mWithFollowUps = 0;
            std::size_t mWithPrompt = 0;
            std::size_t mWithSpeaker = 0;
            std::size_t mWithChallenge = 0;
            std::map<std::uint32_t, std::size_t> mChallenges; // by the difficulty in DNAM
            std::size_t mWithBeginScript = 0;
            std::size_t mWithEndScript = 0;
            std::size_t mScriptsWithSource = 0; // begin or end scripts that keep their source
            std::size_t mPreviousSet = 0; // PNAM names a response
            std::size_t mPreviousNotRead = 0; // PNAM names a response that no file read has
            std::size_t mPreviousOtherTopic = 0; // PNAM names a response of another topic
            std::size_t mChoicesNotTopics = 0; // TCLT names a record that is not a topic that was read
            std::size_t mAddTopicsNotTopics = 0; // NAME names a record that is not a topic that was read
            std::array<std::size_t, maxCounted + 1> mFirstInTopic{}; // topics by the responses that have no PNAM

            // How the conditions of the responses call functions, by function number
            struct FunctionUse
            {
                std::size_t mUses = 0;
                std::size_t mOnTarget = 0; // runs on the player (run on 1)
                std::size_t mOnReference = 0; // runs on a reference (2)
                std::size_t mOnOther = 0; // another thing to run on
            };
            std::map<std::uint32_t, FunctionUse> mFunctions;
            std::size_t mOrConditions = 0;
            std::size_t mConditionsWithGlobal = 0; // the value is a global variable
        };

        void collect(Reader& reader);

        Summary summarize() const;

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }
        const std::vector<std::string>& getReadErrors() const { return mReadErrors; }
        std::size_t getUnreadableCount() const { return mUnreadable; }

        void write(std::ostream& stream) const;

    private:
        struct Topic
        {
            std::uint8_t mType = 0;
            std::uint8_t mFlags = 0;
            std::size_t mQuests = 0;
            std::size_t mSharedInfos = 0;
            bool mNamed = false;
            float mPriority = 0;
        };

        struct Response
        {
            ESM::FormId mTopic;
            ESM::FormId mQuest;
            ESM::FormId mPrevious;
            std::uint8_t mType = 0;
            std::uint8_t mNextSpeaker = 0;
            std::uint16_t mFlags = 0;
            std::size_t mLines = 0;
            std::size_t mLinesNoText = 0;
            std::size_t mLinesWithSound = 0;
            std::size_t mLinesWithAnimation = 0;
            std::vector<std::uint32_t> mEmotions;
            std::vector<std::uint32_t> mFunctions;
            std::vector<std::uint32_t> mRunOn;
            std::size_t mConditions = 0;
            std::size_t mOr = 0;
            std::size_t mGlobals = 0;
            std::vector<ESM::FormId> mChoices;
            std::vector<ESM::FormId> mAddTopics;
            std::size_t mLinkedFrom = 0;
            std::size_t mFollowUps = 0;
            bool mPrompt = false;
            bool mSpeaker = false;
            bool mChallenge = false;
            std::uint32_t mChallengeLevel = 0;
            bool mBeginScript = false;
            bool mEndScript = false;
            std::size_t mScriptsWithSource = 0;
        };

        std::unordered_map<ESM::FormId, Topic> mTopics;
        std::unordered_map<ESM::FormId, Response> mResponses;
        std::unordered_set<ESM::FormId> mQuests;
        std::vector<std::string> mFatalErrors;
        std::vector<std::string> mReadErrors;
        std::size_t mUnreadable = 0;
        FunctionNames mFunctionNames;
    };
}

#endif
