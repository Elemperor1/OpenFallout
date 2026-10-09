#ifndef OPENFALLOUT_COMPONENTS_ESM4_SCRIPTARGS_H
#define OPENFALLOUT_COMPONENTS_ESM4_SCRIPTARGS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "scriptcode.hpp"

namespace ESM4::ScriptCode
{
    // The arguments of a call of a command, which `decode` keeps as bytes. How an argument is written depends on the
    // type of the parameter of the command that it fills (the table of commands gives those types), in one of a few
    // ways: a text, a number (a tag and a constant or a variable), a bare 16 bit or 8 bit value, a form (the index of a
    // reference to an object, a quest or any other record) or a variable. The layout is taken from the code other tools
    // use to read the scripts of the games back (the decompiler of xNVSE) and checked against the real scripts by
    // `esmtool scriptcode`.

    // How the bytes of an argument are written, by the type of the parameter
    enum class ParamClass : std::uint8_t
    {
        String, // u16 length, then the characters
        Integer, // a number: 'n' and an i32, 'z' and an f64, 'G' and a global, or a variable
        Short, // a bare u16 (an actor value, an animation group, a sex, an alignment ...)
        Byte, // a bare u8 (an axis, a form type)
        Float, // a number, as for an integer
        Double, // a number, as for an integer
        Form, // 'r' and the u16 index of a reference, or a variable that holds one
        Variable, // a variable
        Other, // written in no way a script can hold (the name of a variable in a condition)
    };

    // The class of a parameter type: the ParamInfo type of the command table, 0 for a string, 1 for an integer, 2 for a
    // float and so on. Types this does not know are forms, which is what most are.
    ParamClass classifyParameter(std::uint32_t type);

    struct Parameter
    {
        std::uint32_t mType = 0;
        bool mOptional = false;
    };

    struct Argument
    {
        enum class Kind : std::uint8_t
        {
            Number, // mNumber: a constant written in the script, or the value of a bare u16 or u8
            String, // mText
            Variable, // mVariable
            Global, // mIndex is the 1-based index of the reference that names the global variable
            Reference, // mIndex is the 1-based index of the reference (the object, quest or form)
        };

        Kind mKind = Kind::Number;
        ParamClass mClass = ParamClass::Integer;
        double mNumber = 0;
        std::string mText;
        VariableRef mVariable;
        std::uint16_t mIndex = 0;
        std::uint32_t mOffset = 0; // where in the script the argument starts
    };

    enum class ArgumentError : std::uint8_t
    {
        None,
        Truncated, // an argument runs past the end of the data of the call
        TooMany, // more arguments than the command has parameters
        BadReference, // a reference index outside the references of the script
        BadVariable, // a variable index outside the variables of the script, or of no known kind
        BadArgument, // a byte where a tag is expected that is no tag, or an argument that a script can not hold
        InlineExpression, // an argument written as an expression, which only the extensions of the language do
    };

    struct Arguments
    {
        std::vector<Argument> mValues;
        // How many arguments the call says it has (the first u16), which can be fewer than the parameters of the
        // command when the last ones are optional
        std::uint16_t mCount = 0;
        // Bytes of the data of the call that the arguments do not use. A few commands (the ones that write a message
        // with values in it) put more arguments after these.
        std::uint32_t mTrailing = 0;
        ArgumentError mError = ArgumentError::None;
        std::uint32_t mErrorOffset = 0;
    };

    // Reads the arguments of `call` as the parameters say. `limits` are those of the script that holds the call.
    Arguments decodeArguments(const std::vector<std::uint8_t>& code, const Call& call,
        const std::vector<Parameter>& parameters, const Limits& limits);

    std::string_view argumentErrorText(ArgumentError error);
}

#endif
