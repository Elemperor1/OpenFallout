#include <components/esm4/scriptargs.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4::ScriptCode;

    enum : std::uint32_t
    {
        TypeString = 0x00,
        TypeInteger = 0x01,
        TypeFloat = 0x02,
        TypeObjectRef = 0x04,
        TypeActorValue = 0x05,
        TypeAxis = 0x08,
        TypeQuest = 0x0E,
        TypeVariableName = 0x16,
        TypeQuestStage = 0x17,
        TypeDouble = 0x2C,
        TypeScriptVariable = 0x2D,
    };

    class Bytes
    {
    public:
        Bytes& u8(std::uint8_t value)
        {
            mData.push_back(value);
            return *this;
        }

        Bytes& u16(std::uint16_t value)
        {
            mData.push_back(static_cast<std::uint8_t>(value & 0xFF));
            mData.push_back(static_cast<std::uint8_t>(value >> 8));
            return *this;
        }

        Bytes& i32(std::int32_t value)
        {
            const auto bits = static_cast<std::uint32_t>(value);
            u16(static_cast<std::uint16_t>(bits & 0xFFFF));
            return u16(static_cast<std::uint16_t>(bits >> 16));
        }

        Bytes& f64(double value)
        {
            std::uint8_t raw[sizeof(value)];
            std::memcpy(raw, &value, sizeof(value));
            mData.insert(mData.end(), raw, raw + sizeof(value));
            return *this;
        }

        Bytes& text(const std::string& value)
        {
            u16(static_cast<std::uint16_t>(value.size()));
            mData.insert(mData.end(), value.begin(), value.end());
            return *this;
        }

        // 'n' <i32>
        Bytes& integer(std::int32_t value) { return u8('n').i32(value); }
        // 'z' <f64>
        Bytes& real(double value) { return u8('z').f64(value); }
        // 'G' <reference>
        Bytes& global(std::uint16_t reference) { return u8('G').u16(reference); }
        // 's' or 'f' <index>
        Bytes& variable(char type, std::uint16_t index) { return u8(static_cast<std::uint8_t>(type)).u16(index); }
        // 'r' <reference>
        Bytes& reference(std::uint16_t reference) { return u8('r').u16(reference); }

        const std::vector<std::uint8_t>& data() const { return mData; }

    private:
        std::vector<std::uint8_t> mData;
    };

    // The data of a call: the number of arguments, then the arguments
    Bytes call(std::uint16_t count)
    {
        Bytes bytes;
        bytes.u16(count);
        return bytes;
    }

    Arguments decode(const Bytes& bytes, const std::vector<Parameter>& parameters, std::size_t references = 4,
        std::size_t variables = 3)
    {
        Call command;
        command.mOpcode = firstCommand;
        command.mOffset = 0;
        command.mLength = static_cast<std::uint16_t>(bytes.data().size());
        Limits limits;
        limits.mReferences = references;
        limits.mVariables = variables;
        return decodeArguments(bytes.data(), command, parameters, limits);
    }

    TEST(ESM4ScriptArgsTest, classifiesTheParameterTypes)
    {
        EXPECT_EQ(classifyParameter(TypeString), ParamClass::String);
        EXPECT_EQ(classifyParameter(TypeInteger), ParamClass::Integer);
        EXPECT_EQ(classifyParameter(TypeFloat), ParamClass::Float);
        EXPECT_EQ(classifyParameter(TypeDouble), ParamClass::Double);
        EXPECT_EQ(classifyParameter(TypeActorValue), ParamClass::Short);
        EXPECT_EQ(classifyParameter(TypeAxis), ParamClass::Byte);
        EXPECT_EQ(classifyParameter(TypeQuestStage), ParamClass::Integer);
        EXPECT_EQ(classifyParameter(TypeScriptVariable), ParamClass::Variable);
        EXPECT_EQ(classifyParameter(TypeVariableName), ParamClass::Other);
        EXPECT_EQ(classifyParameter(TypeObjectRef), ParamClass::Form);
        EXPECT_EQ(classifyParameter(TypeQuest), ParamClass::Form);
        // a type that no table lists is a form
        EXPECT_EQ(classifyParameter(0x45), ParamClass::Form);
        EXPECT_EQ(classifyParameter(0x1000), ParamClass::Form);
    }

    TEST(ESM4ScriptArgsTest, aCallWithNoArgumentsHasTheCountOnly)
    {
        const Arguments arguments = decode(call(0), {});
        EXPECT_EQ(arguments.mError, ArgumentError::None);
        EXPECT_EQ(arguments.mCount, 0);
        EXPECT_THAT(arguments.mValues, IsEmpty());
        EXPECT_EQ(arguments.mTrailing, 0u);
    }

    TEST(ESM4ScriptArgsTest, readsAForm)
    {
        Bytes bytes = call(1);
        bytes.reference(3);
        const Arguments arguments = decode(bytes, { { TypeQuest, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        ASSERT_EQ(arguments.mValues.size(), 1u);
        EXPECT_EQ(arguments.mValues[0].mKind, Argument::Kind::Reference);
        EXPECT_EQ(arguments.mValues[0].mIndex, 3);
        EXPECT_EQ(arguments.mValues[0].mOffset, 2u);
    }

    TEST(ESM4ScriptArgsTest, aFormCanBeHeldByAVariable)
    {
        Bytes bytes = call(1);
        bytes.variable('f', 2);
        const Arguments arguments = decode(bytes, { { TypeObjectRef, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        ASSERT_EQ(arguments.mValues.size(), 1u);
        EXPECT_EQ(arguments.mValues[0].mKind, Argument::Kind::Variable);
        EXPECT_EQ(arguments.mValues[0].mVariable.mType, 'f');
        EXPECT_EQ(arguments.mValues[0].mVariable.mIndex, 2);
        EXPECT_EQ(arguments.mValues[0].mVariable.mRemote, 0);
    }

    TEST(ESM4ScriptArgsTest, readsAStageAndAFormLikeSetStage)
    {
        Bytes bytes = call(2);
        bytes.reference(1).integer(30);
        const Arguments arguments = decode(bytes, { { TypeQuest, false }, { TypeQuestStage, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        ASSERT_EQ(arguments.mValues.size(), 2u);
        EXPECT_EQ(arguments.mValues[1].mKind, Argument::Kind::Number);
        EXPECT_EQ(arguments.mValues[1].mNumber, 30);
        EXPECT_EQ(arguments.mValues[1].mClass, ParamClass::Integer);
        EXPECT_EQ(arguments.mValues[1].mOffset, 5u);
    }

    TEST(ESM4ScriptArgsTest, readsNegativeIntegersAndRealNumbers)
    {
        Bytes bytes = call(2);
        bytes.integer(-7).real(2.5);
        const Arguments arguments = decode(bytes, { { TypeInteger, false }, { TypeFloat, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        EXPECT_EQ(arguments.mValues[0].mNumber, -7);
        EXPECT_EQ(arguments.mValues[1].mNumber, 2.5);
    }

    TEST(ESM4ScriptArgsTest, aNumberCanBeAGlobalOrAVariable)
    {
        Bytes bytes = call(3);
        bytes.global(2).variable('s', 3).reference(4).variable('f', 9);
        const Arguments arguments
            = decode(bytes, { { TypeInteger, false }, { TypeInteger, false }, { TypeFloat, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        ASSERT_EQ(arguments.mValues.size(), 3u);
        EXPECT_EQ(arguments.mValues[0].mKind, Argument::Kind::Global);
        EXPECT_EQ(arguments.mValues[0].mIndex, 2);
        EXPECT_EQ(arguments.mValues[1].mKind, Argument::Kind::Variable);
        EXPECT_EQ(arguments.mValues[1].mVariable.mType, 's');
        EXPECT_EQ(arguments.mValues[1].mVariable.mIndex, 3);
        // the variable of another script: the reference that names it comes first
        EXPECT_EQ(arguments.mValues[2].mKind, Argument::Kind::Variable);
        EXPECT_EQ(arguments.mValues[2].mVariable.mRemote, 4);
        EXPECT_EQ(arguments.mValues[2].mVariable.mType, 'f');
        EXPECT_EQ(arguments.mValues[2].mVariable.mIndex, 9);
    }

    TEST(ESM4ScriptArgsTest, readsStringsAndBareValues)
    {
        Bytes bytes = call(3);
        bytes.text("Idle").u16(0x1234).u8(2);
        const Arguments arguments
            = decode(bytes, { { TypeString, false }, { TypeActorValue, false }, { TypeAxis, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        ASSERT_EQ(arguments.mValues.size(), 3u);
        EXPECT_EQ(arguments.mValues[0].mKind, Argument::Kind::String);
        EXPECT_EQ(arguments.mValues[0].mText, "Idle");
        EXPECT_EQ(arguments.mValues[1].mNumber, 0x1234);
        EXPECT_EQ(arguments.mValues[2].mNumber, 2);
    }

    TEST(ESM4ScriptArgsTest, anEmptyStringIsAnArgument)
    {
        Bytes bytes = call(1);
        bytes.text("");
        const Arguments arguments = decode(bytes, { { TypeString, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        ASSERT_EQ(arguments.mValues.size(), 1u);
        EXPECT_EQ(arguments.mValues[0].mText, "");
    }

    TEST(ESM4ScriptArgsTest, readsAVariableParameter)
    {
        Bytes bytes = call(1);
        bytes.variable('s', 1);
        const Arguments arguments = decode(bytes, { { TypeScriptVariable, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        EXPECT_EQ(arguments.mValues[0].mKind, Argument::Kind::Variable);
        EXPECT_EQ(arguments.mValues[0].mClass, ParamClass::Variable);
    }

    TEST(ESM4ScriptArgsTest, theLastArgumentsMayBeLeftOutWhenTheyAreOptional)
    {
        Bytes bytes = call(1);
        bytes.reference(1);
        const Arguments arguments = decode(bytes, { { TypeQuest, false }, { TypeInteger, true } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        EXPECT_EQ(arguments.mCount, 1);
        EXPECT_EQ(arguments.mValues.size(), 1u);
    }

    TEST(ESM4ScriptArgsTest, reportsTheBytesThatTheArgumentsDoNotUse)
    {
        Bytes bytes = call(1);
        bytes.reference(1).u16(2).u8(0x41);
        const Arguments arguments = decode(bytes, { { TypeQuest, false } });
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        EXPECT_EQ(arguments.mTrailing, 3u);
    }

    TEST(ESM4ScriptArgsTest, rejectsMoreArgumentsThanTheCommandHasParameters)
    {
        Bytes bytes = call(2);
        bytes.reference(1).integer(1);
        const Arguments arguments = decode(bytes, { { TypeQuest, false } });
        EXPECT_EQ(arguments.mError, ArgumentError::TooMany);
    }

    TEST(ESM4ScriptArgsTest, aCallWithNoDataHasNoArguments)
    {
        // How the games write a call of a command that takes no parameters: not even a count
        const Arguments arguments = decode(Bytes(), {});
        EXPECT_EQ(arguments.mError, ArgumentError::None);
        EXPECT_EQ(arguments.mCount, 0);
        EXPECT_THAT(arguments.mValues, IsEmpty());
        EXPECT_EQ(arguments.mTrailing, 0u);
        EXPECT_EQ(decode(Bytes(), { { TypeInteger, true } }).mError, ArgumentError::None);
    }

    TEST(ESM4ScriptArgsTest, rejectsADataSectionThatIsTooShortForACount)
    {
        EXPECT_EQ(decode(Bytes().u8(0), {}).mError, ArgumentError::Truncated);
    }

    TEST(ESM4ScriptArgsTest, rejectsAnArgumentThatRunsPastTheData)
    {
        const std::vector<Parameter> integer{ { TypeInteger, false } };
        EXPECT_EQ(decode(call(1), integer).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1).u8('n').u16(1), integer).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1).u8('z').u16(1), integer).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1).u8('G').u8(1), integer).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1).u8('s').u8(1), integer).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1).u8('r').u16(1), integer).mError, ArgumentError::Truncated);

        EXPECT_EQ(decode(call(1).u16(5).u8('a'), { { TypeString, false } }).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1).u8(1), { { TypeActorValue, false } }).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1), { { TypeAxis, false } }).mError, ArgumentError::Truncated);
        EXPECT_EQ(decode(call(1).u8('r').u8(1), { { TypeQuest, false } }).mError, ArgumentError::Truncated);
    }

    TEST(ESM4ScriptArgsTest, rejectsReferencesAndVariablesTheScriptDoesNotHave)
    {
        EXPECT_EQ(decode(call(1).reference(0), { { TypeQuest, false } }).mError, ArgumentError::BadReference);
        EXPECT_EQ(decode(call(1).reference(5), { { TypeQuest, false } }).mError, ArgumentError::BadReference);
        EXPECT_EQ(decode(call(1).global(5), { { TypeInteger, false } }).mError, ArgumentError::BadReference);
        EXPECT_EQ(decode(call(1).reference(5).variable('s', 1), { { TypeInteger, false } }).mError,
            ArgumentError::BadReference);
        EXPECT_EQ(decode(call(1).variable('s', 4), { { TypeInteger, false } }).mError, ArgumentError::BadVariable);
        EXPECT_EQ(decode(call(1).variable('s', 0), { { TypeInteger, false } }).mError, ArgumentError::BadVariable);
        EXPECT_EQ(decode(call(1).variable('f', 4), { { TypeObjectRef, false } }).mError, ArgumentError::BadVariable);
    }

    TEST(ESM4ScriptArgsTest, aVariableOfAnotherScriptIsNotCheckedAgainstThisOne)
    {
        const Arguments arguments = decode(call(1).reference(2).variable('s', 40), { { TypeInteger, false } });
        EXPECT_EQ(arguments.mError, ArgumentError::None);
        // but no script has a variable 0
        EXPECT_EQ(decode(call(1).reference(2).variable('s', 0), { { TypeInteger, false } }).mError,
            ArgumentError::BadVariable);
    }

    TEST(ESM4ScriptArgsTest, aVariableInAGapOfTheDeclaredOnesIsAnError)
    {
        Call command;
        command.mOpcode = firstCommand;
        command.mOffset = 0;
        Limits limits;
        limits.mReferences = 4;
        limits.mVariables = 3;
        limits.mUndeclared = { 2 };
        const auto errorOf = [&](const Bytes& bytes) {
            command.mLength = static_cast<std::uint16_t>(bytes.data().size());
            return decodeArguments(bytes.data(), command, { { TypeInteger, false } }, limits).mError;
        };

        EXPECT_EQ(errorOf(call(1).variable('s', 3)), ArgumentError::None);
        EXPECT_EQ(errorOf(call(1).variable('s', 2)), ArgumentError::BadVariable);
        EXPECT_EQ(errorOf(call(1).reference(2).variable('s', 2)), ArgumentError::None);
    }

    TEST(ESM4ScriptArgsTest, rejectsAByteThatIsNoTag)
    {
        EXPECT_EQ(decode(call(1).u8('q').u16(1), { { TypeObjectRef, false } }).mError, ArgumentError::BadArgument);
        EXPECT_EQ(decode(call(1).u8('q').u16(1), { { TypeInteger, false } }).mError, ArgumentError::BadVariable);
        EXPECT_EQ(decode(call(1).u16(1), { { TypeVariableName, false } }).mError, ArgumentError::BadArgument);
    }

    TEST(ESM4ScriptArgsTest, anArgumentThatIsAnExpressionIsNotRead)
    {
        const Arguments arguments = decode(call(1).u16(0xFFFF).u16(0), { { TypeInteger, false } });
        EXPECT_EQ(arguments.mError, ArgumentError::InlineExpression);
        EXPECT_EQ(arguments.mErrorOffset, 2u);
    }

    TEST(ESM4ScriptArgsTest, tellsWhereAnErrorIs)
    {
        Bytes bytes = call(2);
        bytes.reference(1).u8('q');
        const Arguments arguments = decode(bytes, { { TypeQuest, false }, { TypeObjectRef, false } });
        EXPECT_EQ(arguments.mError, ArgumentError::BadArgument);
        EXPECT_EQ(arguments.mErrorOffset, 5u);
    }

    TEST(ESM4ScriptArgsTest, readsTheCallWhereItIsInTheScript)
    {
        // some bytes before the call, which the call's offset skips
        Bytes bytes;
        bytes.u16(0x1111).u16(1).reference(2);
        Call command;
        command.mOffset = 2;
        command.mLength = 5;
        Limits limits;
        limits.mReferences = 2;
        const Arguments arguments = decodeArguments(bytes.data(), command, { { TypeQuest, false } }, limits);
        ASSERT_EQ(arguments.mError, ArgumentError::None);
        ASSERT_EQ(arguments.mValues.size(), 1u);
        EXPECT_EQ(arguments.mValues[0].mIndex, 2);
        EXPECT_EQ(arguments.mValues[0].mOffset, 4u);

        // a call whose length runs past the script
        command.mLength = 9;
        EXPECT_EQ(
            decodeArguments(bytes.data(), command, { { TypeQuest, false } }, limits).mError, ArgumentError::Truncated);
    }

    TEST(ESM4ScriptArgsTest, describesEveryError)
    {
        for (const ArgumentError error :
            { ArgumentError::None, ArgumentError::Truncated, ArgumentError::TooMany, ArgumentError::BadReference,
                ArgumentError::BadVariable, ArgumentError::BadArgument, ArgumentError::InlineExpression })
            EXPECT_THAT(std::string(argumentErrorText(error)), Not(IsEmpty()));
    }
}
