#include "scriptbuilder.hpp"

#include <components/falloutscript/dialogue.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::DialogueSpeaker;
    using FalloutScript::FormId;

    constexpr FormId runningQuest = 0x00001000;
    constexpr FormId sleepingQuest = 0x00001001;
    constexpr FormId greeting = 0x00002000;
    constexpr FormId topicA = 0x00002001;
    constexpr FormId topicB = 0x00002002;
    constexpr FormId topicC = 0x00002003;
    constexpr FormId goodbye = 0x00002004;
    constexpr FormId hidden = 0x00002005; // not at the top of the list
    constexpr std::uint32_t functionGetStage = OpGetStage - 0x1000;
    constexpr std::uint32_t functionTouch = OpTouch - 0x1000;
    constexpr DialogueSpeaker speaker{ 0x00ff0001, 0x00003000 };

    ESM::FormId id(FormId value)
    {
        return ESM::FormId::fromUint32(value);
    }

    class TestRecords : public FalloutScript::QuestRecords, public FalloutScript::DialogueRecords
    {
    public:
        std::deque<ESM4::Quest> mQuests;
        std::deque<ESM4::Dialogue> mTopics;
        std::deque<ESM4::DialogInfo> mInfos; // in file order; the addresses must not change

        const ESM4::Script* findScript(FormId) const override { return nullptr; }

        void forEachQuest(const std::function<void(const ESM4::Quest&)>& function) const override
        {
            for (const ESM4::Quest& quest : mQuests)
                function(quest);
        }

        void forEachTopic(const std::function<void(const ESM4::Dialogue&)>& function) const override
        {
            for (const ESM4::Dialogue& topic : mTopics)
                function(topic);
        }

        void forEachInfo(const std::function<void(const ESM4::DialogInfo&)>& function) const override
        {
            for (const ESM4::DialogInfo& info : mInfos)
                function(info);
        }

        void addQuest(FormId quest, bool running)
        {
            ESM4::Quest& record = mQuests.emplace_back();
            record.mId = id(quest);
            record.mData.flags = running ? ESM4::Quest::Flag_StartGameEnabled : 0;
        }

        ESM4::Dialogue& addTopic(
            FormId topic, std::string name, std::string editorId = {}, bool topLevel = false, float priority = 0)
        {
            ESM4::Dialogue& record = mTopics.emplace_back();
            record.mId = id(topic);
            record.mTopicName = std::move(name);
            record.mEditorId = std::move(editorId);
            record.mDialFlags = topLevel ? ESM4::Dialogue::Flag_TopLevel : 0;
            record.mPriority = priority;
            return record;
        }

        /// A response of the topic that says the text, in the quest that runs
        ESM4::DialogInfo& addInfo(FormId info, FormId topic, std::string text, FormId previous = 0)
        {
            ESM4::DialogInfo& record = mInfos.emplace_back();
            record.mId = id(info);
            record.mTopic = id(topic);
            record.mQuest = id(runningQuest);
            if (previous != 0)
                record.mPreviousInfo = id(previous);
            ESM4::DialogResponse& line = record.mResponses.emplace_back();
            line.mText = std::move(text);
            line.mNumber = 1;
            return record;
        }
    };

    ESM4::TargetCondition talkedToPlayer(float value)
    {
        ESM4::TargetCondition result{};
        result.condition = ESM4::CTF_EqualTo;
        result.comparison = value;
        result.functionIndex = OpGetTalkedToPC - 0x1000;
        return result;
    }

    ESM4::TargetCondition stageIs(FormId quest, float stage)
    {
        ESM4::TargetCondition result{};
        result.condition = ESM4::CTF_EqualTo;
        result.comparison = stage;
        result.functionIndex = functionGetStage;
        result.param1 = quest;
        return result;
    }

    struct Harness
    {
        FalloutScript::CommandTable mCommands = testCommands();
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FalloutScript::ConditionEvaluator mConditions{ mInterpreter };
        TestRecords mRecords;
        FalloutScript::QuestManager mQuests{ mInterpreter, mRecords };
        FalloutScript::DialogueManager mDialogue{ mInterpreter, mConditions, mQuests, mRecords };
        std::vector<std::string> mLogged;
        std::vector<FormId> mTouched; // the objects that conditions with Touch ran on
        std::map<FormId, double> mTouchValue;
        double mHours = 34; // the second day, ten o'clock
        std::size_t mRandomPick = 0;
        std::vector<std::size_t> mRandomCounts;

        Harness()
        {
            mRecords.addQuest(runningQuest, true);
            mRecords.addQuest(sleepingQuest, false);
            mInterpreter.setHandler("Log", [this](FalloutScript::CallContext& call) {
                mLogged.push_back(call.mArguments.at(0).mText);
                return 0.0;
            });
            mInterpreter.setHandler("Touch", [this](FalloutScript::CallContext& call) {
                mTouched.push_back(call.mReference);
                return mTouchValue[call.mReference];
            });
            FalloutScript::addQuestCommands(mInterpreter, mQuests);
            FalloutScript::addConditionFunctions(mConditions);
            FalloutScript::addDialogueCommands(mInterpreter, mDialogue);
            mDialogue.setGameHours([this] { return mHours; });
            mDialogue.setRandom([this](std::size_t count) {
                mRandomCounts.push_back(count);
                return std::min(mRandomPick, count - 1);
            });
        }

        void start()
        {
            mQuests.reset();
            mDialogue.reset();
        }

        std::vector<std::string> texts(const FalloutScript::DialogueView& view) const
        {
            std::vector<std::string> result;
            for (const FalloutScript::DialogueLine& line : view.mLines)
                result.push_back(line.mText);
            return result;
        }

        std::vector<FormId> topics(const FalloutScript::DialogueView& view) const
        {
            std::vector<FormId> result;
            for (const FalloutScript::DialogueOption& option : view.mOptions)
                result.push_back(option.mTopic);
            return result;
        }
    };

    ScriptBuilder logScript(const std::string& text)
    {
        ScriptBuilder script;
        script.code(command(OpLog, arguments(1).text(text)));
        return script;
    }

    std::vector<FormId> ids(const std::vector<const ESM4::DialogInfo*>& infos)
    {
        std::vector<FormId> result;
        for (const ESM4::DialogInfo* info : infos)
            result.push_back(info->mId.toUint32());
        return result;
    }

    TEST(FalloutScriptDialogueTest, triesTheResponsesOfATopicInTheOrderOfTheirChain)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        // The file defines the middle one last, as a later file does that puts a response between two others
        setup.mRecords.addInfo(0x3001, topicA, "first");
        setup.mRecords.addInfo(0x3003, topicA, "third", 0x3002);
        setup.mRecords.addInfo(0x3002, topicA, "second", 0x3001);
        setup.mRecords.addInfo(0x3004, topicA, "fourth", 0x3003);
        setup.start();

        EXPECT_THAT(ids(setup.mDialogue.responses(topicA)), ElementsAre(0x3001, 0x3002, 0x3003, 0x3004));
    }

    TEST(FalloutScriptDialogueTest, leavesOutTheRecordsThatAreDeleted)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addTopic(topicB, "B").mFlags = ESM4::Rec_Deleted;
        setup.mRecords.addInfo(0x3001, topicA, "kept");
        setup.mRecords.addInfo(0x3002, topicA, "deleted", 0x3001).mFlags = ESM4::Rec_Deleted;
        setup.mRecords.addInfo(0x3003, topicB, "of a deleted topic");
        setup.start();

        EXPECT_THAT(ids(setup.mDialogue.responses(topicA)), ElementsAre(0x3001));
        EXPECT_THAT(ids(setup.mDialogue.responses(topicB)), IsEmpty());
        EXPECT_EQ(setup.mDialogue.topic(topicB), nullptr);
        EXPECT_EQ(setup.mDialogue.responseFor(topicB, speaker), nullptr);
    }

    TEST(FalloutScriptDialogueTest, aTopicTriesTheResponsesOfOtherTopicsItShowsAfterItsOwn)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addTopic(topicB, "B", "TopicB", true);
        setup.mRecords.addTopic(topicC, "C");
        setup.mRecords.addInfo(0x3001, topicA, "shared one");
        setup.mRecords.addInfo(0x3002, topicA, "shared two", 0x3001);
        setup.mRecords.addInfo(0x3003, topicA, "shared but deleted", 0x3002).mFlags = ESM4::Rec_Deleted;
        ESM4::DialogInfo& own = setup.mRecords.addInfo(0x3004, topicB, "own");
        own.mQuest = id(sleepingQuest);
        // B shows two responses of A (the later one first), one that is deleted, one that is not there at all, and one
        // of its own that it lists again
        ESM4::Dialogue& shower = setup.mRecords.mTopics[1];
        shower.mSharedInfos
            = { { id(0x3002), 1 }, { id(0x3003), 2 }, { id(0x3999), 3 }, { id(0x3004), 4 }, { id(0x3001), 5 } };
        setup.start();

        EXPECT_THAT(ids(setup.mDialogue.responses(topicA)), ElementsAre(0x3001, 0x3002));
        EXPECT_THAT(ids(setup.mDialogue.responses(topicB)), ElementsAre(0x3004, 0x3002, 0x3001));
        // The own response is not said (its quest sleeps), so the first that is shown is
        const ESM4::DialogInfo* given = setup.mDialogue.responseFor(topicB, speaker);
        ASSERT_NE(given, nullptr);
        EXPECT_EQ(given->mId.toUint32(), 0x3002u);
        // and it is said under the topic that was picked
        setup.mDialogue.start(speaker);
        EXPECT_EQ(setup.mDialogue.choose(topicB).mTopic, topicB);
    }

    TEST(FalloutScriptDialogueTest, keepsTheFileOrderWhereThereIsNoChain)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3002, topicA, "b");
        setup.mRecords.addInfo(0x3001, topicA, "a");
        setup.mRecords.addInfo(0x3003, topicA, "c");
        // Two responses that name each other have no start, but are not lost
        setup.mRecords.addInfo(0x3005, topicA, "e", 0x3004);
        setup.mRecords.addInfo(0x3004, topicA, "d", 0x3005);
        setup.start();

        EXPECT_THAT(ids(setup.mDialogue.responses(topicA)), ElementsAre(0x3002, 0x3001, 0x3003, 0x3005, 0x3004));
    }

    TEST(FalloutScriptDialogueTest, theFirstResponseWhoseConditionsHoldIsTheOneThatIsGiven)
    {
        Harness setup;
        ESM4::QuestStage& stage = setup.mRecords.mQuests.front().mStages.emplace_back();
        stage.mIndex = 20;
        stage.mLogEntries.emplace_back();
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "needs stage 10").mConditions.push_back(stageIs(runningQuest, 10));
        setup.mRecords.addInfo(0x3002, topicA, "needs stage 20").mConditions.push_back(stageIs(runningQuest, 20));
        setup.mRecords.addInfo(0x3003, topicA, "always");
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3003u);
        EXPECT_TRUE(setup.mQuests.setStage(runningQuest, 20));
        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3002u);
    }

    TEST(FalloutScriptDialogueTest, aTopicWithNoResponseThatCanBeSaidGivesNone)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "needs stage 10").mConditions.push_back(stageIs(runningQuest, 10));
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker), nullptr);
        EXPECT_EQ(setup.mDialogue.responseFor(topicB, speaker), nullptr);
    }

    TEST(FalloutScriptDialogueTest, conditionsAreAboutTheSpeaker)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        ESM4::TargetCondition touch{};
        touch.condition = ESM4::CTF_GreaterThan;
        touch.comparison = 0;
        touch.functionIndex = functionTouch;
        setup.mRecords.addInfo(0x3001, topicA, "yes").mConditions.push_back(touch);
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker), nullptr);
        EXPECT_THAT(setup.mTouched, ElementsAre(speaker.mReference));
        setup.mTouchValue[speaker.mReference] = 1;
        EXPECT_NE(setup.mDialogue.responseFor(topicA, speaker), nullptr);
    }

    TEST(FalloutScriptDialogueTest, aResponseSaidToACharacterAsksTheConditionsOnTheTargetAboutIt)
    {
        constexpr FormId listener = 0x00ff0005;
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        ESM4::TargetCondition touch{};
        touch.condition = static_cast<std::uint32_t>(ESM4::CTF_GreaterThan | ESM4::CTF_RunOnTarget);
        touch.comparison = 0;
        touch.functionIndex = functionTouch;
        setup.mRecords.addInfo(0x3001, topicA, "yes").mConditions.push_back(touch);
        setup.start();
        setup.mTouchValue[listener] = 1;

        // said to the player, the condition on the target is about the player, who does not meet it
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre());
        EXPECT_THAT(setup.mTouched, ElementsAre(FalloutScript::playerReference));
        // said to another character, it is about that one
        setup.mTouched.clear();
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA, listener)), ElementsAre("yes"));
        EXPECT_THAT(setup.mTouched, ElementsAre(listener));
    }

    TEST(FalloutScriptDialogueTest, aResponseSaidInAConversationBetweenCharactersGoesOnWithItsFollowUps)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3001, topicA, "first").mEndScript = logScript("end first").definition();
        setup.mRecords.addInfo(0x3002, topicA, "asleep", 0x3001).mQuest = id(sleepingQuest);
        ESM4::DialogInfo& second = setup.mRecords.addInfo(0x3003, topicA, "second", 0x3001);
        second.mScript = logScript("begin second").definition();
        second.mEndScript = logScript("end second").definition();
        setup.mRecords.addInfo(0x3004, topicA, "third", 0x3003);
        setup.mRecords.mInfos[0].mFollowUps = { id(0x3999), id(0x3002), id(0x3003) };
        setup.mRecords.mInfos[2].mFollowUps = { id(0x3004), id(0x3001) };
        setup.start();

        // Said as a single response (Say, SayTo) it stops at the first
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("first"));
        EXPECT_THAT(setup.mLogged, ElementsAre("end first"));

        // Said in a conversation it goes on: the follow-ups that are not there or can not be said are passed over,
        // the scripts of each response run, and the one that was said already is not said again
        setup.mLogged.clear();
        EXPECT_THAT(
            setup.texts(setup.mDialogue.say(speaker, topicA, 0, true)), ElementsAre("first", "second", "third"));
        EXPECT_THAT(setup.mLogged, ElementsAre("end first", "begin second", "end second"));
        EXPECT_TRUE(setup.mDialogue.wasSaid(0x3004, speaker.mReference));
    }

    TEST(FalloutScriptDialogueTest, aResponseNeedsItsQuestToRun)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "asleep").mQuest = id(sleepingQuest);
        setup.mRecords.addInfo(0x3002, topicA, "unknown").mQuest = id(0x00009999);
        setup.mRecords.addInfo(0x3003, topicA, "no quest").mQuest = ESM::FormId();
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3003u);
        setup.mQuests.start(sleepingQuest);
        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3001u);
    }

    TEST(FalloutScriptDialogueTest, aResponseNeedsTheConditionsOfItsQuestToHoldForTheSpeaker)
    {
        Harness setup;
        // The quest keeps the test of who may say its responses, which have none of their own
        ESM4::TargetCondition touch{};
        touch.condition = ESM4::CTF_GreaterThan;
        touch.comparison = 0;
        touch.functionIndex = functionTouch;
        setup.mRecords.mQuests.front().mTargetConditions.push_back(touch);
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "of the quest with conditions");
        setup.mRecords.addInfo(0x3002, topicA, "of no quest").mQuest = ESM::FormId();
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3002u);
        EXPECT_THAT(setup.mTouched, ElementsAre(speaker.mReference));
        setup.mTouchValue[speaker.mReference] = 1;
        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3001u);
    }

    TEST(FalloutScriptDialogueTest, aResponseNeedsItsOwnConditionsAndThoseOfItsQuest)
    {
        Harness setup;
        setup.mRecords.mQuests.front().mTargetConditions.push_back(stageIs(runningQuest, 0));
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "own condition fails").mConditions.push_back(stageIs(runningQuest, 10));
        setup.mRecords.addInfo(0x3002, topicA, "both hold");
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3002u);
    }

    TEST(FalloutScriptDialogueTest, theQuestOfTheHighestPriorityIsTriedFirst)
    {
        Harness setup;
        constexpr FormId highQuest = 0x00001002;
        constexpr FormId asleepQuest = 0x00001003;
        setup.mRecords.addQuest(highQuest, true);
        setup.mRecords.mQuests.back().mData.priority = 50;
        setup.mRecords.addQuest(asleepQuest, false);
        setup.mRecords.mQuests.back().mData.priority = 90;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "of the quest of priority 0");
        setup.mRecords.addInfo(0x3002, topicA, "of the quest of priority 50").mQuest = id(highQuest);
        setup.mRecords.addInfo(0x3003, topicA, "next of that quest", 0x3002).mQuest = id(highQuest);
        setup.mRecords.addInfo(0x3004, topicA, "of a quest that sleeps").mQuest = id(asleepQuest);
        setup.start();

        // The responses of one quest keep the order of their chain
        EXPECT_THAT(ids(setup.mDialogue.responses(topicA)), ElementsAre(0x3004u, 0x3002u, 0x3003u, 0x3001u));
        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3002u);
    }

    TEST(FalloutScriptDialogueTest, aRunOfRandomResponsesStaysInTheQuestItStartedIn)
    {
        Harness setup;
        constexpr FormId highQuest = 0x00001002;
        setup.mRecords.addQuest(highQuest, true);
        setup.mRecords.mQuests.back().mData.priority = 50;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "of the quest of priority 0").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.addInfo(0x3002, topicA, "of the quest of priority 50").mQuest = id(highQuest);
        setup.mRecords.mInfos.back().mInfoFlags = ESM4::INFO_Random;
        setup.start();
        setup.mRandomPick = 5;

        // The order is now 0x3002, 0x3001: the run that starts at the first is not joined by the one of the other quest
        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3002u);
        EXPECT_THAT(setup.mRandomCounts, IsEmpty());
    }

    TEST(FalloutScriptDialogueTest, aQuestOfHigherPriorityThatCannotBeSaidLeavesTheTopicToTheNext)
    {
        Harness setup;
        constexpr FormId highQuest = 0x00001002;
        setup.mRecords.addQuest(highQuest, true);
        setup.mRecords.mQuests.back().mData.priority = 50;
        setup.mRecords.mQuests.back().mTargetConditions.push_back(stageIs(highQuest, 10));
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "of the quest of priority 0");
        setup.mRecords.addInfo(0x3002, topicA, "of the quest of priority 50").mQuest = id(highQuest);
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3001u);
    }

    TEST(FalloutScriptDialogueTest, aResponseForOneSpeakerIsNotSaidByAnother)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "only for one").mSpeaker = id(0x00004000);
        setup.mRecords.addInfo(0x3002, topicA, "for anyone");
        setup.start();

        EXPECT_EQ(setup.mDialogue.responseFor(topicA, speaker)->mId.toUint32(), 0x3002u);
        EXPECT_EQ(setup.mDialogue.responseFor(topicA, { 0x00ff0002, 0x00004000 })->mId.toUint32(), 0x3001u);
    }

    TEST(FalloutScriptDialogueTest, aResponseSaidOnceIsNotSaidAgain)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "once").mInfoFlags = ESM4::INFO_SayOnce;
        setup.mRecords.addInfo(0x3002, topicA, "always");
        setup.start();

        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("once"));
        EXPECT_TRUE(setup.mDialogue.wasSaid(0x3001, speaker.mReference));
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("always"));
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("always"));
        // Another character has not said it
        EXPECT_FALSE(setup.mDialogue.wasSaid(0x3001, 0x00ff0002));
        EXPECT_THAT(setup.texts(setup.mDialogue.say({ 0x00ff0002, 0x00004000 }, topicA)), ElementsAre("once"));
        EXPECT_THAT(setup.texts(setup.mDialogue.say({ 0x00ff0002, 0x00004000 }, topicA)), ElementsAre("always"));

        setup.start();
        EXPECT_FALSE(setup.mDialogue.wasSaid(0x3001, speaker.mReference));
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("once"));
    }

    TEST(FalloutScriptDialogueTest, aResponseSaidOnceADayIsSaidAgainAfterTheIntervalHasPassed)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "daily").mInfoFlags = ESM4::INFO_SayOnceADay;
        setup.mRecords.addInfo(0x3002, topicA, "otherwise");
        setup.start();

        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("daily"));
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("otherwise"));
        // Not before the 12 hours are over, and then at once, though it is the same day (midnight is 14 hours on)
        setup.mHours += 11.5;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("otherwise"));
        setup.mHours += 0.5;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("daily"));
        // The 12 hours start again from the time it was said
        setup.mHours += 11;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("otherwise"));
    }

    TEST(FalloutScriptDialogueTest, theIntervalOfAResponseSaidOnceADayCanBeSet)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "daily").mInfoFlags = ESM4::INFO_SayOnceADay;
        setup.start();
        setup.mDialogue.setDailyInterval(24);

        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("daily"));
        setup.mHours += 23;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), IsEmpty());
        setup.mHours += 1;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("daily"));
    }

    TEST(FalloutScriptDialogueTest, aResponseSaidOnceADayIsKeptForEachCharacter)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "daily").mInfoFlags = ESM4::INFO_SayOnceADay;
        setup.start();

        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("daily"));
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), IsEmpty());
        EXPECT_THAT(setup.texts(setup.mDialogue.say({ 0x00ff0002, 0x00004000 }, topicA)), ElementsAre("daily"));
    }

    TEST(FalloutScriptDialogueTest, aRunOfRandomResponsesGivesOneOfThoseThatCanBeSaid)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "before").mConditions.push_back(stageIs(runningQuest, 5));
        setup.mRecords.addInfo(0x3002, topicA, "r1").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.addInfo(0x3003, topicA, "r2 cannot").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.mInfos.back().mConditions.push_back(stageIs(runningQuest, 5));
        setup.mRecords.addInfo(0x3004, topicA, "r3").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.addInfo(0x3005, topicA, "r4 ends").mInfoFlags = ESM4::INFO_Random | ESM4::INFO_RandomEnd;
        setup.mRecords.addInfo(0x3006, topicA, "r5 after the end").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.addInfo(0x3007, topicA, "plain");
        setup.start();

        setup.mRandomPick = 0;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("r1"));
        setup.mRandomPick = 1;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("r3"));
        setup.mRandomPick = 2;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("r4 ends"));
        setup.mRandomPick = 9;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("r4 ends"));
        // r1, r3 and r4 are the candidates: the run stops at the response flagged Random End
        EXPECT_THAT(setup.mRandomCounts, Each(3u));
    }

    TEST(FalloutScriptDialogueTest, aRunOfRandomResponsesEndsAtAResponseThatIsNotRandom)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "r1").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.addInfo(0x3002, topicA, "r2").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.addInfo(0x3003, topicA, "plain");
        setup.mRecords.addInfo(0x3004, topicA, "r3").mInfoFlags = ESM4::INFO_Random;
        setup.start();

        setup.mRandomPick = 5;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("r2"));
        EXPECT_THAT(setup.mRandomCounts, ElementsAre(2u));
    }

    TEST(FalloutScriptDialogueTest, aRunOfRandomResponsesPassesOverAResponseThatCannotBeSaid)
    {
        Harness setup;
        ESM4::QuestStage& stage = setup.mRecords.mQuests.front().mStages.emplace_back();
        stage.mIndex = 10;
        stage.mLogEntries.emplace_back();
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "r1").mInfoFlags = ESM4::INFO_Random;
        setup.mRecords.addInfo(0x3002, topicA, "later").mConditions.push_back(stageIs(runningQuest, 10));
        setup.mRecords.addInfo(0x3003, topicA, "r2").mInfoFlags = ESM4::INFO_Random;
        setup.start();

        setup.mRandomPick = 5;
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("r2"));
        EXPECT_THAT(setup.mRandomCounts, ElementsAre(2u));

        // Once it can be said, it is a response that is not Random and the run ends at it
        EXPECT_TRUE(setup.mQuests.setStage(runningQuest, 10));
        setup.mRandomCounts.clear();
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("r1"));
        EXPECT_THAT(setup.mRandomCounts, IsEmpty());
    }

    TEST(FalloutScriptDialogueTest, aSpeechChallengeIsPassedUnlessACheckSaysOtherwise)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A");
        setup.mRecords.addInfo(0x3001, topicA, "success").mInfoFlags = ESM4::INFO_SpeechChallenge;
        setup.mRecords.addInfo(0x3002, topicA, "failure");
        setup.start();

        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("success"));
        setup.mDialogue.setChallengeCheck([](const ESM4::DialogInfo&) { return false; });
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicA)), ElementsAre("failure"));
    }

    TEST(FalloutScriptDialogueTest, aConversationStartsWithTheGreetingAndOffersTheTopicsAtTheTopOfTheList)
    {
        Harness setup;
        setup.mRecords.addTopic(greeting, "Greeting", "GREETING");
        setup.mRecords.addInfo(0x3001, greeting, "Hello there");
        setup.mRecords.addTopic(topicA, "Rumours", "TopicA", true, 10);
        setup.mRecords.addInfo(0x3002, topicA, "a");
        setup.mRecords.addTopic(topicB, "Trade", "TopicB", true, 50);
        setup.mRecords.addInfo(0x3003, topicB, "b").mPrompt = "I want to trade";
        setup.mRecords.addTopic(topicC, "Past", "TopicC", true, 50);
        setup.mRecords.addInfo(0x3004, topicC, "c").mConditions.push_back(stageIs(runningQuest, 7));
        setup.mRecords.addTopic(hidden, "Secret", "Hidden", false, 99);
        setup.mRecords.addInfo(0x3005, hidden, "h");
        setup.start();

        const FalloutScript::DialogueView& started = setup.mDialogue.start(speaker);

        EXPECT_TRUE(setup.mDialogue.active());
        EXPECT_THAT(setup.texts(started), ElementsAre("Hello there"));
        EXPECT_EQ(started.mSpeaker, speaker.mReference);
        EXPECT_EQ(started.mTopic, greeting);
        EXPECT_TRUE(started.mSpeaking);
        EXPECT_THAT(started.mOptions, IsEmpty());
        EXPECT_TRUE(setup.mDialogue.talkedToPlayer(speaker.mReference));

        const FalloutScript::DialogueView& finished = setup.mDialogue.finish();
        EXPECT_FALSE(finished.mSpeaking);
        // The higher priority first, with the words of the response when it has any, and no topic that has no
        // response to give or is not at the top of the list
        EXPECT_THAT(setup.topics(finished), ElementsAre(topicB, topicA));
        EXPECT_EQ(finished.mOptions[0].mText, "I want to trade");
        EXPECT_EQ(finished.mOptions[1].mText, "Rumours");
    }

    TEST(FalloutScriptDialogueTest, theGreetingOfAFirstMeetingSeesASpeakerThePlayerHasNotTalkedTo)
    {
        Harness setup;
        setup.mRecords.addTopic(greeting, "Greeting", "GREETING");
        setup.mRecords.addInfo(0x3001, greeting, "Hello stranger").mConditions.push_back(talkedToPlayer(0));
        setup.mRecords.addInfo(0x3002, greeting, "Back again").mConditions.push_back(talkedToPlayer(1));
        setup.start();

        EXPECT_THAT(setup.texts(setup.mDialogue.start(speaker)), ElementsAre("Hello stranger"));
        EXPECT_TRUE(setup.mDialogue.talkedToPlayer(speaker.mReference));
        setup.mDialogue.end();
        EXPECT_THAT(setup.texts(setup.mDialogue.start(speaker)), ElementsAre("Back again"));
    }

    TEST(FalloutScriptDialogueTest, aSpeakerWithNothingToSayHasNotBeenTalkedTo)
    {
        Harness setup;
        setup.mRecords.addTopic(greeting, "Greeting", "GREETING");
        // The only greeting is for a speaker that has been talked to
        setup.mRecords.addInfo(0x3001, greeting, "Back again").mConditions.push_back(talkedToPlayer(1));
        setup.start();

        const FalloutScript::DialogueView& started = setup.mDialogue.start(speaker);

        EXPECT_THAT(started.mLines, IsEmpty());
        EXPECT_THAT(started.mOptions, IsEmpty());
        EXPECT_FALSE(setup.mDialogue.talkedToPlayer(speaker.mReference));
    }

    TEST(FalloutScriptDialogueTest, aResponseSaidOnceIsSaidAgainWhenTheConversationLeftItBeforeItsEnd)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        ESM4::DialogInfo& info = setup.mRecords.addInfo(0x3001, topicA, "once");
        info.mInfoFlags = ESM4::INFO_SayOnce;
        info.mEndScript = logScript("end").definition();
        setup.start();
        setup.mDialogue.start(speaker);
        setup.mDialogue.choose(topicA);

        // Nothing of it was done: not its end script, and it was not said for good
        setup.mDialogue.end();
        EXPECT_THAT(setup.mLogged, IsEmpty());
        EXPECT_FALSE(setup.mDialogue.wasSaid(0x3001, speaker.mReference));

        setup.mDialogue.start(speaker);
        setup.mDialogue.choose(topicA);
        setup.mDialogue.finish();
        EXPECT_THAT(setup.mLogged, ElementsAre("end"));
        EXPECT_TRUE(setup.mDialogue.wasSaid(0x3001, speaker.mReference));
    }

    TEST(FalloutScriptDialogueTest, aConversationWithoutAGreetingGoesStraightToTheTopics)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3002, topicA, "a");
        setup.start();

        const FalloutScript::DialogueView& started = setup.mDialogue.start(speaker);

        EXPECT_THAT(started.mLines, IsEmpty());
        EXPECT_FALSE(started.mSpeaking);
        EXPECT_THAT(setup.topics(started), ElementsAre(topicA));
        // A second start does nothing while the conversation is open
        EXPECT_EQ(&setup.mDialogue.start({ 0x00ff0009, 0 }), &started);
        EXPECT_EQ(setup.mDialogue.speaker().mReference, speaker.mReference);
    }

    TEST(FalloutScriptDialogueTest, thePlayerPicksATopicAndThenSeesTheOptionsAgain)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true, 2);
        setup.mRecords.addInfo(0x3001, topicA, "about a");
        setup.mRecords.addTopic(topicB, "B", "TopicB", true, 1);
        setup.mRecords.addInfo(0x3002, topicB, "about b");
        setup.start();
        setup.mDialogue.start(speaker);

        // Not a topic on offer, and not while a response is being said
        EXPECT_THAT(setup.texts(setup.mDialogue.choose(hidden)), IsEmpty());
        EXPECT_THAT(setup.texts(setup.mDialogue.choose(topicB)), ElementsAre("about b"));
        EXPECT_TRUE(setup.mDialogue.view().mSpeaking);
        EXPECT_THAT(setup.texts(setup.mDialogue.choose(topicA)), ElementsAre("about b"));

        const FalloutScript::DialogueView& finished = setup.mDialogue.finish();
        EXPECT_THAT(setup.topics(finished), ElementsAre(topicA, topicB));
        EXPECT_TRUE(setup.mDialogue.active());
    }

    TEST(FalloutScriptDialogueTest, aResponseListsTheTopicsThePlayerCanPickAfterIt)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true, 3);
        setup.mRecords.addInfo(0x3001, topicA, "a").mChoices = { id(hidden), id(topicC), id(topicC) };
        setup.mRecords.addTopic(topicB, "B", "TopicB", true, 2);
        setup.mRecords.addInfo(0x3002, topicB, "b");
        setup.mRecords.addTopic(topicC, "C", "TopicC", false);
        setup.mRecords.addInfo(0x3003, topicC, "c");
        setup.mRecords.addTopic(hidden, "H", "Hidden", false);
        setup.mRecords.addInfo(0x3004, hidden, "h");
        setup.start();
        setup.mDialogue.start(speaker);

        setup.mDialogue.choose(topicA);
        const FalloutScript::DialogueView& finished = setup.mDialogue.finish();

        // In the order the response lists them, each once, in place of the list at the top
        EXPECT_THAT(setup.topics(finished), ElementsAre(hidden, topicC));
    }

    TEST(FalloutScriptDialogueTest, choicesThatCannotBeFollowedLeaveTheListAtTheTop)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3001, topicA, "a").mChoices = { id(topicC) };
        setup.mRecords.addTopic(topicC, "C", "TopicC", false);
        setup.mRecords.addInfo(0x3003, topicC, "c").mConditions.push_back(stageIs(runningQuest, 9));
        setup.start();
        setup.mDialogue.start(speaker);

        setup.mDialogue.choose(topicA);

        EXPECT_THAT(setup.topics(setup.mDialogue.finish()), ElementsAre(topicA));
    }

    TEST(FalloutScriptDialogueTest, aResponseGivesThePlayerTheTopicsItNames)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3001, topicA, "a").mAddTopics = { id(hidden) };
        setup.mRecords.addTopic(hidden, "H", "Hidden", false);
        setup.mRecords.addInfo(0x3004, hidden, "h");
        setup.start();
        setup.mDialogue.start(speaker);
        EXPECT_FALSE(setup.mDialogue.hasTopic(hidden));

        setup.mDialogue.choose(topicA);
        // Not before the response is over
        EXPECT_FALSE(setup.mDialogue.hasTopic(hidden));

        EXPECT_THAT(setup.topics(setup.mDialogue.finish()), ElementsAre(topicA, hidden));
        EXPECT_TRUE(setup.mDialogue.hasTopic(hidden));

        setup.start();
        EXPECT_FALSE(setup.mDialogue.hasTopic(hidden));
    }

    TEST(FalloutScriptDialogueTest, aResponseFlaggedGoodbyeEndsTheConversation)
    {
        Harness setup;
        setup.mRecords.addTopic(goodbye, "Goodbye", "GOODBYE", true);
        setup.mRecords.addInfo(0x3001, goodbye, "Farewell").mInfoFlags = ESM4::INFO_Goodbye;
        setup.start();
        setup.mDialogue.start(speaker);

        setup.mDialogue.choose(goodbye);
        const FalloutScript::DialogueView& finished = setup.mDialogue.finish();

        EXPECT_TRUE(finished.mEnded);
        EXPECT_THAT(finished.mOptions, IsEmpty());
        EXPECT_FALSE(setup.mDialogue.active());
        // A new conversation can start
        EXPECT_TRUE(setup.mDialogue.start(speaker).mOptions.size() == 1);
    }

    TEST(FalloutScriptDialogueTest, aResponseThatGoesOnIsFollowedByTheFirstFollowUpThatCanBeSaid)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3001, topicA, "first").mEndScript = logScript("end first").definition();
        ESM4::DialogInfo& asleep = setup.mRecords.addInfo(0x3002, topicA, "asleep", 0x3001);
        asleep.mQuest = id(sleepingQuest);
        setup.mRecords.addInfo(0x3003, topicA, "second", 0x3002);
        setup.mRecords.addInfo(0x3004, topicA, "third", 0x3003);
        setup.mRecords.mInfos[0].mFollowUps = { id(0x3999), id(0x3002), id(0x3003) };
        setup.mRecords.mInfos[2].mFollowUps = { id(0x3004) };
        setup.start();
        setup.mDialogue.start(speaker);

        EXPECT_THAT(setup.texts(setup.mDialogue.choose(topicA)), ElementsAre("first"));
        // Not there and not running are passed over; the player has no say in between
        const FalloutScript::DialogueView& second = setup.mDialogue.finish();
        EXPECT_THAT(setup.texts(second), ElementsAre("second"));
        EXPECT_TRUE(second.mSpeaking);
        EXPECT_THAT(second.mOptions, IsEmpty());
        EXPECT_THAT(setup.mLogged, ElementsAre("end first"));
        EXPECT_TRUE(setup.mDialogue.wasSaid(0x3001, speaker.mReference));
        EXPECT_THAT(setup.texts(setup.mDialogue.finish()), ElementsAre("third"));

        // The last one gives the player the topics again
        const FalloutScript::DialogueView& over = setup.mDialogue.finish();
        EXPECT_FALSE(over.mSpeaking);
        EXPECT_THAT(setup.topics(over), ElementsAre(topicA));
    }

    TEST(FalloutScriptDialogueTest, followUpsThatNameEachOtherInACircleEndWithTheExchange)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3001, topicA, "ping");
        setup.mRecords.addInfo(0x3002, topicA, "pong", 0x3001);
        setup.mRecords.mInfos[0].mFollowUps = { id(0x3002) };
        setup.mRecords.mInfos[1].mFollowUps = { id(0x3001), id(0x3002) };
        setup.start();
        setup.mDialogue.start(speaker);

        setup.mDialogue.choose(topicA);
        EXPECT_THAT(setup.texts(setup.mDialogue.finish()), ElementsAre("pong"));
        EXPECT_THAT(setup.topics(setup.mDialogue.finish()), ElementsAre(topicA));

        // The next time the player picks the topic the exchange starts again
        EXPECT_THAT(setup.texts(setup.mDialogue.choose(topicA)), ElementsAre("ping"));
        EXPECT_THAT(setup.texts(setup.mDialogue.finish()), ElementsAre("pong"));
    }

    TEST(FalloutScriptDialogueTest, aGoodbyeResponseIsNotFollowedUp)
    {
        Harness setup;
        setup.mRecords.addTopic(goodbye, "Goodbye", "GOODBYE", true);
        setup.mRecords.addInfo(0x3001, goodbye, "Farewell").mInfoFlags = ESM4::INFO_Goodbye;
        setup.mRecords.addInfo(0x3002, goodbye, "more", 0x3001);
        setup.mRecords.mInfos[0].mFollowUps = { id(0x3002) };
        setup.start();
        setup.mDialogue.start(speaker);

        setup.mDialogue.choose(goodbye);

        EXPECT_TRUE(setup.mDialogue.finish().mEnded);
    }

    TEST(FalloutScriptDialogueTest, theScriptsOfAResponseRunWhenItStartsAndWhenItIsFinished)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        ESM4::DialogInfo& info = setup.mRecords.addInfo(0x3001, topicA, "a");
        info.mScript = logScript("begin").definition();
        info.mEndScript = logScript("end").definition();
        info.mAddTopics = { id(topicB) };
        setup.mRecords.addTopic(topicB, "B", "TopicB", false);
        setup.mRecords.addInfo(0x3002, topicB, "b");
        setup.start();
        setup.mDialogue.start(speaker);

        setup.mDialogue.choose(topicA);
        EXPECT_THAT(setup.mLogged, ElementsAre("begin"));
        setup.mDialogue.finish();
        EXPECT_THAT(setup.mLogged, ElementsAre("begin", "end"));
        // Finishing twice does not run it twice
        setup.mDialogue.finish();
        EXPECT_THAT(setup.mLogged, ElementsAre("begin", "end"));
    }

    TEST(FalloutScriptDialogueTest, endingAConversationInTheMiddleOfAResponseDoesNotRunItsEndScript)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        ESM4::DialogInfo& info = setup.mRecords.addInfo(0x3001, topicA, "a");
        info.mScript = logScript("begin").definition();
        info.mEndScript = logScript("end").definition();
        setup.start();
        setup.mDialogue.start(speaker);
        setup.mDialogue.choose(topicA);

        setup.mDialogue.end();

        EXPECT_FALSE(setup.mDialogue.active());
        EXPECT_THAT(setup.mLogged, ElementsAre("begin"));
        EXPECT_TRUE(setup.mDialogue.view().mEnded);
    }

    TEST(FalloutScriptDialogueTest, aResponseSaidOutsideAConversationRunsBothScriptsAtOnce)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", false);
        ESM4::DialogInfo& info = setup.mRecords.addInfo(0x3001, topicA, "a line");
        info.mScript = logScript("begin").definition();
        info.mEndScript = logScript("end").definition();
        info.mAddTopics = { id(topicB) };
        setup.start();

        const FalloutScript::DialogueView view = setup.mDialogue.say(speaker, topicA);

        EXPECT_THAT(setup.texts(view), ElementsAre("a line"));
        EXPECT_FALSE(view.mSpeaking);
        EXPECT_THAT(setup.mLogged, ElementsAre("begin", "end"));
        EXPECT_TRUE(setup.mDialogue.hasTopic(topicB));
        EXPECT_FALSE(setup.mDialogue.active());
        EXPECT_THAT(setup.texts(setup.mDialogue.say(speaker, topicC)), IsEmpty());
    }

    TEST(FalloutScriptDialogueTest, theEndScriptOfAResponseSaidOutsideAConversationSeesWhatWasSaid)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", false);
        ESM4::DialogInfo& info = setup.mRecords.addInfo(0x3001, topicA, "a line");
        info.mResponses[0].mEmotionType = 3;
        info.mResponses[0].mEmotionValue = 60;
        ScriptBuilder script;
        script.code(command(OpTouch, arguments(0)));
        info.mEndScript = script.definition();
        std::vector<double> seen;
        setup.mInterpreter.setHandler("Touch", [&](FalloutScript::CallContext&) {
            seen.push_back(setup.mInterpreter.call(OpGetDialogueEmotion, 0, false, {}));
            seen.push_back(setup.mInterpreter.call(OpGetDialogueEmotionValue, 0, false, {}));
            return 0.0;
        });
        setup.start();

        setup.mDialogue.say(speaker, topicA);

        EXPECT_THAT(seen, ElementsAre(3.0, 60.0));
        // and what it saw is not left behind
        EXPECT_THAT(setup.texts(setup.mDialogue.view()), IsEmpty());
    }

    TEST(FalloutScriptDialogueTest, aLineKeepsWhatTheRecordSaysAboutHowItIsSaid)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", false);
        ESM4::DialogInfo& info = setup.mRecords.addInfo(0x3001, topicA, "first");
        info.mResponses[0].mEmotionType = 5;
        info.mResponses[0].mEmotionValue = 40;
        info.mResponses[0].mSound = id(0x00005000);
        info.mResponses[0].mSpeakerAnimation = id(0x00005001);
        ESM4::DialogResponse& second = info.mResponses.emplace_back();
        second.mText = "second";
        second.mNumber = 2;
        second.mListenerAnimation = id(0x00005002);
        setup.start();

        const FalloutScript::DialogueView view = setup.mDialogue.say(speaker, topicA);

        ASSERT_EQ(view.mLines.size(), 2u);
        EXPECT_EQ(view.mLines[0].mNumber, 1);
        EXPECT_EQ(view.mLines[0].mEmotion, 5u);
        EXPECT_EQ(view.mLines[0].mEmotionValue, 40);
        EXPECT_EQ(view.mLines[0].mSound, 0x5000u);
        EXPECT_EQ(view.mLines[0].mSpeakerAnimation, 0x5001u);
        EXPECT_EQ(view.mLines[0].mInfo, 0x3001u);
        EXPECT_EQ(view.mLines[1].mNumber, 2);
        EXPECT_EQ(view.mLines[1].mListenerAnimation, 0x5002u);
        EXPECT_EQ(view.mTopic, topicA);
        EXPECT_EQ(view.mInfo, 0x3001u);
    }

    TEST(FalloutScriptDialogueTest, scriptsCanGiveTopicsAndAskWhoTalkedToThePlayer)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", false);
        setup.mRecords.addInfo(0x3001, topicA, "a");
        setup.start();

        setup.mInterpreter.call(0x1058, 0, false, { { static_cast<double>(topicA), {}, false } });
        EXPECT_TRUE(setup.mDialogue.hasTopic(topicA));

        EXPECT_EQ(setup.mInterpreter.call(0x1032, speaker.mReference, true, {}), 0.0);
        setup.mDialogue.start(speaker);
        EXPECT_EQ(setup.mInterpreter.call(0x1032, speaker.mReference, true, {}), 1.0);
        EXPECT_EQ(setup.mInterpreter.call(0x1032, 0x00ff0002, true, {}), 0.0);
    }

    TEST(FalloutScriptDialogueTest, aConversationCanStartWithATopicInPlaceOfTheGreeting)
    {
        Harness setup;
        setup.mRecords.addTopic(greeting, "", "Greeting", false);
        setup.mRecords.addInfo(0x3000, greeting, "hello");
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3001, topicA, "about a");
        setup.start();

        const FalloutScript::DialogueView& view = setup.mDialogue.start(speaker, topicA);

        EXPECT_THAT(setup.texts(view), ElementsAre("about a"));
        EXPECT_TRUE(view.mSpeaking);
        EXPECT_THAT(setup.topics(setup.mDialogue.finish()), ElementsAre(topicA));
    }

    TEST(FalloutScriptDialogueTest, scriptsCanAskWhoIsTalkingAndWhoTalkedToThePlayer)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA", true);
        setup.mRecords.addInfo(0x3001, topicA, "a");
        setup.start();

        EXPECT_EQ(setup.mInterpreter.call(0x108D, speaker.mReference, true, {}), 0.0);
        EXPECT_EQ(setup.mInterpreter.call(0x10AC, 0, false, { { double(speaker.mReference), {}, false } }), 0.0);

        setup.mDialogue.start(speaker);

        EXPECT_EQ(setup.mInterpreter.call(0x108D, speaker.mReference, true, {}), 1.0);
        EXPECT_EQ(setup.mInterpreter.call(0x108D, 0x00ff0002, true, {}), 0.0);
        EXPECT_EQ(setup.mInterpreter.call(0x10AC, 0, false, { { double(speaker.mReference), {}, false } }), 1.0);

        setup.mDialogue.end();
        EXPECT_EQ(setup.mInterpreter.call(0x108D, speaker.mReference, true, {}), 0.0);
    }

    TEST(FalloutScriptDialogueTest, findsATopicByItsEditorIdWhateverTheCase)
    {
        Harness setup;
        setup.mRecords.addTopic(topicA, "A", "TopicA");
        setup.start();

        EXPECT_EQ(setup.mDialogue.findTopic("topica"), topicA);
        EXPECT_EQ(setup.mDialogue.findTopic("TOPICB"), 0u);
        EXPECT_NE(setup.mDialogue.topic(topicA), nullptr);
        EXPECT_EQ(setup.mDialogue.topic(topicB), nullptr);
    }
}
