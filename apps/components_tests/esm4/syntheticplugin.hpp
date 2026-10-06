#ifndef OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM4_SYNTHETICPLUGIN_H
#define OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM4_SYNTHETICPLUGIN_H

#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>

#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <zlib.h>

// Builds the bytes of small TES4-format plugins, so the loaders can be tested without any game data.
// The layout is Oblivion's: records have a 20 byte header, which ESM4::Reader detects from the TES4 record.
namespace ESM4Test
{
    /// Append the object representation of value to the synthetic plugin bytes.
    template <class T>
    void append(std::string& out, T value)
    {
        out.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    /// Build a subrecord from a four-character type, a 16-bit payload size and raw data.
    inline std::string subRecord(std::string_view type, std::string_view data)
    {
        std::string result(type);
        append<std::uint16_t>(result, static_cast<std::uint16_t>(data.size()));
        result.append(data);
        return result;
    }

    /// Build a subrecord containing text followed by a null terminator.
    inline std::string zString(std::string_view type, std::string_view text)
    {
        return subRecord(type, std::string(text) + '\0');
    }

    /// Build a subrecord containing the object representation of a single value.
    template <class T>
    std::string valueSubRecord(std::string_view type, T value)
    {
        std::string data;
        append(data, value);
        return subRecord(type, data);
    }

    /// Wrap raw subrecords in an Oblivion record header with the given type, form ID and flags.
    inline std::string record(std::string_view type, std::uint32_t id, std::string_view data, std::uint32_t flags = 0)
    {
        std::string result(type);
        append<std::uint32_t>(result, static_cast<std::uint32_t>(data.size()));
        append<std::uint32_t>(result, flags);
        append<std::uint32_t>(result, id);
        append<std::uint32_t>(result, 0); // revision
        result.append(data);
        return result;
    }

    /// Build a record with a zlib-compressed payload and its uncompressed size.
    /// Throws std::runtime_error if compression fails.
    inline std::string compressedRecord(std::string_view type, std::uint32_t id, std::string_view data)
    {
        uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
        std::string compressed(compressedSize, '\0');
        if (compress(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                reinterpret_cast<const Bytef*>(data.data()), static_cast<uLong>(data.size()))
            != Z_OK)
            throw std::runtime_error("Failed to compress test data");
        compressed.resize(compressedSize);

        std::string payload;
        append<std::uint32_t>(payload, static_cast<std::uint32_t>(data.size()));
        payload.append(compressed);
        return record(type, id, payload, ESM4::Rec_Compressed);
    }

    /// Wrap child records in a top-level GRUP labelled with their four-character record type.
    inline std::string topGroup(std::string_view recordType, std::string_view children)
    {
        std::string result("GRUP");
        append<std::uint32_t>(result, static_cast<std::uint32_t>(20 + children.size()));
        result.append(recordType);
        append<std::int32_t>(result, 0); // top level group
        append<std::uint16_t>(result, 0); // stamp
        append<std::uint16_t>(result, 0);
        result.append(children);
        return result;
    }

    /// Build the minimal TES4/HEDR record used to select Oblivion headers in the test reader.
    inline std::string header()
    {
        std::string hedr;
        append<float>(hedr, 0.8f);
        append<std::int32_t>(hedr, 0);
        append<std::uint32_t>(hedr, 0x800);
        return record("TES4", 0, subRecord("HEDR", hedr));
    }

    /// Load all records as T from a synthetic plugin containing one group, preserving file order. The last cutBytes
    /// bytes of the file are dropped, so the record and group headers still promise them.
    /// Loader and reader errors propagate to the caller.
    template <class T>
    std::vector<T> loadRecords(std::string_view group, const std::string& records, std::size_t cutBytes = 0)
    {
        std::string plugin = header() + topGroup(group, records);
        plugin.resize(plugin.size() - cutBytes);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        std::vector<T> result;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                result.emplace_back().load(r);
                return true;
            },
            [](ESM4::Reader&) {});
        return result;
    }
}

#endif
