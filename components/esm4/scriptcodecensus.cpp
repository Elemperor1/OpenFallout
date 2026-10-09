#include "scriptcodecensus.hpp"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <ostream>
#include <set>
#include <sstream>

#include "censusreading.hpp"
#include "common.hpp"
#include "loadinfo.hpp"
#include "loadpack.hpp"
#include "loadperk.hpp"
#include "loadqust.hpp"
#include "loadscpt.hpp"
#include "loadterm.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    using namespace CensusReading;
    using ScriptCode::Error;
    using ScriptCode::Program;
    using ScriptCode::Statement;
    using ScriptCode::Token;

    namespace
    {
        constexpr std::size_t maxErrorValues = 40;

        std::string hex(std::uint32_t value, int width)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
            return stream.str();
        }

        std::string scriptTypeName(std::uint16_t type)
        {
            switch (type)
            {
                case 0:
                    return "object";
                case 1:
                    return "quest";
                case 0x100:
                    return "magic effect";
            }
            return "type " + hex(type, 4);
        }

        const char* statementName(std::uint16_t code)
        {
            switch (code)
            {
                case ScriptCode::Statement_Begin:
                    return "Begin";
                case ScriptCode::Statement_End:
                    return "End";
                case ScriptCode::Statement_Short:
                    return "Short";
                case ScriptCode::Statement_Long:
                    return "Long";
                case ScriptCode::Statement_Float:
                    return "Float";
                case ScriptCode::Statement_SetTo:
                    return "SetTo";
                case ScriptCode::Statement_If:
                    return "If";
                case ScriptCode::Statement_Else:
                    return "Else";
                case ScriptCode::Statement_ElseIf:
                    return "ElseIf";
                case ScriptCode::Statement_EndIf:
                    return "EndIf";
                case ScriptCode::Statement_ReferenceCall:
                    return "ReferenceCall";
                case ScriptCode::Statement_ScriptName:
                    return "ScriptName";
                case ScriptCode::Statement_Return:
                    return "Return";
                case ScriptCode::Statement_Ref:
                    return "Ref";
            }
            return "command";
        }

        const char* fromName(ScriptCodeCensus::JumpFrom from)
        {
            switch (from)
            {
                case ScriptCodeCensus::FromStatementStart:
                    return "start of the statement";
                case ScriptCodeCensus::FromDataStart:
                    return "start of its data";
                case ScriptCodeCensus::FromAfterJumpField:
                    return "after the jump field";
                case ScriptCodeCensus::FromStatementEnd:
                    return "end of the statement";
                default:
                    return "?";
            }
        }

        const char* toName(ScriptCodeCensus::JumpTo to)
        {
            return to == ScriptCodeCensus::ToStatementStart ? "start of the target" : "end of the target";
        }

        void tallyJump(ScriptCodeCensus::JumpTally& tally, const Statement& from, const Statement& target)
        {
            const bool isBegin = from.mKind == Statement::Kind::Begin;
            const std::uint64_t jump = isBegin ? from.mBlockLength : from.mJump;
            const std::array<std::uint64_t, ScriptCodeCensus::JumpFromCount> bases{ from.mOffset, from.mDataOffset,
                isBegin ? from.mArgumentsOffset : from.mDataOffset + 2, from.mEnd };
            const std::array<std::uint64_t, ScriptCodeCensus::JumpToCount> ends{ target.mOffset, target.mEnd };
            ++tally.mTotal;
            for (std::size_t f = 0; f < bases.size(); ++f)
                for (std::size_t t = 0; t < ends.size(); ++t)
                    if (bases[f] + jump == ends[t])
                        ++tally.mMatches[f][t];
        }

        void writeJumps(std::ostream& stream, const char* name, const ScriptCodeCensus::JumpTally& tally)
        {
            stream << "  " << name << ": " << tally.mTotal << " jumps";
            if (tally.mUnresolved != 0)
                stream << ", " << tally.mUnresolved << " without a statement to jump to";
            stream << '\n';
            for (std::size_t f = 0; f < tally.mMatches.size(); ++f)
                for (std::size_t t = 0; t < tally.mMatches[f].size(); ++t)
                    if (tally.mMatches[f][t] != 0)
                        stream << "    counted from the " << fromName(static_cast<ScriptCodeCensus::JumpFrom>(f))
                               << " to the " << toName(static_cast<ScriptCodeCensus::JumpTo>(t)) << ": "
                               << tally.mMatches[f][t] << '\n';
        }
    }

    void ScriptCodeCensus::checkStructure(const Program& program)
    {
        const std::vector<Statement>& statements = program.mStatements;

        // Blocks: each Begin is followed by an End, and blocks do not nest
        bool open = false;
        std::size_t begin = 0;
        for (std::size_t i = 0; i < statements.size(); ++i)
        {
            const Statement& statement = statements[i];
            if (statement.mKind == Statement::Kind::Begin)
            {
                if (open)
                    ++mStructure["Begin inside a block"];
                open = true;
                begin = i;
            }
            else if (statement.mKind == Statement::Kind::End)
            {
                if (!open)
                    ++mStructure["End without Begin"];
                else
                    tallyJump(mBeginJumps, statements[begin], statement);
                open = false;
            }
        }
        if (open)
        {
            ++mStructure["Begin without End"];
            ++mBeginJumps.mTotal;
            ++mBeginJumps.mUnresolved;
        }

        // Conditions: the branch that was last seen jumps to the one that follows it
        std::vector<std::size_t> branches;
        auto tally = [&](std::size_t branch, const Statement& target) {
            const Statement& from = statements[branch];
            switch (from.mKind)
            {
                case Statement::Kind::If:
                    tallyJump(mIfJumps, from, target);
                    break;
                case Statement::Kind::ElseIf:
                    tallyJump(mElseIfJumps, from, target);
                    break;
                default:
                    tallyJump(mElseJumps, from, target);
                    break;
            }
        };
        for (std::size_t i = 0; i < statements.size(); ++i)
        {
            const Statement& statement = statements[i];
            switch (statement.mKind)
            {
                case Statement::Kind::If:
                    branches.push_back(i);
                    break;
                case Statement::Kind::ElseIf:
                case Statement::Kind::Else:
                    if (branches.empty())
                    {
                        ++mStructure["ElseIf or Else without If"];
                        break;
                    }
                    tally(branches.back(), statement);
                    branches.back() = i;
                    break;
                case Statement::Kind::EndIf:
                    if (branches.empty())
                    {
                        ++mStructure["EndIf without If"];
                        break;
                    }
                    tally(branches.back(), statement);
                    branches.pop_back();
                    break;
                default:
                    break;
            }
        }
        for (const std::size_t branch : branches)
        {
            ++mStructure["If without EndIf"];
            switch (statements[branch].mKind)
            {
                case Statement::Kind::If:
                    ++mIfJumps.mTotal;
                    ++mIfJumps.mUnresolved;
                    break;
                case Statement::Kind::ElseIf:
                    ++mElseIfJumps.mTotal;
                    ++mElseIfJumps.mUnresolved;
                    break;
                default:
                    ++mElseJumps.mTotal;
                    ++mElseJumps.mUnresolved;
                    break;
            }
        }
    }

    void ScriptCodeCensus::addScript(const std::string& holder, ESM::FormId record, const ScriptDefinition& script)
    {
        const bool holdsScript = !script.compiledScript.empty() || script.scriptHeader.compiledSize != 0
            || script.scriptHeader.refCount != 0 || !script.scriptSource.empty() || !script.localVarData.empty()
            || !script.references.empty();
        if (!holdsScript)
            return;

        HolderTotals& totals = mHolders[holder];
        ++totals.mScripts;
        ++totals.mTypes[script.scriptHeader.type];
        if (script.compiledScript.empty())
        {
            ++totals.mEmpty;
            return;
        }
        totals.mBytes += script.compiledScript.size();
        mMaxBytes = std::max(mMaxBytes, script.compiledScript.size());

        ScriptCode::Limits limits;
        limits.mReferences = script.references.size();
        limits.mVariables = script.highestVariableIndex();
        const Program program = ScriptCode::decode(script.compiledScript, limits);
        totals.mStatements += program.mStatements.size();
        mMaxStatements = std::max(mMaxStatements, program.mStatements.size());
        mMaxTokens = std::max(mMaxTokens, program.mTokens.size());

        if (program.mError != Error::None)
        {
            ++totals.mFailed;
            ++mErrors[program.mError];
            if (mErrorValues.size() < maxErrorValues || mErrorValues.count({ program.mError, program.mErrorValue }))
                ++mErrorValues[{ program.mError, program.mErrorValue }];
            std::vector<Failure>& failures = mFailures[program.mError];
            if (failures.size() < maxFailuresPerError)
            {
                Failure failure;
                failure.mHolder = holder;
                failure.mRecord = record;
                failure.mSize = script.compiledScript.size();
                failure.mOffset = program.mErrorOffset;
                failure.mValue = program.mErrorValue;
                constexpr std::size_t before = 12;
                constexpr std::size_t length = 36;
                const std::size_t from = failure.mOffset > before ? failure.mOffset - before : 0;
                const std::size_t to = std::min(script.compiledScript.size(), from + length);
                std::ostringstream bytes;
                bytes << std::hex << std::setfill('0');
                for (std::size_t i = from; i < to; ++i)
                    bytes << (i == from ? "" : " ") << (i == failure.mOffset ? "[" : "") << std::setw(2)
                          << static_cast<unsigned>(script.compiledScript[i]);
                failure.mBytes = bytes.str();
                failures.push_back(std::move(failure));
            }
        }
        else
            ++totals.mDecoded;

        std::set<std::uint16_t> called;
        for (const Statement& statement : program.mStatements)
        {
            ++mStatementCodes[statement.mCode < ScriptCode::firstCommand ? statement.mCode : 0xFFFF];
            if (statement.mKind == Statement::Kind::Begin)
            {
                ++mBlocks[{ script.scriptHeader.type, statement.mBlockType }];
                if (statement.mEnd > statement.mArgumentsOffset)
                    ++mBlockArguments[statement.mBlockType];
            }
            else if (statement.mKind == Statement::Kind::Call)
            {
                const ScriptCode::Call& call = program.mCalls[statement.mCall];
                CommandUse& use = mCommands[call.mOpcode];
                ++use.mStatements;
                if (call.mReference != 0)
                    ++use.mOnReference;
                use.mTotalArgumentBytes += call.mLength;
                use.mMaxArgumentBytes = std::max(use.mMaxArgumentBytes, call.mLength);
                called.insert(call.mOpcode);
            }
        }
        for (const Token& token : program.mTokens)
        {
            switch (token.mKind)
            {
                case Token::Kind::Number:
                    ++mTokens["number"];
                    break;
                case Token::Kind::Variable:
                    ++mTokens[token.mVariable.mRemote != 0 ? "variable of another script" : "local variable"];
                    break;
                case Token::Kind::Global:
                    ++mTokens["global variable"];
                    break;
                case Token::Kind::Reference:
                    ++mTokens["reference"];
                    break;
                case Token::Kind::String:
                    ++mTokens["string"];
                    break;
                case Token::Kind::Operator:
                    ++mTokens["operator"];
                    ++mOperators[std::string(ScriptCode::operatorText(token.mOperator))];
                    break;
                case Token::Kind::Call:
                {
                    ++mTokens["call"];
                    const ScriptCode::Call& call = program.mCalls[token.mCall];
                    CommandUse& use = mCommands[call.mOpcode];
                    ++use.mExpressions;
                    if (call.mReference != 0)
                        ++use.mOnReference;
                    use.mTotalArgumentBytes += call.mLength;
                    use.mMaxArgumentBytes = std::max(use.mMaxArgumentBytes, call.mLength);
                    called.insert(call.mOpcode);
                    break;
                }
            }
        }
        for (const std::uint16_t opcode : called)
            ++mCommands[opcode].mScripts;

        checkStructure(program);
    }

    void ScriptCodeCensus::collect(Reader& reader)
    {
        auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            if (type != REC_SCPT && type != REC_INFO && type != REC_QUST && type != REC_TERM && type != REC_PACK
                && type != REC_PERK)
                return false;

            const ReaderContext recordStart = r.getContext();
            if ((r.hdr().record.flags & Rec_Deleted) == 0)
            {
                try
                {
                    r.getRecordData();
                    switch (type)
                    {
                        case REC_SCPT:
                        {
                            Script record;
                            record.load(r);
                            addScript("SCPT", record.mId, record.mScript);
                            break;
                        }
                        case REC_INFO:
                        {
                            DialogInfo record;
                            record.load(r);
                            addScript("INFO", record.mId, record.mScript);
                            addScript("INFO", record.mId, record.mEndScript);
                            break;
                        }
                        case REC_QUST:
                        {
                            Quest record;
                            record.load(r);
                            for (const QuestStage& stage : record.mStages)
                                for (const QuestLogEntry& entry : stage.mLogEntries)
                                    addScript("QUST", record.mId, entry.mScript);
                            break;
                        }
                        case REC_TERM:
                        {
                            Terminal record;
                            record.load(r);
                            for (const Terminal::MenuItem& item : record.mMenuItems)
                                addScript("TERM", record.mId, item.mScript);
                            break;
                        }
                        case REC_PACK:
                        {
                            AIPackage record;
                            record.load(r);
                            addScript("PACK", record.mId, record.mBegin.mScript);
                            addScript("PACK", record.mId, record.mEnd.mScript);
                            addScript("PACK", record.mId, record.mChange.mScript);
                            break;
                        }
                        default:
                        {
                            Perk record;
                            record.load(r);
                            addScript("PERK", record.mId, record.mScript);
                            for (const Perk::Entry& entry : record.mEntries)
                                addScript("PERK", record.mId, entry.mScript);
                            break;
                        }
                    }
                }
                catch (const std::exception&)
                {
                    // The record cannot be read, so its script counts for nothing
                    ++mRecordsFailed[std::string(ESM::NAME(type).toStringView())];
                }
            }
            r.skipFailedRecord(recordStart);
            return true;
        };

        try
        {
            ReaderUtils::readAll(reader, visitRecord, [](Reader&) {});
        }
        catch (const std::exception& e)
        {
            // Raised by the reader while it walks record and group headers, so it holds offsets and sizes only.
            mFatalErrors.push_back(firstLine(e.what()));
        }
    }

    void ScriptCodeCensus::write(std::ostream& stream) const
    {
        constexpr int countWidth = 9;
        constexpr int nameWidth = 14;

        stream << "Scripts by the record that holds them\n";
        stream << "  " << std::left << std::setw(nameWidth) << "Record" << std::right << std::setw(countWidth)
               << "Scripts" << std::setw(countWidth) << "No code" << std::setw(countWidth) << "Decoded"
               << std::setw(countWidth) << "Failed" << std::setw(countWidth + 3) << "Bytes" << std::setw(countWidth + 2)
               << "Statements" << '\n';
        HolderTotals all;
        for (const auto& [holder, totals] : mHolders)
        {
            stream << "  " << std::left << std::setw(nameWidth) << holder << std::right << std::setw(countWidth)
                   << totals.mScripts << std::setw(countWidth) << totals.mEmpty << std::setw(countWidth)
                   << totals.mDecoded << std::setw(countWidth) << totals.mFailed << std::setw(countWidth + 3)
                   << totals.mBytes << std::setw(countWidth + 2) << totals.mStatements << '\n';
            all.mScripts += totals.mScripts;
            all.mEmpty += totals.mEmpty;
            all.mDecoded += totals.mDecoded;
            all.mFailed += totals.mFailed;
            all.mBytes += totals.mBytes;
            all.mStatements += totals.mStatements;
            for (const auto& [type, count] : totals.mTypes)
                all.mTypes[type] += count;
        }
        stream << "  " << std::left << std::setw(nameWidth) << "all" << std::right << std::setw(countWidth)
               << all.mScripts << std::setw(countWidth) << all.mEmpty << std::setw(countWidth) << all.mDecoded
               << std::setw(countWidth) << all.mFailed << std::setw(countWidth + 3) << all.mBytes
               << std::setw(countWidth + 2) << all.mStatements << '\n';
        stream << "  largest script: " << mMaxBytes << " bytes, " << mMaxStatements << " statements, " << mMaxTokens
               << " expression tokens\n";
        for (const auto& [holder, count] : mRecordsFailed)
            stream << "  " << holder << " records that did not load: " << count << '\n';

        stream << "\nScript types (the type in SCHR)\n";
        for (const auto& [type, count] : all.mTypes)
            stream << "  " << std::left << std::setw(nameWidth) << scriptTypeName(type) << std::right
                   << std::setw(countWidth) << count << '\n';

        stream << "\nScripts that could not be decoded: " << all.mFailed << '\n';
        for (const auto& [error, count] : mErrors)
            stream << "  " << ScriptCode::errorText(error) << ": " << count << '\n';
        for (const auto& [key, count] : mErrorValues)
            stream << "    " << ScriptCode::errorText(key.first) << ", value " << hex(key.second, 4) << ": " << count
                   << '\n';

        if (!mFailures.empty())
        {
            stream << "\nThe first scripts that failed, by kind of failure: record, its form id, size of the code,\n"
                      "offset of the failure and the bytes from 12 before it ([ marks the offset)\n";
            for (const auto& [error, failures] : mFailures)
            {
                stream << "  " << ScriptCode::errorText(error) << '\n';
                for (const Failure& failure : failures)
                    stream << "    " << failure.mHolder << ' ' << failure.mRecord.toString() << ", " << failure.mSize
                           << " bytes, offset " << failure.mOffset << ", value " << hex(failure.mValue, 4) << ": "
                           << failure.mBytes << '\n';
            }
        }

        stream << "\nStatements (all the commands are under one code)\n";
        for (const auto& [code, count] : mStatementCodes)
            stream << "  " << hex(code, 4) << ' ' << std::left << std::setw(nameWidth)
                   << statementName(code == 0xFFFF ? ScriptCode::firstCommand : code) << std::right
                   << std::setw(countWidth) << count << '\n';

        stream << "\nBlocks (Begin) by the type of script and the number of the block type\n";
        for (const auto& [key, count] : mBlocks)
            stream << "  " << std::left << std::setw(nameWidth) << scriptTypeName(key.first) << std::right
                   << std::setw(4) << key.second << std::setw(countWidth) << count << '\n';
        if (!mBlockArguments.empty())
        {
            stream << "  blocks with bytes after the block length, by block type:";
            for (const auto& [blockType, count] : mBlockArguments)
                stream << ' ' << blockType << ':' << count;
            stream << '\n';
        }

        stream << "\nNesting\n";
        if (mStructure.empty())
            stream << "  every Begin has its End and every If its EndIf\n";
        for (const auto& [problem, count] : mStructure)
            stream << "  " << problem << ": " << count << '\n';

        stream << "\nJumps: how many bytes each jump counts, and what it leads to. A rule that matches every jump is\n"
                  "the one the compiler uses.\n";
        writeJumps(stream, "Begin to End", mBeginJumps);
        writeJumps(stream, "If to ElseIf, Else or EndIf", mIfJumps);
        writeJumps(stream, "ElseIf to ElseIf, Else or EndIf", mElseIfJumps);
        writeJumps(stream, "Else to EndIf", mElseJumps);

        stream << "\nExpression tokens\n";
        for (const auto& [kind, count] : mTokens)
            stream << "  " << std::left << std::setw(28) << kind << std::right << std::setw(countWidth) << count
                   << '\n';
        stream << "  operators:";
        for (const auto& [text, count] : mOperators)
            stream << ' ' << text << ':' << count;
        stream << '\n';

        stream << "\nCommands by code: statements, calls in expressions, calls on a reference, scripts, largest\n"
                  "argument bytes, mean argument bytes\n";
        std::size_t commandStatements = 0;
        std::size_t nvseCommands = 0;
        for (const auto& [opcode, use] : mCommands)
        {
            commandStatements += use.mStatements;
            if (opcode >= 0x1400)
                nvseCommands += use.mScripts;
            const std::size_t calls = use.mStatements + use.mExpressions;
            stream << "  " << hex(opcode, 4) << std::setw(countWidth) << use.mStatements << std::setw(countWidth)
                   << use.mExpressions << std::setw(countWidth) << use.mOnReference << std::setw(countWidth)
                   << use.mScripts << std::setw(countWidth) << use.mMaxArgumentBytes << std::setw(countWidth)
                   << (calls == 0 ? 0 : use.mTotalArgumentBytes / calls) << '\n';
        }
        stream << "  " << mCommands.size() << " different commands, " << commandStatements
               << " statements call one; codes from 0x1400 on (extensions of the script language) are used by "
               << nvseCommands << " (script, command) pairs\n";

        for (const std::string& error : mFatalErrors)
            stream << "\nFATAL ERROR: " << error << '\n';
    }
}
