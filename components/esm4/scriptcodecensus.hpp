#ifndef OPENFALLOUT_COMPONENTS_ESM4_SCRIPTCODECENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_SCRIPTCODECENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "script.hpp"
#include "scriptcode.hpp"

namespace ESM4
{
    class Reader;

    // Decodes the compiled script (SCDA) of every record of the Fallout 3 and New Vegas plugins that holds one (SCPT,
    // INFO, QUST, TERM, PACK and PERK records) and counts what it finds: how many scripts decode and why the others do
    // not, the statements, the types of block, the commands (by their code), the tokens of expressions, and how the
    // jumps that statements write down relate to the statements they lead to. It is for checking the decoder against
    // the real scripts and for choosing which commands to implement first, so it keeps codes and counts, never the
    // contents of a script (no names, strings, numbers or source text).
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

        void collect(Reader& reader);

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        const std::map<std::string, HolderTotals>& getHolders() const { return mHolders; }
        const std::map<std::string, std::size_t>& getRecordsFailed() const { return mRecordsFailed; }
        const std::map<ScriptCode::Error, std::size_t>& getErrors() const { return mErrors; }
        const std::map<std::pair<std::uint16_t, std::uint16_t>, std::size_t>& getBlocks() const { return mBlocks; }
        const std::map<std::uint16_t, CommandUse>& getCommands() const { return mCommands; }
        const std::map<std::string, std::size_t>& getStructure() const { return mStructure; }
        const JumpTally& getBeginJumps() const { return mBeginJumps; }
        const JumpTally& getIfJumps() const { return mIfJumps; }
        const JumpTally& getElseIfJumps() const { return mElseIfJumps; }
        const JumpTally& getElseJumps() const { return mElseJumps; }

        void write(std::ostream& stream) const;

    private:
        void addScript(const std::string& holder, const ScriptDefinition& script);
        void checkStructure(const ScriptCode::Program& program);

        std::map<std::string, HolderTotals> mHolders;
        std::map<std::string, std::size_t> mRecordsFailed;
        std::map<ScriptCode::Error, std::size_t> mErrors;
        std::map<std::pair<ScriptCode::Error, std::uint16_t>, std::size_t> mErrorValues;
        std::map<std::uint16_t, std::size_t> mStatementCodes;
        // Blocks by the type in SCHR (object, quest, magic effect) and the number of the block type
        std::map<std::pair<std::uint16_t, std::uint16_t>, std::size_t> mBlocks;
        std::map<std::uint16_t, std::size_t> mBlockArguments; // blocks with bytes after the block length, by type
        std::map<std::uint16_t, CommandUse> mCommands;
        std::map<std::string, std::size_t> mTokens;
        std::map<std::string, std::size_t> mOperators;
        std::map<std::string, std::size_t> mStructure; // problems with the nesting of blocks and conditions
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
