#include "census.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iomanip>
#include <ostream>
#include <utility>
#include <vector>

#include <components/esm/common.hpp>

#include "loadinfo.hpp"
#include "loadpack.hpp"
#include "loadqust.hpp"
#include "loadscpt.hpp"
#include "loadterm.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    namespace
    {
        constexpr std::size_t maxMessagesPerRecordType = 10;

        std::string firstLine(std::string_view message)
        {
            return std::string(message.substr(0, message.find('\n')));
        }

        bool isCodeChar(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
        }

        // Loader messages can embed record contents, for example the editor ID in the LVLO size errors of LVLI,
        // LVLC and LVLN. Only the unknown subrecord message, which names a loader and a four character code, is
        // known to hold nothing else, so it is the only one kept.
        std::string describeFailure(std::string_view message)
        {
            constexpr std::string_view marker = " - Unknown subrecord ";
            const std::size_t position = message.find(marker);
            if (position != std::string_view::npos)
            {
                const std::string_view loader = message.substr(0, position);
                const std::string_view code = message.substr(position + marker.size());
                const auto isLoaderChar = [](char c) { return isCodeChar(c) || c == ':' || c == ' ' || c == '/'; };
                if (code.size() == 4 && std::all_of(code.begin(), code.end(), isCodeChar)
                    && std::all_of(loader.begin(), loader.end(), isLoaderChar))
                    return std::string(message);
            }
            return "loader error (message withheld, it may contain record contents)";
        }
    }

    void Census::add(const std::string& type, CensusOutcome outcome, std::string_view failure)
    {
        CensusRecord& record = mRecords[type];
        ++record.mTotal;
        switch (outcome)
        {
            case CensusOutcome::Parsed:
                ++record.mParsed;
                break;
            case CensusOutcome::NoParser:
                ++record.mNoParser;
                break;
            case CensusOutcome::Failed:
                ++record.mFailed;
                ++record.mFailures[std::string(failure)];
                break;
        }
    }

    /// Accumulate script count, bytecode size and header mismatches for the owning record type.
    /// Ignore blocks without bytecode, source, locals, references or declared bytecode/references.
    void Census::addScript(const std::string& type, const ScriptDefinition& script)
    {
        const bool holdsScript = !script.compiledScript.empty() || script.scriptHeader.compiledSize != 0
            || script.scriptHeader.refCount != 0 || !script.scriptSource.empty() || !script.localVarData.empty()
            || !script.references.empty();
        if (!holdsScript)
            return;

        CensusScripts& scripts = mScripts[type];
        ++scripts.mCount;
        scripts.mBytecode += script.compiledScript.size();
        if (!script.hasConsistentSize())
            ++scripts.mWrongSize;
        if (!script.hasConsistentReferences())
            ++scripts.mWrongReferences;
        if (!script.hasConsistentVariables())
            ++scripts.mWrongVariables;
    }

    /// Add the loaded SCPT record's script to the census without retaining its contents.
    void Census::addScripts(const Script& record)
    {
        addScript("SCPT", record.mScript);
    }

    /// Add both response scripts of a loaded INFO record to the census.
    void Census::addScripts(const DialogInfo& record)
    {
        addScript("INFO", record.mScript);
        addScript("INFO", record.mEndScript);
    }

    /// Add every stage log entry's script from a loaded QUST record to the census.
    void Census::addScripts(const Quest& record)
    {
        for (const QuestStage& stage : record.mStages)
            for (const QuestLogEntry& entry : stage.mLogEntries)
                addScript("QUST", entry.mScript);
    }

    /// Add the scripts that a loaded PACK record runs when it begins, ends and changes to the census.
    void Census::addScripts(const AIPackage& record)
    {
        addScript("PACK", record.mBegin.mScript);
        addScript("PACK", record.mEnd.mScript);
        addScript("PACK", record.mChange.mScript);
    }

    /// Add the script of every menu item of a loaded TERM record to the census.
    void Census::addScripts(const Terminal& record)
    {
        for (const Terminal::MenuItem& item : record.mMenuItems)
            addScript("TERM", item.mScript);
    }

    void Census::collect(Reader& reader, const std::function<bool(Reader&)>& parse)
    {
        auto visitRecord = [&](Reader& r) {
            const std::string type = ESM::printName(r.hdr().record.typeId);
            const ReaderContext recordStart = r.getContext();
            try
            {
                if (parse(r))
                {
                    add(type, CensusOutcome::Parsed);
                    return true;
                }
                add(type, CensusOutcome::NoParser);
                // Returning false makes ReaderUtils skip the record data, which nothing has read.
                return false;
            }
            catch (const std::exception& e)
            {
                add(type, CensusOutcome::Failed, describeFailure(firstLine(e.what())));
            }
            // The loader stopped part-way, so the stream is somewhere inside the record.
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
            mFatalError = firstLine(e.what());
        }
    }

    /// Write record totals, bounded failure summaries, optional script totals and any fatal error.
    void Census::write(std::ostream& stream) const
    {
        CensusRecord total;
        for (const auto& [type, record] : mRecords)
        {
            total.mTotal += record.mTotal;
            total.mParsed += record.mParsed;
            total.mNoParser += record.mNoParser;
            total.mFailed += record.mFailed;
        }

        constexpr int typeWidth = 8;
        constexpr int countWidth = 11;
        auto writeRow = [&](std::string_view type, const CensusRecord& record) {
            stream << std::left << std::setw(typeWidth) << type << std::right << std::setw(countWidth) << record.mTotal
                   << std::setw(countWidth) << record.mParsed << std::setw(countWidth) << record.mNoParser
                   << std::setw(countWidth) << record.mFailed << '\n';
        };

        stream << std::left << std::setw(typeWidth) << "Record" << std::right << std::setw(countWidth) << "Total"
               << std::setw(countWidth) << "Parsed" << std::setw(countWidth) << "No parser" << std::setw(countWidth)
               << "Failed" << '\n';
        for (const auto& [type, record] : mRecords)
            writeRow(type, record);
        writeRow("All", total);

        for (const auto& [type, record] : mRecords)
        {
            if (record.mFailures.empty())
                continue;

            std::vector<std::pair<std::string_view, std::size_t>> failures;
            failures.reserve(record.mFailures.size());
            for (const auto& [message, count] : record.mFailures)
                failures.emplace_back(message, count);
            std::stable_sort(failures.begin(), failures.end(),
                [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

            stream << "\nFailures in " << type << " records:\n";
            const std::size_t shown = std::min(failures.size(), maxMessagesPerRecordType);
            for (std::size_t i = 0; i < shown; ++i)
                stream << "  " << failures[i].second << " x " << failures[i].first << '\n';
            if (failures.size() > shown)
                stream << "  ... and " << failures.size() - shown << " more distinct messages\n";
        }

        if (!mScripts.empty())
        {
            stream << "\nScripts held in line by records:\n"
                   << std::left << std::setw(typeWidth) << "Record" << std::right << std::setw(countWidth) << "Scripts"
                   << std::setw(countWidth) << "Bytecode" << std::setw(countWidth) << "Bad size"
                   << std::setw(countWidth) << "Bad refs" << std::setw(countWidth) << "Bad vars" << '\n';
            for (const auto& [type, scripts] : mScripts)
                stream << std::left << std::setw(typeWidth) << type << std::right << std::setw(countWidth)
                       << scripts.mCount << std::setw(countWidth) << scripts.mBytecode << std::setw(countWidth)
                       << scripts.mWrongSize << std::setw(countWidth) << scripts.mWrongReferences
                       << std::setw(countWidth) << scripts.mWrongVariables << '\n';
        }

        if (!mFatalError.empty())
            stream << "\nReading stopped early: " << mFatalError << '\n';
    }
}
