#include <gtest/gtest.h>

#include <components/falloutscript/exesettings.hpp>
#include <components/toutf8/toutf8.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    using namespace FalloutScript;

    // A small Windows executable made in memory: a header, a code section at 0x1000 (file offset 0x400) and a data
    // section at 0x2000 (file offset 0x800), the image based at 0x400000, as the games' are
    constexpr std::uint32_t imageBase = 0x400000;
    constexpr std::size_t textFile = 0x400;
    constexpr std::size_t dataFile = 0x800;

    class Executable
    {
    public:
        Executable()
            : mBytes(0xC00, '\0')
        {
            mBytes[0] = 'M';
            mBytes[1] = 'Z';
            put32(0x3c, 0x80);
            put32(0x80, 0x00004550);
            put16(0x86, 2);
            put16(0x80 + 20, 0xE0);
            put16(0x80 + 24, 0x10b);
            put32(0x80 + 24 + 28, imageBase);
            section(0, 0x1000, textFile);
            section(1, 0x2000, dataFile);
        }

        void put16(std::size_t at, std::uint32_t value)
        {
            mBytes[at] = static_cast<char>(value & 0xFF);
            mBytes[at + 1] = static_cast<char>((value >> 8) & 0xFF);
        }

        void put32(std::size_t at, std::uint32_t value)
        {
            put16(at, value & 0xFFFF);
            put16(at + 2, value >> 16);
        }

        void put8(std::size_t at, std::uint32_t value) { mBytes[at] = static_cast<char>(value); }

        /// A zero-ended text in the data section; its address in memory
        std::uint32_t text(std::size_t offset, const std::string& value)
        {
            mBytes.replace(dataFile + offset, value.size(), value);
            return imageBase + 0x2000 + static_cast<std::uint32_t>(offset);
        }

        /// push text, push name, mov ecx object, call (the object is a static one)
        void staticInitialiser(std::size_t offset, std::uint32_t textAddress, std::uint32_t nameAddress)
        {
            const std::size_t at = textFile + offset;
            put8(at, 0x68);
            put32(at + 1, textAddress);
            put8(at + 5, 0x68);
            put32(at + 6, nameAddress);
            put8(at + 10, 0xB9);
            put32(at + 11, imageBase + 0x3000);
            put8(at + 15, 0xE8);
        }

        /// the same with the object in eax, moved to ecx
        void movedInitialiser(std::size_t offset, std::uint32_t textAddress, std::uint32_t nameAddress)
        {
            const std::size_t at = textFile + offset;
            put8(at, 0x68);
            put32(at + 1, textAddress);
            put8(at + 5, 0x68);
            put32(at + 6, nameAddress);
            put8(at + 10, 0x8B);
            put8(at + 11, 0xCE);
            put8(at + 12, 0xE8);
        }

        /// a record of the table in the data section: text, name, table of functions
        void record(std::size_t offset, std::uint32_t textAddress, std::uint32_t nameAddress)
        {
            const std::size_t at = dataFile + offset;
            put32(at, textAddress);
            put32(at + 4, nameAddress);
            put32(at + 8, imageBase + 0x1000);
        }

        const std::string& bytes() const { return mBytes; }

    private:
        void section(std::size_t index, std::uint32_t address, std::size_t fileOffset)
        {
            const std::size_t at = 0x80 + 24 + 0xE0 + index * 40;
            put32(at + 8, 0x400);
            put32(at + 12, address);
            put32(at + 16, 0x400);
            put32(at + 20, static_cast<std::uint32_t>(fileOffset));
        }

        std::string mBytes;
    };

    TEST(FalloutExeSettings, a_setting_made_by_a_static_initialiser_is_found_with_its_text)
    {
        Executable exe;
        exe.staticInitialiser(0x20, exe.text(0x10, "First text"), exe.text(0x40, "sFirstThing"));
        exe.movedInitialiser(0x60, exe.text(0x80, "Second text"), exe.text(0xA0, "sSecondThing"));
        const ExeSettings::Texts texts = ExeSettings::scan(exe.bytes());
        ASSERT_EQ(texts.size(), 2u);
        // the names are in lower case
        EXPECT_EQ(texts.at("sfirstthing"), "First text");
        EXPECT_EQ(texts.at("ssecondthing"), "Second text");
    }

    /// An executable with the loop that builds the counts of the general page: the function that builds a setting pushes the
    /// format, the loop loads the text out of a table and calls it (the start of that function is the one with the frame
    /// pointer, or, as in Fallout 3, the one that pushes -1 for the exception handler)
    std::string countedExecutable(bool handlerFrame)
    {
        Executable exe;
        const std::uint32_t format = exe.text(0x10, "sMiscStat%03d");
        const std::size_t builder = textFile + 0x11;
        exe.put8(textFile + 0x10, 0xCC);
        if (handlerFrame)
        {
            exe.put8(builder, 0x6A);
            exe.put8(builder + 1, 0xFF);
            exe.put8(builder + 2, 0x90);
        }
        else
        {
            exe.put8(builder, 0x55);
            exe.put8(builder + 1, 0x8B);
            exe.put8(builder + 2, 0xEC);
        }
        exe.put8(builder + 3, 0x68);
        exe.put32(builder + 4, format);
        const std::uint32_t table = imageBase + 0x2000 + 0x100;
        const std::size_t loop = textFile + 0x80;
        exe.put8(loop, 0x83);
        exe.put8(loop + 1, 0x7D);
        exe.put8(loop + 2, 0xF0);
        exe.put8(loop + 3, 3);
        exe.put8(loop + 4, 0x8B);
        exe.put8(loop + 5, 0x04);
        exe.put8(loop + 6, 0x95);
        exe.put32(loop + 7, table);
        exe.put8(loop + 11, 0xE8);
        const std::uint32_t next = imageBase + 0x1000 + 0x80 + 16;
        exe.put32(loop + 12, imageBase + 0x1000 + 0x11 - next);
        exe.put32(0x800 + 0x100, exe.text(0x40, "First count"));
        exe.put32(0x800 + 0x104, exe.text(0x60, "Second count"));
        exe.put32(0x800 + 0x108, exe.text(0x80, "Third count"));
        return exe.bytes();
    }

    TEST(FalloutExeSettings, the_counts_of_the_general_page_are_read_from_the_table_the_loop_reads)
    {
        const ExeSettings::Texts texts = ExeSettings::scan(countedExecutable(false));
        ASSERT_EQ(texts.size(), 3u);
        EXPECT_EQ(texts.at("smiscstat000"), "First count");
        EXPECT_EQ(texts.at("smiscstat001"), "Second count");
        EXPECT_EQ(texts.at("smiscstat002"), "Third count");
    }

    TEST(FalloutExeSettings, the_counts_are_found_when_the_builder_starts_with_the_exception_handler_frame)
    {
        // (Fallout 3: the function at 0x48C610 begins with push -1, not with the frame pointer)
        const ExeSettings::Texts texts = ExeSettings::scan(countedExecutable(true));
        ASSERT_EQ(texts.size(), 3u);
        EXPECT_EQ(texts.at("smiscstat002"), "Third count");
    }

    TEST(FalloutExeSettings, a_setting_in_a_table_of_records_is_found)
    {
        Executable exe;
        exe.record(0x200, exe.text(0x10, "Table text"), exe.text(0x40, "sTableThing"));
        const ExeSettings::Texts texts = ExeSettings::scan(exe.bytes());
        ASSERT_EQ(texts.size(), 1u);
        EXPECT_EQ(texts.at("stablething"), "Table text");
    }

    TEST(FalloutExeSettings, the_code_takes_the_place_of_the_table_for_a_setting_in_both)
    {
        Executable exe;
        const std::uint32_t name = exe.text(0x40, "sBoth");
        exe.staticInitialiser(0x20, exe.text(0x10, "From the code"), name);
        exe.record(0x200, exe.text(0x80, "From the table"), name);
        EXPECT_EQ(ExeSettings::scan(exe.bytes()).at("sboth"), "From the code");
    }

    TEST(FalloutExeSettings, only_names_of_settings_with_texts_are_taken)
    {
        Executable exe;
        const std::uint32_t text = exe.text(0x10, "A text");
        // a name that is not a setting's (no 's' and a capital), one with a character a name does not have
        exe.staticInitialiser(0x20, text, exe.text(0x40, "fSomeFloat"));
        exe.staticInitialiser(0x40, text, exe.text(0x60, "sbad"));
        exe.staticInitialiser(0x60, text, exe.text(0x80, "sBad Name"));
        // a text outside the sections, a name outside the sections
        exe.staticInitialiser(0x80, 0x10, exe.text(0xA0, "sNoText"));
        exe.staticInitialiser(0xA0, text, 0x20);
        EXPECT_TRUE(ExeSettings::scan(exe.bytes()).empty());
    }

    TEST(FalloutExeSettings, the_texts_are_made_utf_8_from_windows_1252)
    {
        Executable exe;
        // the trade mark sign (0x99) and an e with an acute accent (0xE9), and a plain letter
        exe.staticInitialiser(0x20, exe.text(0x10, "A\x99 caf\xE9"), exe.text(0x40, "sCoded"));
        EXPECT_EQ(ExeSettings::scan(exe.bytes()).at("scoded"), "A\xE2\x84\xA2 caf\xC3\xA9");
    }

    TEST(FalloutExeSettings, the_texts_of_a_localised_executable_are_made_utf_8_from_the_code_page_that_is_set)
    {
        Executable exe;
        // two Cyrillic capitals (0xC0 and 0xC1 in Windows-1251) and a plain letter
        exe.staticInitialiser(0x20, exe.text(0x10, "\xC0\xC1x"), exe.text(0x40, "sCoded"));
        const ToUTF8::StatelessUtf8Encoder cyrillic(ToUTF8::WINDOWS_1251);
        EXPECT_EQ(ExeSettings::scan(exe.bytes(), &cyrillic).at("scoded"), "\xD0\x90\xD0\x91x");
        // without a code page the bytes are read as Windows-1252
        EXPECT_EQ(ExeSettings::scan(exe.bytes()).at("scoded"), "\xC3\x80\xC3\x81x");
    }

    TEST(FalloutExeSettings, a_file_that_is_not_an_executable_gives_nothing)
    {
        EXPECT_TRUE(ExeSettings::scan("").empty());
        EXPECT_TRUE(ExeSettings::scan("MZ").empty());
        EXPECT_TRUE(ExeSettings::scan(std::string(0x1000, 'x')).empty());
        EXPECT_TRUE(ExeSettings::scan(std::string(0x1000, '\xFF')).empty());
        // a header of a 64-bit file (the games are 32-bit)
        Executable exe;
        exe.put16(0x80 + 24, 0x20b);
        EXPECT_TRUE(ExeSettings::scan(exe.bytes()).empty());
    }

    TEST(FalloutExeSettings, a_damaged_file_is_read_without_going_outside_it)
    {
        Executable exe;
        exe.staticInitialiser(0x20, exe.text(0x10, "First text"), exe.text(0x40, "sFirstThing"));
        exe.record(0x200, exe.text(0x80, "Table text"), exe.text(0xA0, "sTableThing"));
        const std::string& whole = exe.bytes();
        // cut at every length, and with the section table pointing out of the file
        for (std::size_t length = 0; length <= whole.size(); length += 5)
            ExeSettings::scan(std::string_view(whole).substr(0, length));
        Executable beyond;
        beyond.put32(0x80 + 24 + 0xE0 + 20, 0xFFFFFF00u);
        beyond.put32(0x80 + 24 + 0xE0 + 40 + 20, 0xFFFFFFFFu);
        EXPECT_TRUE(ExeSettings::scan(beyond.bytes()).empty());
        Executable huge;
        huge.put32(0x80 + 24 + 0xE0 + 8, 0xFFFFFFFFu);
        huge.put32(0x80 + 24 + 0xE0 + 16, 0xFFFFFFFFu);
        huge.staticInitialiser(0x20, huge.text(0x10, "Text"), huge.text(0x40, "sHuge"));
        // the section is taken as far as the file goes
        EXPECT_EQ(ExeSettings::scan(huge.bytes()).count("shuge"), 1u);
    }

    TEST(FalloutExeSettings, the_executable_of_a_game_is_found_next_to_its_data_directory_whatever_its_case)
    {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "openfallout_exe_settings_test";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "Game" / "Data");
        {
            std::ofstream(root / "Game" / "FALLOUTNV.EXE") << "x";
        }
        const std::vector<std::filesystem::path> data = { root / "Elsewhere", root / "Game" / "Data" };
        EXPECT_EQ(ExeSettings::find(data, "FalloutNV.exe"), root / "Game" / "FALLOUTNV.EXE");
        EXPECT_TRUE(ExeSettings::find(data, "Fallout3.exe").empty());
        // a directory with a slash at its end is the same directory
        EXPECT_EQ(ExeSettings::find({ root / "Game" / "Data" / "" }, "falloutnv.exe"), root / "Game" / "FALLOUTNV.EXE");
        std::filesystem::remove_all(root);
    }

    TEST(FalloutExeSettings, a_file_that_is_not_there_gives_nothing)
    {
        EXPECT_TRUE(ExeSettings::read(std::filesystem::temp_directory_path() / "openfallout_no_such_file.exe").empty());
    }
}
