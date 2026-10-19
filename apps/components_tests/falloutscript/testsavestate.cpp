#include "scriptbuilder.hpp"

#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/quests.hpp>
#include <components/falloutscript/savescripts.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <deque>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::Instance;
    using FalloutScript::SaveError;
    using FalloutScript::SaveReader;
    using FalloutScript::SaveWriter;

    constexpr FormId questA = 0x01001000;
    constexpr FormId questB = 0x01001001;
    constexpr FormId questFromGonePlugin = 0x05001002;
    constexpr FormId scriptOfA = 0x01002000;
    constexpr FormId crate = 0x01000100;
    constexpr FormId otherCrate = 0x01000101;
    constexpr FormId scriptOfCrates = 0x01003000;
    constexpr FormId otherScript = 0x01003001;
    constexpr FormId somebody = 0x01000200;
    constexpr FormId madeObject = 0xFF000007;
    constexpr std::uint32_t integerVariable = 1;
    constexpr std::uint32_t floatVariable = 0;

    TEST(FalloutScriptSaveStateTest, numbers_texts_and_forms_come_back_as_they_were_written)
    {
        SaveWriter out;
        out.boolean(true);
        out.u8(200);
        out.u16(60000);
        out.u32(4000000000u);
        out.u64(0x1234567890ABCDEFull);
        out.i32(-5);
        out.i64(-1234567890123ll);
        out.f32(1.5f);
        out.f64(-0.1);
        out.string(std::string("two\0words", 9));
        out.string("");
        out.form(0x01000014);
        out.count(3);

        SaveReader in(out.bytes());
        EXPECT_TRUE(in.boolean());
        EXPECT_EQ(in.u8(), 200);
        EXPECT_EQ(in.u16(), 60000);
        EXPECT_EQ(in.u32(), 4000000000u);
        EXPECT_EQ(in.u64(), 0x1234567890ABCDEFull);
        EXPECT_EQ(in.i32(), -5);
        EXPECT_EQ(in.i64(), -1234567890123ll);
        EXPECT_EQ(in.f32(), 1.5f);
        EXPECT_EQ(in.f64(), -0.1);
        EXPECT_EQ(in.string(), std::string("two\0words", 9));
        EXPECT_EQ(in.string(), "");
        EXPECT_THAT(in.form(), Optional(0x01000014u));
        EXPECT_EQ(in.count(0), 3u);
        EXPECT_TRUE(in.atEnd());
    }

    TEST(FalloutScriptSaveStateTest, numbers_are_little_endian_whatever_the_machine)
    {
        SaveWriter out;
        out.u32(0x01020304);
        EXPECT_EQ(out.bytes(), std::string("\x04\x03\x02\x01", 4));
    }

    TEST(FalloutScriptSaveStateTest, a_save_that_is_cut_short_is_an_error)
    {
        SaveWriter out;
        out.u32(7);
        out.string("some text");
        const std::string bytes = out.bytes();
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            SaveReader in(std::string_view(bytes).substr(0, size));
            EXPECT_THROW(
                {
                    in.u32();
                    in.string();
                },
                SaveError)
                << size;
        }
    }

    TEST(FalloutScriptSaveStateTest, a_count_that_the_rest_of_the_save_has_no_room_for_is_an_error)
    {
        SaveWriter out;
        out.count(std::numeric_limits<std::uint32_t>::max());
        out.u32(1);
        SaveReader in(out.bytes());
        EXPECT_THROW(in.count(4), SaveError);

        // a text that is longer than the save is
        SaveWriter text;
        text.count(1000);
        text.u8(1);
        SaveReader textIn(text.bytes());
        EXPECT_THROW(textIn.string(), SaveError);
    }

    TEST(FalloutScriptSaveStateTest, forms_follow_the_plugins_to_their_place_in_the_load_order)
    {
        SaveWriter out;
        out.form(0x02000123); // a plugin that moved
        out.form(0x05000456); // a plugin that is gone
        out.form(0);
        out.form(madeObject); // made while playing
        out.form(0x00000014);

        const auto mapping = [](FormId saved) -> std::optional<FormId> {
            switch (saved >> 24)
            {
                case 0:
                    return saved;
                case 2:
                    return 0x03000000 | (saved & 0xFFFFFF);
                default:
                    return std::nullopt;
            }
        };
        SaveReader in(out.bytes(), mapping);
        EXPECT_THAT(in.form(), Optional(0x03000123u));
        EXPECT_EQ(in.form(), std::nullopt);
        EXPECT_THAT(in.form(), Optional(0u));
        EXPECT_THAT(in.form(), Optional(madeObject));
        EXPECT_THAT(in.form(), Optional(0x14u));
    }

    class TestRecords : public FalloutScript::QuestRecords
    {
    public:
        std::deque<ESM4::Quest> mQuests;
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

        ESM4::Quest& addQuest(FormId id)
        {
            ESM4::Quest& quest = mQuests.emplace_back();
            quest.mId = ESM::FormId::fromUint32(id);
            return quest;
        }

        static void addStage(ESM4::Quest& quest, int stage, std::string text)
        {
            ESM4::QuestStage& entry = quest.mStages.emplace_back();
            entry.mIndex = static_cast<std::uint16_t>(stage);
            entry.mLogEntries.emplace_back().mText = std::move(text);
        }
    };

    struct QuestRunner
    {
        FalloutScript::CommandTable mCommands = testCommands();
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        TestRecords mRecords;
        FalloutScript::QuestManager mQuests{ mInterpreter, mRecords };

        QuestRunner()
        {
            ESM4::Quest& a = mRecords.addQuest(questA);
            TestRecords::addStage(a, 10, "Go to Goodsprings");
            TestRecords::addStage(a, 20, "Talk to Sunny");
            a.mQuestScript = ESM::FormId::fromUint32(scriptOfA);
            a.mData.questDelay = 5.f;
            ScriptBuilder script;
            script.mType = 1;
            script.variable(1, integerVariable, "visits").variable(2, floatVariable, "share");
            script.code(scriptName());
            ESM4::Script& record = mRecords.mScripts[scriptOfA];
            record.mId = ESM::FormId::fromUint32(scriptOfA);
            record.mScript = script.definition();
            ESM4::Quest& b = mRecords.addQuest(questB);
            TestRecords::addStage(b, 5, "");
            mQuests.reset();
        }
    };

    std::string quests(const FalloutScript::QuestManager& manager)
    {
        SaveWriter out;
        FalloutScript::writeQuests(manager, out);
        return out.take();
    }

    void readQuests(
        FalloutScript::QuestManager& manager, const std::string& bytes, SaveReader::FormMapping mapping = {})
    {
        SaveReader in(bytes, std::move(mapping));
        FalloutScript::readQuests(manager, in, FalloutScript::questsSectionVersion);
        EXPECT_TRUE(in.atEnd());
    }

    TEST(FalloutScriptSaveStateTest, the_progress_in_quests_comes_back)
    {
        QuestRunner before;
        before.mQuests.setStage(questA, 10);
        before.mQuests.setStage(questA, 20);
        before.mQuests.setObjectiveDisplayed(questA, 1, true);
        before.mQuests.setObjectiveDisplayed(questA, 2, true);
        before.mQuests.setObjectiveCompleted(questA, 1, true);
        before.mQuests.setDelay(questA, 12.5f);
        before.mQuests.update(2.f);
        before.mQuests.complete(questA);
        before.mQuests.instance(questA)->setVariable(1, 7);
        before.mQuests.instance(questA)->setVariable(2, 0.25);
        before.mQuests.setStage(questB, 5);
        before.mQuests.stop(questB);

        const std::string bytes = quests(before.mQuests);
        QuestRunner after;
        readQuests(after.mQuests, bytes);

        EXPECT_EQ(after.mQuests.stage(questA), 20);
        EXPECT_TRUE(after.mQuests.stageDone(questA, 10));
        EXPECT_TRUE(after.mQuests.stageDone(questA, 20));
        EXPECT_TRUE(after.mQuests.running(questA));
        EXPECT_TRUE(after.mQuests.completed(questA));
        EXPECT_TRUE(after.mQuests.objectiveDisplayed(questA, 1));
        EXPECT_TRUE(after.mQuests.objectiveDisplayed(questA, 2));
        EXPECT_TRUE(after.mQuests.objectiveCompleted(questA, 1));
        EXPECT_FALSE(after.mQuests.objectiveCompleted(questA, 2));
        EXPECT_EQ(after.mQuests.delay(questA), 12.5f);
        EXPECT_EQ(after.mQuests.instance(questA)->variable(1), 7);
        EXPECT_EQ(after.mQuests.instance(questA)->variable(2), 0.25);
        EXPECT_FALSE(after.mQuests.running(questB));
        EXPECT_EQ(after.mQuests.stage(questB), 5);

        const FalloutScript::QuestState* state = after.mQuests.state(questA);
        ASSERT_NE(state, nullptr);
        ASSERT_EQ(state->mJournal.size(), 2u);
        EXPECT_EQ(state->mJournal[0].mText, "Go to Goodsprings");
        EXPECT_EQ(state->mJournal[1].mStage, 20);
        EXPECT_EQ(state->mTimer, 2.f);

        // what was read is what is written again
        EXPECT_EQ(quests(after.mQuests), bytes);
    }

    TEST(FalloutScriptSaveStateTest, a_quest_that_the_records_no_longer_have_is_left_out)
    {
        QuestRunner before;
        before.mQuests.setStage(questA, 10);
        const std::string bytes = quests(before.mQuests);

        // the plugin of questA is gone: the other quest stays as the records made it
        QuestRunner after;
        readQuests(after.mQuests, bytes, [](FormId saved) -> std::optional<FormId> {
            return (saved >> 24) == 1 && (saved & 0xFFFFFF) == 0x1000 ? std::nullopt : std::optional<FormId>(saved);
        });
        EXPECT_EQ(after.mQuests.stage(questA), 0);
        EXPECT_FALSE(after.mQuests.running(questA));
        EXPECT_FALSE(after.mQuests.known(questFromGonePlugin));
    }

    TEST(FalloutScriptSaveStateTest, a_quest_script_with_other_variables_starts_over)
    {
        QuestRunner before;
        before.mQuests.instance(questA)->setVariable(1, 7);
        const std::string bytes = quests(before.mQuests);

        QuestRunner after;
        // the script of the quest was changed since: a variable of another kind
        ScriptBuilder script;
        script.mType = 1;
        script.variable(1, floatVariable, "visits").variable(2, floatVariable, "share");
        script.code(scriptName());
        after.mRecords.mScripts[scriptOfA].mScript = script.definition();
        after.mQuests.reset();
        readQuests(after.mQuests, bytes);
        EXPECT_EQ(after.mQuests.instance(questA)->variable(1), 0);
    }

    TEST(FalloutScriptSaveStateTest, damaged_quest_state_is_an_error_not_a_crash)
    {
        QuestRunner before;
        before.mQuests.setStage(questA, 10);
        const std::string bytes = quests(before.mQuests);
        for (std::size_t size = 0; size < bytes.size(); size += 3)
        {
            QuestRunner after;
            SaveReader in(std::string_view(bytes).substr(0, size));
            EXPECT_THROW(FalloutScript::readQuests(after.mQuests, in, 1), SaveError) << size;
        }
    }

    struct ObjectRunner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        FalloutScript::ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                                  const auto found = mScripts.find(id);
                                                  return found == mScripts.end() ? nullptr : &found->second;
                                              } };

        ObjectRunner()
        {
            ScriptBuilder script;
            script.variable(1, integerVariable, "frames").variable(2, floatVariable, "share");
            script.variable(3, integerVariable, "holder");
            script.formVariable(3); // the third variable holds a form
            script.code(scriptName());
            addScript(scriptOfCrates, script);
            // a script of another shape
            ScriptBuilder other;
            other.variable(1, integerVariable, "frames");
            other.code(scriptName());
            addScript(otherScript, other);
        }

        void addScript(FormId id, const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[id];
            record.mId = ESM::FormId::fromUint32(id);
            record.mScript = builder.definition();
        }
    };

    std::string objects(const FalloutScript::ObjectScripts& scripts)
    {
        SaveWriter out;
        FalloutScript::writeObjectScripts(scripts, out);
        return out.take();
    }

    void readObjects(
        FalloutScript::ObjectScripts& scripts, const std::string& bytes, SaveReader::FormMapping mapping = {})
    {
        SaveReader in(bytes, std::move(mapping));
        FalloutScript::readObjectScripts(scripts, in, FalloutScript::objectScriptsSectionVersion);
        EXPECT_TRUE(in.atEnd());
    }

    TEST(FalloutScriptSaveStateTest, the_variables_of_object_scripts_are_given_when_the_script_is)
    {
        ObjectRunner before;
        before.mObjects.add(crate, scriptOfCrates)->setVariable(1, 41);
        before.mObjects.instance(crate)->setVariable(2, 0.5);
        before.mObjects.add(otherCrate, scriptOfCrates)->setVariable(1, 3);
        const std::string bytes = objects(before.mObjects);

        // a new game: the cell is not loaded, so nothing has a script yet
        ObjectRunner after;
        readObjects(after.mObjects, bytes);
        EXPECT_EQ(after.mObjects.size(), 0u);
        // and the save of a game that was just loaded holds them still
        EXPECT_EQ(objects(after.mObjects), bytes);

        Instance* const first = after.mObjects.add(crate, scriptOfCrates);
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->variable(1), 41);
        EXPECT_EQ(first->variable(2), 0.5);
        // each reference has what it had
        EXPECT_EQ(after.mObjects.add(otherCrate, scriptOfCrates)->variable(1), 3);
        EXPECT_EQ(objects(after.mObjects), bytes);
    }

    TEST(FalloutScriptSaveStateTest, an_object_whose_script_changed_starts_over)
    {
        ObjectRunner before;
        before.mObjects.add(crate, scriptOfCrates)->setVariable(1, 41);
        before.mObjects.add(otherCrate, otherScript)->setVariable(1, 9);
        const std::string bytes = objects(before.mObjects);

        ObjectRunner after;
        readObjects(after.mObjects, bytes);
        // the reference has another script now, or the same script with other variables
        EXPECT_EQ(after.mObjects.add(crate, otherScript)->variable(1), 0);
        EXPECT_EQ(after.mObjects.add(otherCrate, otherScript)->variable(1), 9);
    }

    TEST(FalloutScriptSaveStateTest, objects_that_were_made_while_playing_and_forms_of_variables_follow_the_load_order)
    {
        ObjectRunner before;
        Instance* made = before.mObjects.add(madeObject, scriptOfCrates);
        ASSERT_NE(made, nullptr);
        made->setVariable(1, 5);
        made->setVariable(3, somebody);
        before.mObjects.add(otherCrate, scriptOfCrates)->setVariable(1, 6);
        const std::string bytes = objects(before.mObjects);

        // the plugin of the crates moved from 1 to 2
        const auto mapping = [](FormId saved) -> std::optional<FormId> {
            return (saved >> 24) == 1 ? std::optional<FormId>(0x02000000 | (saved & 0xFFFFFF)) : saved;
        };
        ObjectRunner after;
        after.mScripts[0x02003000] = after.mScripts[scriptOfCrates];
        readObjects(after.mObjects, bytes, mapping);
        Instance* const again = after.mObjects.add(madeObject, 0x02003000);
        ASSERT_NE(again, nullptr);
        EXPECT_EQ(again->variable(1), 5);
        // the form that a variable holds moved with its plugin
        EXPECT_EQ(again->variable(3), 0x02000200);
        EXPECT_EQ(after.mObjects.add(0x02000101, 0x02003000)->variable(1), 6);
    }

    TEST(FalloutScriptSaveStateTest, damaged_object_state_is_an_error_not_a_crash)
    {
        ObjectRunner before;
        before.mObjects.add(crate, scriptOfCrates)->setVariable(1, 41);
        const std::string bytes = objects(before.mObjects);
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            ObjectRunner after;
            SaveReader in(std::string_view(bytes).substr(0, size));
            EXPECT_THROW(FalloutScript::readObjectScripts(after.mObjects, in, 1), SaveError) << size;
        }
    }
}
