#include "scriptbuilder.hpp"

#include <components/falloutscript/quests.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::Interpreter;
    using FalloutScript::QuestManager;

    constexpr FormId questA = 0x00001000;
    constexpr FormId questB = 0x00001001;
    constexpr FormId questC = 0x00001002;
    constexpr FormId scriptOfA = 0x00002000;
    constexpr std::uint16_t GameMode = 0;

    class TestRecords : public FalloutScript::QuestRecords
    {
    public:
        std::deque<ESM4::Quest> mQuests; // the addresses of the records must not change
        std::map<FormId, ESM4::Script> mScripts;

        const ESM4::Script* findScript(FormId id) const override
        {
            const auto it = mScripts.find(id);
            return it == mScripts.end() ? nullptr : &it->second;
        }

        void forEachQuest(const std::function<void(const ESM4::Quest&)>& function) const override
        {
            for (const ESM4::Quest& quest : mQuests)
                function(quest);
        }

        ESM4::Quest& addQuest(FormId id, std::uint8_t flags = 0, float delay = 0)
        {
            ESM4::Quest& quest = mQuests.emplace_back();
            quest.mId = ESM::FormId::fromUint32(id);
            quest.mData.flags = flags;
            quest.mData.questDelay = delay;
            return quest;
        }

        static ESM4::QuestLogEntry& addEntry(ESM4::Quest& quest, int stage, std::string text = {},
            std::uint8_t flags = 0, const ScriptBuilder* script = nullptr)
        {
            ESM4::QuestStage* found = nullptr;
            for (ESM4::QuestStage& candidate : quest.mStages)
                if (candidate.mIndex == stage)
                    found = &candidate;
            if (found == nullptr)
            {
                found = &quest.mStages.emplace_back();
                found->mIndex = static_cast<std::uint16_t>(stage);
            }
            ESM4::QuestLogEntry& entry = found->mLogEntries.emplace_back();
            entry.mText = std::move(text);
            entry.mFlags = flags;
            if (script != nullptr)
                entry.mScript = script->definition();
            return entry;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = testCommands();
        TestHost mHost;
        Interpreter mInterpreter{ mHost, mCommands };
        TestRecords mRecords;
        QuestManager mQuests{ mInterpreter, mRecords };
        std::vector<std::string> mLogged; // what Log was given

        Runner()
        {
            mInterpreter.setHandler("Log", [this](FalloutScript::CallContext& call) {
                mLogged.push_back(call.mArguments.at(0).mText);
                return 0.0;
            });
            FalloutScript::addQuestCommands(mInterpreter, mQuests);
        }

        /// Makes the manager read the records, and lets scripts find the variables of quests
        void start()
        {
            mQuests.reset();
            for (const ESM4::Quest& quest : mRecords.mQuests)
            {
                const FormId id = quest.mId.toUint32();
                mHost.mInstances[id] = mQuests.instance(id);
            }
        }
    };

    Bytes logCall(const std::string& text)
    {
        return command(OpLog, arguments(1).text(text));
    }

    Bytes questCall(std::uint16_t opcode, std::uint16_t slot, const Bytes& rest = {}, std::uint16_t count = 1)
    {
        return command(opcode, arguments(count).add(refArg(slot)).add(rest));
    }

    TEST(FalloutScriptQuestsTest, quest_that_is_enabled_at_the_start_of_the_game_is_running)
    {
        Runner setup;
        setup.mRecords.addQuest(questA, ESM4::Quest::Flag_StartGameEnabled);
        setup.mRecords.addQuest(questB);
        setup.start();

        EXPECT_TRUE(setup.mQuests.running(questA));
        EXPECT_FALSE(setup.mQuests.running(questB));
        EXPECT_TRUE(setup.mQuests.known(questB));
        EXPECT_FALSE(setup.mQuests.known(questC));
        EXPECT_EQ(setup.mQuests.stage(questA), 0);
    }

    TEST(FalloutScriptQuestsTest, setting_a_stage_marks_it_done_and_starts_the_quest)
    {
        Runner setup;
        ESM4::Quest& quest = setup.mRecords.addQuest(questA);
        TestRecords::addEntry(quest, 10, "Go to Goodsprings");
        TestRecords::addEntry(quest, 20);
        setup.start();

        EXPECT_TRUE(setup.mQuests.setStage(questA, 10));
        EXPECT_EQ(setup.mQuests.stage(questA), 10);
        EXPECT_TRUE(setup.mQuests.stageDone(questA, 10));
        EXPECT_FALSE(setup.mQuests.stageDone(questA, 20));
        EXPECT_TRUE(setup.mQuests.running(questA));

        const FalloutScript::QuestState* state = setup.mQuests.state(questA);
        ASSERT_NE(state, nullptr);
        ASSERT_EQ(state->mJournal.size(), 1u);
        EXPECT_EQ(state->mJournal[0].mText, "Go to Goodsprings");
        EXPECT_EQ(state->mJournal[0].mStage, 10);
    }

    TEST(FalloutScriptQuestsTest, a_stage_the_quest_does_not_have_does_nothing)
    {
        Runner setup;
        TestRecords::addEntry(setup.mRecords.addQuest(questA), 10);
        setup.start();

        EXPECT_FALSE(setup.mQuests.setStage(questA, 15));
        EXPECT_FALSE(setup.mQuests.setStage(questC, 10));
        EXPECT_EQ(setup.mQuests.stage(questA), 0);
        EXPECT_FALSE(setup.mQuests.running(questA));
    }

    TEST(FalloutScriptQuestsTest, the_script_of_a_log_entry_runs_and_can_set_other_stages)
    {
        Runner setup;
        ESM4::Quest& quest = setup.mRecords.addQuest(questA);
        ScriptBuilder first;
        const std::uint16_t self = first.form(questA);
        first.code(logCall("ten") + questCall(OpSetStage, self, intArg(20), 2));
        ScriptBuilder second;
        second.code(logCall("twenty"));
        TestRecords::addEntry(quest, 10, "", 0, &first);
        TestRecords::addEntry(quest, 20, "", 0, &second);
        setup.start();

        setup.mQuests.setStage(questA, 10);

        EXPECT_THAT(setup.mLogged, ElementsAre("ten", "twenty"));
        EXPECT_TRUE(setup.mQuests.stageDone(questA, 20));
        EXPECT_EQ(setup.mQuests.stage(questA), 20);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptQuestsTest, a_script_sets_the_stage_of_a_quest_by_the_command)
    {
        Runner setup;
        TestRecords::addEntry(setup.mRecords.addQuest(questB), 5, "Hello");
        setup.start();
        ScriptBuilder builder;
        const std::uint16_t quest = builder.form(questB);
        builder.code(questCall(OpSetStage, quest, intArg(5), 2));
        FalloutScript::Instance caller(prepare(builder, setup.mCommands), questA);

        setup.mInterpreter.runResult(caller);

        EXPECT_TRUE(setup.mQuests.stageDone(questB, 5));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptQuestsTest, a_stage_is_run_once_unless_the_quest_allows_it_again)
    {
        Runner setup;
        ScriptBuilder script;
        script.code(logCall("stage"));
        TestRecords::addEntry(setup.mRecords.addQuest(questA), 10, "", 0, &script);
        TestRecords::addEntry(setup.mRecords.addQuest(questB, ESM4::Quest::Flag_AllowRepeatStages), 10, "", 0, &script);
        setup.start();

        EXPECT_TRUE(setup.mQuests.setStage(questA, 10));
        EXPECT_FALSE(setup.mQuests.setStage(questA, 10));
        EXPECT_EQ(setup.mLogged.size(), 1u);

        EXPECT_TRUE(setup.mQuests.setStage(questB, 10));
        EXPECT_TRUE(setup.mQuests.setStage(questB, 10));
        EXPECT_EQ(setup.mLogged.size(), 3u);
        EXPECT_EQ(setup.mQuests.state(questB)->mStagesDone.size(), 1u);
    }

    TEST(FalloutScriptQuestsTest, stages_that_set_each_other_are_followed_a_long_way_and_then_stopped)
    {
        Runner setup;
        ESM4::Quest& quest = setup.mRecords.addQuest(questA);
        constexpr int length = 400;
        std::deque<ScriptBuilder> scripts;
        for (int stage = 0; stage < length; ++stage)
        {
            ScriptBuilder& script = scripts.emplace_back();
            const std::uint16_t self = script.form(questA);
            script.code(questCall(OpSetStage, self, intArg(stage + 1), 2));
            TestRecords::addEntry(quest, stage, "", 0, &script);
        }
        TestRecords::addEntry(quest, length);
        setup.start();

        setup.mQuests.setStage(questA, 0);

        const FalloutScript::QuestState* state = setup.mQuests.state(questA);
        ASSERT_NE(state, nullptr);
        EXPECT_GT(state->mStagesDone.size(), 240u);
        EXPECT_LT(state->mStagesDone.size(), static_cast<std::size_t>(length));
        EXPECT_THAT(setup.mHost.mLog, Not(IsEmpty()));
    }

    TEST(FalloutScriptQuestsTest, only_the_entries_whose_conditions_are_met_count)
    {
        Runner setup;
        ESM4::Quest& quest = setup.mRecords.addQuest(questA);
        ScriptBuilder female;
        female.code(logCall("female"));
        ScriptBuilder male;
        male.code(logCall("male"));
        ESM4::QuestLogEntry& first = TestRecords::addEntry(quest, 10, "She goes", 0, &female);
        first.mTargetConditions.push_back({});
        first.mTargetConditions.back().functionIndex = 1;
        ESM4::QuestLogEntry& second = TestRecords::addEntry(quest, 10, "He goes", 0, &male);
        second.mTargetConditions.push_back({});
        second.mTargetConditions.back().functionIndex = 2;
        setup.start();
        setup.mQuests.setConditionCheck([](const std::vector<ESM4::TargetCondition>& conditions) {
            return conditions.empty() || conditions.front().functionIndex == 2;
        });

        setup.mQuests.setStage(questA, 10);

        EXPECT_THAT(setup.mLogged, ElementsAre("male"));
        ASSERT_EQ(setup.mQuests.state(questA)->mJournal.size(), 1u);
        EXPECT_EQ(setup.mQuests.state(questA)->mJournal[0].mText, "He goes");
    }

    TEST(FalloutScriptQuestsTest, the_journal_listener_hears_of_each_entry_with_text)
    {
        Runner setup;
        ESM4::Quest& quest = setup.mRecords.addQuest(questA);
        TestRecords::addEntry(quest, 10, "One");
        TestRecords::addEntry(quest, 20);
        TestRecords::addEntry(quest, 30, "Three");
        setup.start();
        std::vector<std::string> heard;
        setup.mQuests.setJournalListener([&](FormId id, const FalloutScript::JournalEntry& entry) {
            heard.push_back(std::to_string(id) + ":" + entry.mText);
        });

        setup.mQuests.setStage(questA, 10);
        setup.mQuests.setStage(questA, 20);
        setup.mQuests.setStage(questA, 30);

        EXPECT_THAT(heard, ElementsAre(std::to_string(questA) + ":One", std::to_string(questA) + ":Three"));
    }

    TEST(FalloutScriptQuestsTest, an_entry_can_complete_or_fail_the_quest_and_start_the_next)
    {
        Runner setup;
        ESM4::Quest& quest = setup.mRecords.addQuest(questA, ESM4::Quest::Flag_StartGameEnabled);
        TestRecords::addEntry(quest, 10, "", ESM4::QuestLogEntry::Flag_CompleteQuest);
        ESM4::QuestLogEntry& chain = TestRecords::addEntry(quest, 20);
        chain.mNextQuest = ESM::FormId::fromUint32(questB);
        TestRecords::addEntry(quest, 30, "", ESM4::QuestLogEntry::Flag_FailQuest);
        setup.mRecords.addQuest(questB);
        setup.start();

        setup.mQuests.setStage(questA, 20);
        EXPECT_TRUE(setup.mQuests.running(questB));
        EXPECT_FALSE(setup.mQuests.completed(questA));

        setup.mQuests.setStage(questA, 30);
        EXPECT_TRUE(setup.mQuests.state(questA)->mFailed);
        EXPECT_FALSE(setup.mQuests.running(questA));

        setup.mQuests.setStage(questA, 10);
        EXPECT_TRUE(setup.mQuests.completed(questA));
        EXPECT_FALSE(setup.mQuests.running(questA));

        // a quest that is done is not started again by a later stage, but its stages are still set
        setup.mQuests.setStage(questA, 20);
        EXPECT_FALSE(setup.mQuests.running(questA));
    }

    TEST(FalloutScriptQuestsTest, starting_and_stopping_a_quest)
    {
        Runner setup;
        setup.mRecords.addQuest(questA);
        setup.start();

        setup.mQuests.start(questA);
        EXPECT_TRUE(setup.mQuests.running(questA));
        setup.mQuests.stop(questA);
        EXPECT_FALSE(setup.mQuests.running(questA));
        setup.mQuests.complete(questA);
        EXPECT_TRUE(setup.mQuests.completed(questA));
        setup.mQuests.start(questA);
        EXPECT_FALSE(setup.mQuests.running(questA));

        // quests the game does not have are left alone
        setup.mQuests.start(questC);
        EXPECT_FALSE(setup.mQuests.running(questC));
    }

    TEST(FalloutScriptQuestsTest, objectives_are_shown_and_completed_apart_from_each_other)
    {
        Runner setup;
        setup.mRecords.addQuest(questA);
        setup.start();

        setup.mQuests.setObjectiveDisplayed(questA, 10, true);
        EXPECT_TRUE(setup.mQuests.objectiveDisplayed(questA, 10));
        EXPECT_FALSE(setup.mQuests.objectiveCompleted(questA, 10));
        EXPECT_FALSE(setup.mQuests.objectiveDisplayed(questA, 20));

        setup.mQuests.setObjectiveCompleted(questA, 10, true);
        EXPECT_TRUE(setup.mQuests.objectiveCompleted(questA, 10));
        setup.mQuests.setObjectiveDisplayed(questA, 10, false);
        EXPECT_FALSE(setup.mQuests.objectiveDisplayed(questA, 10));
    }

    TEST(FalloutScriptQuestsTest, the_commands_of_a_script_drive_the_quests)
    {
        Runner setup;
        TestRecords::addEntry(setup.mRecords.addQuest(questB), 5);
        setup.start();

        ScriptBuilder builder;
        builder.variable(1, TypeInteger, "done");
        builder.variable(2, TypeInteger, "stage");
        builder.variable(3, TypeInteger, "running");
        builder.variable(4, TypeInteger, "objective");
        const std::uint16_t quest = builder.form(questB);
        const Bytes stageDone = callToken(OpGetStageDone, arguments(2).add(refArg(quest)).add(intArg(5)));
        builder.code(questCall(OpStartQuest, quest) + questCall(OpSetStage, quest, intArg(5), 2)
            + questCall(OpSetObjectiveDisplayed, quest, intArg(1).add(intArg(1)), 3) + setTo(var('s', 1), stageDone)
            + setTo(var('s', 2), callToken(OpGetStage, arguments(1).add(refArg(quest))))
            + setTo(var('s', 3), callToken(OpGetQuestRunning, arguments(1).add(refArg(quest))))
            + setTo(var('s', 4), callToken(OpGetObjectiveDisplayed, arguments(2).add(refArg(quest)).add(intArg(1)))));
        FalloutScript::Instance caller(prepare(builder, setup.mCommands), questA);

        setup.mInterpreter.runResult(caller);

        EXPECT_EQ(caller.variable(1), 1);
        EXPECT_EQ(caller.variable(2), 5);
        EXPECT_EQ(caller.variable(3), 1);
        EXPECT_EQ(caller.variable(4), 1);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptQuestsTest, the_script_of_a_quest_runs_on_the_delay_of_the_quest_while_it_is_running)
    {
        Runner setup;
        ScriptBuilder questScript;
        questScript.mType = 1;
        questScript.variable(1, TypeInteger, "count");
        questScript.code(
            scriptName() + begin(GameMode) + setTo(var('s', 1), var('s', 1) + number("1") + op("+")) + end());
        ESM4::Script& record = setup.mRecords.mScripts[scriptOfA];
        record.mId = ESM::FormId::fromUint32(scriptOfA);
        record.mScript = questScript.definition();
        ESM4::Quest& quest = setup.mRecords.addQuest(questA, ESM4::Quest::Flag_StartGameEnabled, 5.f);
        quest.mQuestScript = ESM::FormId::fromUint32(scriptOfA);
        setup.mRecords.addQuest(questB, 0, 5.f).mQuestScript = ESM::FormId::fromUint32(scriptOfA);
        setup.start();

        FalloutScript::Instance* instance = setup.mQuests.instance(questA);
        ASSERT_NE(instance, nullptr);
        setup.mQuests.update(2.f);
        setup.mQuests.update(2.f);
        EXPECT_EQ(instance->variable(1), 0);
        setup.mQuests.update(2.f);
        EXPECT_EQ(instance->variable(1), 1);
        setup.mQuests.update(5.f);
        EXPECT_EQ(instance->variable(1), 2);

        // a quest that is not running keeps its script still, and the two do not share their variables
        FalloutScript::Instance* other = setup.mQuests.instance(questB);
        ASSERT_NE(other, nullptr);
        EXPECT_NE(other, instance);
        EXPECT_EQ(other->variable(1), 0);
        setup.mQuests.start(questB);
        setup.mQuests.update(5.f);
        EXPECT_EQ(other->variable(1), 1);
        EXPECT_EQ(instance->variable(1), 3);
    }

    TEST(FalloutScriptQuestsTest, a_quest_without_a_delay_runs_its_script_every_time)
    {
        Runner setup;
        ScriptBuilder questScript;
        questScript.mType = 1;
        questScript.variable(1, TypeInteger, "count");
        questScript.code(
            scriptName() + begin(GameMode) + setTo(var('s', 1), var('s', 1) + number("1") + op("+")) + end());
        ESM4::Script& record = setup.mRecords.mScripts[scriptOfA];
        record.mScript = questScript.definition();
        setup.mRecords.addQuest(questA, ESM4::Quest::Flag_StartGameEnabled).mQuestScript
            = ESM::FormId::fromUint32(scriptOfA);
        setup.start();

        setup.mQuests.update(0.01f);
        setup.mQuests.update(0.01f);

        EXPECT_EQ(setup.mQuests.instance(questA)->variable(1), 2);
    }

    TEST(FalloutScriptQuestsTest, a_stage_can_set_the_variables_of_a_quest)
    {
        Runner setup;
        ScriptBuilder questScript;
        questScript.mType = 1;
        questScript.variable(1, TypeInteger, "flag");
        ESM4::Script& record = setup.mRecords.mScripts[scriptOfA];
        record.mScript = questScript.definition();
        setup.mRecords.addQuest(questB).mQuestScript = ESM::FormId::fromUint32(scriptOfA);
        ScriptBuilder stageScript;
        const std::uint16_t other = stageScript.form(questB);
        stageScript.code(setTo(remoteVar(other, 's', 1), number("7")));
        TestRecords::addEntry(setup.mRecords.addQuest(questA), 10, "", 0, &stageScript);
        setup.start();

        setup.mQuests.setStage(questA, 10);

        EXPECT_EQ(setup.mQuests.instance(questB)->variable(1), 7);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptQuestsTest, a_script_that_cannot_be_run_is_reported_and_the_stage_is_still_set)
    {
        Runner setup;
        ScriptBuilder broken;
        broken.code(Bytes().u8(0xFF).u8(0xFF).u8(0xFF).u8(0xFF).u8(0xFF));
        ESM4::Quest& quest = setup.mRecords.addQuest(questA);
        TestRecords::addEntry(quest, 10, "", 0, &broken);
        setup.start();

        EXPECT_TRUE(setup.mQuests.setStage(questA, 10));

        EXPECT_TRUE(setup.mQuests.stageDone(questA, 10));
        EXPECT_THAT(setup.mHost.mLog, SizeIs(1));
    }
}
