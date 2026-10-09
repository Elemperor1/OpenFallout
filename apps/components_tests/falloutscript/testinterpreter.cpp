#include "scriptbuilder.hpp"

#include <components/falloutscript/interpreter.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::Instance;
    using FalloutScript::Interpreter;

    constexpr FormId owner = 0x00001000;
    constexpr std::uint16_t GameMode = 0;
    constexpr std::uint16_t OnActivate = 2;

    struct Runner
    {
        FalloutScript::CommandTable mCommands = testCommands();
        TestHost mHost;
        Interpreter mInterpreter{ mHost, mCommands };
        std::vector<std::string> mLogged; // what Log was given

        Runner()
        {
            mInterpreter.setHandler("Log", [this](FalloutScript::CallContext& call) {
                mLogged.push_back(call.mArguments.at(0).mText);
                return 0.0;
            });
            mInterpreter.setHandler(
                "Twice", [](FalloutScript::CallContext& call) { return call.mArguments.at(0).mNumber * 2; });
        }

        Instance instance(const ScriptBuilder& builder, FormId id = owner)
        {
            return Instance(prepare(builder, mCommands), id);
        }
    };

    Bytes logCall(const std::string& text)
    {
        return command(OpLog, arguments(1).text(text));
    }

    ScriptBuilder block(const Bytes& body, std::uint16_t type = GameMode)
    {
        ScriptBuilder builder;
        builder.code(scriptName() + begin(type) + body + end());
        return builder;
    }

    TEST(FalloutScriptInterpreterTest, assignsToVariablesInTheirOwnKind)
    {
        Runner setup;
        ScriptBuilder builder = block(setTo(var('s', 1), number("7") + number("2") + op("/"))
            + setTo(var('f', 2), number("1") + number("3") + op("/")));
        builder.variable(1, 1, "count").variable(2, 0, "ratio");
        Instance instance = setup.instance(builder);

        EXPECT_EQ(setup.mInterpreter.run(instance, GameMode).mStatus, Interpreter::Status::Done);
        EXPECT_EQ(instance.variable(1), 3);
        EXPECT_EQ(instance.variable(2), static_cast<double>(static_cast<float>(1.0 / 3.0)));
        EXPECT_EQ(instance.variable(3), 0);
    }

    TEST(FalloutScriptInterpreterTest, aNegativeNumberIsDroppedTowardsZeroInAnInteger)
    {
        Runner setup;
        ScriptBuilder builder = block(setTo(var('s', 1), number("7") + number("2") + op("/") + op("~")));
        builder.variable(1, 1);
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(instance.variable(1), -3);
    }

    TEST(FalloutScriptInterpreterTest, evaluatesAnExpressionInReversePolishOrder)
    {
        Runner setup;
        // (2 + 3) * 4 - 6 / 3 % 5
        ScriptBuilder builder = block(setTo(var('f', 1),
            number("2") + number("3") + op("+") + number("4") + op("*") + number("6") + number("3") + op("/")
                + number("5") + op("%") + op("-")));
        builder.variable(1, 0);
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(instance.variable(1), 18);
    }

    TEST(FalloutScriptInterpreterTest, comparesAndCombines)
    {
        Runner setup;
        ScriptBuilder builder = block(setTo(var('s', 1), number("3") + number("3") + op("==")) // 1
            + setTo(var('s', 2), number("3") + number("4") + op("==")) // 0
            + setTo(var('s', 3), number("3") + number("4") + op("<")) // 1
            + setTo(var('s', 4), number("3") + number("4") + op(">=")) // 0
            + setTo(var('s', 5), number("1") + number("0") + op("||") + number("1") + op("&&")) // 1
            + setTo(var('s', 6), number("1") + number("0") + op("&&")) // 0
            + setTo(var('s', 7), number("3") + number("4") + op("!=")) // 1
            + setTo(var('s', 8), number("4") + number("4") + op("<=")) // 1
            + setTo(var('s', 9), number("5") + number("4") + op(">"))); // 1
        for (std::uint32_t i = 1; i <= 9; ++i)
            builder.variable(i, 1);
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);

        const std::vector<double> expected{ 1, 0, 1, 0, 1, 0, 1, 1, 1 };
        for (std::uint32_t i = 1; i <= 9; ++i)
            EXPECT_EQ(instance.variable(i), expected[i - 1]) << "variable " << i;
    }

    TEST(FalloutScriptInterpreterTest, divisionByZeroGivesZero)
    {
        Runner setup;
        ScriptBuilder builder = block(setTo(var('f', 1), number("5") + number("0") + op("/"))
            + setTo(var('f', 2), number("5") + number("0") + op("%")));
        builder.variable(1, 0).variable(2, 0);
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(instance.variable(1), 0);
        EXPECT_EQ(instance.variable(2), 0);
    }

    TEST(FalloutScriptInterpreterTest, takesTheBranchThatHolds)
    {
        const Bytes body = ifStatement(var('s', 1) + number("1") + op("==")) + logCall("one")
            + elseIfStatement(var('s', 1) + number("2") + op("==")) + logCall("two")
            + elseIfStatement(var('s', 1) + number("3") + op("==")) + logCall("three") + elseStatement()
            + logCall("other") + endIf() + logCall("after");
        ScriptBuilder builder = block(body);
        builder.variable(1, 1);

        for (const auto& [value, expected] :
            std::vector<std::pair<int, std::vector<std::string>>>{ { 1, { "one", "after" } }, { 2, { "two", "after" } },
                { 3, { "three", "after" } }, { 4, { "other", "after" } } })
        {
            Runner run;
            Instance instance = run.instance(builder);
            instance.setVariable(1, value);
            EXPECT_EQ(run.mInterpreter.run(instance, GameMode).mStatus, Interpreter::Status::Done);
            EXPECT_EQ(run.mLogged, expected) << "value " << value;
        }
    }

    TEST(FalloutScriptInterpreterTest, aConditionWithNoElseCanSkipEverything)
    {
        Runner setup;
        ScriptBuilder builder = block(ifStatement(number("0")) + logCall("no") + endIf() + logCall("yes"));
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_THAT(setup.mLogged, ElementsAre("yes"));
    }

    TEST(FalloutScriptInterpreterTest, conditionsCanBeNested)
    {
        Runner setup;
        const Bytes body = ifStatement(number("1")) + ifStatement(number("0")) + logCall("a") + elseStatement()
            + ifStatement(number("1")) + logCall("b") + endIf() + endIf() + elseStatement() + logCall("c") + endIf()
            + ifStatement(number("0")) + ifStatement(number("1")) + logCall("d") + endIf() + elseStatement()
            + logCall("e") + endIf();
        ScriptBuilder builder = block(body);
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_THAT(setup.mLogged, ElementsAre("b", "e"));
    }

    TEST(FalloutScriptInterpreterTest, runsTheBlocksOfTheTypeOnly)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(GameMode) + logCall("game") + end() + begin(OnActivate) + logCall("activate")
            + end() + begin(GameMode) + logCall("game again") + end());
        Instance instance = setup.instance(builder);

        setup.mInterpreter.run(instance, OnActivate);
        EXPECT_THAT(setup.mLogged, ElementsAre("activate"));
        setup.mLogged.clear();
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_THAT(setup.mLogged, ElementsAre("game", "game again"));
        setup.mLogged.clear();
        EXPECT_EQ(setup.mInterpreter.run(instance, 7).mStatus, Interpreter::Status::Done);
        EXPECT_THAT(setup.mLogged, IsEmpty());
    }

    TEST(FalloutScriptInterpreterTest, aReturnEndsTheBlock)
    {
        Runner setup;
        ScriptBuilder builder
            = block(logCall("first") + ifStatement(number("1")) + returnStatement() + endIf() + logCall("never"));
        Instance instance = setup.instance(builder);
        EXPECT_EQ(setup.mInterpreter.run(instance, GameMode).mStatus, Interpreter::Status::Returned);
        EXPECT_THAT(setup.mLogged, ElementsAre("first"));
    }

    TEST(FalloutScriptInterpreterTest, runsAScriptWithoutBlocksFromStartToEnd)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(logCall("one") + logCall("two"));
        Instance instance = setup.instance(builder);
        ASSERT_TRUE(instance.script().isResult());
        EXPECT_EQ(setup.mInterpreter.runResult(instance).mStatus, Interpreter::Status::Done);
        EXPECT_THAT(setup.mLogged, ElementsAre("one", "two"));
        // a script that has blocks is not a result and is not run as one
        setup.mLogged.clear();
        ScriptBuilder blocks = block(logCall("block"));
        Instance other = setup.instance(blocks);
        setup.mInterpreter.runResult(other);
        EXPECT_THAT(setup.mLogged, IsEmpty());
    }

    TEST(FalloutScriptInterpreterTest, usesTheValueOfACommandInAnExpression)
    {
        Runner setup;
        ScriptBuilder builder
            = block(setTo(var('f', 1), number("1.5") + callToken(OpTwice, arguments(1).add(intArg(4))) + op("+")));
        builder.variable(1, 0);
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(instance.variable(1), 9.5);
    }

    TEST(FalloutScriptInterpreterTest, giveTheCommandItsArgumentsWorkedOut)
    {
        Runner setup;
        std::vector<double> numbers;
        setup.mInterpreter.setHandler("Touch", [&](FalloutScript::CallContext& call) {
            for (const FalloutScript::Value& value : call.mArguments)
                numbers.push_back(value.mNumber);
            return 0.0;
        });
        ScriptBuilder builder;
        const std::uint16_t crate = builder.form(0x00ABCDEF);
        builder.formVariable(2);
        builder.variable(1, 1).variable(2, 0);
        builder.code(scriptName() + begin(GameMode) + setTo(var('s', 1), number("40") + number("2") + op("+"))
            + command(OpTouch, arguments(2).add(refArg(crate)).add(Bytes().u8('s').u16(1)))
            + setTo(var('f', 2), object(crate)) + command(OpTouch, arguments(1).add(Bytes().u8('f').u16(2)))
            + command(OpTouch, arguments(0)) + end());
        Instance instance = setup.instance(builder);

        setup.mInterpreter.run(instance, GameMode);
        EXPECT_THAT(numbers, ElementsAre(static_cast<double>(0x00ABCDEF), 42, static_cast<double>(0x00ABCDEF)));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptInterpreterTest, aCommandCalledOnAReferenceKnowsIt)
    {
        Runner setup;
        std::vector<std::pair<FormId, bool>> calls;
        setup.mInterpreter.setHandler("Touch", [&](FalloutScript::CallContext& call) {
            calls.emplace_back(call.mReference, call.mOnReference);
            return 0.0;
        });
        ScriptBuilder builder;
        const std::uint16_t crate = builder.form(0x00ABCDEF);
        builder.code(scriptName() + begin(GameMode) + command(OpTouch, arguments(0))
            + command(OpTouch, arguments(0), crate) + end());
        Instance instance = setup.instance(builder, 0x00001234);

        setup.mInterpreter.run(instance, GameMode);
        ASSERT_EQ(calls.size(), 2u);
        EXPECT_EQ(calls[0], std::make_pair(FormId(0x00001234), false));
        EXPECT_EQ(calls[1], std::make_pair(FormId(0x00ABCDEF), true));
    }

    TEST(FalloutScriptInterpreterTest, readsAndSetsGlobalVariables)
    {
        Runner setup;
        setup.mHost.mGlobals[0x00000500] = 10;
        setup.mHost.mGlobals[0x00000501] = 0;
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(0x00000500);
        const std::uint16_t second = builder.form(0x00000501);
        builder.variable(1, 1);
        // Set second to first + 5, and a local to the second
        const Bytes expression = global(first) + number("5") + op("+");
        builder.code(scriptName() + begin(GameMode)
            + statement(
                0x15, Bytes().u8('G').u16(second).u16(static_cast<std::uint16_t>(expression.size())).add(expression))
            + setTo(var('s', 1), global(second)) + end());
        Instance instance = setup.instance(builder);

        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(setup.mHost.mGlobals[0x00000501], 15);
        EXPECT_EQ(instance.variable(1), 15);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptInterpreterTest, reportsAGlobalThatDoesNotExistOnce)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t missing = builder.form(0x00000999);
        builder.variable(1, 1);
        builder.code(scriptName() + begin(GameMode) + setTo(var('s', 1), global(missing))
            + setTo(var('s', 1), global(missing)) + end());
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(setup.mHost.mLog.size(), 1u);
    }

    TEST(FalloutScriptInterpreterTest, readsAndSetsTheVariablesOfAnotherScript)
    {
        Runner setup;
        ScriptBuilder quest;
        quest.variable(1, 1, "stage");
        quest.code(scriptName());
        Instance questInstance = setup.instance(quest, 0x00002000);
        questInstance.setVariable(1, 5);
        setup.mHost.mInstances[0x00002000] = &questInstance;

        ScriptBuilder builder;
        const std::uint16_t questRef = builder.form(0x00002000);
        builder.variable(1, 1);
        builder.code(scriptName() + begin(GameMode)
            + setTo(var('s', 1), remoteVar(questRef, 's', 1) + number("1") + op("+"))
            + setTo(remoteVar(questRef, 's', 1), number("9")) + end());
        Instance instance = setup.instance(builder);

        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(instance.variable(1), 6);
        EXPECT_EQ(questInstance.variable(1), 9);
    }

    TEST(FalloutScriptInterpreterTest, reportsAScriptThatIsNotThere)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t questRef = builder.form(0x00002000);
        builder.variable(1, 1);
        builder.code(scriptName() + begin(GameMode) + setTo(var('s', 1), remoteVar(questRef, 's', 1)) + end());
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_EQ(instance.variable(1), 0);
        EXPECT_EQ(setup.mHost.mLog.size(), 1u);
    }

    TEST(FalloutScriptInterpreterTest, aCommandWithoutAHandlerIsReportedOnceAndSkipped)
    {
        Runner setup;
        ScriptBuilder builder
            = block(command(OpNothing, arguments(0)) + logCall("after") + command(OpNothing, arguments(0)));
        Instance instance = setup.instance(builder);
        EXPECT_EQ(setup.mInterpreter.run(instance, GameMode).mStatus, Interpreter::Status::Done);
        EXPECT_THAT(setup.mLogged, ElementsAre("after"));
        ASSERT_EQ(setup.mHost.mLog.size(), 1u);
        EXPECT_THAT(setup.mHost.mLog[0], HasSubstr("Nothing"));
    }

    TEST(FalloutScriptInterpreterTest, aCommandThatIsNotInTheTableIsReportedAndSkipped)
    {
        Runner setup;
        ScriptBuilder builder = block(command(0x1FFF, arguments(0)) + logCall("after"));
        Instance instance = setup.instance(builder);
        EXPECT_EQ(setup.mInterpreter.run(instance, GameMode).mStatus, Interpreter::Status::Done);
        EXPECT_THAT(setup.mLogged, ElementsAre("after"));
        ASSERT_EQ(setup.mHost.mLog.size(), 1u);
        EXPECT_THAT(setup.mHost.mLog[0], HasSubstr("0x1fff"));
    }

    TEST(FalloutScriptInterpreterTest, aCallWithArgumentsThatDoNotFitIsReportedAndSkipped)
    {
        Runner setup;
        // Log takes a string, and this call has a number
        ScriptBuilder builder = block(command(OpLog, arguments(1).add(intArg(4))) + logCall("after"));
        Instance instance = setup.instance(builder);
        setup.mInterpreter.run(instance, GameMode);
        EXPECT_THAT(setup.mLogged, ElementsAre("after"));
        EXPECT_EQ(setup.mHost.mLog.size(), 1u);
    }

    TEST(FalloutScriptInterpreterTest, aScriptCanRunAnotherAndTheDepthIsLimited)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.variable(1, 1);
        builder.code(logCall("again"));
        Instance instance = setup.instance(builder);
        std::size_t depth = 0;
        setup.mInterpreter.setHandler("Log", [&](FalloutScript::CallContext& call) {
            ++depth;
            call.mInterpreter.runResult(instance);
            return 0.0;
        });
        const Interpreter::Result result = setup.mInterpreter.runResult(instance);
        EXPECT_EQ(depth, Interpreter::maxDepth);
        EXPECT_EQ(result.mStatus, Interpreter::Status::Done);
        EXPECT_EQ(setup.mHost.mLog.size(), 1u);
    }

    TEST(FalloutScriptInterpreterTest, stopsAScriptThatRunsTooLong)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.variable(1, 1);
        Bytes code;
        const Bytes increment = setTo(var('s', 1), var('s', 1) + number("1") + op("+"));
        for (std::size_t i = 0; i < Interpreter::maxStatements + 10; ++i)
            code.add(increment);
        builder.code(code);
        Instance instance = setup.instance(builder);
        const Interpreter::Result result = setup.mInterpreter.runResult(instance);
        EXPECT_EQ(result.mStatus, Interpreter::Status::Failed);
        EXPECT_EQ(instance.variable(1), static_cast<double>(Interpreter::maxStatements));
        EXPECT_EQ(setup.mHost.mLog.size(), 1u);
    }

    TEST(FalloutScriptInterpreterTest, aScriptThatDoesNotDecodeDoesNothing)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + statement(0x40));
        Instance instance = setup.instance(builder);
        EXPECT_FALSE(instance.script().usable());
        EXPECT_EQ(instance.script().problem(), FalloutScript::ScriptProblem::DoesNotDecode);
        EXPECT_EQ(setup.mInterpreter.run(instance, GameMode).mStatus, Interpreter::Status::Failed);
        EXPECT_EQ(setup.mInterpreter.runResult(instance).mStatus, Interpreter::Status::Failed);
    }

    TEST(FalloutScriptInterpreterTest, aNameGivenToSetHandlerMustBeInTheTable)
    {
        Runner setup;
        EXPECT_TRUE(setup.mInterpreter.setHandler("getstage", [](FalloutScript::CallContext&) { return 3.0; }));
        EXPECT_TRUE(setup.mInterpreter.hasHandler(OpGetStage));
        EXPECT_FALSE(setup.mInterpreter.setHandler("NoSuchCommand", [](FalloutScript::CallContext&) { return 0.0; }));
    }
}
