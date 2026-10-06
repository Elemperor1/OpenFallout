#include "survey.hpp"

#include <algorithm>
#include <exception>
#include <ostream>
#include <string_view>
#include <utility>

#include <components/esm/common.hpp>

#include "census.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    namespace
    {
        constexpr std::size_t maxSizesPerSubRecord = 24;
        constexpr std::size_t maxOrdersPerType = 16;

        std::string firstLine(std::string_view message)
        {
            return std::string(message.substr(0, message.find('\n')));
        }

        // A code is written as it is except for bytes that are not printable ASCII, which are written as \xNN. Some
        // sub-record codes start with a byte below 0x20, and a line feed in a code would cut its line in two.
        std::string codeName(std::uint32_t typeId)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result;
            for (const unsigned char c : ESM::printName(typeId))
            {
                if (c >= 0x20 && c <= 0x7e && c != '\\')
                    result += static_cast<char>(c);
                else
                {
                    result += "\\x";
                    result += digits[c >> 4];
                    result += digits[c & 0xf];
                }
            }
            return result;
        }

        // Add the sub-records of the current record, which the reader has been rewound to the start of.
        void addSubRecords(Reader& reader, SurveyType& type)
        {
            std::map<std::string, std::size_t> counts;
            std::string order;
            std::string lastCode;
            std::size_t run = 0;
            bool overrunning = false;

            const auto endRun = [&] {
                if (run == 0)
                    return;
                if (!order.empty())
                    order += ' ';
                order += lastCode;
                if (run > 1)
                    order += '*' + std::to_string(run);
            };

            while (reader.getSubRecordHeader())
            {
                const SubRecordHeader& header = reader.subRecordHeader();
                if (!reader.subRecordFitsRecord())
                {
                    overrunning = true;
                    break;
                }

                const std::string code = codeName(header.typeId);
                SurveySubRecord& subRecord = type.mSubRecords[code];
                ++subRecord.mTotal;
                ++subRecord.mSizes[header.dataSize];
                ++counts[code];

                if (run != 0 && code == lastCode)
                    ++run;
                else
                {
                    endRun();
                    lastCode = code;
                    run = 1;
                }
                reader.skipSubRecordData();
            }
            endRun();

            for (const auto& [code, count] : counts)
            {
                SurveySubRecord& subRecord = type.mSubRecords[code];
                ++subRecord.mRecords;
                subRecord.mLeast = std::min(subRecord.mLeast, count);
                subRecord.mMost = std::max(subRecord.mMost, count);
            }
            ++type.mSurveyed;
            ++type.mOrders[order];
            if (overrunning)
                ++type.mOverrunning;
        }
    }

    Survey::Survey(std::set<std::string> types, bool failedOnly)
        : mSelected(std::move(types))
        , mFailedOnly(failedOnly)
    {
    }

    void Survey::collect(Reader& reader, const std::function<bool(Reader&)>& parse)
    {
        auto visitRecord = [&](Reader& r) {
            if (!mSelected.empty() && !mSelected.contains(ESM::printName(r.hdr().record.typeId)))
                return false; // nothing has been read, the record data is skipped

            const std::string code = codeName(r.hdr().record.typeId);

            const ReaderContext recordStart = r.getContext();
            SurveyType& type = mTypes[code];
            ++type.mTotal;

            CensusOutcome outcome = CensusOutcome::NoParser;
            try
            {
                if (parse && parse(r))
                    outcome = CensusOutcome::Parsed;
            }
            catch (const std::exception&)
            {
                outcome = CensusOutcome::Failed;
            }

            switch (outcome)
            {
                case CensusOutcome::Parsed:
                    ++type.mParsed;
                    break;
                case CensusOutcome::NoParser:
                    ++type.mNoParser;
                    break;
                case CensusOutcome::Failed:
                    ++type.mFailed;
                    break;
            }

            if (!mFailedOnly || outcome == CensusOutcome::Failed)
            {
                r.rewindRecordData(recordStart);
                addSubRecords(r, type);
            }
            // Whatever was read of the record, the stream is somewhere inside it now.
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

    void Survey::write(std::ostream& stream) const
    {
        stream << "Survey of sub-records" << (mFailedOnly ? ", only of records a loader rejects" : "") << '\n';
        for (const auto& [code, type] : mTypes)
        {
            stream << '\n'
                   << code << ": " << type.mTotal << " records, " << type.mParsed << " parsed, " << type.mNoParser
                   << " no parser, " << type.mFailed << " failed";
            if (type.mSurveyed != type.mTotal)
                stream << ", " << type.mSurveyed << " listed";
            stream << '\n';
            if (type.mOverrunning != 0)
                stream << "  " << type.mOverrunning << " listed records have a sub-record that runs past the end\n";

            for (const auto& [subCode, subRecord] : type.mSubRecords)
            {
                stream << "  " << subCode << ": in " << subRecord.mRecords << " records, " << subRecord.mLeast;
                if (subRecord.mMost != subRecord.mLeast)
                    stream << " to " << subRecord.mMost;
                stream << " each, " << subRecord.mTotal << " in all, sizes";
                std::size_t shown = 0;
                for (const auto& [size, count] : subRecord.mSizes)
                {
                    if (shown++ == maxSizesPerSubRecord)
                    {
                        stream << " ... and " << subRecord.mSizes.size() - maxSizesPerSubRecord
                               << " more sizes, the largest " << subRecord.mSizes.rbegin()->first;
                        break;
                    }
                    stream << ' ' << size << ':' << count;
                }
                stream << '\n';
            }

            std::vector<std::pair<std::string_view, std::size_t>> orders;
            orders.reserve(type.mOrders.size());
            for (const auto& [order, count] : type.mOrders)
                orders.emplace_back(order, count);
            std::stable_sort(
                orders.begin(), orders.end(), [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });
            stream << "  Orders of sub-records, " << orders.size() << " distinct";
            if (orders.size() > maxOrdersPerType)
                stream << ", the " << maxOrdersPerType << " most common";
            stream << ":\n";
            for (std::size_t i = 0; i < std::min(orders.size(), maxOrdersPerType); ++i)
                stream << "    " << orders[i].second << " x " << (orders[i].first.empty() ? "(none)" : orders[i].first)
                       << '\n';
        }

        for (const std::string& error : mFatalErrors)
            stream << "\nReading stopped early: " << error << '\n';
    }
}
