#include "scriptbuilder.hpp"

#include <components/falloutscript/messagecommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId crate = 0x01000100;
    constexpr FormId otherCrate = 0x01000101;
    constexpr FormId scriptOfCrates = 0x01002000;
    constexpr FormId warning = 0x01004000;
    constexpr FormId greeting = 0x01004001;

    // The code of the command in the table of the games
    constexpr std::uint16_t OpShowMessage = 0x1059;
    constexpr std::uint16_t OpGetButtonPressed = 0x101F;
    constexpr std::uint16_t OpShowWarning = 0x11B9;
    constexpr std::uint16_t OpMenuMode = 0x1024;
    constexpr std::uint32_t integerVariable = 1;

    class FakeScreen : public FalloutScript::MessageWorld
    {
    public:
        struct Shown
        {
            FormId mMessage = 0;
            std::vector<double> mValues;
            FormId mOwner = 0;
        };
        std::vector<FormId> mShown;
        std::vector<Shown> mCalls;
        FalloutScript::MessageAnswer mAnswer;
        std::vector<int> mOpen;
        std::vector<int> mAsked;
        std::vector<FormId> mAskedBy;

        void showMessage(FormId message, const std::vector<double>& values, FormId owner) override
        {
            mShown.push_back(message);
            mCalls.push_back({ message, values, owner });
            mAnswer.shown(owner);
        }
        int takeButtonPressed(FormId owner) override
        {
            mAskedBy.push_back(owner);
            return mAnswer.take(owner);
        }
        bool menuOpen(int menu) override
        {
            mAsked.push_back(menu);
            return std::find(mOpen.begin(), mOpen.end(), menu) != mOpen.end();
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        FakeScreen mScreen;

        Runner() { FalloutScript::addMessageCommands(mInterpreter, mScreen); }

        void addScript(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfCrates];
            record.mId = ESM::FormId::fromUint32(scriptOfCrates);
            record.mScript = builder.definition();
        }
    };

    TEST(FalloutScriptMessageCommandsTest, show_message_shows_the_messages_in_the_order_they_are_called)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(warning);
        const std::uint16_t second = builder.form(greeting);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpShowMessage, arguments(1).add(refArg(first)))
            + command(OpShowMessage, arguments(1).add(refArg(second)))
            + command(OpShowMessage, arguments(1).add(refArg(first))) + end());
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_THAT(setup.mScreen.mShown, ElementsAre(warning, greeting, warning));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptMessageCommandsTest, no_button_is_pressed_and_no_menu_is_open)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetButtonPressed, Bytes()))
            + setTo(var('s', 2), callToken(OpMenuMode, arguments(0))) + end());
        builder.variable(1, integerVariable, "button").variable(2, integerVariable, "menu");
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), -1);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(2), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptMessageCommandsTest, the_button_that_was_pressed_is_told_once)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetButtonPressed, Bytes()))
            + setTo(var('s', 2), callToken(OpGetButtonPressed, Bytes())) + end());
        builder.variable(1, integerVariable, "first").variable(2, integerVariable, "second");
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mScreen.mAnswer.shown(crate);
        setup.mScreen.mAnswer.picked(2);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), 2);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(2), -1);
    }

    TEST(FalloutScriptMessageCommandsTest, the_button_is_told_to_the_object_that_showed_the_message_only)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetButtonPressed, Bytes())) + end());
        builder.variable(1, integerVariable, "button");
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.add(otherCrate, scriptOfCrates);
        setup.mScreen.mAnswer.shown(crate);
        setup.mScreen.mAnswer.picked(1);

        setup.mObjects.trigger(otherCrate, BlockType::OnActivate);
        EXPECT_EQ(setup.mObjects.instance(otherCrate)->variable(1), -1);
        // and the other did not take it
        setup.mObjects.trigger(crate, BlockType::OnActivate);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), 1);
        EXPECT_THAT(setup.mScreen.mAskedBy, ElementsAre(otherCrate, crate));
    }

    TEST(FalloutScriptMessageCommandsTest, show_message_tells_who_asked_and_the_values_after_its_form)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t message = builder.form(warning);
        const std::uint16_t globalIndex = builder.form(0x01003000);
        setup.mHost.mGlobals[0x01003000] = 12;
        builder.variable(1, integerVariable, "count");
        builder.code(scriptName() + begin(BlockType::OnActivate) + setTo(var('s', 1), number("4"))
            + command(OpShowMessage,
                arguments(1)
                    .add(refArg(message))
                    .u16(3)
                    .add(var('s', 1))
                    .add(intArg(-2))
                    .add(global(globalIndex))
                    .u32(0))
            + end());
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        ASSERT_EQ(setup.mScreen.mCalls.size(), 1u);
        EXPECT_EQ(setup.mScreen.mCalls[0].mMessage, warning);
        EXPECT_THAT(setup.mScreen.mCalls[0].mValues, ElementsAre(4, -2, 12));
        EXPECT_EQ(setup.mScreen.mCalls[0].mOwner, crate);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptMessageCommandsTest, showing_a_message_forgets_the_answer_to_the_one_before)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t message = builder.form(warning);
        builder.variable(1, integerVariable, "button");
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpShowMessage, arguments(1).add(refArg(message)))
            + setTo(var('s', 1), callToken(OpGetButtonPressed, Bytes())) + end());
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mScreen.mAnswer.shown(crate);
        setup.mScreen.mAnswer.picked(3);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), -1);
    }

    TEST(FalloutScriptMessageCommandsTest, the_answer_of_a_message_is_for_the_one_who_showed_it)
    {
        FalloutScript::MessageAnswer answer;
        answer.shown(0x01005000);
        answer.picked(0);
        // the answer is for the quest, and 0 is a button like the others
        EXPECT_EQ(answer.take(0x01005001), -1);
        EXPECT_EQ(answer.take(0), -1);
        EXPECT_EQ(answer.take(0x01005000), 0);
        EXPECT_EQ(answer.take(0x01005000), -1);
        answer.shown(0x01005000);
        answer.picked(4);
        answer.clear();
        EXPECT_EQ(answer.take(0x01005000), -1);
    }

    TEST(FalloutScriptMessageCommandsTest, menu_mode_asks_for_the_menu_of_its_number_or_for_any)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpMenuMode, arguments(1).add(intArg(1001))))
            + setTo(var('s', 2), callToken(OpMenuMode, arguments(1).add(intArg(1009))))
            + setTo(var('s', 3), callToken(OpMenuMode, arguments(0))) + end());
        builder.variable(1, integerVariable, "message")
            .variable(2, integerVariable, "dialogue")
            .variable(3, integerVariable, "any");
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mScreen.mOpen = { 1001, 0 };

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), 1);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(2), 0);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(3), 1);
        EXPECT_THAT(setup.mScreen.mAsked, ElementsAre(1001, 1009, 0));
    }

    TEST(FalloutScriptMessageCommandsTest, a_warning_is_formatted_and_shown_nowhere_as_in_the_games)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.variable(1, integerVariable, "result");
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpShowWarning, arguments(1).text("Careful, %.0f").u16(1).add(intArg(3))))
            + end());
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_THAT(setup.mScreen.mShown, IsEmpty());
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), 1);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptMessageCommandsTest, a_message_that_is_no_form_shows_nothing)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(
            scriptName() + begin(BlockType::OnActivate) + command(OpShowMessage, arguments(1).add(intArg(0))) + end());
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_THAT(setup.mScreen.mShown, IsEmpty());
    }
}
