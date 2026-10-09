#include "scriptcode.hpp"

#include <array>
#include <cctype>
#include <locale>
#include <sstream>

namespace ESM4::ScriptCode
{
    namespace
    {
        struct OperatorSpelling
        {
            std::string_view mText;
            Operator mOperator;
        };

        // Longest first, so that "<=" is found before "<". There is no unary plus, and "-" is only the binary operator:
        // a negative number is written as the operator "~" after the number.
        constexpr std::array<OperatorSpelling, 14> operatorSpellings{ {
            { "&&", Operator::And },
            { "||", Operator::Or },
            { "<=", Operator::LessOrEqual },
            { ">=", Operator::GreaterOrEqual },
            { "==", Operator::Equal },
            { "!=", Operator::NotEqual },
            { "<", Operator::Less },
            { ">", Operator::Greater },
            { "-", Operator::Subtract },
            { "+", Operator::Add },
            { "*", Operator::Multiply },
            { "/", Operator::Divide },
            { "%", Operator::Modulo },
            { "~", Operator::Negate },
        } };

        class Decoder
        {
        public:
            Decoder(const std::vector<std::uint8_t>& code, const Limits& limits, Program& program)
                : mCode(code)
                , mLimits(limits)
                , mProgram(program)
            {
            }

            void run()
            {
                std::size_t position = 0;
                while (position < mCode.size() && mProgram.mError == Error::None)
                    position = statement(position);
            }

        private:
            const std::vector<std::uint8_t>& mCode;
            const Limits& mLimits;
            Program& mProgram;

            bool fail(Error error, std::size_t offset, std::uint16_t value = 0)
            {
                if (mProgram.mError == Error::None)
                {
                    mProgram.mError = error;
                    mProgram.mErrorOffset = static_cast<std::uint32_t>(offset);
                    mProgram.mErrorValue = value;
                }
                return false;
            }

            std::uint16_t u16(std::size_t offset) const
            {
                return static_cast<std::uint16_t>(mCode[offset] | (mCode[offset + 1] << 8));
            }

            std::uint32_t u32(std::size_t offset) const
            {
                return static_cast<std::uint32_t>(u16(offset)) | (static_cast<std::uint32_t>(u16(offset + 2)) << 16);
            }

            bool referenceInRange(std::uint16_t index) const { return index >= 1 && index <= mLimits.mReferences; }

            // A variable written as 's' or 'f' and a 16 bit index, after an optional 'r' and a reference index for the
            // variable of another script. Reads from `position` up to `end` and moves position past it.
            bool variable(std::size_t& position, std::size_t end, VariableRef& out)
            {
                const std::size_t start = position;
                out = VariableRef{};
                if (position < end && mCode[position] == 'r')
                {
                    if (end - position < 3)
                        return fail(Error::Truncated, start);
                    out.mRemote = u16(position + 1);
                    if (!referenceInRange(out.mRemote))
                        return fail(Error::BadReference, start, out.mRemote);
                    position += 3;
                }
                if (position >= end)
                    return fail(Error::Truncated, start);
                const std::uint8_t type = mCode[position];
                // 'l' only appears in expressions of scripts that other tools wrote, but costs nothing to accept
                if (type != 's' && type != 'f' && type != 'l')
                    return fail(Error::BadVariable, position, type);
                if (end - position < 3)
                    return fail(Error::Truncated, start);
                out.mType = type == 'f' ? 'f' : 's';
                out.mIndex = u16(position + 1);
                // A variable of another script can not be checked against the variables of this one
                if (out.mRemote == 0 && (out.mIndex < 1 || out.mIndex > mLimits.mVariables))
                    return fail(Error::BadVariable, start, out.mIndex);
                position += 3;
                return true;
            }

            // The expression in [position, end): tokens in reverse Polish order, appended to the program.
            bool expression(std::size_t position, std::size_t end, std::uint32_t& first, std::uint32_t& count)
            {
                first = static_cast<std::uint32_t>(mProgram.mTokens.size());
                std::size_t depth = 0;
                // The reference that the next variable or call belongs to
                std::uint16_t remote = 0;
                bool haveRemote = false;

                auto push = [&](Token token) {
                    mProgram.mTokens.push_back(std::move(token));
                    ++depth;
                };

                while (position < end)
                {
                    const std::size_t tokenStart = position;
                    const std::uint8_t c = mCode[position];
                    if (c <= 0x20)
                    {
                        ++position;
                        continue;
                    }

                    if (haveRemote && c != 's' && c != 'l' && c != 'f' && c != 'X')
                        return fail(Error::BadExpression, tokenStart, c);

                    switch (c)
                    {
                        case 's':
                        case 'l':
                        case 'f':
                        {
                            if (end - position < 3)
                                return fail(Error::Truncated, tokenStart);
                            Token token;
                            token.mKind = Token::Kind::Variable;
                            // An 'r' token before it names the object or quest whose variable it is
                            token.mVariable.mRemote = haveRemote ? remote : 0;
                            token.mVariable.mType = c == 'f' ? 'f' : 's';
                            token.mVariable.mIndex = u16(position + 1);
                            if (!haveRemote
                                && (token.mVariable.mIndex < 1 || token.mVariable.mIndex > mLimits.mVariables))
                                return fail(Error::BadVariable, tokenStart, token.mVariable.mIndex);
                            position += 3;
                            haveRemote = false;
                            push(std::move(token));
                            continue;
                        }
                        case 'G':
                        case 'Z':
                        {
                            if (end - position < 3)
                                return fail(Error::Truncated, tokenStart);
                            Token token;
                            token.mKind = c == 'G' ? Token::Kind::Global : Token::Kind::Reference;
                            token.mIndex = u16(position + 1);
                            if (!referenceInRange(token.mIndex))
                                return fail(Error::BadReference, tokenStart, token.mIndex);
                            position += 3;
                            push(std::move(token));
                            continue;
                        }
                        case 'r':
                        {
                            if (end - position < 3)
                                return fail(Error::Truncated, tokenStart);
                            remote = u16(position + 1);
                            if (!referenceInRange(remote))
                                return fail(Error::BadReference, tokenStart, remote);
                            haveRemote = true;
                            position += 3;
                            continue;
                        }
                        case '"':
                        {
                            if (end - position < 3)
                                return fail(Error::Truncated, tokenStart);
                            const std::size_t length = u16(position + 1);
                            if (end - position - 3 < length)
                                return fail(Error::Truncated, tokenStart);
                            Token token;
                            token.mKind = Token::Kind::String;
                            token.mText.assign(reinterpret_cast<const char*>(mCode.data() + position + 3), length);
                            position += 3 + length;
                            push(std::move(token));
                            continue;
                        }
                        case 'X':
                        {
                            if (end - position < 5)
                                return fail(Error::Truncated, tokenStart);
                            Call call;
                            call.mReference = haveRemote ? remote : 0;
                            call.mOpcode = u16(position + 1);
                            call.mLength = u16(position + 3);
                            call.mOffset = static_cast<std::uint32_t>(position + 5);
                            if (end - position - 5 < call.mLength)
                                return fail(Error::Truncated, tokenStart);
                            if (call.mOpcode < firstCommand)
                                return fail(Error::UnknownStatement, tokenStart, call.mOpcode);
                            position += 5 + call.mLength;
                            haveRemote = false;
                            Token token;
                            token.mKind = Token::Kind::Call;
                            token.mCall = static_cast<std::uint32_t>(mProgram.mCalls.size());
                            mProgram.mCalls.push_back(call);
                            push(std::move(token));
                            continue;
                        }
                        default:
                            break;
                    }

                    if (const Operator* op = readOperator(position, end))
                    {
                        const std::size_t operands = *op == Operator::Negate ? 1 : 2;
                        if (depth < operands)
                            return fail(Error::BadOperands, tokenStart);
                        depth -= operands - 1;
                        Token token;
                        token.mKind = Token::Kind::Operator;
                        token.mOperator = *op;
                        mProgram.mTokens.push_back(std::move(token));
                        continue;
                    }

                    double number;
                    if (readNumber(position, end, number))
                    {
                        Token token;
                        token.mKind = Token::Kind::Number;
                        token.mNumber = number;
                        push(std::move(token));
                        continue;
                    }

                    return fail(Error::BadExpression, tokenStart, c);
                }

                if (haveRemote)
                    return fail(Error::BadExpression, end);
                if (depth != 1)
                    return fail(Error::BadOperands, end);
                count = static_cast<std::uint32_t>(mProgram.mTokens.size()) - first;
                return true;
            }

            const Operator* readOperator(std::size_t& position, std::size_t end) const
            {
                for (const OperatorSpelling& spelling : operatorSpellings)
                {
                    if (end - position < spelling.mText.size())
                        continue;
                    bool same = true;
                    for (std::size_t i = 0; i < spelling.mText.size() && same; ++i)
                        same = mCode[position + i] == static_cast<std::uint8_t>(spelling.mText[i]);
                    if (same)
                    {
                        position += spelling.mText.size();
                        return &spelling.mOperator;
                    }
                }
                return nullptr;
            }

            // A number is written as text: digits, a decimal point and perhaps an exponent.
            bool readNumber(std::size_t& position, std::size_t end, double& out) const
            {
                std::size_t at = position;
                bool digits = false;
                while (at < end && std::isdigit(mCode[at]))
                {
                    ++at;
                    digits = true;
                }
                if (at < end && mCode[at] == '.')
                {
                    ++at;
                    while (at < end && std::isdigit(mCode[at]))
                    {
                        ++at;
                        digits = true;
                    }
                }
                if (!digits)
                    return false;
                if (at < end && (mCode[at] == 'e' || mCode[at] == 'E'))
                {
                    std::size_t exponent = at + 1;
                    if (exponent < end && (mCode[exponent] == '+' || mCode[exponent] == '-'))
                        ++exponent;
                    std::size_t exponentDigits = exponent;
                    while (exponentDigits < end && std::isdigit(mCode[exponentDigits]))
                        ++exponentDigits;
                    if (exponentDigits > exponent)
                        at = exponentDigits;
                }
                std::istringstream stream(
                    std::string(reinterpret_cast<const char*>(mCode.data() + position), at - position));
                stream.imbue(std::locale::classic());
                stream >> out;
                if (stream.fail())
                    return false;
                position = at;
                return true;
            }

            // Decodes the statement that starts at `start` and returns where the next one starts.
            std::size_t statement(std::size_t start)
            {
                if (mCode.size() - start < 4)
                {
                    fail(Error::Truncated, start);
                    return mCode.size();
                }
                Statement statement;
                statement.mOffset = static_cast<std::uint32_t>(start);
                std::uint16_t code = u16(start);
                std::size_t position = start + 2;
                std::uint16_t reference = 0;
                if (code == Statement_ReferenceCall)
                {
                    // 1C <reference> <code> <length>
                    if (mCode.size() - start < 8)
                    {
                        fail(Error::Truncated, start);
                        return mCode.size();
                    }
                    reference = u16(position);
                    if (!referenceInRange(reference))
                    {
                        fail(Error::BadReference, start, reference);
                        return mCode.size();
                    }
                    code = u16(position + 2);
                    position += 4;
                    if (code < firstCommand)
                    {
                        fail(Error::UnknownStatement, start, code);
                        return mCode.size();
                    }
                }
                const std::size_t length = u16(position);
                position += 2;
                const std::size_t end = position + length;
                if (end > mCode.size())
                {
                    fail(Error::Truncated, start);
                    return mCode.size();
                }
                statement.mCode = code;
                statement.mDataOffset = static_cast<std::uint32_t>(position);
                statement.mEnd = static_cast<std::uint32_t>(end);

                bool ok = true;
                switch (code)
                {
                    case Statement_ScriptName:
                        statement.mKind = Statement::Kind::ScriptName;
                        break;
                    case Statement_Begin:
                        statement.mKind = Statement::Kind::Begin;
                        if (length < 6)
                        {
                            ok = fail(Error::BadLength, start);
                            break;
                        }
                        statement.mBlockType = u16(position);
                        statement.mBlockLength = u32(position + 2);
                        statement.mArgumentsOffset = static_cast<std::uint32_t>(position + 6);
                        break;
                    case Statement_End:
                        statement.mKind = Statement::Kind::End;
                        break;
                    case Statement_Short:
                    case Statement_Long:
                    case Statement_Float:
                    case Statement_Ref:
                        statement.mKind = Statement::Kind::Declaration;
                        break;
                    case Statement_Return:
                        statement.mKind = Statement::Kind::Return;
                        break;
                    case Statement_EndIf:
                        statement.mKind = Statement::Kind::EndIf;
                        break;
                    case Statement_Else:
                        statement.mKind = Statement::Kind::Else;
                        if (length != 2)
                        {
                            ok = fail(Error::BadLength, start);
                            break;
                        }
                        statement.mJump = u16(position);
                        break;
                    case Statement_If:
                    case Statement_ElseIf:
                    {
                        statement.mKind = code == Statement_If ? Statement::Kind::If : Statement::Kind::ElseIf;
                        if (length < 4)
                        {
                            ok = fail(Error::BadLength, start);
                            break;
                        }
                        statement.mJump = u16(position);
                        const std::size_t expressionLength = u16(position + 2);
                        if (expressionLength != length - 4)
                        {
                            ok = fail(Error::BadLength, start);
                            break;
                        }
                        ok = expression(position + 4, end, statement.mFirstToken, statement.mTokenCount);
                        break;
                    }
                    case Statement_SetTo:
                    {
                        statement.mKind = Statement::Kind::SetTo;
                        std::size_t at = position;
                        if (at < end && mCode[at] == 'G')
                        {
                            if (end - at < 3)
                            {
                                ok = fail(Error::Truncated, start);
                                break;
                            }
                            statement.mGlobal = u16(at + 1);
                            if (!referenceInRange(statement.mGlobal))
                            {
                                ok = fail(Error::BadReference, start, statement.mGlobal);
                                break;
                            }
                            at += 3;
                        }
                        else if (!variable(at, end, statement.mTarget))
                        {
                            ok = false;
                            break;
                        }
                        if (end - at < 2)
                        {
                            ok = fail(Error::Truncated, start);
                            break;
                        }
                        const std::size_t expressionLength = u16(at);
                        at += 2;
                        if (expressionLength != end - at)
                        {
                            ok = fail(Error::BadLength, start);
                            break;
                        }
                        ok = expression(at, end, statement.mFirstToken, statement.mTokenCount);
                        break;
                    }
                    default:
                        if (code < firstCommand)
                        {
                            ok = fail(Error::UnknownStatement, start, code);
                            break;
                        }
                        statement.mKind = Statement::Kind::Call;
                        statement.mCall = static_cast<std::uint32_t>(mProgram.mCalls.size());
                        {
                            Call call;
                            call.mReference = reference;
                            call.mOpcode = code;
                            call.mOffset = static_cast<std::uint32_t>(position);
                            call.mLength = static_cast<std::uint16_t>(length);
                            mProgram.mCalls.push_back(call);
                        }
                        break;
                }

                if (!ok)
                    return mCode.size();
                mProgram.mStatements.push_back(std::move(statement));
                return end;
            }
        };
    }

    std::string_view operatorText(Operator op)
    {
        for (const OperatorSpelling& spelling : operatorSpellings)
            if (spelling.mOperator == op)
                return spelling.mText;
        return {};
    }

    std::string_view errorText(Error error)
    {
        switch (error)
        {
            case Error::None:
                return "none";
            case Error::Truncated:
                return "a statement or expression runs past its end";
            case Error::UnknownStatement:
                return "a code that is no statement or command";
            case Error::BadReference:
                return "a reference index outside the references of the script";
            case Error::BadVariable:
                return "a variable index outside the variables of the script, or of no known kind";
            case Error::BadExpression:
                return "a byte in an expression that is no token";
            case Error::BadOperands:
                return "an expression whose operators and operands do not fit";
            case Error::BadLength:
                return "a statement whose data does not match its parts";
        }
        return "unknown";
    }

    Program decode(const std::vector<std::uint8_t>& code, const Limits& limits)
    {
        Program program;
        Decoder(code, limits, program).run();
        return program;
    }
}
