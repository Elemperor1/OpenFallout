#ifndef OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM4_SYNTHETICPLUGIN_H
#define OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM4_SYNTHETICPLUGIN_H

#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/toutf8/toutf8.hpp>

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

    /// Build a subrecord that is longer than a 16-bit size can say: a XXXX subrecord with the size, then a subrecord
    /// whose header says that it has no data, then the data.
    inline std::string extendedSubRecord(std::string_view type, std::string_view data)
    {
        std::string size;
        append<std::uint32_t>(size, static_cast<std::uint32_t>(data.size()));
        std::string result = subRecord("XXXX", size);
        result.append(subRecord(type, ""));
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

    /// Bytes that are all different, so that a field that holds them can be told from its neighbours.
    inline std::string bytePattern(std::size_t size, std::uint8_t first)
    {
        std::string result(size, '\0');
        for (std::size_t i = 0; i < size; ++i)
            result[i] = static_cast<char>(first + i);
        return result;
    }

    /// Bytes that are all different, with the form ID id at each of the offsets.
    inline std::string bytesWithFormIds(
        std::size_t size, std::uint8_t first, const std::vector<std::size_t>& offsets, std::uint32_t id)
    {
        std::string result = bytePattern(size, first);
        for (const std::size_t offset : offsets)
            result.replace(offset, sizeof(id), reinterpret_cast<const char*>(&id), sizeof(id));
        return result;
    }

    /// One entry of the data of an alternate texture subrecord (MODS, DMDS and the like).
    struct TextureEntry
    {
        std::string mName;
        std::uint32_t mTexture;
        std::int32_t mIndex;
    };

    /// The data of an alternate texture subrecord: a count, then for each entry the length of its name, the name, a
    /// texture and an index.
    inline std::string alternateTextureData(const std::vector<TextureEntry>& entries)
    {
        std::string data;
        append<std::uint32_t>(data, static_cast<std::uint32_t>(entries.size()));
        for (const TextureEntry& entry : entries)
        {
            append<std::uint32_t>(data, static_cast<std::uint32_t>(entry.mName.size()));
            data.append(entry.mName);
            append(data, entry.mTexture);
            append(data, entry.mIndex);
        }
        return data;
    }

    /// The data of a CTDA subrecord as Fallout 3 and New Vegas write it, 28 bytes.
    inline std::string conditionData(
        std::uint32_t type, float comparison, std::uint32_t function, std::uint32_t reference)
    {
        std::string data;
        append(data, type);
        append(data, comparison);
        append(data, function);
        append<std::uint32_t>(data, 7); // first parameter
        append<std::uint32_t>(data, 8); // second parameter
        append<std::uint32_t>(data, 1); // run on
        append(data, reference);
        return data;
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

    /// Wrap raw subrecords in the 24 byte record header of Fallout 3 and the games after it, which has a form version.
    inline std::string versionedRecord(
        std::string_view type, std::uint32_t id, std::string_view data, std::uint16_t formVersion)
    {
        std::string result(type);
        append<std::uint32_t>(result, static_cast<std::uint32_t>(data.size()));
        append<std::uint32_t>(result, 0); // flags
        append<std::uint32_t>(result, id);
        append<std::uint32_t>(result, 0); // revision
        append<std::uint16_t>(result, formVersion);
        append<std::uint16_t>(result, 0); // version control information
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
    /// bytes of the file are dropped, so the record and group headers still promise them. The engine and esmtool
    /// read plugins with an encoder, which reads strings by another path than a reader without one. The plugin has no
    /// masters and the load order index modIndex, which is what the form IDs of its records are adjusted to.
    /// Loader and reader errors propagate to the caller.
    template <class T>
    std::vector<T> loadRecords(std::string_view group, const std::string& records, std::size_t cutBytes = 0,
        const ToUTF8::StatelessUtf8Encoder* encoder = nullptr, std::uint32_t modIndex = 0)
    {
        std::string plugin = header() + topGroup(group, records);
        plugin.resize(plugin.size() - cutBytes);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, encoder);
        reader.setModIndex(modIndex);
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
