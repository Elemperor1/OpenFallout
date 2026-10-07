#include "referencecensus.hpp"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <ostream>
#include <set>
#include <string_view>
#include <utility>

#include <components/esm/common.hpp>

#include "common.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    namespace
    {
        // The records that place an object in a cell: a reference, a character, a creature, a grenade, a missile and
        // a beam. The last two are written by Fallout only and have no record type of their own in common.hpp.
        bool isReference(std::uint32_t type)
        {
            return type == REC_REFR || type == REC_ACHR || type == REC_ACRE || type == REC_PGRE
                || type == ESM::fourCC("PMIS") || type == ESM::fourCC("PBEA");
        }

        std::string firstLine(std::string_view message)
        {
            return std::string(message.substr(0, message.find('\n')));
        }

        // The base object of the current reference, whose data has been read: the form ID of its NAME. Zero when
        // the record has none, as a record that changes only a part of a reference may not.
        ESM::FormId readBase(Reader& reader)
        {
            ESM::FormId base;
            while (reader.getSubRecordHeader())
            {
                if (reader.subRecordHeader().typeId == ESM::fourCC("NAME") && reader.subRecordHeader().dataSize == 4)
                    reader.getFormId(base);
                else
                    reader.skipSubRecordData();
            }
            return base;
        }
    }

    void ReferenceCensus::collect(Reader& reader)
    {
        auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            const ESM::FormId id = r.getFormIdFromHeader();

            if (!isReference(type))
            {
                mRecordTypes[id] = type;
                return false; // nothing has been read, the record data is skipped
            }

            const ReaderContext recordStart = r.getContext();
            const std::uint32_t flags = r.hdr().record.flags;
            try
            {
                r.getRecordData();
                const ESM::FormId base = readBase(r);

                if ((flags & Rec_Deleted) != 0)
                {
                    if (mPlaced.erase(id) != 0)
                        ++mDeleted;
                }
                else
                {
                    Placed& placed = mPlaced[id];
                    placed.mType = type;
                    // A record that overrides a reference may leave out what it does not change.
                    if (base.mIndex != 0 || placed.mBase.mIndex == 0)
                        placed.mBase = base;
                    placed.mDisabled = (flags & Rec_Disabled) != 0;
                }
            }
            catch (const std::exception&)
            {
                // The record cannot be read. It is left out; the file goes on with the next one.
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

    std::map<std::string, std::map<std::string, ReferenceCensus::Count>> ReferenceCensus::getCounts() const
    {
        std::map<std::string, std::map<std::string, Count>> result;
        for (const auto& [id, placed] : mPlaced)
        {
            std::string base = noBase;
            if (placed.mBase.mIndex != 0)
            {
                const auto it = mRecordTypes.find(placed.mBase);
                base = it != mRecordTypes.end() ? ESM::printName(it->second) : unknownBase;
            }

            Count& count = result[base][ESM::printName(placed.mType)];
            ++count.mTotal;
            if (placed.mDisabled)
                ++count.mDisabled;
        }
        return result;
    }

    void ReferenceCensus::write(std::ostream& stream) const
    {
        const auto counts = getCounts();

        // One column for each type of reference there is, in the order of the types of placed records.
        std::set<std::string> columns;
        for (const auto& [base, references] : counts)
            for (const auto& [reference, count] : references)
                columns.insert(reference);

        constexpr int typeWidth = 8;
        constexpr int countWidth = 11;

        struct Row
        {
            std::string mBase;
            Count mTotal;
        };
        std::vector<Row> rows;
        Count all;
        for (const auto& [base, references] : counts)
        {
            Row& row = rows.emplace_back();
            row.mBase = base;
            for (const auto& [reference, count] : references)
            {
                row.mTotal.mTotal += count.mTotal;
                row.mTotal.mDisabled += count.mDisabled;
            }
            all.mTotal += row.mTotal.mTotal;
            all.mDisabled += row.mTotal.mDisabled;
        }
        // The types with the most references first, those with as many by name.
        std::stable_sort(rows.begin(), rows.end(),
            [](const Row& left, const Row& right) { return left.mTotal.mTotal > right.mTotal.mTotal; });

        stream << "Placed references by the record type of their base object\n";
        stream << std::left << std::setw(typeWidth) << "Base" << std::right;
        for (const std::string& column : columns)
            stream << std::setw(countWidth) << column;
        stream << std::setw(countWidth) << "All" << std::setw(countWidth) << "Disabled" << '\n';

        const auto writeRow
            = [&](std::string_view name, const std::map<std::string, Count>* references, const Count& total) {
                  stream << std::left << std::setw(typeWidth) << name << std::right;
                  for (const std::string& column : columns)
                  {
                      std::size_t number = 0;
                      if (references != nullptr)
                          if (const auto it = references->find(column); it != references->end())
                              number = it->second.mTotal;
                      stream << std::setw(countWidth) << number;
                  }
                  stream << std::setw(countWidth) << total.mTotal << std::setw(countWidth) << total.mDisabled << '\n';
              };

        for (const Row& row : rows)
            writeRow(row.mBase, &counts.at(row.mBase), row.mTotal);

        // The totals of each column.
        std::map<std::string, Count> columnTotals;
        for (const auto& [base, references] : counts)
            for (const auto& [reference, count] : references)
            {
                Count& total = columnTotals[reference];
                total.mTotal += count.mTotal;
                total.mDisabled += count.mDisabled;
            }
        writeRow("All", &columnTotals, all);

        stream << '\n' << mDeleted << " references of earlier files are deleted by later ones\n";
        for (const std::string& error : mFatalErrors)
            stream << "Stopped before the end of a file: " << error << '\n';
    }
}
