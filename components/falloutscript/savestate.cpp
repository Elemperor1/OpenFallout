#include "savestate.hpp"

#include <cstring>

namespace FalloutScript
{
    namespace
    {
        constexpr std::uint8_t dynamicPlugin = 0xFF;
    }

    void SaveWriter::u16(std::uint16_t value)
    {
        for (int i = 0; i < 2; ++i)
            mBytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }

    void SaveWriter::u32(std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            mBytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }

    void SaveWriter::u64(std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
            mBytes.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }

    void SaveWriter::f32(float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }

    void SaveWriter::f64(double value)
    {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }

    void SaveWriter::string(std::string_view value)
    {
        count(value.size());
        mBytes.append(value);
    }

    void SaveWriter::count(std::size_t size)
    {
        if (size > 0xFFFFFFFFu)
            throw SaveError("A list of the save has more entries than a save can hold");
        u32(static_cast<std::uint32_t>(size));
    }

    const char* SaveReader::take(std::size_t size)
    {
        if (size > remaining())
            throw SaveError("The save is cut short");
        const char* result = mBytes.data() + mOffset;
        mOffset += size;
        return result;
    }

    std::uint8_t SaveReader::u8()
    {
        return static_cast<std::uint8_t>(*take(1));
    }

    std::uint16_t SaveReader::u16()
    {
        const char* bytes = take(2);
        std::uint16_t value = 0;
        for (int i = 0; i < 2; ++i)
            value |= static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[i])) << (8 * i);
        return value;
    }

    std::uint32_t SaveReader::u32()
    {
        const char* bytes = take(4);
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[i])) << (8 * i);
        return value;
    }

    std::uint64_t SaveReader::u64()
    {
        const char* bytes = take(8);
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i)
            value |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(bytes[i])) << (8 * i);
        return value;
    }

    float SaveReader::f32()
    {
        const std::uint32_t bits = u32();
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    double SaveReader::f64()
    {
        const std::uint64_t bits = u64();
        double value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::string SaveReader::string()
    {
        const std::size_t size = count();
        return std::string(take(size), size);
    }

    std::optional<FormId> SaveReader::form()
    {
        const FormId saved = u32();
        if (!mMapping || saved == 0 || (saved >> 24) == dynamicPlugin)
            return saved;
        return mMapping(saved);
    }

    std::size_t SaveReader::count(std::size_t minimumBytes)
    {
        const std::size_t size = u32();
        if (minimumBytes != 0 && size > remaining() / minimumBytes)
            throw SaveError("The save names more entries than it has room for");
        return size;
    }
}
