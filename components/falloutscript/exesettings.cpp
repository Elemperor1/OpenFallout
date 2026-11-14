#include "exesettings.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <system_error>

#include <components/misc/strings/algorithm.hpp>
#include <components/toutf8/toutf8.hpp>

namespace FalloutScript::ExeSettings
{
    namespace
    {
        /// The part of a section that is in the file and in memory: where it is in memory (as an address, with the
        /// base of the image), where in the file, and how many bytes
        struct Section
        {
            std::uint64_t mAddress;
            std::size_t mOffset;
            std::size_t mLength;
        };

        struct Image
        {
            std::string_view mData;
            std::uint64_t mBase = 0;
            std::vector<Section> mSections;
        };

        constexpr std::size_t maxSections = 96;
        /// The longest name of a setting, and the longest text of one, that are taken (the names are short; the
        /// longest default text of the games is a few hundred characters)
        constexpr std::size_t maxName = 80;
        constexpr std::size_t maxText = 2048;
        /// The addresses in a record of the table that can be a text, a name or the functions of a setting: those of
        /// the image, which is a few tens of megabytes
        constexpr std::uint64_t addressRange = 0x4000000;

        std::uint32_t byteAt(std::string_view data, std::size_t at)
        {
            return static_cast<unsigned char>(data[at]);
        }

        /// The 16 or 32 bits at `at` (little endian), if they are in the data
        bool readU16(std::string_view data, std::size_t at, std::uint32_t& value)
        {
            if (at > data.size() || data.size() - at < 2)
                return false;
            value = byteAt(data, at) | (byteAt(data, at + 1) << 8);
            return true;
        }

        bool readU32(std::string_view data, std::size_t at, std::uint32_t& value)
        {
            if (at > data.size() || data.size() - at < 4)
                return false;
            value = byteAt(data, at) | (byteAt(data, at + 1) << 8) | (byteAt(data, at + 2) << 16)
                | (byteAt(data, at + 3) << 24);
            return true;
        }

        /// The sections of a 32-bit Windows executable (the games are all of that kind)
        bool parse(std::string_view data, Image& image)
        {
            std::uint32_t peOffset = 0;
            if (data.size() < 0x40 || data[0] != 'M' || data[1] != 'Z' || !readU32(data, 0x3c, peOffset))
                return false;
            const std::size_t pe = peOffset;
            std::uint32_t signature = 0;
            std::uint32_t count = 0;
            std::uint32_t optionalSize = 0;
            std::uint32_t magic = 0;
            std::uint32_t base = 0;
            if (!readU32(data, pe, signature) || signature != 0x00004550 || !readU16(data, pe + 6, count)
                || !readU16(data, pe + 20, optionalSize) || !readU16(data, pe + 24, magic) || magic != 0x10b
                || optionalSize < 32 || !readU32(data, pe + 24 + 28, base) || count > maxSections)
                return false;
            image.mData = data;
            image.mBase = base;
            const std::size_t table = pe + 24 + optionalSize;
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t at = table + i * 40;
                std::uint32_t virtualSize = 0;
                std::uint32_t virtualAddress = 0;
                std::uint32_t rawSize = 0;
                std::uint32_t rawOffset = 0;
                if (!readU32(data, at + 8, virtualSize) || !readU32(data, at + 12, virtualAddress)
                    || !readU32(data, at + 16, rawSize) || !readU32(data, at + 20, rawOffset))
                    return false;
                if (rawOffset >= data.size())
                    continue;
                const std::size_t inFile = std::min<std::size_t>({ virtualSize, rawSize, data.size() - rawOffset });
                image.mSections.push_back({ static_cast<std::uint64_t>(base) + virtualAddress, rawOffset, inFile });
            }
            return !image.mSections.empty();
        }

        /// The text that ends with a zero at the address, if it is inside a section and ends within `limit`
        bool textAt(const Image& image, std::uint64_t address, std::size_t limit, std::string_view& text)
        {
            for (const Section& section : image.mSections)
            {
                if (address < section.mAddress || address - section.mAddress >= section.mLength)
                    continue;
                const std::size_t offset = section.mOffset + static_cast<std::size_t>(address - section.mAddress);
                const std::size_t available = std::min(limit, image.mData.size() - offset);
                const std::size_t end = image.mData.substr(offset, available).find('\0');
                if (end == std::string_view::npos)
                    return false;
                text = image.mData.substr(offset, end);
                return true;
            }
            return false;
        }

        /// The name of a setting: an 's', a capital, then letters, digits and underscores
        bool isSettingName(std::string_view name)
        {
            if (name.size() < 2 || name[0] != 's' || name[1] < 'A' || name[1] > 'Z')
                return false;
            return std::all_of(name.begin() + 2, name.end(), [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
            });
        }

        std::string lowered(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(),
                [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
            return result;
        }

        void appendUtf8(std::string& out, std::uint32_t codePoint)
        {
            if (codePoint < 0x80)
                out += static_cast<char>(codePoint);
            else if (codePoint < 0x800)
            {
                out += static_cast<char>(0xC0 | (codePoint >> 6));
                out += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xE0 | (codePoint >> 12));
                out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
        }

        /// Windows-1252 to UTF-8 (the executables' texts are in the code page of Western Europe)
        std::string fromWindows1252(std::string_view text)
        {
            // the code points of the bytes 0x80 to 0x9F; the five bytes that have none stay as they are
            static constexpr std::array<std::uint16_t, 32> high
                = { 0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039,
                      0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                      0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178 };
            std::string result;
            result.reserve(text.size());
            for (const char c : text)
            {
                const std::uint32_t byte = static_cast<unsigned char>(c);
                appendUtf8(result, byte >= 0x80 && byte < 0xA0 ? high[byte - 0x80] : byte);
            }
            return result;
        }

        /// The text in UTF-8: in the code page that the player set for the data of the game when there is one (a
        /// localised executable has its words in the code page of its language), else in Windows-1252
        std::string toUtf8(std::string_view text, const ToUTF8::StatelessUtf8Encoder* encoder)
        {
            if (encoder == nullptr)
                return fromWindows1252(text);
            std::string buffer;
            return std::string(encoder->getUtf8(text, ToUTF8::BufferAllocationPolicy::FitToRequiredSize, buffer));
        }

        void add(Texts& texts, std::string_view name, std::string_view text, bool replace,
            const ToUTF8::StatelessUtf8Encoder* encoder)
        {
            if (replace)
                texts[lowered(name)] = toUtf8(text, encoder);
            else
                texts.emplace(lowered(name), toUtf8(text, encoder));
        }

        /// The code of a static initialiser: push the address of the text, push the address of the name, load the
        /// object into ecx (a static one: `B9 <object>`; one that was just made: `8B C8+n`) and call the constructor
        void scanCode(const Image& image, Texts& texts, const ToUTF8::StatelessUtf8Encoder* encoder)
        {
            for (const Section& section : image.mSections)
            {
                const std::string_view region = image.mData.substr(section.mOffset, section.mLength);
                for (std::size_t i = 0; i + 13 <= region.size(); ++i)
                {
                    if (byteAt(region, i) != 0x68 || byteAt(region, i + 5) != 0x68)
                        continue;
                    const std::uint32_t load = byteAt(region, i + 10);
                    bool call = false;
                    if (load == 0xB9)
                        call = i + 16 <= region.size() && byteAt(region, i + 15) == 0xE8;
                    else if (load == 0x8B)
                        call = byteAt(region, i + 11) >= 0xC8 && byteAt(region, i + 11) <= 0xCF
                            && byteAt(region, i + 12) == 0xE8;
                    if (!call)
                        continue;
                    std::uint32_t textAddress = 0;
                    std::uint32_t nameAddress = 0;
                    readU32(region, i + 1, textAddress);
                    readU32(region, i + 6, nameAddress);
                    std::string_view name;
                    std::string_view text;
                    if (textAt(image, nameAddress, maxName, name) && isSettingName(name)
                        && textAt(image, textAddress, maxText, text))
                        add(texts, name, text, true, encoder);
                }
            }
        }

        /// A static setting in the data: three addresses, those of the text, of the name and of the table of
        /// functions of the class
        void scanTables(const Image& image, Texts& texts, const ToUTF8::StatelessUtf8Encoder* encoder)
        {
            const std::uint64_t first = image.mBase;
            const std::uint64_t last = image.mBase + addressRange;
            for (const Section& section : image.mSections)
            {
                const std::string_view region = image.mData.substr(section.mOffset, section.mLength);
                for (std::size_t at = 0; at + 12 <= region.size(); at += 4)
                {
                    std::uint32_t textAddress = 0;
                    std::uint32_t nameAddress = 0;
                    std::uint32_t functions = 0;
                    readU32(region, at, textAddress);
                    readU32(region, at + 4, nameAddress);
                    readU32(region, at + 8, functions);
                    if (textAddress < first || textAddress >= last || nameAddress < first || nameAddress >= last
                        || functions < first || functions >= last)
                        continue;
                    std::string_view name;
                    std::string_view text;
                    if (textAt(image, nameAddress, maxName, name) && isSettingName(name)
                        && textAt(image, textAddress, maxText, text))
                        add(texts, name, text, false, encoder);
                }
            }
        }

        /// The counts of the general page are settings made in a loop: a function builds the name with the format
        /// `sMiscStat%03d` and the text is the one of the table that the loop reads (`mov r, [r*4 + table]`, the
        /// address of the text pushed to the function that builds the setting, the number of entries in the compare
        /// of the loop). Found by the format: the function that pushes it, its one caller, the table before the call.
        void scanCounted(const Image& image, Texts& texts, const ToUTF8::StatelessUtf8Encoder* encoder)
        {
            constexpr std::string_view format = "sMiscStat%03d";
            std::uint32_t formatAddress = 0;
            for (const Section& section : image.mSections)
            {
                const std::string_view region = image.mData.substr(section.mOffset, section.mLength);
                for (std::size_t at = region.find(format); at != std::string_view::npos;
                     at = region.find(format, at + 1))
                {
                    // the whole text, not the tail of a longer one
                    if (at + format.size() < region.size() && region[at + format.size()] == '\0')
                        formatAddress = static_cast<std::uint32_t>(section.mAddress + at);
                }
            }
            if (formatAddress == 0)
                return;
            const auto codeAt = [&](const Section& section, std::size_t at) {
                return static_cast<std::uint32_t>(section.mAddress + at);
            };
            std::uint32_t builder = 0;
            for (const Section& section : image.mSections)
            {
                const std::string_view region = image.mData.substr(section.mOffset, section.mLength);
                for (std::size_t i = 0; i + 5 <= region.size() && builder == 0; ++i)
                {
                    std::uint32_t pushed = 0;
                    if (byteAt(region, i) != 0x68 || !readU32(region, i + 1, pushed) || pushed != formatAddress)
                        continue;
                    // the start of the function: the padding of the one before it, then a prologue (the frame pointer one, or
                    // the one that pushes -1 for the frame of the exception handler, which Fallout 3 has here)
                    for (std::size_t back = i; back >= 3 && i - back < 0x200; --back)
                    {
                        const bool frame = byteAt(region, back) == 0x55 && byteAt(region, back + 1) == 0x8B
                            && byteAt(region, back + 2) == 0xEC;
                        const bool handler = byteAt(region, back) == 0x6A && byteAt(region, back + 1) == 0xFF;
                        if ((frame || handler) && byteAt(region, back - 1) == 0xCC)
                        {
                            builder = codeAt(section, back);
                            break;
                        }
                    }
                }
            }
            if (builder == 0)
                return;
            for (const Section& section : image.mSections)
            {
                const std::string_view region = image.mData.substr(section.mOffset, section.mLength);
                for (std::size_t i = 64; i + 5 <= region.size(); ++i)
                {
                    std::uint32_t relative = 0;
                    if (byteAt(region, i) != 0xE8 || !readU32(region, i + 1, relative)
                        || codeAt(section, i + 5) + relative != builder)
                        continue;
                    // the load of the text out of the table: mov r32, [index*4 + table]
                    std::uint32_t table = 0;
                    std::size_t load = 0;
                    for (std::size_t back = i; back + 64 > i && back >= 7; --back)
                        if (byteAt(region, back) == 0x8B && (byteAt(region, back + 1) & 0xC7) == 0x04
                            && (byteAt(region, back + 2) & 0xC7) == 0x85 && readU32(region, back + 3, table))
                        {
                            load = back;
                            break;
                        }
                    if (load == 0)
                        continue;
                    // the number of entries: the compare of the loop (before the load in one of the games, after the
                    // call in the other); the largest number compared with in that stretch, as the others are tests
                    // for zero
                    std::uint32_t count = 0;
                    const std::size_t from = load > 80 ? load - 80 : 0;
                    const std::size_t to = std::min(region.size() - 4, i + 40);
                    for (std::size_t at = from; at < to; ++at)
                    {
                        const std::uint32_t modrm = byteAt(region, at + 1);
                        if (byteAt(region, at) == 0x83 && (modrm & 0x38) == 0x38
                            && (modrm == 0x7D || (modrm & 0xC0) == 0xC0))
                            count = std::max(count, byteAt(region, at + (modrm == 0x7D ? 3 : 2)));
                    }
                    for (std::uint32_t number = 0; number < count && number < 256; ++number)
                    {
                        // the table is in the data: read the address of the text and the text
                        std::uint32_t textAddress = 0;
                        bool found = false;
                        for (const Section& data : image.mSections)
                        {
                            const std::uint64_t entry = table + std::uint64_t{ number } * 4;
                            if (entry < data.mAddress || entry + 4 > data.mAddress + data.mLength)
                                continue;
                            found = readU32(image.mData.substr(data.mOffset, data.mLength),
                                static_cast<std::size_t>(entry - data.mAddress), textAddress);
                        }
                        std::string_view text;
                        if (!found || !textAt(image, textAddress, maxText, text))
                            break;
                        char name[24];
                        std::snprintf(name, sizeof(name), "sMiscStat%03u", number);
                        add(texts, name, text, false, encoder);
                    }
                    return;
                }
            }
        }
    }

    Texts scan(std::string_view image, const ToUTF8::StatelessUtf8Encoder* encoder)
    {
        Texts texts;
        Image parsed;
        if (!parse(image, parsed))
            return texts;
        scanCode(parsed, texts, encoder);
        scanTables(parsed, texts, encoder);
        scanCounted(parsed, texts, encoder);
        return texts;
    }

    Texts read(const std::filesystem::path& path, const ToUTF8::StatelessUtf8Encoder* encoder)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return {};
        const std::streamoff size = file.tellg();
        // the executables of the games are a few tens of megabytes
        if (size <= 0 || size > 0x10000000)
            return {};
        std::string image(static_cast<std::size_t>(size), '\0');
        file.seekg(0);
        if (!file.read(image.data(), size))
            return {};
        return scan(image, encoder);
    }

    std::filesystem::path find(const std::vector<std::filesystem::path>& directories, std::string_view fileName)
    {
        for (std::filesystem::path directory : directories)
        {
            if (!directory.has_filename())
                directory = directory.parent_path();
            for (const std::filesystem::path& place : { directory, directory.parent_path() })
            {
                std::error_code error;
                std::filesystem::directory_iterator it(place, error);
                for (; !error && it != std::filesystem::directory_iterator(); it.increment(error))
                {
                    std::error_code fileError;
                    if (Misc::StringUtils::ciEqual(it->path().filename().string(), fileName)
                        && it->is_regular_file(fileError))
                        return it->path();
                }
            }
        }
        return {};
    }
}
