#include "scriptcodecensus.hpp"

#include <algorithm>
#include <cstdlib>
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
                case ScriptCodeCensus::FromExpression:
                    return "start of the expression";
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
                isBegin ? from.mArgumentsOffset : from.mDataOffset + 2, from.mEnd, from.mDataOffset + 4 };
            const std::array<std::uint64_t, ScriptCodeCensus::JumpToCount> ends{ target.mOffset, target.mEnd };
            ++tally.mTotal;
            for (std::size_t f = 0; f < bases.size(); ++f)
                for (std::size_t t = 0; t < ends.size(); ++t)
                {
                    if (bases[f] + jump == ends[t])
                        ++tally.mMatches[f][t];
                    const std::int64_t difference
                        = static_cast<std::int64_t>(ends[t]) - static_cast<std::int64_t>(bases[f] + jump);
                    auto& counts = tally.mDifferences[f][t];
                    const auto found = counts.find(difference);
                    if (found != counts.end())
                        ++found->second;
                    else if (counts.size() < ScriptCodeCensus::maxDifferences)
                        counts[difference] = 1;
                }
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

            // The rule that comes closest: the one whose most common difference is the most common of all
            std::size_t bestFrom = 0;
            std::size_t bestTo = 0;
            std::size_t bestCount = 0;
            std::int64_t bestDifference = 0;
            for (std::size_t f = 0; f < tally.mDifferences.size(); ++f)
                for (std::size_t t = 0; t < tally.mDifferences[f].size(); ++t)
                    for (const auto& [difference, count] : tally.mDifferences[f][t])
                        if (count > bestCount
                            || (count == bestCount && std::abs(difference) < std::abs(bestDifference)))
                        {
                            bestCount = count;
                            bestFrom = f;
                            bestTo = t;
                            bestDifference = difference;
                        }
            if (bestCount == 0)
                return;
            std::vector<std::pair<std::size_t, std::int64_t>> common;
            for (const auto& [difference, count] : tally.mDifferences[bestFrom][bestTo])
                common.emplace_back(count, difference);
            std::sort(common.begin(), common.end(), [](const auto& left, const auto& right) {
                return left.first != right.first ? left.first > right.first : left.second < right.second;
            });
            stream << "    the closest rule is from the " << fromName(static_cast<ScriptCodeCensus::JumpFrom>(bestFrom))
                   << " to the " << toName(static_cast<ScriptCodeCensus::JumpTo>(bestTo))
                   << ", the jump falls short by (bytes: jumps):";
            for (std::size_t i = 0; i < common.size() && i < 6; ++i)
                stream << ' ' << common[i].second << ':' << common[i].first;
            stream << '\n';
        }

        const char* statementKind(Statement::Kind kind)
        {
            switch (kind)
            {
                case Statement::Kind::ScriptName:
                    return "Sn";
                case Statement::Kind::Begin:
                    return "Bg";
                case Statement::Kind::End:
                    return "En";
                case Statement::Kind::Declaration:
                    return "Dc";
                case Statement::Kind::SetTo:
                    return "St";
                case Statement::Kind::If:
                    return "If";
                case Statement::Kind::ElseIf:
                    return "Ei";
                case Statement::Kind::Else:
                    return "El";
                case Statement::Kind::EndIf:
                    return "Ef";
                case Statement::Kind::Return:
                    return "Rt";
                case Statement::Kind::Call:
                    return "Cl";
            }
            return "??";
        }

        // The kinds of the statements around one: [ marks the statement at the index, which may be just past the
        // last one when the script stopped there
        std::string statementKinds(const Program& program, std::size_t index, std::size_t before, std::size_t after)
        {
            const std::vector<Statement>& statements = program.mStatements;
            const std::size_t from = index > before ? index - before : 0;
            const std::size_t to = std::min(statements.size(), index + after + 1);
            std::string text;
            for (std::size_t i = from; i < to; ++i)
            {
                if (!text.empty())
                    text += ' ';
                if (i == index)
                    text += '[';
                text += statementKind(statements[i].mKind);
            }
            if (index >= statements.size())
                text += text.empty() ? "[" : " [";
            return text;
        }

        // The bytes of a script from 12 before an offset, in hexadecimal, [ marking the offset
        std::string bytesAround(const std::vector<std::uint8_t>& code, std::size_t offset)
        {
            constexpr std::size_t before = 12;
            constexpr std::size_t length = 36;
            const std::size_t from = offset > before ? offset - before : 0;
            const std::size_t to = std::min(code.size(), from + length);
            std::ostringstream bytes;
            bytes << std::hex << std::setfill('0');
            for (std::size_t i = from; i < to; ++i)
                bytes << (i == from ? "" : " ") << (i == offset ? "[" : "") << std::setw(2)
                      << static_cast<unsigned>(code[i]);
            return bytes.str();
        }

        ScriptCodeCensus::Failure makeFailure(const std::string& holder, ESM::FormId record,
            const std::vector<std::uint8_t>& code, std::size_t offset, std::uint16_t value, std::string statements)
        {
            ScriptCodeCensus::Failure failure;
            failure.mHolder = holder;
            failure.mRecord = record;
            failure.mSize = code.size();
            failure.mOffset = static_cast<std::uint32_t>(offset);
            failure.mValue = value;
            failure.mBytes = bytesAround(code, offset);
            failure.mStatements = std::move(statements);
            return failure;
        }

        void writeFailures(std::ostream& stream, const std::vector<ScriptCodeCensus::Failure>& failures)
        {
            for (const ScriptCodeCensus::Failure& failure : failures)
                stream << "    " << failure.mHolder << ' ' << failure.mRecord.toString() << ", " << failure.mSize
                       << " bytes, offset " << failure.mOffset << ", value " << hex(failure.mValue, 4) << ": "
                       << failure.mBytes << "\n      statements: " << failure.mStatements << '\n';
        }
    }

    void ScriptCodeCensus::checkStructure(
        const Program& program, const std::string& holder, ESM::FormId record, const ScriptDefinition& script)
    {
        const std::vector<Statement>& statements = program.mStatements;
        auto problem = [&](const char* name, std::size_t index) {
            ++mStructure[name];
            std::vector<Failure>& examples = mStructureExamples[name];
            if (examples.size() < maxFailuresPerError)
                examples.push_back(makeFailure(holder, record, script.compiledScript, statements[index].mOffset,
                    statements[index].mCode, statementKinds(program, index, 8, 3)));
        };

        // Blocks: each Begin is followed by an End, and blocks do not nest
        bool open = false;
        std::size_t begin = 0;
        for (std::size_t i = 0; i < statements.size(); ++i)
        {
            const Statement& statement = statements[i];
            if (statement.mKind == Statement::Kind::Begin)
            {
                if (open)
                    problem("Begin inside a block", i);
                open = true;
                begin = i;
            }
            else if (statement.mKind == Statement::Kind::End)
            {
                if (!open)
                    problem("End without Begin", i);
                else
                    tallyJump(mBeginJumps, statements[begin], statement);
                open = false;
            }
        }
        if (open)
        {
            problem("Begin without End", begin);
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
                        problem("ElseIf or Else without If", i);
                        break;
                    }
                    tally(branches.back(), statement);
                    branches.back() = i;
                    break;
                case Statement::Kind::EndIf:
                    if (branches.empty())
                    {
                        problem("EndIf without If", i);
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
            problem("If without EndIf", branch);
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
                Failure failure = makeFailure(holder, record, script.compiledScript, program.mErrorOffset,
                    program.mErrorValue, statementKinds(program, program.mStatements.size(), 8, 0));
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

        // A script that stopped at an error holds the statements before it only, so its blocks and conditions are
        // not closed and its jumps lead nowhere
        if (program.mError == Error::None)
            checkStructure(program, holder, record, script);
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
                      "offset of the failure and the bytes from 12 before it ([ marks the offset), then the kinds of\n"
                      "the statements before it (Sn ScriptName, Bg Begin, En End, Dc declaration, St SetTo, If, Ei "
                      "ElseIf,\n"
                      "El Else, Ef EndIf, Rt Return, Cl command call)\n";
            for (const auto& [error, failures] : mFailures)
            {
                stream << "  " << ScriptCode::errorText(error) << '\n';
                writeFailures(stream, failures);
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
        if (!mStructureExamples.empty())
        {
            stream
                << "\nThe first scripts of each nesting problem: the same as above, with the kinds of the statements\n"
                   "from 8 before the one at fault to 3 after it ([ marks it)\n";
            for (const auto& [problem, examples] : mStructureExamples)
            {
                stream << "  " << problem << '\n';
                writeFailures(stream, examples);
            }
        }

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
