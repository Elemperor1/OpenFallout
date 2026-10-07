#include "referencecensus.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iomanip>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
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
        // The records that place an object in a cell: a reference, a character, a creature, a grenade, a hazard, a
        // missile and a beam. The last two are written by Fallout only and have no record type of their own in
        // common.hpp.
        bool isReference(std::uint32_t type)
        {
            return type == REC_REFR || type == REC_ACHR || type == REC_ACRE || type == REC_PGRE || type == REC_PHZD
                || type == ESM::fourCC("PMIS") || type == ESM::fourCC("PBEA");
        }

        std::string firstLine(std::string_view message)
        {
            return std::string(message.substr(0, message.find('\n')));
        }

        // The base object of the current reference, whose data has been read: the form ID of its NAME, which is empty
        // when the record has none (a record that changes only a part of a reference may leave it out). A sub-record
        // that runs past the end of the record, a record that ends inside a sub-record header, a NAME that is not 4
        // bytes and a file that ends inside the NAME are errors.
        std::optional<ESM::FormId> readBase(Reader& reader)
        {
            std::optional<ESM::FormId> base;
            while (true)
            {
                const bool found = reader.getSubRecordHeader();
                if (!reader.subRecordFitsRecord())
                    throw std::runtime_error("A sub-record runs past the end of its record");
                if (!found)
                {
                    // The same answer comes at the end of the record and when a few bytes of a header are left.
                    if (reader.unreadRecordBytes() != 0)
                        throw std::runtime_error("A record ends inside a sub-record header");
                    break;
                }

                if (reader.subRecordHeader().typeId == ESM::fourCC("NAME"))
                {
                    if (reader.subRecordHeader().dataSize != sizeof(std::uint32_t))
                        throw std::runtime_error("A base object is not a form ID");
                    ESM::FormId id;
                    if (!reader.getFormId(id))
                        throw std::runtime_error("The file ends inside a base object");
                    base = id;
                }
                else
                    reader.skipSubRecordData();
            }
            return base;
        }

        // The extension of the first model that the current record names, in lower case, or empty when it names none.
        // Reading stops at the model, the rest of the record is left unread.
        std::string readModelExtension(Reader& reader)
        {
            constexpr std::size_t longest = 8;
            while (true)
            {
                const bool found = reader.getSubRecordHeader();
                if (!reader.subRecordFitsRecord())
                    throw std::runtime_error("A sub-record runs past the end of its record");
                if (!found)
                    return {};

                if (reader.subRecordHeader().typeId != ESM::fourCC("MODL"))
                {
                    reader.skipSubRecordData();
                    continue;
                }

                std::string path;
                if (!reader.getZString(path))
                    throw std::runtime_error("The file ends inside a model");
                const std::size_t dot = path.rfind('.');
                if (dot == std::string::npos || path.find_first_of("/\\", dot) != std::string::npos)
                    return {};
                std::string extension = path.substr(dot + 1, longest);
                std::transform(extension.begin(), extension.end(), extension.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return extension;
            }
        }
    }

    std::size_t ReferenceCensus::modelKind(const std::string& extension)
    {
        if (extension.empty())
            return 0;
        const auto it = std::find(mModelKinds.begin(), mModelKinds.end(), extension);
        if (it != mModelKinds.end())
            return static_cast<std::size_t>(it - mModelKinds.begin());
        mModelKinds.push_back(extension);
        return mModelKinds.size() - 1;
    }

    void ReferenceCensus::collect(Reader& reader)
    {
        auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            const ESM::FormId id = r.getFormIdFromHeader();

            const ReaderContext recordStart = r.getContext();
            const std::uint32_t flags = r.hdr().record.flags;

            if (!isReference(type))
            {
                Record& record = mRecords[id];
                record.mType = type;
                try
                {
                    r.getRecordData();
                    // A record that overrides another may leave the model out, it keeps the one it had.
                    if (const std::string extension = readModelExtension(r); !extension.empty())
                        record.mModel = modelKind(extension);
                }
                catch (const std::exception&)
                {
                    // The record cannot be read, so it has no model that is known. Its type counts.
                }
                r.skipFailedRecord(recordStart);
                return true;
            }

            try
            {
                r.getRecordData();
                const std::optional<ESM::FormId> base = readBase(r);

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
                    if (base.has_value())
                        placed.mBase = *base;
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
                const auto it = mRecords.find(placed.mBase);
                base = it != mRecords.end() ? ESM::printName(it->second.mType) : unknownBase;
            }

            Count& count = result[base][ESM::printName(placed.mType)];
            ++count.mTotal;
            if (placed.mDisabled)
                ++count.mDisabled;
        }
        return result;
    }

    std::map<std::string, std::map<std::string, std::size_t>> ReferenceCensus::getModels() const
    {
        std::map<std::string, std::map<std::string, std::size_t>> result;
        for (const auto& [id, placed] : mPlaced)
        {
            const auto it = placed.mBase.mIndex != 0 ? mRecords.find(placed.mBase) : mRecords.end();
            if (it == mRecords.end())
                continue;
            ++result[ESM::printName(it->second.mType)][mModelKinds[it->second.mModel]];
        }
        return result;
    }

    std::vector<std::pair<ESM::FormId, std::size_t>> ReferenceCensus::getUnknownBases(std::size_t limit) const
    {
        std::map<ESM::FormId, std::size_t> counts;
        for (const auto& [id, placed] : mPlaced)
            if (placed.mBase.mIndex != 0 && !mRecords.contains(placed.mBase))
                ++counts[placed.mBase];

        std::vector<std::pair<ESM::FormId, std::size_t>> result(counts.begin(), counts.end());
        std::stable_sort(result.begin(), result.end(),
            [](const auto& left, const auto& right) { return left.second > right.second; });
        if (result.size() > limit)
            result.resize(limit);
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

        // The kinds of model, for each type of base object that has references with a model of a kind.
        const auto models = getModels();
        std::set<std::string> kinds;
        for (const auto& [base, perKind] : models)
            for (const auto& [kind, number] : perKind)
                kinds.insert(kind);
        stream << "\nPlaced references by the file extension of the model of their base object (" << noModel
               << " is no model)\n";
        stream << std::left << std::setw(typeWidth) << "Base" << std::right;
        for (const std::string& kind : kinds)
            stream << std::setw(countWidth) << kind;
        stream << '\n';
        for (const Row& row : rows)
        {
            const auto it = models.find(row.mBase);
            if (it == models.end())
                continue;
            stream << std::left << std::setw(typeWidth) << row.mBase << std::right;
            for (const std::string& kind : kinds)
            {
                const auto number = it->second.find(kind);
                stream << std::setw(countWidth) << (number == it->second.end() ? 0 : number->second);
            }
            stream << '\n';
        }

        constexpr std::size_t unknownShown = 10;
        if (const auto unknown = getUnknownBases(unknownShown); !unknown.empty())
        {
            stream << "\nThe base objects that no record was found for, most references first (form ID: references)\n";
            for (const auto& [id, number] : unknown)
                stream << id.toString() << ": " << number << '\n';
        }

        stream << '\n' << mDeleted << " references of earlier files are deleted by later ones\n";
        for (const std::string& error : mFatalErrors)
            stream << "Stopped before the end of a file: " << error << '\n';
    }
}
