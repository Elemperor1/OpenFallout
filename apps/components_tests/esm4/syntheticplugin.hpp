#ifndef OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM4_SYNTHETICPLUGIN_H
#define OPENFALLOUT_APPS_COMPONENTS_TESTS_ESM4_SYNTHETICPLUGIN_H

#include <components/esm4/common.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <zlib.h>

// Builds the bytes of small TES4-format plugins, so the loaders can be tested without any game data.
// The layout is Oblivion's: records have a 20 byte header, which ESM4::Reader detects from the TES4 record.
namespace ESM4Test
{
    template <class T>
    void append(std::string& out, T value)
    {
        out.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    inline std::string subRecord(std::string_view type, std::string_view data)
    {
        std::string result(type);
        append<std::uint16_t>(result, static_cast<std::uint16_t>(data.size()));
        result.append(data);
        return result;
    }

    // A sub-record that holds a null terminated string.
    inline std::string zString(std::string_view type, std::string_view text)
    {
        return subRecord(type, std::string(text) + '\0');
    }

    // A sub-record that holds one value.
    template <class T>
    std::string valueSubRecord(std::string_view type, T value)
    {
        std::string data;
        append(data, value);
        return subRecord(type, data);
    }

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

    inline std::string header()
    {
        std::string hedr;
        append<float>(hedr, 0.8f);
        append<std::int32_t>(hedr, 0);
        append<std::uint32_t>(hedr, 0x800);
        return record("TES4", 0, subRecord("HEDR", hedr));
    }
}

#endif
