#include "scriptbuilder.hpp"

#include <components/falloutscript/script.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::CallProblem;
    using FalloutScript::Script;
    using FalloutScript::ScriptProblem;
    using FalloutScript::VariableKind;

    Script prepareScript(const ScriptBuilder& builder)
    {
        return Script::prepare(builder.definition(), testCommands());
    }

    // A statement that does nothing of interest
    Bytes logStatement()
    {
        return command(OpLog, arguments(1).text("x"));
    }

    ScriptBuilder withCode(const Bytes& code)
    {
        ScriptBuilder builder;
        builder.code(code);
        return builder;
    }

    TEST(FalloutScriptScriptTest, listsTheVariablesWithTheirKindAndName)
    {
        ScriptBuilder builder;
        builder.variable(1, 1, "count").variable(3, 0, "ratio").variable(2, 0, "holder");
        builder.formVariable(2);
        builder.code(scriptName());
        const Script script = prepareScript(builder);

        ASSERT_TRUE(script.usable());
        ASSERT_EQ(script.variables().size(), 4u);
        EXPECT_EQ(script.variables()[1].mKind, VariableKind::Integer);
        EXPECT_EQ(script.variables()[1].mName, "count");
        EXPECT_EQ(script.variables()[2].mKind, VariableKind::Reference);
        EXPECT_EQ(script.variables()[2].mName, "holder");
        EXPECT_EQ(script.variables()[3].mKind, VariableKind::Float);
    }

    TEST(FalloutScriptScriptTest, aVariableThatOnlyAReferenceNamesStillExists)
    {
        ScriptBuilder builder;
        builder.formVariable(3);
        builder.code(scriptName());
        const Script script = prepareScript(builder);
        ASSERT_EQ(script.variables().size(), 4u);
        EXPECT_EQ(script.variables()[3].mKind, VariableKind::Reference);
    }

    TEST(FalloutScriptScriptTest, listsTheReferencesInTheOrderTheBytecodeCountsThem)
    {
        ScriptBuilder builder;
        EXPECT_EQ(builder.form(0x0A000123), 1);
        EXPECT_EQ(builder.formVariable(1), 2);
        EXPECT_EQ(builder.form(0x00000014), 3);
        builder.variable(1, 0);
        builder.code(scriptName());
        const Script script = prepareScript(builder);

        ASSERT_EQ(script.references().size(), 3u);
        EXPECT_FALSE(script.references()[0].mIsVariable);
        EXPECT_EQ(script.references()[0].mForm, 0x0A000123u);
        EXPECT_TRUE(script.references()[1].mIsVariable);
        EXPECT_EQ(script.references()[1].mVariable, 1u);
        EXPECT_EQ(script.references()[2].mForm, 0x00000014u);
    }

    TEST(FalloutScriptScriptTest, findsTheBlocks)
    {
        const Script script
            = prepareScript(withCode(scriptName() + begin(0) + logStatement() + end() + begin(2) + end()));
        ASSERT_TRUE(script.usable());
        EXPECT_FALSE(script.isResult());
        ASSERT_EQ(script.blocks().size(), 2u);
        EXPECT_EQ(script.blocks()[0].mType, 0);
        EXPECT_EQ(script.blocks()[0].mBegin, 1u);
        EXPECT_EQ(script.blocks()[0].mEnd, 3u);
        EXPECT_EQ(script.blocks()[1].mType, 2);
        EXPECT_EQ(script.blocks()[1].mBegin, 4u);
        EXPECT_EQ(script.blocks()[1].mEnd, 5u);
    }

    TEST(FalloutScriptScriptTest, aScriptWithoutBlocksIsOneBlockOfAllItsStatements)
    {
        const Script script = prepareScript(withCode(logStatement() + logStatement()));
        ASSERT_TRUE(script.usable());
        EXPECT_TRUE(script.isResult());
        ASSERT_EQ(script.blocks().size(), 1u);
        EXPECT_EQ(script.blocks()[0].mBegin, 0u);
        EXPECT_EQ(script.blocks()[0].mEnd, 2u);
    }

    TEST(FalloutScriptScriptTest, anEmptyScriptHasNothingToRun)
    {
        const Script script = prepareScript(withCode(Bytes()));
        EXPECT_TRUE(script.usable());
        EXPECT_TRUE(script.isResult());
        ASSERT_EQ(script.blocks().size(), 1u);
        EXPECT_EQ(script.blocks()[0].mEnd, 0u);
    }

    TEST(FalloutScriptScriptTest, matchesTheBranchesOfACondition)
    {
        // statements: 0 ScriptName, 1 Begin, 2 If, 3 Log, 4 ElseIf, 5 Log, 6 Else, 7 Log, 8 EndIf, 9 End
        const Script script = prepareScript(withCode(scriptName() + begin(0) + ifStatement(number("1")) + logStatement()
            + elseIfStatement(number("0")) + logStatement() + elseStatement() + logStatement() + endIf() + end()));
        ASSERT_TRUE(script.usable());
        EXPECT_EQ(script.branches()[2].mNext, 4u);
        EXPECT_EQ(script.branches()[2].mEndIf, 8u);
        EXPECT_EQ(script.branches()[4].mNext, 6u);
        EXPECT_EQ(script.branches()[4].mEndIf, 8u);
        EXPECT_EQ(script.branches()[6].mNext, 8u);
        EXPECT_EQ(script.branches()[6].mEndIf, 8u);
    }

    TEST(FalloutScriptScriptTest, matchesNestedConditions)
    {
        // 0 If, 1 If, 2 EndIf, 3 Else, 4 EndIf
        const Script script = prepareScript(
            withCode(ifStatement(number("1")) + ifStatement(number("1")) + endIf() + elseStatement() + endIf()));
        ASSERT_TRUE(script.usable());
        EXPECT_EQ(script.branches()[0].mNext, 3u);
        EXPECT_EQ(script.branches()[0].mEndIf, 4u);
        EXPECT_EQ(script.branches()[1].mNext, 2u);
        EXPECT_EQ(script.branches()[1].mEndIf, 2u);
        EXPECT_EQ(script.branches()[3].mNext, 4u);
    }

    TEST(FalloutScriptScriptTest, rejectsBlocksAndConditionsThatAreNotClosed)
    {
        const std::vector<Bytes> broken{
            begin(0) + logStatement(), // Begin with no End
            end(), // End with no Begin
            begin(0) + begin(1) + end() + end(), // a block in a block
            ifStatement(number("1")) + logStatement(), // If with no EndIf
            endIf(), // EndIf with no If
            elseStatement() + endIf(), // Else with no If
            ifStatement(number("1")) + elseStatement() + elseStatement() + endIf(), // two Elses
            ifStatement(number("1")) + elseStatement() + elseIfStatement(number("1")) + endIf(), // ElseIf after Else
            begin(0) + ifStatement(number("1")) + end() + endIf(), // End inside a condition
            ifStatement(number("1")) + begin(0) + end() + endIf(), // Begin inside a condition
        };
        for (std::size_t i = 0; i < broken.size(); ++i)
        {
            const Script script = prepareScript(withCode(broken[i]));
            EXPECT_EQ(script.problem(), ScriptProblem::BadStructure) << "case " << i;
            EXPECT_FALSE(script.usable());
        }
    }

    TEST(FalloutScriptScriptTest, keepsAScriptThatDoesNotDecodeWithItsError)
    {
        const Script script = prepareScript(withCode(scriptName() + statement(0x40)));
        EXPECT_EQ(script.problem(), ScriptProblem::DoesNotDecode);
        EXPECT_EQ(script.decodeError(), ESM4::ScriptCode::Error::UnknownStatement);
    }

    TEST(FalloutScriptScriptTest, readsTheArgumentsOfACallByTheParametersOfItsCommand)
    {
        ScriptBuilder builder;
        const std::uint16_t quest = builder.form(0x00ABCDEF);
        builder.code(command(OpSetStage, arguments(2).add(refArg(quest)).add(intArg(30))));
        const Script script = prepareScript(builder);

        ASSERT_TRUE(script.usable());
        ASSERT_EQ(script.calls().size(), 1u);
        const FalloutScript::PreparedCall& call = script.calls()[0];
        EXPECT_EQ(call.mProblem, CallProblem::None);
        ASSERT_NE(call.mCommand, nullptr);
        EXPECT_EQ(call.mCommand->mName, "SetStage");
        ASSERT_EQ(call.mArguments.mValues.size(), 2u);
        EXPECT_EQ(call.mArguments.mValues[0].mIndex, quest);
        EXPECT_EQ(call.mArguments.mValues[1].mNumber, 30);
    }

    TEST(FalloutScriptScriptTest, marksTheCallsItCanNotRun)
    {
        ScriptBuilder builder;
        const std::uint16_t quest = builder.form(0x00ABCDEF);
        builder.code(command(0x1FFF, arguments(0)) + command(OpSetStage, arguments(1).add(intArg(1)))
            + command(OpSetStage, arguments(2).add(refArg(quest)).add(intArg(30))));
        const Script script = prepareScript(builder);

        ASSERT_TRUE(script.usable());
        ASSERT_EQ(script.calls().size(), 3u);
        EXPECT_EQ(script.calls()[0].mProblem, CallProblem::UnknownCommand);
        EXPECT_EQ(script.calls()[0].mCommand, nullptr);
        EXPECT_EQ(script.calls()[1].mProblem, CallProblem::BadArguments);
        EXPECT_EQ(script.calls()[2].mProblem, CallProblem::None);
    }

    TEST(FalloutScriptScriptTest, takesTheTypeOfTheScript)
    {
        ScriptBuilder builder;
        builder.mType = 1;
        builder.code(scriptName());
        EXPECT_EQ(prepareScript(builder).type(), 1);
    }
}
