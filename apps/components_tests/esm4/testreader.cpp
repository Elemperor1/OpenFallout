#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/testing/util.hpp>
#include <components/toutf8/toutf8.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string pluginHeader(std::uint32_t flags, const std::vector<std::string>& masters = {})
    {
        std::string hedr;
        append<float>(hedr, 0.8f);
        append<std::int32_t>(hedr, 0);
        append<std::uint32_t>(hedr, 0x800);
        std::string data = subRecord("HEDR", hedr);
        for (const std::string& master : masters)
            data += zString("MAST", master) + valueSubRecord<std::uint64_t>("DATA", 0);
        return record("TES4", 0, data, flags);
    }

    std::string groupHeader(std::uint32_t size, std::uint32_t label, std::int32_t type)
    {
        std::string result("GRUP");
        append(result, size);
        append(result, label);
        append(result, type);
        append<std::uint16_t>(result, 0);
        append<std::uint16_t>(result, 0);
        return result;
    }

    std::unique_ptr<std::istream> stream(const std::string& bytes)
    {
        return std::make_unique<std::istringstream>(bytes);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Localized strings

    struct StringEntry
    {
        std::uint32_t mId;
        std::uint32_t mOffset;
    };

    /// A strings file: the number of entries, the size of the data, the entries and the data.
    std::string stringsFile(const std::vector<StringEntry>& entries, const std::string& data)
    {
        std::string result;
        append<std::uint32_t>(result, static_cast<std::uint32_t>(entries.size()));
        append<std::uint32_t>(result, static_cast<std::uint32_t>(data.size()));
        for (const StringEntry& entry : entries)
        {
            append(result, entry.mId);
            append(result, entry.mOffset);
        }
        return result + data;
    }

    std::string nullTerminated(const std::string& text)
    {
        return text + '\0';
    }

    /// The string of an ILSTRINGS and DLSTRINGS file: the length including the terminator, the text and the terminator
    std::string lengthPrefixed(const std::string& text)
    {
        std::string result;
        append<std::uint32_t>(result, static_cast<std::uint32_t>(text.size() + 1));
        return result + nullTerminated(text);
    }

    /// A plugin that is a master and has localized strings, with a cell for each of the string ids. The cells have the
    /// ids 0x801 and up.
    std::string localizedPlugin(const std::vector<std::uint32_t>& stringIds)
    {
        std::string cells;
        std::uint32_t formId = 0x801;
        for (const std::uint32_t id : stringIds)
            cells += record("CELL", formId++, valueSubRecord<std::uint32_t>("FULL", id));
        return pluginHeader(ESM4::Rec_ESM | ESM4::Rec_Localized) + topGroup("CELL", cells);
    }

    std::vector<std::string> loadNames(const std::string& plugin, const std::filesystem::path& file,
        const VFS::Manager* vfs, bool ignoreMissing = false)
    {
        ESM4::Reader reader(stream(plugin), file, vfs, nullptr, ignoreMissing);
        reader.setModIndex(0);
        std::vector<std::string> names;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                ESM4::Cell cell;
                cell.load(r);
                names.push_back(cell.mFullName);
                return true;
            },
            [](ESM4::Reader&) {});
        return names;
    }

    // The strings start with an empty one: the loader takes the string at the offset 0 for a copy of the string before
    // it, which is none, so that string is read as empty
    const std::string sPlainData = nullTerminated("") + nullTerminated("Hello") + nullTerminated("World")
        + std::string(3, '\x7F') + nullTerminated("After a gap") + std::string(5000, '\x7F')
        + nullTerminated("After a long gap");
    const std::vector<StringEntry> sPlainEntries = {
        { 0x01, 1 }, // Hello
        { 0x02, 7 }, // World
        { 0x03, 7 }, // the same string as 2
        { 0x04, 16 }, // After a gap
        { 0x05, 16 + 12 + 5000 }, // After a long gap
    };

    TEST(ESM4ReaderStringsTest, aLocalizedPluginReadsItsStringsFromTheVirtualFileSystem)
    {
        const std::string plain = stringsFile(sPlainEntries, sPlainData);
        const std::string il = stringsFile({ { 0x0F, 0 }, { 0x10, 5 }, { 0x11, 5 + 11 } },
            lengthPrefixed("") + lengthPrefixed("Il one") + lengthPrefixed("Il two"));
        const std::string dl = stringsFile({ { 0x1F, 0 }, { 0x20, 5 } }, lengthPrefixed("") + lengthPrefixed("Dl one"));
        TestingOpenMW::VFSTestFile plainFile(plain);
        TestingOpenMW::VFSTestFile ilFile(il);
        TestingOpenMW::VFSTestFile dlFile(dl);
        const auto vfs = TestingOpenMW::createTestVFS({
            { VFS::Path::NormalizedView("strings/localmod_en.strings"), &plainFile },
            { VFS::Path::NormalizedView("strings/localmod_en.ilstrings"), &ilFile },
            { VFS::Path::NormalizedView("strings/localmod_en.dlstrings"), &dlFile },
        });

        const std::vector<std::string> names
            = loadNames(localizedPlugin({ 0x01, 0x02, 0x03, 0x04, 0x05, 0x10, 0x11, 0x20 }), "LocalMod.esm", vfs.get());

        EXPECT_THAT(names,
            ElementsAre("Hello", "World", "World", "After a gap", "After a long gap", "Il one", "Il two", "Dl one"));
    }

    TEST(ESM4ReaderStringsTest, theStringIdZeroIsNoString)
    {
        TestingOpenMW::VFSTestFile plainFile(stringsFile(sPlainEntries, sPlainData));
        const auto vfs = TestingOpenMW::createTestVFS(
            { { VFS::Path::NormalizedView("strings/localmod_en.strings"), &plainFile } });

        EXPECT_THAT(loadNames(localizedPlugin({ 0 }), "LocalMod.esm", vfs.get()), ElementsAre(""));
    }

    TEST(ESM4ReaderStringsTest, aStringsFileMayBeNamedAfterTheWholeNameOfTheLanguage)
    {
        TestingOpenMW::VFSTestFile plainFile(stringsFile(sPlainEntries, sPlainData));
        const auto vfs = TestingOpenMW::createTestVFS(
            { { VFS::Path::NormalizedView("strings/localmod_english.strings"), &plainFile } });

        EXPECT_THAT(loadNames(localizedPlugin({ 0x02 }), "LocalMod.esm", vfs.get()), ElementsAre("World"));
    }

    TEST(ESM4ReaderStringsTest, aStringThatNoStringsFileHasIsAnErrorUnlessThePluginIsToldToIgnoreIt)
    {
        const auto vfs = TestingOpenMW::createTestVFS({});

        EXPECT_THAT([&] { loadNames(localizedPlugin({ 0x02 }), "LocalMod.esm", vfs.get()); },
            ThrowsMessage<std::runtime_error>(HasSubstr("localized string not found")));
        EXPECT_THAT(loadNames(localizedPlugin({ 0x02 }), "LocalMod.esm", vfs.get(), true), ElementsAre(""));
    }

    TEST(ESM4ReaderStringsTest, aLocalizedPluginReadsItsStringsFromItsStringsDirectory)
    {
        const std::filesystem::path dir = TestingOpenMW::currentTestDirPath();
        std::filesystem::create_directories(dir / "Strings");
        std::ofstream(dir / "Strings" / "LocalMod_En.STRINGS", std::ios::binary)
            << stringsFile(sPlainEntries, sPlainData);
        std::ofstream(dir / "Strings" / "LocalMod_En.DLSTRINGS", std::ios::binary)
            << stringsFile({ { 0x1F, 0 }, { 0x20, 5 } }, lengthPrefixed("") + lengthPrefixed("Dl one"));

        EXPECT_THAT(loadNames(localizedPlugin({ 0x01, 0x04, 0x20 }), dir / "LocalMod.esm", nullptr),
            ElementsAre("Hello", "After a gap", "Dl one"));
    }

    TEST(ESM4ReaderStringsTest, theStringsDirectoryMayNameTheLanguageInFull)
    {
        const std::filesystem::path dir = TestingOpenMW::currentTestDirPath();
        std::filesystem::create_directories(dir / "Strings");
        std::ofstream(dir / "Strings" / "LocalMod_English.STRINGS", std::ios::binary)
            << stringsFile(sPlainEntries, sPlainData);

        EXPECT_THAT(loadNames(localizedPlugin({ 0x02 }), dir / "LocalMod.esm", nullptr), ElementsAre("World"));
    }

    TEST(ESM4ReaderStringsTest, aMissingStringsDirectoryIsAnErrorUnlessThePluginIsToldToIgnoreIt)
    {
        const std::filesystem::path dir = TestingOpenMW::currentTestDirPath();

        EXPECT_THAT([&] { loadNames(localizedPlugin({ 0x02 }), dir / "LocalMod.esm", nullptr); },
            ThrowsMessage<std::runtime_error>(HasSubstr("localized string not found")));
        EXPECT_THAT(loadNames(localizedPlugin({ 0x02 }), dir / "LocalMod.esm", nullptr, true), ElementsAre(""));
    }

    TEST(ESM4ReaderStringsTest, aPluginThatIsNotLocalizedHasItsStringsInItsRecords)
    {
        const std::string cell = record("CELL", 0x801, zString("FULL", "Plain name"));
        const std::string plugin = pluginHeader(ESM4::Rec_ESM) + topGroup("CELL", cell);

        EXPECT_THAT(loadNames(plugin, "Plain.esm", nullptr), ElementsAre("Plain name"));
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Files, contexts and groups

    TEST(ESM4ReaderTest, aFileThatDoesNotStartWithAPluginHeaderIsRefused)
    {
        const std::string bytes = record("CELL", 0x801, zString("EDID", "Cell")) + std::string(40, '\0');

        EXPECT_THAT([&] { ESM4::Reader reader(stream(bytes), "NoHeader.esp", nullptr, nullptr); },
            ThrowsMessage<std::runtime_error>(HasSubstr("Unknown file format")));
    }

    TEST(ESM4ReaderTest, openingAnotherFileIsNotSupported)
    {
        ESM4::Reader reader(stream(pluginHeader(0)), "Plugin.esp", nullptr, nullptr);

        const std::filesystem::path small = TestingOpenMW::outputFilePath("test_esm4_reader_small.esp");
        std::ofstream(small, std::ios::binary) << std::string(8, '\0');
        EXPECT_THAT([&] { reader.open(small); }, ThrowsMessage<std::runtime_error>(HasSubstr("File too small")));

        const std::filesystem::path plugin = TestingOpenMW::outputFilePath("test_esm4_reader_open.esp");
        std::ofstream(plugin, std::ios::binary) << pluginHeader(0);
        EXPECT_THAT([&] { reader.open(plugin); }, ThrowsMessage<std::runtime_error>(HasSubstr("Unknown file format")));

        const std::filesystem::path zeros = TestingOpenMW::outputFilePath("test_esm4_reader_zeros.esp");
        std::ofstream(zeros, std::ios::binary) << std::string(40, '\0');
        EXPECT_THAT([&] { reader.open(zeros); }, ThrowsMessage<std::runtime_error>(HasSubstr("Unknown file format")));
    }

    TEST(ESM4ReaderTest, theRecordHeaderSizeCanBeSet)
    {
        ESM4::Reader reader(stream(pluginHeader(0)), "Plugin.esp", nullptr, nullptr);
        EXPECT_EQ(reader.hdr().record.typeId, ESM4::REC_TES4);

        reader.setRecHeaderSize(24);
        EXPECT_TRUE(reader.hasFormVersion());
        reader.setRecHeaderSize(20);
        EXPECT_FALSE(reader.hasFormVersion());
    }

    TEST(ESM4ReaderTest, aRecordCanBeReadAgainFromASavedContext)
    {
        const std::string plugin = pluginHeader(0) + record("REFR", 0x801, zString("EDID", "First"))
            + record("REFR", 0x802, zString("EDID", "Second")) + record("REFR", 0x803, zString("EDID", "Third"));
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);
        reader.setModIndex(0);

        ASSERT_TRUE(reader.getRecordHeader());
        reader.skipRecordData();
        ASSERT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.getFormId().toUint32(), 0x802u);
        const ESM4::ReaderContext second = reader.getContext();
        reader.skipRecordData();
        ASSERT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.getFormId().toUint32(), 0x803u);

        ASSERT_TRUE(reader.restoreContext(second));
        EXPECT_EQ(reader.hdr().record.getFormId().toUint32(), 0x802u);
        reader.getRecordData();
        ESM4::Reference cell;
        cell.load(reader);
        EXPECT_EQ(cell.mEditorId, "Second");
        // The reader goes on from there
        ASSERT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.getFormId().toUint32(), 0x803u);
    }

    TEST(ESM4ReaderTest, aContextCanBeRestoredWhileTheDataOfACompressedRecordIsOpen)
    {
        const std::string plugin = pluginHeader(0) + compressedRecord("REFR", 0x801, zString("EDID", "Packed"));
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);
        reader.setModIndex(0);

        ASSERT_TRUE(reader.getRecordHeader());
        const ESM4::ReaderContext context = reader.getContext();
        reader.getRecordData();

        // The file stream was put aside for the inflated data, and has to be put back
        ASSERT_TRUE(reader.restoreContext(context));
        reader.getRecordData();
        ESM4::Reference cell;
        cell.load(reader);
        EXPECT_EQ(cell.mEditorId, "Packed");
    }

    TEST(ESM4ReaderTest, theContextAtTheEndOfTheFileIsTheOneThatIsKept)
    {
        ESM4::Reader reader(stream(pluginHeader(0)), "Plugin.esp", nullptr, nullptr);

        // Reading after the last record leaves the stream in a failed state, which has no position
        EXPECT_FALSE(reader.getRecordHeader());
        const ESM4::ReaderContext context = reader.getContext();
        EXPECT_EQ(context.filePos, std::streampos(-1));
    }

    TEST(ESM4ReaderTest, theGroupsOfAFileAreAStackThatCanBeLookedInto)
    {
        const std::string plugin = pluginHeader(0) + topGroup("CELL", record("CELL", 0x801, zString("EDID", "Inner")));
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);

        EXPECT_THAT([&] { reader.grp(); }, ThrowsMessage<std::runtime_error>(HasSubstr("is zero")));

        ASSERT_TRUE(reader.getRecordHeader());
        reader.enterGroup();
        EXPECT_EQ(reader.grp().type, ESM4::Grp_RecordType);
        EXPECT_THAT([&] { reader.grp(1); }, ThrowsMessage<std::runtime_error>(HasSubstr("exceeded stack depth")));
    }

    TEST(ESM4ReaderTest, anEmptyGroupAmongTheRecordsOfAGroupIsPassedOver)
    {
        const std::string empty = groupHeader(20, 0, ESM4::Grp_CellChild);
        const std::string cell = record("CELL", 0x801, zString("EDID", "After empty"));
        const std::string plugin = pluginHeader(0)
            + groupHeader(static_cast<std::uint32_t>(20 + empty.size() + cell.size()), 0x43454C4C, 0) + empty + cell;
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);
        reader.setModIndex(0);

        std::vector<std::string> editorIds;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                ESM4::Cell loaded;
                loaded.load(r);
                editorIds.push_back(loaded.mEditorId);
                return true;
            },
            [](ESM4::Reader&) {});

        EXPECT_THAT(editorIds, ElementsAre("After empty"));
    }

    TEST(ESM4ReaderTest, anEmptyGroupAtTheTopOfAFileIsPassedOver)
    {
        const std::string cell = record("CELL", 0x801, zString("EDID", "After empty"));
        const std::string plugin = pluginHeader(0) + groupHeader(20, 0x4E554C4C, 0) + topGroup("CELL", cell);
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);
        reader.setModIndex(0);

        std::vector<std::string> editorIds;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                ESM4::Cell loaded;
                loaded.load(r);
                editorIds.push_back(loaded.mEditorId);
                return true;
            },
            [](ESM4::Reader&) {});

        EXPECT_THAT(editorIds, ElementsAre("After empty"));
    }

    TEST(ESM4ReaderTest, aGroupOfAnUnknownTypeIsSkipped)
    {
        const std::string skipped = record("CELL", 0x801, zString("EDID", "Skipped"));
        const std::string cell = record("CELL", 0x802, zString("EDID", "Read"));
        const std::string plugin = pluginHeader(0) + groupHeader(static_cast<std::uint32_t>(20 + skipped.size()), 0, 99)
            + skipped + topGroup("CELL", cell);
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);
        reader.setModIndex(0);

        std::vector<std::string> editorIds;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                ESM4::Cell loaded;
                loaded.load(r);
                editorIds.push_back(loaded.mEditorId);
                return true;
            },
            [](ESM4::Reader&) {});

        EXPECT_THAT(editorIds, ElementsAre("Read"));
    }

    TEST(ESM4ReaderTest, theRestOfAGroupCanBeSkipped)
    {
        const std::string first = record("CELL", 0x801, zString("EDID", "First"));
        const std::string second = record("CELL", 0x802, zString("EDID", "Second"));
        const std::string plugin = pluginHeader(0) + topGroup("CELL", first + second);
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);

        EXPECT_THAT([&] { reader.skipGroupData(); }, ThrowsMessage<std::runtime_error>(HasSubstr("empty stack")));

        ASSERT_TRUE(reader.getRecordHeader());
        reader.enterGroup();
        ASSERT_TRUE(reader.getRecordHeader());
        reader.skipRecordData();
        reader.skipGroupData();
        reader.exitGroupCheck();

        EXPECT_FALSE(reader.getRecordHeader());
    }

    TEST(ESM4ReaderTest, aGroupThatIsLargerThanTheGroupThatHoldsItIsAnError)
    {
        // The outer group has room for the header of the inner group and a record header, but the inner group says that
        // it has more
        const std::string record1 = record("CELL", 0x801, "");
        const std::string inner
            = groupHeader(static_cast<std::uint32_t>(20 + 20 + record1.size()), 0, ESM4::Grp_CellChild);
        const std::string plugin = pluginHeader(0)
            + groupHeader(static_cast<std::uint32_t>(20 + inner.size()), 0x43454C4C, 0) + inner + record1 + record1
            + record1;
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);
        reader.setModIndex(0);

        EXPECT_THAT(
            [&] {
                ESM4::ReaderUtils::readAll(
                    reader,
                    [&](ESM4::Reader& r) {
                        r.skipRecordData();
                        return true;
                    },
                    [](ESM4::Reader&) {});
            },
            ThrowsMessage<std::runtime_error>(HasSubstr("Read more records than available")));
    }

    TEST(ESM4ReaderTest, aCellGridIsOnlyAvailableOnceItIsSet)
    {
        ESM4::Reader reader(stream(pluginHeader(0)), "Plugin.esp", nullptr, nullptr);

        EXPECT_FALSE(reader.hasCellGrid());
        EXPECT_THAT([&] { reader.currCellGrid(); }, ThrowsMessage<std::runtime_error>(HasSubstr("invalid cell grid")));

        reader.setCurrCellGrid(ESM4::Grid{ 3, -4 });
        EXPECT_TRUE(reader.hasCellGrid());
        EXPECT_EQ(std::get<ESM4::Grid>(reader.currCellGrid()).x, 3);
    }

    TEST(ESM4ReaderTest, theMastersOfAPluginHaveToBeInTheLoadOrder)
    {
        ESM4::Reader reader(stream(pluginHeader(0, { "Master.esm", "Other.esm" })), "Plugin.esp", nullptr, nullptr);

        EXPECT_NO_THROW(reader.updateModIndices({ { "master.esm", 0 }, { "other.esm", 1 } }));
        EXPECT_THAT(
            [&] {
                reader.updateModIndices({ { "master.esm", 0 } });
            },
            ThrowsMessage<std::runtime_error>(HasSubstr("'Other.esm' not found")));
    }

    TEST(ESM4ReaderTest, aGroupLabelIsPrintedByItsType)
    {
        ESM4::GroupLabel label{};
        std::memcpy(label.recordType, "NPC_", 4);
        EXPECT_EQ(ESM4::printLabel(label, ESM4::Grp_RecordType), "Record Type: NPC_");

        label.grid[0] = 7; // y
        label.grid[1] = -3; // x
        EXPECT_EQ(ESM4::printLabel(label, ESM4::Grp_ExteriorCell), "Exterior Cell: grid (x, y) -3, 7");
        EXPECT_EQ(ESM4::printLabel(label, ESM4::Grp_ExteriorSubCell), "Exterior Sub Cell: grid (x, y) -3, 7");

        label.value = 0x1A;
        EXPECT_EQ(ESM4::printLabel(label, ESM4::Grp_InteriorCell), "Interior Cell: block 0x1a");
        EXPECT_EQ(ESM4::printLabel(label, ESM4::Grp_InteriorSubCell), "Interior Sub Cell: block 0x1a");
        EXPECT_THAT(ESM4::printLabel(label, ESM4::Grp_WorldChild), StartsWith("World Child: "));
        EXPECT_THAT(ESM4::printLabel(label, ESM4::Grp_CellChild), StartsWith("Cell Child: "));
        EXPECT_EQ(ESM4::printLabel(label, 99), "Unknown");
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Strings and records

    TEST(ESM4ReaderTest, aStringThatIsNotEndedByANullIsAnError)
    {
        // The size says that the last character is the terminator
        EXPECT_THAT([] { loadRecords<ESM4::Cell>("CELL", record("CELL", 0x801, subRecord("EDID", "AB"))); },
            ThrowsMessage<std::runtime_error>(HasSubstr("not terminated with a null")));
    }

    TEST(ESM4ReaderTest, aListOfStringsIsReadWithAnEncoderToo)
    {
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        std::string names("one.nif");
        names.push_back('\0');
        names += "two.nif";
        names.push_back('\0');
        names += "three.nif";
        names.push_back('\0');

        const std::vector<ESM4::Creature> withEncoder
            = loadRecords<ESM4::Creature>("CREA", record("CREA", 0x801, subRecord("NIFZ", names)), 0, &encoder);
        const std::vector<ESM4::Creature> without
            = loadRecords<ESM4::Creature>("CREA", record("CREA", 0x801, subRecord("NIFZ", names)));

        ASSERT_EQ(withEncoder.size(), 1u);
        EXPECT_THAT(withEncoder[0].mNif, ElementsAre("one.nif", "two.nif", "three.nif"));
        ASSERT_EQ(without.size(), 1u);
        EXPECT_THAT(without[0].mNif, ElementsAre("one.nif", "two.nif", "three.nif"));
    }

    TEST(ESM4ReaderTest, aListOfStringsThatTheFileEndsInsideOfIsAnError)
    {
        std::string names("one.nif");
        names.push_back('\0');
        EXPECT_THROW(loadRecords<ESM4::Creature>("CREA", record("CREA", 0x801, subRecord("NIFZ", names)), 4),
            std::runtime_error);
    }

    TEST(ESM4ReaderTest, aSubrecordThatRunsPastItsRecordCannotBeSkippedAfter)
    {
        // The subrecord says that it has more data than the record has
        std::string bad = std::string("EDID");
        append<std::uint16_t>(bad, 200);
        bad += "AB";
        const std::string plugin = pluginHeader(0) + record("CELL", 0x801, bad);
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);

        ASSERT_TRUE(reader.getRecordHeader());
        ASSERT_TRUE(reader.getSubRecordHeader());
        EXPECT_THAT([&] { reader.skipRecordData(); },
            ThrowsMessage<std::runtime_error>(HasSubstr("Skipping after reading more than available")));
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Compressed records

    /// A record that has the given type and form ID, and the data of the zlib stream and the size that it is said to
    /// have inflated to.
    std::string rawCompressedRecord(std::uint32_t id, const std::string& zlibStream, std::uint32_t inflatedSize)
    {
        std::string payload;
        append(payload, inflatedSize);
        payload += zlibStream;
        return record("CELL", id, payload, ESM4::Rec_Compressed);
    }

    std::string deflate(const std::string& data)
    {
        uLongf size = compressBound(static_cast<uLong>(data.size()));
        std::string result(size, '\0');
        if (compress(reinterpret_cast<Bytef*>(result.data()), &size, reinterpret_cast<const Bytef*>(data.data()),
                static_cast<uLong>(data.size()))
            != Z_OK)
            throw std::runtime_error("Failed to compress test data");
        result.resize(size);
        return result;
    }

    std::vector<ESM4::Cell> loadCells(const std::string& records)
    {
        const std::string plugin = pluginHeader(0) + topGroup("CELL", records);
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);
        reader.setModIndex(0);
        std::vector<ESM4::Cell> cells;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                cells.emplace_back().load(r);
                return true;
            },
            [](ESM4::Reader&) {});
        return cells;
    }

    TEST(ESM4ReaderTest, aCompressedRecordIsFollowedByTheRecordsAfterIt)
    {
        const std::vector<ESM4::Cell> cells = loadCells(compressedRecord("CELL", 0x801, zString("EDID", "Packed"))
            + record("CELL", 0x802, zString("EDID", "Plain")));

        ASSERT_EQ(cells.size(), 2u);
        EXPECT_EQ(cells[0].mEditorId, "Packed");
        EXPECT_EQ(cells[1].mEditorId, "Plain");
    }

    TEST(ESM4ReaderTest, aCompressedRecordWhoseDataEndsEarlyIsReadAsFarAsItGoes)
    {
        // The last bytes of the stream are lost; what is before them can be inflated in small blocks
        const std::string data = zString("EDID", "Packed") + zString("FULL", std::string(100, 'x'));
        std::string stream = deflate(data);
        stream.resize(stream.size() - 6);

        const std::vector<ESM4::Cell> cells
            = loadCells(rawCompressedRecord(0x801, stream, static_cast<std::uint32_t>(data.size())));

        ASSERT_EQ(cells.size(), 1u);
        EXPECT_EQ(cells[0].mEditorId, "Packed");
    }

    TEST(ESM4ReaderTest, aCompressedRecordThatInflatesToMoreThanItSaysIsCutThere)
    {
        const std::string first = zString("EDID", "Packed");
        const std::string data = first + zString("FULL", std::string(100, 'x'));

        const std::vector<ESM4::Cell> cells
            = loadCells(rawCompressedRecord(0x801, deflate(data), static_cast<std::uint32_t>(first.size() + 4)));

        ASSERT_EQ(cells.size(), 1u);
        EXPECT_EQ(cells[0].mEditorId, "Packed");
    }

    TEST(ESM4ReaderTest, theDataOfACompressedRecordCanBeDumped)
    {
        const std::string plugin = pluginHeader(0) + compressedRecord("CELL", 0x801, zString("EDID", "Dump this"));
        ESM4::Reader reader(stream(plugin), "Plugin.esp", nullptr, nullptr);

        ASSERT_TRUE(reader.getRecordHeader());
        testing::internal::CaptureStdout();
        reader.getRecordData(true);
        const std::string dump = testing::internal::GetCapturedStdout();

        // Capitals are letters (with two spaces) and everything else is two hexadecimal digits
        EXPECT_THAT(dump, StartsWith("E  D  I  D  0a 00 D  "));
        EXPECT_THAT(dump, HasSubstr("\n"));
    }
}
