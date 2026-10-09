#ifndef OPENFALLOUT_COMPONENTS_ESM4_SCRIPTCODE_H
#define OPENFALLOUT_COMPONENTS_ESM4_SCRIPTCODE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ESM4::ScriptCode
{
    // The compiled form of a script of Oblivion, Fallout 3 and New Vegas (the SCDA sub-record): a list of statements,
    // each with a 16 bit code and a 16 bit length of the data after it, so a reader can step over a statement it does
    // not understand. This decodes the structure that does not depend on which command a statement calls: blocks,
    // conditions, assignments and the expressions of the last two, which are written in reverse Polish order. A call
    // of a command is kept as the command's code and the bytes of its arguments, because how the arguments are written
    // depends on the parameters the command takes, which is a table that the commands of the game define.
    //
    // The layout is taken from the code that other tools use to read these scripts back (the decompiler in xNVSE and
    // the notes of the UESP wiki), and checked against the real scripts of the games by `esmtool scriptcode`.

    // The first code of a command. The codes below it are the statements of the language itself, and the index of a
    // function in a condition (CTDA) is the code of the command minus this.
    constexpr std::uint16_t firstCommand = 0x1000;

    enum StatementCode : std::uint16_t
    {
        Statement_Begin = 0x10, // Begin <block type> <parameters>
        Statement_End = 0x11,
        Statement_Short = 0x12, // declarations: the compiler does not write these, the variables are in SLSD
        Statement_Long = 0x13,
        Statement_Float = 0x14,
        Statement_SetTo = 0x15,
        Statement_If = 0x16,
        Statement_Else = 0x17,
        Statement_ElseIf = 0x18,
        Statement_EndIf = 0x19,
        Statement_ReferenceCall
        = 0x1C, // a command called on a reference (ref.Command), the code of the command follows
        Statement_ScriptName = 0x1D,
        Statement_Return = 0x1E,
        Statement_Ref = 0x1F,
    };

    // The operators of an expression, which are written as text.
    enum class Operator : std::uint8_t
    {
        Negate, // written ~, takes one operand
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Equal,
        NotEqual,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        And,
        Or,
    };

    std::string_view operatorText(Operator op);

    // A variable of the script that holds the bytecode, or of another one (a quest or a reference) when remote is not
    // 0.
    struct VariableRef
    {
        // 1-based index into the references of the script (SCRO and SCRV entries together) of the object or quest
        // whose variable it is, or 0 for a variable of this script.
        std::uint16_t mRemote = 0;
        // 's' for an integer (short or long), 'f' for a float or a reference variable. Which a variable is comes from
        // its declaration (SLSD), and the code only says how it is stored.
        char mType = 0;
        std::uint16_t mIndex = 0; // 1-based, as in SLSD
    };

    // The call of a command. The arguments are the bytes [mOffset, mOffset + mLength) of the script.
    struct Call
    {
        std::uint16_t mReference = 0; // 1-based index into the references of the object it is called on, 0 for none
        std::uint16_t mOpcode = 0; // firstCommand or more
        std::uint32_t mOffset = 0;
        std::uint16_t mLength = 0;
    };

    struct Token
    {
        enum class Kind : std::uint8_t
        {
            Number, // mNumber
            Variable, // mVariable
            Global, // mIndex is the 1-based index of the reference that names the global variable
            Reference, // mIndex is the 1-based index of the reference whose value (the object) is wanted
            Call, // mCall is an index into Program::mCalls, a command that returns a number or a reference
            String, // mText
            Operator, // mOperator
        };

        Kind mKind = Kind::Number;
        Operator mOperator = Operator::Add;
        double mNumber = 0;
        VariableRef mVariable;
        std::uint16_t mIndex = 0;
        std::uint32_t mCall = 0;
        std::string mText;
    };

    struct Statement
    {
        enum class Kind : std::uint8_t
        {
            ScriptName,
            Begin,
            End,
            Declaration, // Short, Long, Float or Ref
            SetTo,
            If,
            ElseIf,
            Else,
            EndIf,
            Return,
            Call, // a command, mCall is an index into Program::mCalls
        };

        Kind mKind = Kind::Return;
        std::uint16_t mCode = 0;
        std::uint32_t mOffset = 0; // where the statement starts, the code is the first two bytes
        std::uint32_t mDataOffset = 0; // where the data of the statement starts, after its length
        std::uint32_t mEnd = 0; // the first byte after the statement

        // Begin: the number of the block type (an index of the table of events, 0 for GameMode, 1 for MenuMode, 2 for
        // OnActivate and so on), the length of the block as it is written and where the arguments of the block start
        // (the data after the block length, which holds the parameters of events such as OnActivate)
        std::uint16_t mBlockType = 0;
        std::uint32_t mBlockLength = 0;
        std::uint32_t mArgumentsOffset = 0;

        // If, ElseIf and Else: the number of bytes to skip as it is written
        std::uint16_t mJump = 0;

        // SetTo: the variable that is assigned, which is a global variable when mGlobal is not 0 (the index of the
        // reference that names it)
        VariableRef mTarget;
        std::uint16_t mGlobal = 0;

        // SetTo and If and ElseIf: the tokens of the expression are mTokens[mFirstToken, mFirstToken + mTokenCount)
        std::uint32_t mFirstToken = 0;
        std::uint32_t mTokenCount = 0;

        // Call, and the command a ReferenceCall calls
        std::uint32_t mCall = 0;
    };

    enum class Error : std::uint8_t
    {
        None,
        Truncated, // a statement or an expression runs past the end of the script, or past its own length
        UnknownStatement, // a code that is neither a statement of the language nor a command
        BadReference, // a reference index of 0 or beyond the references the script has
        BadVariable, // a local variable beyond the ones the script declares, or a kind of variable that does not exist
        BadExpression, // a byte that is no token
        BadOperands, // an operator without its operands, or an expression that does not end in one value
        BadLength, // a statement whose data is longer or shorter than its parts
    };

    std::string_view errorText(Error error);

    struct Program
    {
        std::vector<Statement> mStatements;
        std::vector<Token> mTokens;
        std::vector<Call> mCalls;

        // The first thing that could not be decoded. The statements before it are in mStatements, and the one that
        // failed is not.
        Error mError = Error::None;
        std::uint32_t mErrorOffset = 0;
        // The byte that was no token (BadExpression) or the code that is no statement (UnknownStatement)
        std::uint16_t mErrorValue = 0;
    };

    struct Limits
    {
        // How many references (SCRO and SCRV entries) the script has, and the highest index of the local variables
        // (SLSD). A reference or variable index beyond them is an error.
        std::size_t mReferences = 0;
        std::size_t mVariables = 0;
    };

    // Decodes the compiled script. It does not throw: a script that is damaged or uses something this does not know
    // gives the statements before the problem and the error.
    Program decode(const std::vector<std::uint8_t>& code, const Limits& limits);
}

#endif
