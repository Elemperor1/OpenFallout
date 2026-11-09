#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/scriptcodecensus.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// The parts of the script census that the first tests do not reach: the other kinds of statement, expression token
// and nesting problem, the other records that hold scripts, the limits of the lists it writes, and its fatal errors.
namespace
{
    using namespace testing;
    using namespace ESM4Test;

    void put16(std::string& out, std::uint16_t value)
    {
        append(out, value);
    }

    std::string statement(std::uint16_t code, std::string_view data = {})
    {
        std::string result;
        put16(result, code);
        put16(result, static_cast<std::uint16_t>(data.size()));
        result.append(data);
        return result;
    }

    std::string variable(std::uint16_t index)
    {
        std::string result(1, 's');
        put16(result, index);
        return result;
    }

    // A letter and an index, the way an expression writes a reference (r), a global (G) or a reference value (Z)
    std::string indexed(char tag, std::uint16_t index)
    {
        std::string result(1, tag);
        put16(result, index);
        return result;
    }

    // A call inside an expression
    std::string call(std::uint16_t opcode, std::string_view arguments = {})
    {
        std::string result(1, 'X');
        put16(result, opcode);
        put16(result, static_cast<std::uint16_t>(arguments.size()));
        result.append(arguments);
        return result;
    }

    std::string quoted(std::string_view text)
    {
        std::string result(1, '"');
        put16(result, static_cast<std::uint16_t>(text.size()));
        result.append(text);
        return result;
    }

    std::string scriptHeader(
        std::uint32_t refCount, std::uint32_t compiledSize, std::uint32_t variableCount, std::uint16_t type = 0)
    {
        std::string data;
        append<std::uint32_t>(data, 0);
        append(data, refCount);
        append(data, compiledSize);
        append(data, variableCount);
        append(data, type);
        append<std::uint16_t>(data, 1);
        return subRecord("SCHR", data);
    }

    std::string localVariable(std::uint32_t index)
    {
        std::string data;
        append(data, index);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 1);
        append<std::uint32_t>(data, 0);
        return subRecord("SLSD", data) + zString("SCVR", "iCount");
    }

    std::string formId(std::uint32_t id)
    {
        std::string data;
        append(data, id);
        return data;
    }

    // The sub-records of a script: its header, its code, one local variable and the forms it names
    std::string scriptSubRecords(const std::string& code, std::uint32_t references = 0, std::uint16_t type = 0)
    {
        std::string data = scriptHeader(references, static_cast<std::uint32_t>(code.size()), 1, type)
            + subRecord("SCDA", code) + localVariable(1);
        for (std::uint32_t i = 0; i < references; ++i)
            data += subRecord("SCRO", formId(0x2001 + i));
        return data;
    }

    std::string scriptRecord(
        std::uint32_t id, const std::string& code, std::uint32_t references = 0, std::uint16_t type = 0)
    {
        return record("SCPT", id, scriptSubRecords(code, references, type));
    }

    std::string begin(std::uint16_t type, std::string_view blockArguments = {})
    {
        std::string data;
        put16(data, type);
        append<std::uint32_t>(data, 0);
        data.append(blockArguments);
        return statement(0x10, data);
    }

    // If (0x16) and ElseIf (0x18): the jump and the length of the expression, then the expression
    std::string conditionStatement(std::uint16_t code, const std::string& expression)
    {
        std::string data;
        put16(data, 0);
        put16(data, static_cast<std::uint16_t>(expression.size()));
        return statement(code, data + expression);
    }

    std::string elseStatement()
    {
        std::string data;
        put16(data, 0);
        return statement(0x17, data);
    }

    std::string setTo(std::uint16_t index, const std::string& expression)
    {
        std::string data = variable(index);
        put16(data, static_cast<std::uint16_t>(expression.size()));
        return statement(0x15, data + expression);
    }

    // A command called on a reference: the code of the statement, the reference, the command, then the arguments
    std::string referenceCall(std::uint16_t reference, std::uint16_t opcode, std::string_view arguments = {})
    {
        std::string result;
        put16(result, 0x1C);
        put16(result, reference);
        put16(result, opcode);
        put16(result, static_cast<std::uint16_t>(arguments.size()));
        result.append(arguments);
        return result;
    }

    void collect(ESM4::ScriptCodeCensus& census, const std::string& records, std::string_view group = "SCPT")
    {
        ESM4::Reader reader(
            std::make_unique<std::istringstream>(header() + topGroup(group, records)), "base.esm", nullptr, nullptr);
        reader.setModIndex(0);
        census.collect(reader);
        EXPECT_THAT(census.getFatalErrors(), IsEmpty());
    }

    std::string written(const ESM4::ScriptCodeCensus& census)
    {
        std::ostringstream out;
        census.write(out);
        return out.str();
    }

    TEST(ESM4ScriptCodeCensusMoreTest, countsEveryKindOfStatementAndExpressionToken)
    {
        // Every kind of token in one expression: 2 + local * remote - global == reference != string && call || ~
        const std::string expression = "2 " + variable(1) + " + " + indexed('r', 1) + variable(1) + " * "
            + indexed('G', 1) + " - " + indexed('Z', 2) + " == " + quoted("abc") + " != " + call(0x1020) + " && "
            + indexed('r', 2) + call(0x1021) + " || ~";
        const std::string code = statement(0x1D) + statement(0x12) + statement(0x13) + statement(0x14) + statement(0x1F)
            + begin(2, "xx") + conditionStatement(0x16, "1") + statement(0x1E) + conditionStatement(0x18, "1 1 +")
            + elseStatement() + statement(0x19) + setTo(1, expression) + referenceCall(1, 0x1020) + statement(0x1020)
            + statement(0x11);

        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, code, 2));

        const auto& totals = census.getHolders().at("SCPT");
        EXPECT_EQ(totals.mDecoded, 1u);
        EXPECT_EQ(totals.mFailed, 0u);
        EXPECT_THAT(census.getStructure(), IsEmpty());
        EXPECT_EQ(census.getBlocks().at({ 0, 2 }), 1u);

        // the If leads to the ElseIf, which leads to the Else, which leads to the EndIf
        EXPECT_EQ(census.getIfJumps().mTotal, 1u);
        EXPECT_EQ(census.getElseIfJumps().mTotal, 1u);
        EXPECT_EQ(census.getElseJumps().mTotal, 1u);
        EXPECT_EQ(census.getBeginJumps().mTotal, 1u);

        // one of the commands is called as a statement on a reference and as a statement and in an expression, the
        // other in an expression on a reference
        const auto& first = census.getCommands().at(0x1020);
        EXPECT_EQ(first.mStatements, 2u);
        EXPECT_EQ(first.mExpressions, 1u);
        EXPECT_EQ(first.mOnReference, 1u);
        EXPECT_EQ(first.mScripts, 1u);
        const auto& second = census.getCommands().at(0x1021);
        EXPECT_EQ(second.mStatements, 0u);
        EXPECT_EQ(second.mExpressions, 1u);
        EXPECT_EQ(second.mOnReference, 1u);

        const std::string text = written(census);
        EXPECT_THAT(text, HasSubstr("0x0012 Short"));
        EXPECT_THAT(text, HasSubstr("0x0013 Long"));
        EXPECT_THAT(text, HasSubstr("0x0014 Float"));
        EXPECT_THAT(text, HasSubstr("0x0017 Else"));
        EXPECT_THAT(text, HasSubstr("0x0018 ElseIf"));
        EXPECT_THAT(text, HasSubstr("0x001E Return"));
        EXPECT_THAT(text, HasSubstr("0x001F Ref"));
        EXPECT_THAT(text, HasSubstr("0xFFFF command"));
        EXPECT_THAT(text, HasSubstr("blocks with bytes after the block length, by block type: 2:1"));
        for (const char* kind : { "number", "local variable", "variable of another script", "global variable",
                 "reference", "string", "operator", "call" })
            EXPECT_THAT(text, HasSubstr(kind)) << kind;
        EXPECT_THAT(text, HasSubstr("operators: !=:1 &&:1 *:1 +:2 -:1 ==:1 ||:1 ~:1"));
        // the jumps of the conditions are all counted as written by the compiler, from no rule in particular
        EXPECT_THAT(text, HasSubstr("If to ElseIf, Else or EndIf: 1 jumps"));
        EXPECT_THAT(text, HasSubstr("ElseIf to ElseIf, Else or EndIf: 1 jumps"));
        EXPECT_THAT(text, HasSubstr("Else to EndIf: 1 jumps"));
        EXPECT_THAT(text, HasSubstr("the closest rule is from the"));
    }

    TEST(ESM4ScriptCodeCensusMoreTest, countsTheNestingProblemsOfEveryKindOfBranch)
    {
        // an End with no Begin, an Else with no If, and an ElseIf and an Else that are open when the block ends
        const std::string endOnly = statement(0x1D) + statement(0x11);
        const std::string elseOnly = statement(0x1D) + begin(0) + elseStatement() + statement(0x11);
        const std::string elseIfOpen = statement(0x1D) + begin(0) + conditionStatement(0x16, "1")
            + conditionStatement(0x18, "1") + statement(0x11);
        const std::string elseOpen
            = statement(0x1D) + begin(0) + conditionStatement(0x16, "1") + elseStatement() + statement(0x11);

        ESM4::ScriptCodeCensus census;
        collect(census,
            scriptRecord(0x1001, endOnly) + scriptRecord(0x1002, elseOnly) + scriptRecord(0x1003, elseIfOpen)
                + scriptRecord(0x1004, elseOpen));

        EXPECT_EQ(census.getStructureScripts(), 4u);
        EXPECT_EQ(census.getStructure().at("End without Begin"), 1u);
        EXPECT_EQ(census.getStructure().at("ElseIf or Else without If"), 1u);
        EXPECT_EQ(census.getStructure().at("If without EndIf"), 2u);

        // the If of each of the last two leads to the branch that follows it, and that branch to nothing
        EXPECT_EQ(census.getIfJumps().mTotal, 2u);
        EXPECT_EQ(census.getIfJumps().mUnresolved, 0u);
        EXPECT_EQ(census.getElseIfJumps().mTotal, 1u);
        EXPECT_EQ(census.getElseIfJumps().mUnresolved, 1u);
        EXPECT_EQ(census.getElseJumps().mTotal, 1u);
        EXPECT_EQ(census.getElseJumps().mUnresolved, 1u);

        // the statements around the Else: the Begin before it and the End after it
        EXPECT_EQ(census.getStructureExamples().at("ElseIf or Else without If").front().mStatements, "Sn Bg [El En");

        const std::string text = written(census);
        EXPECT_THAT(text, HasSubstr("ElseIf to ElseIf, Else or EndIf: 1 jumps, 1 without a statement to jump to"));
        EXPECT_THAT(text, HasSubstr("Else to EndIf: 1 jumps, 1 without a statement to jump to"));
        EXPECT_THAT(text, HasSubstr("ElseIf or Else without If: 1"));
    }

    TEST(ESM4ScriptCodeCensusMoreTest, showsTheKindsOfTheStatementsBeforeAScriptThatDoesNotDecode)
    {
        // a declaration, a return and a command, then a code that is no statement
        const std::string code = statement(0x12) + statement(0x1E) + statement(0x1020) + statement(0x0F);

        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, code));

        const auto& failures = census.getFailures().at(ESM4::ScriptCode::Error::UnknownStatement);
        ASSERT_EQ(failures.size(), 1u);
        EXPECT_EQ(failures.front().mStatements, "Dc Rt Cl [");
        EXPECT_EQ(failures.front().mOffset, 12u);
        EXPECT_THAT(written(census), HasSubstr("statements: Dc Rt Cl ["));
    }

    TEST(ESM4ScriptCodeCensusMoreTest, namesTheTypesOfScript)
    {
        const std::string code = statement(0x1D);
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, code, 0, 0x100) + scriptRecord(0x1002, code, 0, 5));

        const std::string text = written(census);
        EXPECT_THAT(text, HasSubstr("magic effect"));
        EXPECT_THAT(text, HasSubstr("type 0x0005"));
        EXPECT_EQ(census.getHolders().at("SCPT").mTypes.at(0x100), 1u);
    }

    TEST(ESM4ScriptCodeCensusMoreTest, countsTheScriptsWithoutCodeButNotTheOnesWithNothing)
    {
        // a script that names a form, one that only has its source, and one with no content at all
        const std::string withReference
            = record("SCPT", 0x1001, scriptHeader(1, 0, 0) + subRecord("SCRO", formId(0x2001)));
        const std::string withSource = record("SCPT", 0x1002, scriptHeader(0, 0, 0) + zString("SCTX", "scn Source"));
        const std::string nothing = record("SCPT", 0x1003, scriptHeader(0, 0, 0));

        ESM4::ScriptCodeCensus census;
        collect(census, withReference + withSource + nothing);

        const auto& totals = census.getHolders().at("SCPT");
        EXPECT_EQ(totals.mScripts, 2u);
        EXPECT_EQ(totals.mEmpty, 2u);
        EXPECT_EQ(totals.mDecoded, 0u);
        EXPECT_EQ(totals.mFailed, 0u);
    }

    TEST(ESM4ScriptCodeCensusMoreTest, keepsTheFirstValuesOfEachErrorOnly)
    {
        // 45 scripts that stop at a different code that is no statement: only the first 40 are kept apart
        std::string scripts;
        for (std::uint32_t i = 0; i < 45; ++i)
            scripts += scriptRecord(0x1000 + i, statement(static_cast<std::uint16_t>(0x20 + i)));
        // and a second script that stops at the same code as the first one
        scripts += scriptRecord(0x2000, statement(0x20));

        ESM4::ScriptCodeCensus census;
        collect(census, scripts);

        EXPECT_EQ(census.getHolders().at("SCPT").mFailed, 46u);
        EXPECT_EQ(census.getErrors().at(ESM4::ScriptCode::Error::UnknownStatement), 46u);
        EXPECT_EQ(census.getFailures().at(ESM4::ScriptCode::Error::UnknownStatement).size(),
            ESM4::ScriptCodeCensus::maxFailuresPerError);
        const std::string text = written(census);
        EXPECT_THAT(text, HasSubstr("value 0x0020: 2"));
        EXPECT_THAT(text, HasSubstr("value 0x0047: 1"));
        EXPECT_THAT(text, Not(HasSubstr("value 0x004C")));
    }

    // The parameters of the commands from 0x1100 to 0x12FF, which take one integer, and a lookup that has no others
    ESM4::ScriptCodeCensus::CommandLookup integerCommands(const std::vector<ESM4::ScriptCode::Parameter>& integer)
    {
        return [&integer](std::uint16_t opcode) {
            ESM4::ScriptCodeCensus::CommandSignature signature;
            if (opcode >= 0x1100 && opcode < 0x1300)
            {
                signature.mName = "Take";
                signature.mParameters = &integer;
            }
            return signature;
        };
    }

    std::string integerArgument(std::int32_t value)
    {
        std::string result(1, 'n');
        append(result, value);
        return result;
    }

    TEST(ESM4ScriptCodeCensusMoreTest, tellsEveryKindOfProblemWithTheArgumentsOfACall)
    {
        const std::vector<ESM4::ScriptCode::Parameter> integer{ { 1, false } };
        const std::vector<ESM4::ScriptCode::Parameter> form{ { 4, false } };
        const auto lookup = [&](std::uint16_t opcode) {
            ESM4::ScriptCodeCensus::CommandSignature signature;
            if (opcode == 0x1100)
            {
                signature.mName = "TakeInteger";
                signature.mParameters = &integer;
            }
            else if (opcode == 0x1101)
            {
                signature.mName = "TakeForm";
                signature.mParameters = &form;
            }
            return signature;
        };
        // an integer that is cut short, a variable the script does not have, an inline expression that is also long
        // enough to be shown in part, and a form that is no tag
        std::string cut;
        put16(cut, 1);
        cut += 'n';
        put16(cut, 1);
        std::string badVariable;
        put16(badVariable, 1);
        badVariable += variable(9);
        std::string inlineExpression;
        put16(inlineExpression, 1);
        put16(inlineExpression, 0xFFFF);
        put16(inlineExpression, 0);
        inlineExpression += std::string(40, 'x');
        std::string noTag;
        put16(noTag, 1);
        noTag += 'q';
        put16(noTag, 1);

        const std::string code = statement(0x1D) + statement(0x1100, cut) + statement(0x1100, badVariable)
            + statement(0x1100, inlineExpression) + statement(0x1101, noTag);
        ESM4::ScriptCodeCensus census;
        census.setCommandLookup(lookup);
        collect(census, scriptRecord(0x1001, code));

        const auto& integers = census.getArguments().at(0x1100);
        EXPECT_EQ(integers.mCalls, 3u);
        EXPECT_EQ(integers.mDecoded, 0u);
        EXPECT_EQ(integers.mErrors.at(ESM4::ScriptCode::ArgumentError::Truncated), 1u);
        EXPECT_EQ(integers.mErrors.at(ESM4::ScriptCode::ArgumentError::BadVariable), 1u);
        EXPECT_EQ(integers.mErrors.at(ESM4::ScriptCode::ArgumentError::InlineExpression), 1u);
        EXPECT_EQ(census.getArguments().at(0x1101).mErrors.at(ESM4::ScriptCode::ArgumentError::BadArgument), 1u);

        // only the first 32 bytes of a long call are shown
        const auto& example
            = census.getArgumentExamples().at({ 0x1100, ESM4::ScriptCode::ArgumentError::InlineExpression }).front();
        EXPECT_THAT(example.mBytes, EndsWith(" ..."));
        EXPECT_THAT(example.mStatements, HasSubstr("TakeInteger (1), the call has"));

        const std::string text = written(census);
        EXPECT_THAT(text, HasSubstr("truncated:1"));
        EXPECT_THAT(text, HasSubstr("badvar:1"));
        EXPECT_THAT(text, HasSubstr("expression:1"));
        EXPECT_THAT(text, HasSubstr("badarg:1"));
    }

    TEST(ESM4ScriptCodeCensusMoreTest, listsOnlyTheFirstCommandsOfEachTable)
    {
        const std::vector<ESM4::ScriptCode::Parameter> integer{ { 1, false } };
        std::string trailing;
        put16(trailing, 1);
        trailing += integerArgument(5) + 'x';
        std::string inlineExpression;
        put16(inlineExpression, 1);
        put16(inlineExpression, 0xFFFF);
        put16(inlineExpression, 0);

        std::string code = statement(0x1D);
        // 65 commands called with a byte left over, 85 with an argument that does not decode, 65 that are not in the
        // table
        for (std::uint16_t i = 0; i < 65; ++i)
            code += statement(static_cast<std::uint16_t>(0x1100 + i), trailing);
        for (std::uint16_t i = 0; i < 85; ++i)
            code += statement(static_cast<std::uint16_t>(0x1200 + i), inlineExpression);
        // the same command a second and third and fourth time, which does not make it a different one
        for (int i = 0; i < 3; ++i)
            code += statement(0x1200, inlineExpression);
        for (std::uint16_t i = 0; i < 65; ++i)
            code += statement(static_cast<std::uint16_t>(0x2000 + i));

        ESM4::ScriptCodeCensus census;
        census.setCommandLookup(integerCommands(integer));
        collect(census, scriptRecord(0x1001, code));

        // examples are kept for the first 80 commands and problems, and for the first three calls of each
        EXPECT_EQ(census.getArgumentExamples().size(), ESM4::ScriptCodeCensus::maxArgumentExampleKeys);
        EXPECT_EQ(census.getArgumentExamples().at({ 0x1200, ESM4::ScriptCode::ArgumentError::InlineExpression }).size(),
            ESM4::ScriptCodeCensus::maxArgumentExamples);
        EXPECT_EQ(census.getUnknownCommands().size(), 65u);

        const std::string text = written(census);
        EXPECT_THAT(text, HasSubstr("and 25 more commands"));
        EXPECT_THAT(text, HasSubstr("and 5 more commands"));
        EXPECT_THAT(text, HasSubstr("and 5 more codes"));
    }

    TEST(ESM4ScriptCodeCensusMoreTest, listsOnlyTheFirstFunctionsOfTheConditions)
    {
        std::string data = zString("EDID", "OFTestQuest");
        for (std::uint32_t function = 1; function <= 85; ++function)
            data += subRecord("CTDA", conditionData(0, 1.f, function, 0));

        ESM4::ScriptCodeCensus census;
        collect(census, record("QUST", 0x1001, data), "QUST");

        EXPECT_EQ(census.getConditionFunctions().size(), 85u);
        EXPECT_THAT(written(census), HasSubstr("and 5 more functions"));
    }

    TEST(ESM4ScriptCodeCensusMoreTest, readsTheScriptsOfDialogueTerminalsPackagesAndPerks)
    {
        const std::string code = statement(0x1D);
        const std::string script
            = scriptHeader(0, static_cast<std::uint32_t>(code.size()), 0, 0) + subRecord("SCDA", code);
        const std::string functionCondition = subRecord("CTDA", conditionData(0x40, 1.f, 14, 0x00010001));

        ESM4::ScriptCodeCensus census;

        // a response with a script that runs when it is said and another when it ends
        collect(census, record("INFO", 0x1001, script + subRecord("NEXT", "") + script), "INFO");
        ASSERT_EQ(census.getHolders().count("INFO"), 1u);
        EXPECT_EQ(census.getHolders().at("INFO").mScripts, 2u);
        EXPECT_EQ(census.getHolders().at("INFO").mDecoded, 2u);

        // a terminal with a condition of its own and a menu item that has a script and a condition
        collect(census,
            record("TERM", 0x1002,
                zString("EDID", "Terminal") + functionCondition + zString("ITXT", "Item") + zString("RNAM", "Result")
                    + subRecord("ANAM", "\x03") + script + functionCondition),
            "TERM");
        ASSERT_EQ(census.getHolders().count("TERM"), 1u);
        EXPECT_EQ(census.getHolders().at("TERM").mDecoded, 1u);

        // a package with a script for each of its events, and a condition
        collect(census,
            record("PACK", 0x1003,
                zString("EDID", "Package") + subRecord("PKDT", bytePattern(8, 1))
                    + subRecord("PLDT", bytePattern(12, 10)) + subRecord("PSDT", bytePattern(8, 20)) + functionCondition
                    + subRecord("POBA", "") + subRecord("INAM", formId(0x00010001)) + script + subRecord("POEA", "")
                    + subRecord("INAM", formId(0x00010002)) + script + subRecord("POCA", "")
                    + subRecord("INAM", formId(0x00010003)) + script),
            "PACK");
        ASSERT_EQ(census.getHolders().count("PACK"), 1u);
        EXPECT_EQ(census.getHolders().at("PACK").mDecoded, 3u);

        // a perk with a script before its first entry, and an entry with a script and a condition group
        collect(census,
            record("PERK", 0x1004,
                zString("EDID", "Perk") + functionCondition + script + subRecord("PRKE", bytePattern(3, 20))
                    + subRecord("DATA", bytePattern(3, 30)) + subRecord("PRKC", "\x04") + functionCondition + script
                    + subRecord("PRKF", "")),
            "PERK");
        ASSERT_EQ(census.getHolders().count("PERK"), 1u);
        EXPECT_EQ(census.getHolders().at("PERK").mDecoded, 2u);

        // the conditions of the terminal and its item, the package, and the perk and its entry
        EXPECT_EQ(census.getConditionFunctions().at(14).mUses, 5u);
        EXPECT_EQ(census.getConditionFunctions().at(14).mOnReference, 5u);
    }

    std::string groupHeader(std::uint32_t size, std::string_view label, std::int32_t type)
    {
        std::string result("GRUP");
        append(result, size);
        result.append(label);
        append(result, type);
        append<std::uint16_t>(result, 0);
        append<std::uint16_t>(result, 0);
        return result;
    }

    TEST(ESM4ScriptCodeCensusMoreTest, writesTheFatalErrorOfAPluginTheReaderCannotWalk)
    {
        // a group that says it holds more than the group around it has room for
        const std::string cell = record("CELL", 0x801, "");
        const std::string inner = groupHeader(
            static_cast<std::uint32_t>(20 + 20 + cell.size()), std::string_view("\0\0\0\0", 4), ESM4::Grp_CellChild);
        const std::string plugin = header() + groupHeader(static_cast<std::uint32_t>(20 + inner.size()), "SCPT", 0)
            + inner + cell + cell + cell;
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "base.esm", nullptr, nullptr);
        reader.setModIndex(0);

        ESM4::ScriptCodeCensus census;
        census.collect(reader);

        ASSERT_THAT(census.getFatalErrors(), SizeIs(1));
        EXPECT_EQ(census.getFatalErrors().front(), "Read more records than available");
        EXPECT_THAT(written(census), HasSubstr("FATAL ERROR: Read more records than available"));
    }
}
