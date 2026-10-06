#include <components/esm4/census.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadlvli.hpp>
#include <components/esm4/loadstat.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string staticRecord(std::uint32_t id, std::string_view editorId)
    {
        return record("STAT", id, subRecord("EDID", std::string(editorId) + '\0'));
    }

    bool parseStatic(ESM4::Reader& reader)
    {
        if (reader.hdr().record.typeId != ESM4::REC_STAT)
            return false;
        reader.getRecordData();
        ESM4::Static value;
        value.load(reader);
        return true;
    }

    std::vector<std::string> splitTokens(const std::string& line)
    {
        std::istringstream stream(line);
        std::vector<std::string> result;
        for (std::string token; stream >> token;)
            result.push_back(token);
        return result;
    }

    std::vector<std::vector<std::string>> tableRows(const std::string& text)
    {
        std::istringstream stream(text);
        std::vector<std::vector<std::string>> result;
        for (std::string line; std::getline(stream, line);)
            result.push_back(splitTokens(line));
        return result;
    }

    TEST(ESM4CensusTest, countsParsedUnparsedAndFailedRecordsAndKeepsReading)
    {
        const std::string statics
            = staticRecord(1, "First") + record("STAT", 2, subRecord("ZZZZ", "1234")) + staticRecord(3, "Third");
        const std::string plugin = header() + topGroup("STAT", statics) + topGroup("ZZZZ", record("ZZZZ", 4, ""));

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "census.esp", nullptr, nullptr);
        ESM4::Census census;
        census.collect(reader, parseStatic);

        EXPECT_EQ(census.getFatalError(), "");
        ASSERT_EQ(census.getRecords().size(), 2u);

        const ESM4::CensusRecord& stat = census.getRecords().at("STAT");
        EXPECT_EQ(stat.mTotal, 3u);
        EXPECT_EQ(stat.mParsed, 2u);
        EXPECT_EQ(stat.mNoParser, 0u);
        EXPECT_EQ(stat.mFailed, 1u);
        ASSERT_EQ(stat.mFailures.size(), 1u);
        EXPECT_EQ(stat.mFailures.begin()->first, "ESM4::STAT::load - Unknown subrecord ZZZZ");
        EXPECT_EQ(stat.mFailures.begin()->second, 1u);

        const ESM4::CensusRecord& unknown = census.getRecords().at("ZZZZ");
        EXPECT_EQ(unknown.mTotal, 1u);
        EXPECT_EQ(unknown.mParsed, 0u);
        EXPECT_EQ(unknown.mNoParser, 1u);
        EXPECT_EQ(unknown.mFailed, 0u);
    }

    TEST(ESM4CensusTest, keepsReadingAfterAFailedCompressedRecord)
    {
        const std::string badData = subRecord("EDID", std::string("Zipped") + '\0') + subRecord("ZZZZ", "1234");
        const std::string statics = compressedRecord("STAT", 1, badData) + staticRecord(2, "Second");
        const std::string plugin = header() + topGroup("STAT", statics);

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "census.esp", nullptr, nullptr);
        ESM4::Census census;
        census.collect(reader, parseStatic);

        EXPECT_EQ(census.getFatalError(), "");
        ASSERT_EQ(census.getRecords().size(), 1u);
        const ESM4::CensusRecord& stat = census.getRecords().at("STAT");
        EXPECT_EQ(stat.mTotal, 2u);
        EXPECT_EQ(stat.mParsed, 1u);
        EXPECT_EQ(stat.mFailed, 1u);
        ASSERT_EQ(stat.mFailures.size(), 1u);
        EXPECT_EQ(stat.mFailures.begin()->first, "ESM4::STAT::load - Unknown subrecord ZZZZ");
    }

    bool parseLevelledItem(ESM4::Reader& reader)
    {
        if (reader.hdr().record.typeId != ESM4::REC_LVLI)
            return false;
        reader.getRecordData();
        ESM4::LevelledItem value;
        value.load(reader);
        return true;
    }

    TEST(ESM4CensusTest, withholdsLoaderMessagesThatEmbedRecordContents)
    {
        // The LVLO size error of the LVLI loader contains the editor ID of the record.
        const std::string editorId = "SecretEditorId";
        const std::string items
            = record("LVLI", 1, subRecord("EDID", editorId + '\0') + subRecord("LVLO", std::string(5, '\0')));
        const std::string plugin = header() + topGroup("LVLI", items);

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "census.esp", nullptr, nullptr);
        ESM4::Census census;
        census.collect(reader, parseLevelledItem);

        const ESM4::CensusRecord& lvli = census.getRecords().at("LVLI");
        EXPECT_EQ(lvli.mFailed, 1u);
        ASSERT_EQ(lvli.mFailures.size(), 1u);
        EXPECT_EQ(lvli.mFailures.begin()->first, "loader error (message withheld, it may contain record contents)");

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), Not(HasSubstr(editorId)));
    }

    TEST(ESM4CensusTest, keepsOnlyPrintableSubrecordCodesInUnknownSubrecordMessages)
    {
        const std::string statics = record("STAT", 1, subRecord("ZZ\n\x01", "1234")) + staticRecord(2, "Second");
        const std::string plugin = header() + topGroup("STAT", statics);

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "census.esp", nullptr, nullptr);
        ESM4::Census census;
        census.collect(reader, parseStatic);

        const ESM4::CensusRecord& stat = census.getRecords().at("STAT");
        EXPECT_EQ(stat.mFailed, 1u);
        ASSERT_EQ(stat.mFailures.size(), 1u);
        EXPECT_EQ(stat.mFailures.begin()->first, "loader error (message withheld, it may contain record contents)");
    }

    TEST(ESM4ReaderUtilsTest, visitsTheLastRecordOfAFile)
    {
        const std::string plugin = header() + topGroup("STAT", staticRecord(1, "Only"));

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "last.esp", nullptr, nullptr);
        std::vector<std::uint32_t> visited;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                visited.push_back(r.hdr().record.typeId);
                return false;
            },
            [](ESM4::Reader&) {});

        EXPECT_EQ(visited, std::vector<std::uint32_t>{ ESM4::REC_STAT });
    }

    TEST(ESM4CensusTest, writesTableTotalsAndFailuresByFrequency)
    {
        ESM4::Census census;
        census.add("STAT", ESM4::CensusOutcome::Failed, "other");
        census.add("ACTI", ESM4::CensusOutcome::Parsed);
        census.add("STAT", ESM4::CensusOutcome::Failed, "boom");
        census.add("ACTI", ESM4::CensusOutcome::Parsed);
        census.add("QUST", ESM4::CensusOutcome::NoParser);
        census.add("STAT", ESM4::CensusOutcome::Failed, "boom");

        std::ostringstream out;
        census.write(out);

        const std::vector<std::vector<std::string>> expected{
            { "Record", "Total", "Parsed", "No", "parser", "Failed" },
            { "ACTI", "2", "2", "0", "0" },
            { "QUST", "1", "0", "1", "0" },
            { "STAT", "3", "0", "0", "3" },
            { "All", "6", "2", "1", "3" },
            {},
            { "Failures", "in", "STAT", "records:" },
            { "2", "x", "boom" },
            { "1", "x", "other" },
        };
        EXPECT_EQ(tableRows(out.str()), expected);
    }

    TEST(ESM4CensusTest, limitsFailureMessagesPerRecordType)
    {
        ESM4::Census census;
        for (int i = 0; i < 12; ++i)
            census.add("STAT", ESM4::CensusOutcome::Failed, "message " + std::to_string(i));

        std::ostringstream out;
        census.write(out);

        // Equal counts keep the messages' alphabetical order, so "message 8" and "message 9" are the ones cut.
        EXPECT_THAT(out.str(), HasSubstr("  ... and 2 more distinct messages\n"));
        EXPECT_THAT(out.str(), HasSubstr("  1 x message 0\n"));
        EXPECT_THAT(out.str(), HasSubstr("  1 x message 7\n"));
        EXPECT_THAT(out.str(), Not(HasSubstr("message 8")));
        EXPECT_THAT(out.str(), Not(HasSubstr("message 9")));
    }
}
