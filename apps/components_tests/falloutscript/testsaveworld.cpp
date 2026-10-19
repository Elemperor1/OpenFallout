#include "scriptbuilder.hpp"

#include <components/falloutscript/animationcommands.hpp>
#include <components/falloutscript/dialogue.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/saveworld.hpp>
#include <components/falloutscript/triggervolumes.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::DialogueSpeaker;
    using FalloutScript::FormId;
    using FalloutScript::OpenStates;
    using FalloutScript::Point;
    using FalloutScript::SaveReader;
    using FalloutScript::SaveWriter;
    using FalloutScript::TriggerBody;
    using FalloutScript::TriggerVolumes;
    using FalloutScript::Volume;

    constexpr FormId quest = 0x00001000;
    constexpr FormId topicA = 0x00002001;
    constexpr FormId topicB = 0x00002002;
    constexpr FormId topicC = 0x00002003;
    constexpr FormId goneTopic = 0x05002004;
    constexpr DialogueSpeaker speaker{ 0x00ff0001, 0x00003000 };
    constexpr DialogueSpeaker otherSpeaker{ 0x00ff0002, 0x00003001 };
    constexpr FormId trigger = 0x00ff0100;
    constexpr FormId otherTrigger = 0x00ff0101;
    constexpr FormId volumeless = 0x00ff0102;
    constexpr FormId player = 0x00000014;
    constexpr FormId guard = 0x00ff0200;
    constexpr FormId raider = 0x00ff0201;
    constexpr FormId door = 0x00ff0300;
    constexpr FormId lever = 0x00ff0301;

    ESM::FormId id(FormId value)
    {
        return ESM::FormId::fromUint32(value);
    }

    class TestRecords : public FalloutScript::QuestRecords, public FalloutScript::DialogueRecords
    {
    public:
        std::deque<ESM4::Quest> mQuests;
        std::deque<ESM4::Dialogue> mTopics;
        std::deque<ESM4::DialogInfo> mInfos;

        const ESM4::Script* findScript(FormId) const override { return nullptr; }

        void forEachQuest(const std::function<void(const ESM4::Quest&)>& function) const override
        {
            for (const ESM4::Quest& record : mQuests)
                function(record);
        }

        void forEachTopic(const std::function<void(const ESM4::Dialogue&)>& function) const override
        {
            for (const ESM4::Dialogue& record : mTopics)
                function(record);
        }

        void forEachInfo(const std::function<void(const ESM4::DialogInfo&)>& function) const override
        {
            for (const ESM4::DialogInfo& record : mInfos)
                function(record);
        }

        void addTopic(FormId topic, std::string name)
        {
            ESM4::Dialogue& record = mTopics.emplace_back();
            record.mId = id(topic);
            record.mTopicName = std::move(name);
        }

        ESM4::DialogInfo& addInfo(FormId info, FormId topic, std::string text)
        {
            ESM4::DialogInfo& record = mInfos.emplace_back();
            record.mId = id(info);
            record.mTopic = id(topic);
            record.mQuest = id(quest);
            ESM4::DialogResponse& line = record.mResponses.emplace_back();
            line.mText = std::move(text);
            line.mNumber = 1;
            return record;
        }
    };

    struct Dialogue
    {
        FalloutScript::CommandTable mCommands = testCommands();
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FalloutScript::ConditionEvaluator mConditions{ mInterpreter };
        TestRecords mRecords;
        FalloutScript::QuestManager mQuests{ mInterpreter, mRecords };
        FalloutScript::DialogueManager mDialogue{ mInterpreter, mConditions, mQuests, mRecords };
        double mHours = 34;

        Dialogue()
        {
            ESM4::Quest& running = mRecords.mQuests.emplace_back();
            running.mId = id(quest);
            running.mData.flags = ESM4::Quest::Flag_StartGameEnabled;
            mRecords.addTopic(topicA, "A");
            mRecords.addTopic(topicB, "B");
            mRecords.addInfo(0x3001, topicA, "once").mInfoFlags = ESM4::INFO_SayOnce;
            mRecords.addInfo(0x3002, topicA, "daily").mInfoFlags = ESM4::INFO_SayOnceADay;
            mRecords.addInfo(0x3003, topicA, "always");
            mRecords.addInfo(0x3004, topicB, "something to pick");
            mDialogue.setGameHours([this] { return mHours; });
            mQuests.reset();
            mDialogue.reset();
        }

        std::vector<std::string> say(const DialogueSpeaker& who, FormId topic)
        {
            std::vector<std::string> result;
            for (const FalloutScript::DialogueLine& line : mDialogue.say(who, topic).mLines)
                result.push_back(line.mText);
            return result;
        }
    };

    struct Raised
    {
        FormId mTrigger;
        std::uint16_t mBlock;
        FormId mActor;

        bool operator==(const Raised& other) const
        {
            return mTrigger == other.mTrigger && mBlock == other.mBlock && mActor == other.mActor;
        }
        friend std::ostream& operator<<(std::ostream& out, const Raised& value)
        {
            return out << std::hex << value.mTrigger << "/" << value.mBlock << "/" << value.mActor;
        }
    };

    Volume box(float x, float y, float z, float half)
    {
        Volume result;
        result.mShape = FalloutScript::VolumeShape::Box;
        result.mExtents = { half, half, half };
        result.mCenter = { x, y, z };
        return result;
    }

    TriggerBody standingAt(FormId reference, float x, float y, float z)
    {
        TriggerBody result;
        result.mReference = reference;
        result.mFeet = { x, y, z };
        result.mHeight = 100;
        result.mRadius = 20;
        return result;
    }

    TEST(FalloutScriptSaveWorldTest, theTopicsTheResponsesAndTheTalksComeBack)
    {
        Dialogue game;
        game.mDialogue.addTopic(topicB);
        game.mDialogue.addTopic(topicC);
        EXPECT_THAT(game.say(speaker, topicA), ElementsAre("once"));
        EXPECT_THAT(game.say(speaker, topicA), ElementsAre("daily"));
        EXPECT_THAT(game.say(otherSpeaker, topicA), ElementsAre("once"));
        game.mDialogue.start(otherSpeaker);
        game.mDialogue.end();
        EXPECT_TRUE(game.mDialogue.talkedToPlayer(otherSpeaker.mReference));

        SaveWriter out;
        FalloutScript::writeDialogue(game.mDialogue, out);

        Dialogue loaded;
        SaveReader in(out.bytes());
        FalloutScript::readDialogue(loaded.mDialogue, in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_TRUE(loaded.mDialogue.hasTopic(topicB));
        EXPECT_TRUE(loaded.mDialogue.hasTopic(topicC));
        EXPECT_FALSE(loaded.mDialogue.hasTopic(topicA));
        EXPECT_TRUE(loaded.mDialogue.wasSaid(0x3001, speaker.mReference));
        EXPECT_TRUE(loaded.mDialogue.wasSaid(0x3001, otherSpeaker.mReference));
        EXPECT_FALSE(loaded.mDialogue.wasSaid(0x3003, speaker.mReference));
        EXPECT_TRUE(loaded.mDialogue.talkedToPlayer(otherSpeaker.mReference));
        EXPECT_FALSE(loaded.mDialogue.talkedToPlayer(speaker.mReference));

        // What is said is what the game would say: the response said once is not said again, the one said once a day
        // waits for the interval from the time it was said
        EXPECT_THAT(loaded.say(speaker, topicA), ElementsAre("always"));
        loaded.mHours += 12;
        EXPECT_THAT(loaded.say(speaker, topicA), ElementsAre("daily"));

        SaveWriter again;
        Dialogue same;
        SaveReader inAgain(out.bytes());
        FalloutScript::readDialogue(same.mDialogue, inAgain, 1);
        FalloutScript::writeDialogue(same.mDialogue, again);
        EXPECT_EQ(again.bytes(), out.bytes());
    }

    TEST(FalloutScriptSaveWorldTest, readingTheDialogueReplacesWhatTheGameHad)
    {
        Dialogue game;
        game.mDialogue.addTopic(topicB);
        EXPECT_THAT(game.say(speaker, topicA), ElementsAre("once"));
        SaveWriter empty;
        {
            Dialogue fresh;
            FalloutScript::writeDialogue(fresh.mDialogue, empty);
        }
        SaveReader in(empty.bytes());
        FalloutScript::readDialogue(game.mDialogue, in, 1);
        EXPECT_FALSE(game.mDialogue.hasTopic(topicB));
        EXPECT_FALSE(game.mDialogue.wasSaid(0x3001, speaker.mReference));
        EXPECT_THAT(game.say(speaker, topicA), ElementsAre("once"));
    }

    TEST(FalloutScriptSaveWorldTest, theTopicsOfAPluginThatIsGoneAreLeftOut)
    {
        Dialogue game;
        game.mDialogue.addTopic(topicB);
        game.mDialogue.addTopic(goneTopic);
        SaveWriter out;
        FalloutScript::writeDialogue(game.mDialogue, out);

        Dialogue loaded;
        SaveReader in(out.bytes(), [](FormId saved) -> std::optional<FormId> {
            if ((saved >> 24) == 5)
                return std::nullopt;
            return saved;
        });
        FalloutScript::readDialogue(loaded.mDialogue, in, 1);
        EXPECT_TRUE(loaded.mDialogue.hasTopic(topicB));
        EXPECT_FALSE(loaded.mDialogue.hasTopic(goneTopic));
    }

    TEST(FalloutScriptSaveWorldTest, whoStandsInTheVolumeOfATriggerComesBackWhenTheVolumeIsPutIn)
    {
        TriggerVolumes triggers;
        triggers.set(trigger, box(0, 0, 50, 100));
        triggers.set(otherTrigger, box(1000, 0, 50, 100));
        triggers.set(volumeless, box(2000, 0, 50, 100));
        const auto active = [](FormId) { return true; };
        const TriggerVolumes::Raise raise = [](FormId, std::uint16_t, FormId) {};
        // Two actors enter the first volume, one the second, and the third volume has nobody. The first volume raises
        // one Enter on a pass, so the guard waits for the next one
        triggers.update({ standingAt(player, 0, 0, 0), standingAt(guard, 10, 0, 0), standingAt(raider, 1000, 0, 0) },
            active, raise);
        ASSERT_THAT(triggers.occupants(trigger), ElementsAre(player, guard));
        ASSERT_THAT(triggers.occupants(otherTrigger), ElementsAre(raider));

        // A loaded game that has not put the volume of a trigger in yet writes what it was told of it
        const std::map<FormId, FalloutScript::SavedTrigger> waiting{ { 0x00ff0400, { { player }, {} } } };
        SaveWriter out;
        FalloutScript::writeTriggers(triggers, waiting, out);
        SaveReader in(out.bytes());
        const std::map<FormId, FalloutScript::SavedTrigger> read = FalloutScript::readTriggers(in, 1);
        EXPECT_TRUE(in.atEnd());
        ASSERT_THAT(read, ElementsAre(Key(trigger), Key(otherTrigger), Key(0x00ff0400u)));
        EXPECT_THAT(read.at(trigger).mOccupants, ElementsAre(player, guard));
        EXPECT_THAT(read.at(otherTrigger).mOccupants, ElementsAre(raider));
        EXPECT_THAT(read.at(0x00ff0400u).mOccupants, ElementsAre(player));
        EXPECT_THAT(read.at(0x00ff0400u).mQueue, IsEmpty());

        // The volumes come from the cells: after a load the player and the guard are still inside, so nobody enters
        // again, and the Enter of the guard that had not been raised is raised after the OnTrigger blocks of the pass
        TriggerVolumes loaded;
        loaded.set(trigger, box(0, 0, 50, 100));
        FalloutScript::restoreTrigger(loaded, trigger, read.at(trigger));
        EXPECT_THAT(loaded.occupants(trigger), ElementsAre(player, guard));
        std::vector<Raised> events;
        loaded.update({ standingAt(player, 0, 0, 0), standingAt(guard, 10, 0, 0) }, active,
            [&](FormId which, std::uint16_t block, FormId actor) { events.push_back({ which, block, actor }); });
        EXPECT_THAT(events,
            ElementsAre(Raised{ trigger, FalloutScript::BlockType::OnTrigger, player },
                Raised{ trigger, FalloutScript::BlockType::OnTrigger, guard },
                Raised{ trigger, FalloutScript::BlockType::OnTriggerEnter, guard }));
    }

    TEST(FalloutScriptSaveWorldTest, theEventsATriggerHasNotRaisedYetComeBack)
    {
        const auto active = [](FormId) { return true; };
        std::vector<Raised> events;
        const TriggerVolumes::Raise record
            = [&](FormId which, std::uint16_t block, FormId actor) { events.push_back({ which, block, actor }); };
        TriggerVolumes triggers;
        triggers.set(trigger, box(0, 0, 50, 100));
        // Three actors enter on one pass: the player's Enter is raised and the other two wait
        triggers.update(
            { standingAt(player, 0, 0, 0), standingAt(guard, 10, 0, 0), standingAt(raider, 20, 0, 0) }, active, record);
        ASSERT_THAT(events, ElementsAre(Raised{ trigger, FalloutScript::BlockType::OnTriggerEnter, player }));
        // Then all of them leave at once: the guard's Enter is raised, and the raider's Enter and the three Leaves wait
        events.clear();
        triggers.update({}, active, record);
        ASSERT_THAT(events, ElementsAre(Raised{ trigger, FalloutScript::BlockType::OnTriggerEnter, guard }));
        ASSERT_THAT(triggers.occupants(trigger), IsEmpty());

        SaveWriter out;
        FalloutScript::writeTriggers(triggers, {}, out);
        SaveReader in(out.bytes());
        const std::map<FormId, FalloutScript::SavedTrigger> read = FalloutScript::readTriggers(in, 1);
        EXPECT_TRUE(in.atEnd());
        ASSERT_THAT(read, ElementsAre(Key(trigger)));
        EXPECT_THAT(read.at(trigger).mOccupants, IsEmpty());
        EXPECT_THAT(read.at(trigger).mQueue.size(), 4u);

        // The loaded game raises them one on each pass, in the order the saved one would have
        TriggerVolumes loaded;
        loaded.set(trigger, box(0, 0, 50, 100));
        FalloutScript::restoreTrigger(loaded, trigger, read.at(trigger));
        std::vector<Raised> after;
        const TriggerVolumes::Raise again
            = [&](FormId which, std::uint16_t block, FormId actor) { after.push_back({ which, block, actor }); };
        events.clear();
        for (int pass = 0; pass < 5; ++pass)
        {
            triggers.update({}, active, record);
            loaded.update({}, active, again);
        }
        EXPECT_THAT(after, ElementsAreArray(events));
        EXPECT_EQ(after.size(), 4u);
    }

    TEST(FalloutScriptSaveWorldTest, anActorThatLeftWhileSavedIsNotKeptInsideAfterTheLoad)
    {
        TriggerVolumes loaded;
        loaded.set(trigger, box(0, 0, 50, 100));
        FalloutScript::restoreTrigger(loaded, trigger, { { player, guard }, {} });
        // The player is still there and the guard is not: the guard leaves and the player does not enter
        std::vector<Raised> events;
        loaded.update(
            { standingAt(player, 0, 0, 0) }, [](FormId) { return true; },
            [&](FormId which, std::uint16_t block, FormId actor) { events.push_back({ which, block, actor }); });
        EXPECT_THAT(events,
            ElementsAre(Raised{ trigger, FalloutScript::BlockType::OnTrigger, player },
                Raised{ trigger, FalloutScript::BlockType::OnTriggerLeave, guard }));
        EXPECT_THAT(loaded.occupants(trigger), ElementsAre(player));
    }

    TEST(FalloutScriptSaveWorldTest, theOccupantsOfATriggerWhoseVolumeIsNotLoadedAreNotInvented)
    {
        TriggerVolumes loaded;
        FalloutScript::restoreTrigger(loaded, trigger, { { player }, {} });
        EXPECT_FALSE(loaded.has(trigger));
        EXPECT_THAT(loaded.occupants(trigger), IsEmpty());
    }

    TEST(FalloutScriptSaveWorldTest, anEmptyTriggerSectionReadsAsNothing)
    {
        TriggerVolumes triggers;
        triggers.set(trigger, box(0, 0, 50, 100));
        SaveWriter out;
        FalloutScript::writeTriggers(triggers, {}, out);
        SaveReader in(out.bytes());
        EXPECT_THAT(FalloutScript::readTriggers(in, 1), IsEmpty());
        EXPECT_TRUE(in.atEnd());
    }

    TEST(FalloutScriptSaveWorldTest, theDoorsAndSwitchesThatRestOpenComeBack)
    {
        OpenStates states;
        states.rest(door, true);
        states.rest(lever, false);
        SaveWriter out;
        FalloutScript::writeOpenStates(states, out);

        OpenStates loaded;
        loaded.rest(raider, true);
        SaveReader in(out.bytes());
        FalloutScript::readOpenStates(loaded, in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_TRUE(loaded.restsOpen(door));
        EXPECT_FALSE(loaded.restsOpen(lever));
        EXPECT_TRUE(loaded.knows(door));
        EXPECT_TRUE(loaded.knows(lever));
        // What the game had is replaced
        EXPECT_FALSE(loaded.knows(raider));
        SaveWriter again;
        FalloutScript::writeOpenStates(loaded, again);
        EXPECT_EQ(again.bytes(), out.bytes());
    }

    TEST(FalloutScriptSaveWorldTest, theOpenStatesOfAPluginThatIsGoneAreLeftOut)
    {
        OpenStates states;
        states.rest(door, true);
        states.rest(0x05000300, true);
        SaveWriter out;
        FalloutScript::writeOpenStates(states, out);

        OpenStates loaded;
        SaveReader in(out.bytes(), [](FormId saved) -> std::optional<FormId> {
            if ((saved >> 24) == 5)
                return std::nullopt;
            return saved;
        });
        FalloutScript::readOpenStates(loaded, in, 1);
        EXPECT_TRUE(loaded.knows(door));
        EXPECT_FALSE(loaded.knows(0x05000300));
    }

    TEST(FalloutScriptSaveWorldTest, aTruncatedSectionIsAnError)
    {
        OpenStates states;
        states.rest(door, true);
        SaveWriter out;
        FalloutScript::writeOpenStates(states, out);
        std::string bytes = out.bytes();
        bytes.pop_back();
        bytes.pop_back();
        OpenStates loaded;
        SaveReader in(bytes);
        EXPECT_THROW(FalloutScript::readOpenStates(loaded, in, 1), FalloutScript::SaveError);
    }
}
