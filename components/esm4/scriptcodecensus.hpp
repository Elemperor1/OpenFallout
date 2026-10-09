#ifndef OPENFALLOUT_COMPONENTS_ESM4_SCRIPTCODECENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_SCRIPTCODECENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "script.hpp"
#include "scriptargs.hpp"
#include "scriptcode.hpp"

namespace ESM4
{
    class Reader;

    // Decodes the compiled script (SCDA) of every record of the Fallout 3 and New Vegas plugins that holds one (SCPT,
    // INFO, QUST, TERM, PACK and PERK records) and counts what it finds: how many scripts decode and why the others do
    // not, the statements, the types of block, the commands (by their code), the tokens of expressions, and how the
    // jumps that statements write down relate to the statements they lead to. It is for checking the decoder against
    // the real scripts and for choosing which commands to implement first, so it keeps codes and counts, never the
    // contents of a script (no names, strings, numbers or source text). The one exception is the first few scripts
    // that fail to decode: for each it names the record and keeps a few bytes around the place where the decoder
    // stopped, which is what it takes to see why.
    //
    // With a table of the commands (setCommandLookup) it also decodes the arguments of every call by the parameters of
    // the command, and counts the calls whose arguments do not decode, per command and per kind of problem, with the
    // bytes of the first few calls of each.
    //
    // The files must be read in load order, with the mod index and the indices of the masters set on the reader.
    class ScriptCodeCensus
    {
    public:
        // The rules for what a jump counts from and to, as compared with the statement the jump leads to
        enum JumpFrom
        {
            FromStatementStart,
            FromDataStart,
            FromAfterJumpField,
            FromStatementEnd,
            FromExpression, // the expression of an If or ElseIf: after the jump and the length of the expression
            JumpFromCount
        };
        enum JumpTo
        {
            ToStatementStart,
            ToStatementEnd,
            JumpToCount
        };
        struct JumpTally
        {
            std::size_t mTotal = 0;
            std::size_t mUnresolved = 0; // no statement of the kind to jump to was found
            std::array<std::array<std::size_t, JumpToCount>, JumpFromCount> mMatches{};
            // For each rule, how many bytes short of the target the jump ends (0 when the rule matches), and how many
            // jumps are that far off. Only the first maxDifferences different values are kept apart.
            std::array<std::array<std::map<std::int64_t, std::size_t>, JumpToCount>, JumpFromCount> mDifferences{};
        };

        struct CommandUse
        {
            std::size_t mStatements = 0; // called as a statement
            std::size_t mExpressions = 0; // called in an expression
            std::size_t mOnReference = 0; // called on a reference (ref.Command)
            std::size_t mScripts = 0; // scripts that call it
            std::size_t mTotalArgumentBytes = 0;
            std::uint16_t mMaxArgumentBytes = 0;
        };

        struct HolderTotals
        {
            std::size_t mScripts = 0; // the sub-record groups that make a script
            std::size_t mEmpty = 0; // with no bytecode
            std::size_t mDecoded = 0;
            std::size_t mFailed = 0;
            std::size_t mBytes = 0;
            std::size_t mStatements = 0;
            std::map<std::uint16_t, std::size_t> mTypes; // by the type in SCHR
        };

        // A script that did not decode, for finding out why
        struct Failure
        {
            std::string mHolder;
            ESM::FormId mRecord;
            std::size_t mSize = 0;
            std::uint32_t mOffset = 0;
            std::uint16_t mValue = 0;
            std::string mBytes; // hexadecimal, from a few bytes before the offset
            std::string mStatements; // the kinds of the statements around the offset, [ marks the one at it
        };
        static constexpr std::size_t maxFailuresPerError = 5;
        static constexpr std::size_t maxDifferences = 24;
        static constexpr std::size_t maxArgumentExamples = 3; // calls kept for each command and kind of problem
        static constexpr std::size_t maxArgumentExampleKeys = 80; // commands and kinds of problem that keep examples

        // What the census needs to know of a command to check the arguments of its calls
        struct CommandSignature
        {
            std::string_view mName;
            // The parameters, or null when the table has no command of that code
            const std::vector<ScriptCode::Parameter>* mParameters = nullptr;
        };
        using CommandLookup = std::function<CommandSignature(std::uint16_t opcode)>;

        // How the calls of one command fared
        struct ArgumentTally
        {
            std::size_t mCalls = 0;
            std::size_t mDecoded = 0;
            std::size_t mTrailing = 0; // decoded, with bytes left that no parameter takes
            std::uint32_t mMaxTrailing = 0;
            std::size_t mFewer
                = 0; // decoded, with fewer arguments than the command has parameters that are not optional
            std::map<ScriptCode::ArgumentError, std::size_t> mErrors;
        };

        void setCommandLookup(CommandLookup lookup) { mLookup = std::move(lookup); }

        void collect(Reader& reader);

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        const std::map<std::string, HolderTotals>& getHolders() const { return mHolders; }
        const std::map<std::string, std::size_t>& getRecordsFailed() const { return mRecordsFailed; }
        const std::map<ScriptCode::Error, std::size_t>& getErrors() const { return mErrors; }
        const std::map<ScriptCode::Error, std::vector<Failure>>& getFailures() const { return mFailures; }
        const std::map<std::pair<std::uint16_t, std::uint16_t>, std::size_t>& getBlocks() const { return mBlocks; }
        const std::map<std::uint16_t, CommandUse>& getCommands() const { return mCommands; }
        const std::map<std::string, std::size_t>& getStructure() const { return mStructure; }
        std::size_t getStructureScripts() const { return mStructureScripts; }
        const std::map<std::string, std::vector<Failure>>& getStructureExamples() const { return mStructureExamples; }
        const std::map<std::uint16_t, ArgumentTally>& getArguments() const { return mArguments; }
        const std::map<std::uint16_t, std::size_t>& getUnknownCommands() const { return mUnknownCommands; }
        const std::map<std::pair<std::uint16_t, ScriptCode::ArgumentError>, std::vector<Failure>>&
        getArgumentExamples() const
        {
            return mArgumentExamples;
        }
        const JumpTally& getBeginJumps() const { return mBeginJumps; }
        const JumpTally& getIfJumps() const { return mIfJumps; }
        const JumpTally& getElseIfJumps() const { return mElseIfJumps; }
        const JumpTally& getElseJumps() const { return mElseJumps; }

        void write(std::ostream& stream) const;

    private:
        void addScript(const std::string& holder, ESM::FormId record, const ScriptDefinition& script);
        void checkStructure(const ScriptCode::Program& program, const std::string& holder, ESM::FormId record,
            const ScriptDefinition& script);
        void checkArguments(const ScriptCode::Call& call, const std::string& holder, ESM::FormId record,
            const ScriptDefinition& script, const ScriptCode::Limits& limits);
        void writeArguments(std::ostream& stream) const;

        std::map<std::string, HolderTotals> mHolders;
        std::map<std::string, std::size_t> mRecordsFailed;
        std::map<ScriptCode::Error, std::size_t> mErrors;
        std::map<std::pair<ScriptCode::Error, std::uint16_t>, std::size_t> mErrorValues;
        std::map<ScriptCode::Error, std::vector<Failure>> mFailures;
        std::map<std::uint16_t, std::size_t> mStatementCodes;
        // Blocks by the type in SCHR (object, quest, magic effect) and the number of the block type
        std::map<std::pair<std::uint16_t, std::uint16_t>, std::size_t> mBlocks;
        std::map<std::uint16_t, std::size_t> mBlockArguments; // blocks with bytes after the block length, by type
        std::map<std::uint16_t, CommandUse> mCommands;
        std::map<std::string, std::size_t> mTokens;
        std::map<std::string, std::size_t> mOperators;
        std::map<std::string, std::size_t> mStructure; // problems with the nesting of blocks and conditions
        std::size_t mStructureScripts = 0; // the scripts that have one or more of them
        std::map<std::string, std::vector<Failure>> mStructureExamples; // the first scripts of each problem
        CommandLookup mLookup;
        std::map<std::uint16_t, ArgumentTally> mArguments;
        std::map<std::uint16_t, std::size_t> mUnknownCommands; // calls of commands the table does not have, by code
        std::map<std::pair<std::uint16_t, ScriptCode::ArgumentError>, std::vector<Failure>> mArgumentExamples;
        JumpTally mBeginJumps;
        JumpTally mIfJumps;
        JumpTally mElseIfJumps;
        JumpTally mElseJumps;
        std::size_t mMaxBytes = 0;
        std::size_t mMaxStatements = 0;
        std::size_t mMaxTokens = 0;
        std::vector<std::string> mFatalErrors;
    };
}

#endif
