#include "census.hpp"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <ostream>
#include <utility>
#include <vector>

#include <components/esm/common.hpp>

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
                add(type, CensusOutcome::Failed, firstLine(e.what()));
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
            mFatalError = firstLine(e.what());
        }
    }

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

        if (!mFatalError.empty())
            stream << "\nReading stopped early: " << mFatalError << '\n';
    }
}
