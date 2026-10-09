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

    // Begin GameMode, If 1, Set local1 to 2, EndIf, End: with the jumps counted from the end of the statement to the
    // start of the one it leads to
    std::string blockCode()
    {
        std::string setData = variable(1);
        put16(setData, 1);
        setData += '2';
        const std::string set = statement(0x15, setData);
        std::string ifData;
        put16(ifData, static_cast<std::uint16_t>(set.size()));
        put16(ifData, 1);
        ifData += '1';
        const std::string branch = statement(0x16, ifData);
        const std::string endIf = statement(0x19);
        std::string beginData;
        put16(beginData, 0);
        append<std::uint32_t>(beginData, static_cast<std::uint32_t>(branch.size() + set.size() + endIf.size()));
        return statement(0x1D) + statement(0x10, beginData) + branch + set + endIf + statement(0x11);
    }

    std::string scriptRecord(std::uint32_t id, const std::string& code, std::uint16_t type = 0)
    {
        const std::string data = scriptHeader(0, static_cast<std::uint32_t>(code.size()), 1, type)
            + subRecord("SCDA", code) + localVariable(1);
        return record("SCPT", id, data);
    }

    void collect(ESM4::ScriptCodeCensus& census, const std::string& records)
    {
        ESM4::Reader reader(
            std::make_unique<std::istringstream>(header() + topGroup("SCPT", records)), "base.esm", nullptr, nullptr);
        reader.setModIndex(0);
        census.collect(reader);
        EXPECT_THAT(census.getFatalErrors(), IsEmpty());
    }

    TEST(ESM4ScriptCodeCensusTest, countsTheScriptsThatDecode)
    {
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, blockCode()) + scriptRecord(0x1002, statement(0x1D)));

        ASSERT_EQ(census.getHolders().count("SCPT"), 1u);
        const auto& totals = census.getHolders().at("SCPT");
        EXPECT_EQ(totals.mScripts, 2u);
        EXPECT_EQ(totals.mDecoded, 2u);
        EXPECT_EQ(totals.mFailed, 0u);
        EXPECT_EQ(totals.mEmpty, 0u);
        EXPECT_EQ(totals.mStatements, 6u + 1u);
        EXPECT_EQ(totals.mTypes.at(0), 2u);
        EXPECT_THAT(census.getStructure(), IsEmpty());

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("every Begin has its End and every If its EndIf"));
    }

    TEST(ESM4ScriptCodeCensusTest, countsBlocksByTypeOfScript)
    {
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, blockCode()) + scriptRecord(0x1002, blockCode(), 1));

        EXPECT_EQ(census.getBlocks().at({ 0, 0 }), 1u);
        EXPECT_EQ(census.getBlocks().at({ 1, 0 }), 1u);
    }

    TEST(ESM4ScriptCodeCensusTest, comparesJumpsWithTheStatementsTheyLeadTo)
    {
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, blockCode()));

        // The jumps of blockCode count from the end of the statement to the start of the one that follows the code
        using Census = ESM4::ScriptCodeCensus;
        for (const Census::JumpTally* tally : { &census.getBeginJumps(), &census.getIfJumps() })
        {
            EXPECT_EQ(tally->mTotal, 1u);
            EXPECT_EQ(tally->mUnresolved, 0u);
            EXPECT_EQ(tally->mMatches[Census::FromStatementEnd][Census::ToStatementStart], 1u);
            EXPECT_EQ(tally->mMatches[Census::FromStatementStart][Census::ToStatementStart], 0u);
            EXPECT_EQ(tally->mMatches[Census::FromStatementEnd][Census::ToStatementEnd], 0u);
        }
        EXPECT_EQ(census.getElseIfJumps().mTotal, 0u);
        EXPECT_EQ(census.getElseJumps().mTotal, 0u);

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("Begin to End: 1 jumps"));
        EXPECT_THAT(out.str(), HasSubstr("counted from the end of the statement to the start of the target: 1"));
    }

    TEST(ESM4ScriptCodeCensusTest, tellsHowFarOffTheJumpsAreFromEachRule)
    {
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, blockCode()));

        using Census = ESM4::ScriptCodeCensus;
        const auto& differences = census.getIfJumps().mDifferences;
        EXPECT_THAT(differences[Census::FromStatementEnd][Census::ToStatementStart], ElementsAre(Pair(0, 1u)));
        // counted from the start of the expression instead, the jump would be too short by the rest of the statement
        const auto& other = differences[Census::FromExpression][Census::ToStatementStart];
        ASSERT_EQ(other.size(), 1u);
        EXPECT_GT(other.begin()->first, 0);

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(),
            HasSubstr("the closest rule is from the end of the statement to the start of the target, the jump "
                      "falls short by (bytes: jumps): 0:1"));
    }

    TEST(ESM4ScriptCodeCensusTest, reportsWhyAScriptDoesNotDecode)
    {
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, statement(0x1D) + statement(0x40)) + scriptRecord(0x1002, blockCode()));

        const auto& totals = census.getHolders().at("SCPT");
        EXPECT_EQ(totals.mScripts, 2u);
        EXPECT_EQ(totals.mDecoded, 1u);
        EXPECT_EQ(totals.mFailed, 1u);
        EXPECT_EQ(census.getErrors().at(ESM4::ScriptCode::Error::UnknownStatement), 1u);

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("a code that is no statement or command: 1"));
        EXPECT_THAT(out.str(), HasSubstr("value 0x0040: 1"));
    }

    TEST(ESM4ScriptCodeCensusTest, keepsTheFirstScriptsThatFail)
    {
        ESM4::ScriptCodeCensus census;
        std::string records;
        for (std::uint32_t i = 0; i < ESM4::ScriptCodeCensus::maxFailuresPerError + 2; ++i)
            records += scriptRecord(0x1001 + i, statement(0x1D) + statement(0x40));
        collect(census, records);

        const auto& failures = census.getFailures().at(ESM4::ScriptCode::Error::UnknownStatement);
        ASSERT_EQ(failures.size(), ESM4::ScriptCodeCensus::maxFailuresPerError);
        EXPECT_EQ(failures[0].mHolder, "SCPT");
        EXPECT_EQ(failures[0].mRecord.mIndex, 0x1001u);
        EXPECT_EQ(failures[0].mSize, 8u);
        EXPECT_EQ(failures[0].mOffset, 4u);
        EXPECT_EQ(failures[0].mValue, 0x40u);
        // the bytes start 12 before the offset, which is the start of the file here, and mark the offset
        EXPECT_EQ(failures[0].mBytes, "1d 00 00 00 [40 00 00 00");
        EXPECT_EQ(failures[0].mStatements, "Sn [");
        EXPECT_EQ(census.getHolders().at("SCPT").mFailed, ESM4::ScriptCodeCensus::maxFailuresPerError + 2);

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("1d 00 00 00 [40 00 00 00"));
    }

    TEST(ESM4ScriptCodeCensusTest, doesNotCheckTheNestingOfAScriptThatStoppedAtAnError)
    {
        // A block is open and the script stops at a code that is nothing: the End and EndIf are not missing, they were
        // not reached
        std::string beginData;
        put16(beginData, 0);
        append<std::uint32_t>(beginData, 0);
        std::string ifData;
        put16(ifData, 0);
        put16(ifData, 1);
        ifData += '1';
        ESM4::ScriptCodeCensus census;
        collect(census,
            scriptRecord(
                0x1001, statement(0x1D) + statement(0x10, beginData) + statement(0x16, ifData) + statement(0x40)));

        EXPECT_EQ(census.getHolders().at("SCPT").mFailed, 1u);
        EXPECT_THAT(census.getStructure(), IsEmpty());
        EXPECT_EQ(census.getBeginJumps().mTotal, 0u);
        EXPECT_EQ(census.getIfJumps().mTotal, 0u);
    }

    TEST(ESM4ScriptCodeCensusTest, keepsTheFirstScriptsOfEachNestingProblem)
    {
        ESM4::ScriptCodeCensus census;
        std::string records;
        std::string setData = variable(1);
        put16(setData, 1);
        setData += '2';
        for (std::uint32_t i = 0; i < ESM4::ScriptCodeCensus::maxFailuresPerError + 2; ++i)
            records += scriptRecord(0x1001 + i, statement(0x1D) + statement(0x15, setData) + statement(0x19));
        collect(census, records);

        EXPECT_EQ(census.getStructure().at("EndIf without If"), ESM4::ScriptCodeCensus::maxFailuresPerError + 2);
        EXPECT_EQ(census.getStructureScripts(), ESM4::ScriptCodeCensus::maxFailuresPerError + 2);
        const auto& examples = census.getStructureExamples().at("EndIf without If");
        ASSERT_EQ(examples.size(), ESM4::ScriptCodeCensus::maxFailuresPerError);
        EXPECT_EQ(examples[0].mHolder, "SCPT");
        EXPECT_EQ(examples[0].mRecord.mIndex, 0x1001u);
        EXPECT_EQ(examples[0].mValue, 0x19u);
        EXPECT_EQ(examples[0].mStatements, "Sn St [Ef");

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("scripts with a problem: 7"));
        EXPECT_THAT(out.str(), HasSubstr("The first scripts of each nesting problem"));
        EXPECT_THAT(out.str(), HasSubstr("statements: Sn St [Ef"));
    }

    TEST(ESM4ScriptCodeCensusTest, reportsABlockThatIsNotClosed)
    {
        ESM4::ScriptCodeCensus census;
        std::string beginData;
        put16(beginData, 0);
        append<std::uint32_t>(beginData, 0);
        collect(census, scriptRecord(0x1001, statement(0x1D) + statement(0x10, beginData)));

        EXPECT_EQ(census.getStructure().at("Begin without End"), 1u);
        EXPECT_EQ(census.getBeginJumps().mUnresolved, 1u);
    }

    TEST(ESM4ScriptCodeCensusTest, aConditionThatIsOpenAtTheEndOfItsBlockDoesNotReachTheNextBlock)
    {
        std::string beginData;
        put16(beginData, 0);
        append<std::uint32_t>(beginData, 0);
        std::string ifData;
        put16(ifData, 0);
        put16(ifData, 1);
        ifData += '1';
        // the first block opens a condition and ends, the second closes one
        const std::string code = statement(0x1D) + statement(0x10, beginData) + statement(0x16, ifData)
            + statement(0x11) + statement(0x10, beginData) + statement(0x19) + statement(0x11);
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, code));

        EXPECT_EQ(census.getStructure().at("If without EndIf"), 1u);
        EXPECT_EQ(census.getStructure().at("EndIf without If"), 1u);
        EXPECT_EQ(census.getIfJumps().mUnresolved, 1u);
    }

    TEST(ESM4ScriptCodeCensusTest, noBranchFollowsAnElse)
    {
        std::string ifData;
        put16(ifData, 0);
        put16(ifData, 1);
        ifData += '1';
        std::string elseData;
        put16(elseData, 0);
        // If, Else, ElseIf, Else, EndIf
        const std::string code = statement(0x1D) + statement(0x16, ifData) + statement(0x17, elseData)
            + statement(0x18, ifData) + statement(0x17, elseData) + statement(0x19);
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, code));

        EXPECT_EQ(census.getStructure().at("ElseIf or Else after Else"), 2u);
        EXPECT_EQ(census.getStructure().count("If without EndIf"), 0u);
        EXPECT_EQ(census.getIfJumps().mTotal, 1u);
        EXPECT_EQ(census.getElseJumps().mTotal, 1u);
        EXPECT_EQ(census.getElseIfJumps().mTotal, 0u);
    }

    TEST(ESM4ScriptCodeCensusTest, countsTheCommandsByCode)
    {
        std::string code = statement(0x1D) + statement(0x1000 + 58, std::string(4, '\0'));
        code += statement(0x1000 + 58, std::string(2, '\0'));
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, code));

        ASSERT_EQ(census.getCommands().size(), 1u);
        const auto& use = census.getCommands().at(0x1000 + 58);
        EXPECT_EQ(use.mStatements, 2u);
        EXPECT_EQ(use.mExpressions, 0u);
        EXPECT_EQ(use.mOnReference, 0u);
        EXPECT_EQ(use.mScripts, 1u);
        EXPECT_EQ(use.mMaxArgumentBytes, 4u);
        EXPECT_EQ(use.mTotalArgumentBytes, 6u);
    }

    TEST(ESM4ScriptCodeCensusTest, skipsRecordsThatAreDeleted)
    {
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, blockCode()) + record("SCPT", 0x1001, "", ESM4::Rec_Deleted));

        EXPECT_EQ(census.getHolders().at("SCPT").mScripts, 1u);
    }

    TEST(ESM4ScriptCodeCensusTest, countsTheRecordsThatDoNotLoad)
    {
        // a sub-record that says it has more data than the record has
        const std::string broken
            = record("SCPT", 0x1002, std::string("SCHR") + std::string("\x32\x00", 2) + std::string(8, '\0'));
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, blockCode()) + broken);

        EXPECT_EQ(census.getRecordsFailed().at("SCPT"), 1u);
        EXPECT_EQ(census.getHolders().at("SCPT").mScripts, 1u);
    }

    // The parameters of a few commands, by code: 0x1010 takes an integer and an optional integer, 0x1011 a reference
    ESM4::ScriptCodeCensus::CommandLookup testCommands(
        const std::vector<ESM4::ScriptCode::Parameter>& integers, const std::vector<ESM4::ScriptCode::Parameter>& form)
    {
        return [&integers, &form](std::uint16_t opcode) {
            ESM4::ScriptCodeCensus::CommandSignature signature;
            if (opcode == 0x1010)
            {
                signature.mName = "TakeNumbers";
                signature.mParameters = &integers;
            }
            else if (opcode == 0x1011)
            {
                signature.mName = "TakeForm";
                signature.mParameters = &form;
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

    TEST(ESM4ScriptCodeCensusTest, readsTheArgumentsOfCallsByTheParametersOfTheirCommands)
    {
        const std::vector<ESM4::ScriptCode::Parameter> integers{ { 1, false }, { 1, true } };
        const std::vector<ESM4::ScriptCode::Parameter> form{ { 14, false } };
        std::string call;
        put16(call, 1);
        call += integerArgument(5);
        std::string twoNumbers;
        put16(twoNumbers, 2);
        twoNumbers += integerArgument(5) + integerArgument(6);
        std::string tooMany;
        put16(tooMany, 3);
        tooMany += integerArgument(5) + integerArgument(6) + integerArgument(7);
        std::string badReference;
        put16(badReference, 1);
        badReference += 'r';
        put16(badReference, 4);
        std::string extra = twoNumbers + std::string(3, 'x');
        std::string none;
        put16(none, 0);

        const std::string code = statement(0x1D) + statement(0x1010, call) + statement(0x1010, twoNumbers)
            + statement(0x1010, tooMany) + statement(0x1010, extra) + statement(0x1011, badReference)
            + statement(0x1010, none) + statement(0x1234, none);
        ESM4::ScriptCodeCensus census;
        census.setCommandLookup(testCommands(integers, form));
        collect(census, scriptRecord(0x1001, code));

        const auto& numbers = census.getArguments().at(0x1010);
        EXPECT_EQ(numbers.mCalls, 5u);
        EXPECT_EQ(numbers.mDecoded, 4u);
        EXPECT_EQ(numbers.mTrailing, 1u);
        EXPECT_EQ(numbers.mMaxTrailing, 3u);
        EXPECT_EQ(numbers.mFewer, 1u);
        EXPECT_EQ(numbers.mErrors.at(ESM4::ScriptCode::ArgumentError::TooMany), 1u);
        const auto& forms = census.getArguments().at(0x1011);
        EXPECT_EQ(forms.mCalls, 1u);
        EXPECT_EQ(forms.mErrors.at(ESM4::ScriptCode::ArgumentError::BadReference), 1u);
        EXPECT_EQ(census.getUnknownCommands().at(0x1234), 1u);

        const auto& examples = census.getArgumentExamples();
        ASSERT_EQ(examples.size(), 2u);
        const auto& example = examples.at({ 0x1011, ESM4::ScriptCode::ArgumentError::BadReference }).front();
        EXPECT_EQ(example.mSize, 5u);
        EXPECT_EQ(example.mOffset, 2u);
        EXPECT_EQ(example.mBytes, "01 00 72 04 00");
        EXPECT_THAT(example.mStatements, HasSubstr("TakeForm (14)"));

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("calls of commands the table has: 6, arguments decoded: 4"));
        EXPECT_THAT(out.str(), HasSubstr("calls of commands the table does not have: 1 (1 different codes)"));
        EXPECT_THAT(out.str(), HasSubstr("0x1010 TakeNumbers"));
        EXPECT_THAT(out.str(), HasSubstr("0x1011 TakeForm"));
        EXPECT_THAT(out.str(), HasSubstr("badref:1"));
    }

    TEST(ESM4ScriptCodeCensusTest, readsNoArgumentsWithoutATableOfCommands)
    {
        std::string none;
        put16(none, 0);
        ESM4::ScriptCodeCensus census;
        collect(census, scriptRecord(0x1001, statement(0x1D) + statement(0x1010, none)));

        EXPECT_THAT(census.getArguments(), IsEmpty());
        EXPECT_THAT(census.getUnknownCommands(), IsEmpty());
        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), Not(HasSubstr("Arguments, read with")));
    }
}
