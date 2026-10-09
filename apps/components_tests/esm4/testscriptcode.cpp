#include <components/esm4/scriptcode.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4::ScriptCode;

    void put16(std::string& out, std::uint16_t value)
    {
        out.push_back(static_cast<char>(value & 0xff));
        out.push_back(static_cast<char>(value >> 8));
    }

    void put32(std::string& out, std::uint32_t value)
    {
        put16(out, static_cast<std::uint16_t>(value & 0xffff));
        put16(out, static_cast<std::uint16_t>(value >> 16));
    }

    // A statement: the code, the length of its data and the data
    std::string statement(std::uint16_t code, std::string_view data = {})
    {
        std::string result;
        put16(result, code);
        put16(result, static_cast<std::uint16_t>(data.size()));
        result.append(data);
        return result;
    }

    // A command called on a reference: 1C, the reference, then the statement of the command
    std::string referenceCall(std::uint16_t reference, std::uint16_t code, std::string_view data = {})
    {
        std::string result;
        put16(result, Statement_ReferenceCall);
        put16(result, reference);
        return result + statement(code, data);
    }

    std::string variable(char type, std::uint16_t index, std::uint16_t remote = 0)
    {
        std::string result;
        if (remote != 0)
        {
            result.push_back('r');
            put16(result, remote);
        }
        result.push_back(type);
        put16(result, index);
        return result;
    }

    std::string indexed(char tag, std::uint16_t index)
    {
        std::string result(1, tag);
        put16(result, index);
        return result;
    }

    std::string text(std::string_view value)
    {
        std::string result(1, '"');
        put16(result, static_cast<std::uint16_t>(value.size()));
        result.append(value);
        return result;
    }

    std::string call(std::uint16_t code, std::string_view arguments = {}, std::uint16_t remote = 0)
    {
        std::string result;
        if (remote != 0)
            result += indexed('r', remote);
        result.push_back('X');
        put16(result, code);
        put16(result, static_cast<std::uint16_t>(arguments.size()));
        result.append(arguments);
        return result;
    }

    // The data of an If or ElseIf: the jump, the length of the expression and the expression
    std::string condition(std::uint16_t jump, std::string_view expression)
    {
        std::string result;
        put16(result, jump);
        put16(result, static_cast<std::uint16_t>(expression.size()));
        result.append(expression);
        return result;
    }

    // The data of a SetTo: the variable, the length of the expression and the expression
    std::string assignment(std::string_view target, std::string_view expression)
    {
        std::string result(target);
        put16(result, static_cast<std::uint16_t>(expression.size()));
        result.append(expression);
        return result;
    }

    std::string beginData(std::uint16_t blockType, std::uint32_t blockLength, std::string_view arguments = {})
    {
        std::string result;
        put16(result, blockType);
        put32(result, blockLength);
        result.append(arguments);
        return result;
    }

    Program decodeText(const std::string& code, std::size_t references = 4, std::size_t variables = 3)
    {
        return decode(std::vector<std::uint8_t>(code.begin(), code.end()), Limits{ references, variables });
    }

    const std::string scriptName = statement(Statement_ScriptName);

    TEST(ESM4ScriptCodeTest, anEmptyScriptHasOnlyItsName)
    {
        const Program program = decodeText(scriptName);

        EXPECT_EQ(program.mError, Error::None);
        ASSERT_EQ(program.mStatements.size(), 1u);
        EXPECT_EQ(program.mStatements[0].mKind, Statement::Kind::ScriptName);
        EXPECT_EQ(program.mStatements[0].mEnd, 4u);
    }

    TEST(ESM4ScriptCodeTest, aScriptWithNoBytesHasNoStatements)
    {
        const Program program = decodeText("");

        EXPECT_EQ(program.mError, Error::None);
        EXPECT_TRUE(program.mStatements.empty());
    }

    TEST(ESM4ScriptCodeTest, readsABlockWithItsTypeAndLength)
    {
        const std::string begin = statement(Statement_Begin, beginData(0, 4));
        const Program program = decodeText(scriptName + begin + statement(Statement_End));

        ASSERT_EQ(program.mError, Error::None);
        ASSERT_EQ(program.mStatements.size(), 3u);
        const Statement& block = program.mStatements[1];
        EXPECT_EQ(block.mKind, Statement::Kind::Begin);
        EXPECT_EQ(block.mBlockType, 0);
        EXPECT_EQ(block.mBlockLength, 4u);
        EXPECT_EQ(block.mOffset, 4u);
        EXPECT_EQ(block.mArgumentsOffset, block.mEnd);
        EXPECT_EQ(program.mStatements[2].mKind, Statement::Kind::End);
        EXPECT_EQ(program.mStatements[2].mOffset, block.mEnd);
    }

    TEST(ESM4ScriptCodeTest, aBlockKeepsTheBytesOfItsParameters)
    {
        std::string arguments;
        put16(arguments, 0); // no parameters
        const Program program = decodeText(statement(Statement_Begin, beginData(2, 4, arguments)));

        ASSERT_EQ(program.mError, Error::None);
        const Statement& block = program.mStatements.at(0);
        EXPECT_EQ(block.mBlockType, 2);
        EXPECT_EQ(block.mEnd - block.mArgumentsOffset, 2u);
    }

    TEST(ESM4ScriptCodeTest, readsAnAssignmentToALocalVariable)
    {
        // Set local2 to 3 + 4
        const std::string expression = "3 4 +";
        const Program program = decodeText(statement(Statement_SetTo, assignment(variable('s', 2), expression)));

        ASSERT_EQ(program.mError, Error::None);
        const Statement& set = program.mStatements.at(0);
        EXPECT_EQ(set.mKind, Statement::Kind::SetTo);
        EXPECT_EQ(set.mTarget.mType, 's');
        EXPECT_EQ(set.mTarget.mIndex, 2);
        EXPECT_EQ(set.mTarget.mRemote, 0);
        ASSERT_EQ(set.mTokenCount, 3u);
        EXPECT_EQ(program.mTokens[set.mFirstToken].mKind, Token::Kind::Number);
        EXPECT_EQ(program.mTokens[set.mFirstToken].mNumber, 3);
        EXPECT_EQ(program.mTokens[set.mFirstToken + 1].mNumber, 4);
        EXPECT_EQ(program.mTokens[set.mFirstToken + 2].mKind, Token::Kind::Operator);
        EXPECT_EQ(program.mTokens[set.mFirstToken + 2].mOperator, Operator::Add);
    }

    TEST(ESM4ScriptCodeTest, readsAnAssignmentToAGlobalAndToTheVariableOfAnotherScript)
    {
        const std::string toGlobal = statement(Statement_SetTo, assignment(indexed('G', 2), "1"));
        const std::string toRemote = statement(Statement_SetTo, assignment(variable('f', 7, 3), "2.5"));
        const Program program = decodeText(toGlobal + toRemote);

        ASSERT_EQ(program.mError, Error::None);
        ASSERT_EQ(program.mStatements.size(), 2u);
        EXPECT_EQ(program.mStatements[0].mGlobal, 2);
        EXPECT_EQ(program.mStatements[1].mTarget.mRemote, 3);
        EXPECT_EQ(program.mStatements[1].mTarget.mType, 'f');
        EXPECT_EQ(program.mStatements[1].mTarget.mIndex, 7);
        EXPECT_DOUBLE_EQ(program.mTokens.at(program.mStatements[1].mFirstToken).mNumber, 2.5);
    }

    TEST(ESM4ScriptCodeTest, readsTheTokensOfAnExpression)
    {
        // quest stage < 10 && local1 != global2: reverse Polish order
        std::string expression = call(0x1000 + 58, "args");
        expression += " 10 < ";
        expression += variable('s', 1);
        expression += indexed('G', 2);
        expression += " != && ";
        expression += indexed('Z', 1) + indexed('Z', 4) + "== ";
        expression += text("text") + "== || ~";
        const Program program = decodeText(statement(Statement_If, condition(5, expression)));

        ASSERT_EQ(program.mError, Error::None) << errorText(program.mError);
        const Statement& statement = program.mStatements.at(0);
        EXPECT_EQ(statement.mKind, Statement::Kind::If);
        EXPECT_EQ(statement.mJump, 5);
        std::vector<Token::Kind> kinds;
        for (std::uint32_t i = 0; i < statement.mTokenCount; ++i)
            kinds.push_back(program.mTokens[statement.mFirstToken + i].mKind);
        using Kind = Token::Kind;
        EXPECT_THAT(kinds,
            ElementsAre(Kind::Call, Kind::Number, Kind::Operator, Kind::Variable, Kind::Global, Kind::Operator,
                Kind::Operator, Kind::Reference, Kind::Reference, Kind::Operator, Kind::String, Kind::Operator,
                Kind::Operator, Kind::Operator));
        ASSERT_EQ(program.mCalls.size(), 1u);
        EXPECT_EQ(program.mCalls[0].mOpcode, 0x1000 + 58);
        EXPECT_EQ(program.mCalls[0].mLength, 4);
        EXPECT_EQ(program.mTokens[statement.mFirstToken + 4].mIndex, 2);
        EXPECT_EQ(program.mTokens[statement.mFirstToken + 8].mIndex, 4);
        EXPECT_EQ(program.mTokens[statement.mFirstToken + 10].mText, "text");
        EXPECT_EQ(program.mTokens[statement.mFirstToken + 13].mOperator, Operator::Negate);
    }

    TEST(ESM4ScriptCodeTest, readsEveryOperator)
    {
        const std::vector<std::pair<std::string, Operator>> operators{ { "&&", Operator::And }, { "||", Operator::Or },
            { "<=", Operator::LessOrEqual }, { ">=", Operator::GreaterOrEqual }, { "==", Operator::Equal },
            { "!=", Operator::NotEqual }, { "<", Operator::Less }, { ">", Operator::Greater },
            { "-", Operator::Subtract }, { "+", Operator::Add }, { "*", Operator::Multiply }, { "/", Operator::Divide },
            { "%", Operator::Modulo } };
        for (const auto& [text, op] : operators)
        {
            const Program program = decodeText(statement(Statement_SetTo, assignment(variable('s', 1), "1 2" + text)));
            ASSERT_EQ(program.mError, Error::None) << text;
            const Statement& set = program.mStatements.at(0);
            ASSERT_EQ(set.mTokenCount, 3u) << text;
            EXPECT_EQ(program.mTokens[set.mFirstToken + 2].mOperator, op) << text;
            EXPECT_EQ(operatorText(op), text);
        }
        EXPECT_EQ(operatorText(Operator::Negate), "~");
    }

    TEST(ESM4ScriptCodeTest, readsNumbersWrittenAsText)
    {
        const Program program
            = decodeText(statement(Statement_SetTo, assignment(variable('f', 1), "0.5 12 + 3. + .25 + 1e3 +")));

        ASSERT_EQ(program.mError, Error::None) << errorText(program.mError);
        const Statement& set = program.mStatements.at(0);
        std::vector<double> numbers;
        for (std::uint32_t i = 0; i < set.mTokenCount; ++i)
            if (program.mTokens[set.mFirstToken + i].mKind == Token::Kind::Number)
                numbers.push_back(program.mTokens[set.mFirstToken + i].mNumber);
        EXPECT_THAT(numbers, ElementsAre(0.5, 12, 3, 0.25, 1000));
    }

    TEST(ESM4ScriptCodeTest, aVariableOrACallBelongsToTheReferenceBeforeIt)
    {
        std::string expression = variable('s', 5, 2);
        expression += call(0x1000 + 14, "", 3);
        expression += "+";
        const Program program = decodeText(statement(Statement_SetTo, assignment(variable('s', 1), expression)));

        ASSERT_EQ(program.mError, Error::None) << errorText(program.mError);
        const Statement& set = program.mStatements.at(0);
        EXPECT_EQ(program.mTokens[set.mFirstToken].mVariable.mRemote, 2);
        EXPECT_EQ(program.mTokens[set.mFirstToken].mVariable.mIndex, 5);
        EXPECT_EQ(program.mCalls.at(0).mReference, 3);
    }

    TEST(ESM4ScriptCodeTest, readsABranchingStatement)
    {
        std::string code = scriptName;
        code += statement(Statement_If, condition(14, "1"));
        code += statement(Statement_ElseIf, condition(6, "0"));
        code += statement(Statement_Else, std::string("\x04\x00", 2));
        code += statement(Statement_EndIf);
        code += statement(Statement_Return);
        const Program program = decodeText(code);

        ASSERT_EQ(program.mError, Error::None) << errorText(program.mError);
        std::vector<Statement::Kind> kinds;
        for (const Statement& s : program.mStatements)
            kinds.push_back(s.mKind);
        using Kind = Statement::Kind;
        EXPECT_THAT(
            kinds, ElementsAre(Kind::ScriptName, Kind::If, Kind::ElseIf, Kind::Else, Kind::EndIf, Kind::Return));
        EXPECT_EQ(program.mStatements[1].mJump, 14);
        EXPECT_EQ(program.mStatements[2].mJump, 6);
        EXPECT_EQ(program.mStatements[3].mJump, 4);
    }

    TEST(ESM4ScriptCodeTest, readsACommandAsAStatement)
    {
        const Program program = decodeText(statement(0x1000 + 100, std::string("\x01\x00\x72\x01\x00", 5)));

        ASSERT_EQ(program.mError, Error::None);
        const Statement& statement = program.mStatements.at(0);
        ASSERT_EQ(statement.mKind, Statement::Kind::Call);
        const Call& call = program.mCalls.at(statement.mCall);
        EXPECT_EQ(call.mOpcode, 0x1000 + 100);
        EXPECT_EQ(call.mReference, 0);
        EXPECT_EQ(call.mOffset, 4u);
        EXPECT_EQ(call.mLength, 5);
    }

    TEST(ESM4ScriptCodeTest, readsACommandCalledOnAReference)
    {
        const Program program = decodeText(referenceCall(3, 0x1000 + 7, std::string("\x00\x00", 2)));

        ASSERT_EQ(program.mError, Error::None) << errorText(program.mError);
        const Statement& statement = program.mStatements.at(0);
        ASSERT_EQ(statement.mKind, Statement::Kind::Call);
        const Call& call = program.mCalls.at(statement.mCall);
        EXPECT_EQ(call.mReference, 3);
        EXPECT_EQ(call.mOpcode, 0x1000 + 7);
        EXPECT_EQ(call.mLength, 2);
        EXPECT_EQ(statement.mOffset, 0u);
        EXPECT_EQ(statement.mEnd, 10u);
    }

    TEST(ESM4ScriptCodeTest, stopsAtAStatementThatRunsPastTheEnd)
    {
        std::string code = scriptName + statement(Statement_Return);
        code.pop_back(); // the length of the last statement is cut short
        code.pop_back();
        code += std::string("\x04\x00", 2);
        const Program program = decodeText(code);

        EXPECT_EQ(program.mError, Error::Truncated);
        EXPECT_EQ(program.mErrorOffset, 4u);
        EXPECT_EQ(program.mStatements.size(), 1u);
    }

    TEST(ESM4ScriptCodeTest, stopsAtACodeThatIsNothing)
    {
        const Program program = decodeText(scriptName + statement(0x40));

        EXPECT_EQ(program.mError, Error::UnknownStatement);
        EXPECT_EQ(program.mErrorValue, 0x40);
        EXPECT_EQ(program.mStatements.size(), 1u);
    }

    TEST(ESM4ScriptCodeTest, rejectsAReferenceThatTheScriptDoesNotHave)
    {
        EXPECT_EQ(decodeText(referenceCall(5, 0x1000), 4).mError, Error::BadReference);
        EXPECT_EQ(decodeText(referenceCall(0, 0x1000), 4).mError, Error::BadReference);
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(indexed('G', 9), "1")), 4).mError, Error::BadReference);
        EXPECT_EQ(decodeText(statement(Statement_SetTo, assignment(variable('s', 1), indexed('Z', 5))), 4).mError,
            Error::BadReference);
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(variable('s', 1), variable('s', 1, 9) + "+")), 4).mError,
            Error::BadReference);
    }

    TEST(ESM4ScriptCodeTest, rejectsAVariableThatTheScriptDoesNotHave)
    {
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(variable('s', 4), "1")), 4, 3).mError, Error::BadVariable);
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(variable('s', 0), "1")), 4, 3).mError, Error::BadVariable);
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(variable('x', 1), "1")), 4, 3).mError, Error::BadVariable);
        EXPECT_EQ(decodeText(statement(Statement_SetTo, assignment(variable('s', 1), variable('f', 4))), 4, 3).mError,
            Error::BadVariable);
        // A variable of another script is not checked against the variables of this one
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(variable('s', 9, 2), "1")), 4, 3).mError, Error::None);
    }

    TEST(ESM4ScriptCodeTest, rejectsAnExpressionThatDoesNotMakeOneValue)
    {
        const auto set = [](std::string_view expression) {
            return decodeText(statement(Statement_SetTo, assignment(variable('s', 1), expression)));
        };
        EXPECT_EQ(set("1 +").mError, Error::BadOperands);
        EXPECT_EQ(set("1 2").mError, Error::BadOperands);
        EXPECT_EQ(set("").mError, Error::BadOperands);
        EXPECT_EQ(set("~").mError, Error::BadOperands);
        EXPECT_EQ(set("1 ~").mError, Error::None);
        EXPECT_EQ(set("1 2 3 + +").mError, Error::None);
    }

    TEST(ESM4ScriptCodeTest, rejectsAByteThatIsNoToken)
    {
        const Program program = decodeText(statement(Statement_SetTo, assignment(variable('s', 1), "1 # +")));

        EXPECT_EQ(program.mError, Error::BadExpression);
        EXPECT_EQ(program.mErrorValue, '#');
    }

    TEST(ESM4ScriptCodeTest, rejectsAnExpressionOrCallThatRunsPastItsStatement)
    {
        std::string data = assignment(variable('s', 1), "1");
        data[3] = 20; // the length of the expression
        EXPECT_EQ(decodeText(statement(Statement_SetTo, data)).mError, Error::BadLength);

        std::string cutCall = call(0x1000, "abcd");
        cutCall.resize(cutCall.size() - 2);
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(variable('s', 1), cutCall))).mError, Error::Truncated);

        std::string cutText = text("abcdef");
        cutText.resize(cutText.size() - 2);
        EXPECT_EQ(
            decodeText(statement(Statement_SetTo, assignment(variable('s', 1), cutText))).mError, Error::Truncated);
    }

    TEST(ESM4ScriptCodeTest, aCommandInAnExpressionMustBeACommand)
    {
        EXPECT_EQ(decodeText(statement(Statement_SetTo, assignment(variable('s', 1), call(0x0040)))).mError,
            Error::UnknownStatement);
    }

    TEST(ESM4ScriptCodeTest, aBlockMustHoldItsTypeAndLength)
    {
        EXPECT_EQ(decodeText(statement(Statement_Begin, "abc")).mError, Error::BadLength);
        EXPECT_EQ(decodeText(statement(Statement_Else, "abc")).mError, Error::BadLength);
        EXPECT_EQ(decodeText(statement(Statement_If, "ab")).mError, Error::BadLength);
    }

    TEST(ESM4ScriptCodeTest, doesNotReadPastItsInput)
    {
        // Every prefix of a script that uses all the kinds of token either decodes or fails cleanly
        std::string code = scriptName + statement(Statement_Begin, beginData(0, 0));
        code += statement(Statement_If, condition(0, variable('s', 1) + indexed('Z', 1) + "==" + text("x") + "||"));
        code += referenceCall(2, 0x1000 + 5, std::string("\x00\x00", 2));
        code += statement(Statement_SetTo, assignment(variable('f', 2, 3), call(0x1010, "ab", 4) + "1.5+"));
        code += statement(Statement_End);
        for (std::size_t size = 0; size <= code.size(); ++size)
        {
            const Program program = decodeText(code.substr(0, size));
            if (size == code.size())
                EXPECT_EQ(program.mError, Error::None) << errorText(program.mError);
        }
    }

    TEST(ESM4ScriptCodeTest, describesEveryError)
    {
        for (const Error error : { Error::None, Error::Truncated, Error::UnknownStatement, Error::BadReference,
                 Error::BadVariable, Error::BadExpression, Error::BadOperands, Error::BadLength })
            EXPECT_FALSE(errorText(error).empty());
    }
}
