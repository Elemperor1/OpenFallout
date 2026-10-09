#include "scriptargs.hpp"

#include <array>
#include <cstring>

namespace ESM4::ScriptCode
{
    namespace
    {
        // The type of each parameter in the command table: 0 a string, 1 an integer, 2 a float, 5 an actor value ...
        // Anything else is a form. The kinds that are not forms:
        constexpr std::array<ParamClass, 70> parameterClasses = [] {
            std::array<ParamClass, 70> classes{};
            for (ParamClass& value : classes)
                value = ParamClass::Form;
            classes[0x00] = ParamClass::String;
            classes[0x01] = ParamClass::Integer;
            classes[0x02] = ParamClass::Float;
            classes[0x05] = ParamClass::Short; // actor value
            classes[0x08] = ParamClass::Byte; // axis
            classes[0x0A] = ParamClass::Short; // animation group
            classes[0x12] = ParamClass::Short; // sex
            classes[0x16] = ParamClass::Other; // variable name, which only a condition can hold
            classes[0x17] = ParamClass::Integer; // quest stage
            classes[0x1C] = ParamClass::Short; // crime type
            classes[0x20] = ParamClass::Byte; // form type
            classes[0x29] = ParamClass::Short; // miscellaneous statistic
            classes[0x2C] = ParamClass::Double;
            classes[0x2D] = ParamClass::Variable;
            classes[0x2E] = ParamClass::Other;
            classes[0x33] = ParamClass::Short; // alignment
            classes[0x34] = ParamClass::Short; // equip type
            classes[0x37] = ParamClass::Short; // critical stage
            return classes;
        }();

        class ArgumentReader
        {
        public:
            ArgumentReader(
                const std::vector<std::uint8_t>& code, const Call& call, const Limits& limits, Arguments& out)
                : mCode(code)
                , mLimits(limits)
                , mOut(out)
                , mPosition(call.mOffset)
                , mEnd(static_cast<std::size_t>(call.mOffset) + call.mLength)
            {
            }

            void run(const std::vector<Parameter>& parameters)
            {
                // A command without parameters is called with no data at all, not even a count
                if (mEnd == mPosition)
                    return;
                if (mEnd - mPosition < 2)
                {
                    fail(ArgumentError::Truncated);
                    return;
                }
                mOut.mCount = u16(mPosition);
                mPosition += 2;
                if (mOut.mCount > parameters.size())
                {
                    fail(ArgumentError::TooMany);
                    return;
                }
                for (std::size_t i = 0; i < mOut.mCount && mOut.mError == ArgumentError::None; ++i)
                {
                    // An argument that is a whole expression starts with 0xFFFF, whatever the parameter is
                    if (mEnd - mPosition >= 2 && u16(mPosition) == 0xFFFF)
                    {
                        fail(ArgumentError::InlineExpression);
                        return;
                    }
                    argument(classifyParameter(parameters[i].mType));
                }
                if (mOut.mError == ArgumentError::None)
                    mOut.mTrailing = static_cast<std::uint32_t>(mEnd - mPosition);
            }

        private:
            const std::vector<std::uint8_t>& mCode;
            const Limits& mLimits;
            Arguments& mOut;
            std::size_t mPosition;
            std::size_t mEnd;

            std::uint16_t u16(std::size_t offset) const
            {
                return static_cast<std::uint16_t>(mCode[offset] | (mCode[offset + 1] << 8));
            }

            std::size_t left() const { return mEnd - mPosition; }

            bool fail(ArgumentError error, std::size_t offset)
            {
                if (mOut.mError == ArgumentError::None)
                {
                    mOut.mError = error;
                    mOut.mErrorOffset = static_cast<std::uint32_t>(offset);
                }
                return false;
            }

            bool fail(ArgumentError error) { return fail(error, mPosition); }

            bool referenceInRange(std::uint16_t index) const { return index >= 1 && index <= mLimits.mReferences; }

            void argument(ParamClass kind)
            {
                Argument argument;
                argument.mClass = kind;
                argument.mOffset = static_cast<std::uint32_t>(mPosition);
                bool ok = false;
                switch (kind)
                {
                    case ParamClass::String:
                        ok = text(argument);
                        break;
                    case ParamClass::Integer:
                    case ParamClass::Float:
                    case ParamClass::Double:
                        ok = number(argument);
                        break;
                    case ParamClass::Short:
                        ok = bare(argument, 2);
                        break;
                    case ParamClass::Byte:
                        ok = bare(argument, 1);
                        break;
                    case ParamClass::Form:
                        ok = form(argument);
                        break;
                    case ParamClass::Variable:
                        ok = variable(argument);
                        break;
                    case ParamClass::Other:
                        fail(ArgumentError::BadArgument);
                        break;
                }
                if (ok)
                    mOut.mValues.push_back(std::move(argument));
            }

            // A value written without a tag, in one or two bytes
            bool bare(Argument& argument, std::size_t size)
            {
                if (left() < size)
                    return fail(ArgumentError::Truncated);
                argument.mKind = Argument::Kind::Number;
                argument.mNumber = size == 1 ? mCode[mPosition] : u16(mPosition);
                mPosition += size;
                return true;
            }

            bool text(Argument& argument)
            {
                if (left() < 2)
                    return fail(ArgumentError::Truncated);
                const std::size_t length = u16(mPosition);
                if (left() - 2 < length)
                    return fail(ArgumentError::Truncated);
                argument.mKind = Argument::Kind::String;
                argument.mText.assign(reinterpret_cast<const char*>(mCode.data() + mPosition + 2), length);
                mPosition += 2 + length;
                return true;
            }

            // 'n' and a 32 bit integer, 'z' and a 64 bit float, 'G' and a global, or a variable
            bool number(Argument& argument)
            {
                if (left() < 1)
                    return fail(ArgumentError::Truncated);
                switch (mCode[mPosition])
                {
                    case 'n':
                    {
                        if (left() < 5)
                            return fail(ArgumentError::Truncated);
                        const std::uint32_t value
                            = u16(mPosition + 1) | (static_cast<std::uint32_t>(u16(mPosition + 3)) << 16);
                        argument.mKind = Argument::Kind::Number;
                        argument.mNumber = static_cast<std::int32_t>(value);
                        mPosition += 5;
                        return true;
                    }
                    case 'z':
                    {
                        if (left() < 9)
                            return fail(ArgumentError::Truncated);
                        double value;
                        std::memcpy(&value, mCode.data() + mPosition + 1, sizeof(value));
                        argument.mKind = Argument::Kind::Number;
                        argument.mNumber = value;
                        mPosition += 9;
                        return true;
                    }
                    case 'G':
                    {
                        if (left() < 3)
                            return fail(ArgumentError::Truncated);
                        argument.mKind = Argument::Kind::Global;
                        argument.mIndex = u16(mPosition + 1);
                        if (!referenceInRange(argument.mIndex))
                            return fail(ArgumentError::BadReference);
                        mPosition += 3;
                        return true;
                    }
                    default:
                        return variable(argument);
                }
            }

            // 'r' and a reference, then the variable of the script it names, or the variable of this script
            bool variable(Argument& argument)
            {
                const std::size_t start = mPosition;
                VariableRef ref;
                if (left() >= 1 && mCode[mPosition] == 'r')
                {
                    if (left() < 3)
                        return fail(ArgumentError::Truncated);
                    ref.mRemote = u16(mPosition + 1);
                    if (!referenceInRange(ref.mRemote))
                        return fail(ArgumentError::BadReference);
                    mPosition += 3;
                }
                if (left() < 3)
                    return fail(ArgumentError::Truncated, start);
                const std::uint8_t type = mCode[mPosition];
                if (type != 's' && type != 'f')
                    return fail(ArgumentError::BadVariable);
                ref.mType = static_cast<char>(type);
                ref.mIndex = u16(mPosition + 1);
                // A variable of another script is not checked against the variables of this one, but an index counts
                // from 1 in any script
                if (ref.mIndex < 1 || (ref.mRemote == 0 && !mLimits.variableDeclared(ref.mIndex)))
                    return fail(ArgumentError::BadVariable);
                mPosition += 3;
                argument.mKind = Argument::Kind::Variable;
                argument.mVariable = ref;
                return true;
            }

            // 'r' and a reference, or the local variable that holds one, which is written with an 'f' (the games read
            // a form from those two and nothing else; an integer variable, 's', is no form)
            bool form(Argument& argument)
            {
                if (left() < 1)
                    return fail(ArgumentError::Truncated);
                if (mCode[mPosition] == 'r')
                {
                    if (left() < 3)
                        return fail(ArgumentError::Truncated);
                    argument.mKind = Argument::Kind::Reference;
                    argument.mIndex = u16(mPosition + 1);
                    if (!referenceInRange(argument.mIndex))
                        return fail(ArgumentError::BadReference);
                    mPosition += 3;
                    return true;
                }
                if (mCode[mPosition] != 'f')
                    return fail(ArgumentError::BadArgument);
                return variable(argument);
            }
        };
    }

    ParamClass classifyParameter(std::uint32_t type)
    {
        return type < parameterClasses.size() ? parameterClasses[type] : ParamClass::Form;
    }

    Arguments decodeArguments(const std::vector<std::uint8_t>& code, const Call& call,
        const std::vector<Parameter>& parameters, const Limits& limits)
    {
        Arguments result;
        if (static_cast<std::size_t>(call.mOffset) + call.mLength > code.size())
        {
            result.mError = ArgumentError::Truncated;
            result.mErrorOffset = call.mOffset;
            return result;
        }
        ArgumentReader(code, call, limits, result).run(parameters);
        return result;
    }

    std::string_view argumentErrorText(ArgumentError error)
    {
        switch (error)
        {
            case ArgumentError::None:
                return "none";
            case ArgumentError::Truncated:
                return "an argument runs past the data of the call";
            case ArgumentError::TooMany:
                return "more arguments than the command has parameters";
            case ArgumentError::BadReference:
                return "a reference index outside the references of the script";
            case ArgumentError::BadVariable:
                return "a variable index outside the variables of the script, or of no known kind";
            case ArgumentError::BadArgument:
                return "an argument that starts with a byte that is no tag, or that a script can not hold";
            case ArgumentError::InlineExpression:
                return "an argument written as an expression";
        }
        return "unknown";
    }
}
