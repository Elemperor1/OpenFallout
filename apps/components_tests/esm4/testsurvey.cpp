#include <components/esm4/common.hpp>
#include <components/esm4/loadstat.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/survey.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    bool parseStatic(ESM4::Reader& reader)
    {
        if (reader.hdr().record.typeId != ESM4::REC_STAT)
            return false;
        reader.getRecordData();
        ESM4::Static value;
        value.load(reader);
        return true;
    }

    std::string written(const ESM4::Survey& survey)
    {
        std::ostringstream stream;
        survey.write(stream);
        return stream.str();
    }

    void collect(ESM4::Survey& survey, const std::string& plugin, const std::function<bool(ESM4::Reader&)>& parse = {})
    {
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "survey.esp", nullptr, nullptr);
        survey.collect(reader, parse);
    }

    TEST(ESM4SurveyTest, listsCodesCountsSizesAndOrderOfRecordsWithoutALoader)
    {
        const std::string records = record("ZZZZ", 1, zString("EDID", "One") + subRecord("DATA", "12345678"))
            + record("ZZZZ", 2, zString("EDID", "Second") + subRecord("DATA", "1234") + subRecord("DATA", "1234"))
            + record("ZZZZ", 3, zString("EDID", "Third") + subRecord("DATA", "12345678"));
        ESM4::Survey survey;
        collect(survey, header() + topGroup("ZZZZ", records));

        ASSERT_EQ(survey.getTypes().size(), 1u);
        const ESM4::SurveyType& type = survey.getTypes().at("ZZZZ");
        EXPECT_EQ(type.mTotal, 3u);
        EXPECT_EQ(type.mNoParser, 3u);
        EXPECT_EQ(type.mSurveyed, 3u);

        const ESM4::SurveySubRecord& edid = type.mSubRecords.at("EDID");
        EXPECT_EQ(edid.mRecords, 3u);
        EXPECT_EQ(edid.mTotal, 3u);
        EXPECT_EQ(edid.mLeast, 1u);
        EXPECT_EQ(edid.mMost, 1u);
        EXPECT_THAT(edid.mSizes, UnorderedElementsAre(Pair(4u, 1u), Pair(7u, 1u), Pair(6u, 1u)));

        const ESM4::SurveySubRecord& data = type.mSubRecords.at("DATA");
        EXPECT_EQ(data.mRecords, 3u);
        EXPECT_EQ(data.mTotal, 4u);
        EXPECT_EQ(data.mLeast, 1u);
        EXPECT_EQ(data.mMost, 2u);
        EXPECT_THAT(data.mSizes, UnorderedElementsAre(Pair(8u, 2u), Pair(4u, 2u)));

        EXPECT_THAT(type.mOrders, UnorderedElementsAre(Pair("EDID DATA", 2u), Pair("EDID DATA*2", 1u)));

        const std::string text = written(survey);
        EXPECT_THAT(text, HasSubstr("ZZZZ: 3 records, 0 parsed, 3 no parser, 0 failed\n"));
        EXPECT_THAT(text, HasSubstr("  DATA: in 3 records, 1 to 2 each, 4 in all, sizes 4:2 8:2\n"));
        EXPECT_THAT(text, HasSubstr("  EDID: in 3 records, 1 each, 3 in all, sizes 4:1 6:1 7:1\n"));
        EXPECT_THAT(text, HasSubstr("    2 x EDID DATA\n"));
        EXPECT_THAT(text, HasSubstr("    1 x EDID DATA*2\n"));
    }

    TEST(ESM4SurveyTest, readsOnlyTheSelectedRecordTypesAndKeepsReadingAfterOthers)
    {
        const std::string plugin = header() + topGroup("ZZZZ", record("ZZZZ", 1, subRecord("AAAA", "1")))
            + topGroup("YYYY", record("YYYY", 2, subRecord("BBBB", "22")))
            + topGroup("XXXX", record("XXXX", 3, subRecord("CCCC", "333")));
        ESM4::Survey survey({ "YYYY", "XXXX" });
        collect(survey, plugin);

        EXPECT_THAT(survey.getTypes(), ElementsAre(Key("XXXX"), Key("YYYY")));
        EXPECT_EQ(survey.getTypes().at("YYYY").mSubRecords.at("BBBB").mSizes.at(2), 1u);
        EXPECT_EQ(survey.getTypes().at("XXXX").mSubRecords.at("CCCC").mSizes.at(3), 1u);
        EXPECT_THAT(survey.getFatalErrors(), IsEmpty());
    }

    TEST(ESM4SurveyTest, listsTheSubrecordsOfCompressedRecordsAndKeepsReading)
    {
        const std::string plugin = header()
            + topGroup("ZZZZ",
                compressedRecord("ZZZZ", 1, subRecord("AAAA", "1234") + subRecord("BBBB", "12"))
                    + record("ZZZZ", 2, subRecord("AAAA", "123")));
        ESM4::Survey survey;
        collect(survey, plugin);

        const ESM4::SurveyType& type = survey.getTypes().at("ZZZZ");
        EXPECT_EQ(type.mTotal, 2u);
        EXPECT_EQ(type.mSurveyed, 2u);
        EXPECT_THAT(type.mSubRecords.at("AAAA").mSizes, UnorderedElementsAre(Pair(4u, 1u), Pair(3u, 1u)));
        // The last sub-record of the compressed record is only 8 bytes long, the reader used to lose such records.
        EXPECT_THAT(type.mSubRecords.at("BBBB").mSizes, UnorderedElementsAre(Pair(2u, 1u)));
        EXPECT_THAT(type.mOrders, UnorderedElementsAre(Pair("AAAA BBBB", 1u), Pair("AAAA", 1u)));
        EXPECT_THAT(survey.getFatalErrors(), IsEmpty());
    }

    TEST(ESM4SurveyTest, listsRecordsALoaderReadsAndRecordsItRejectsFromTheirFirstSubrecord)
    {
        const std::string statics = record("STAT", 1, zString("EDID", "Good") + zString("MODL", "mesh"))
            + record("STAT", 2, zString("EDID", "Bad") + subRecord("ZZZZ", "1234") + zString("MODL", "m"))
            + compressedRecord("STAT", 3, zString("EDID", "Compressed") + subRecord("ZZZZ", "12345"));
        const std::string plugin = header() + topGroup("STAT", statics);

        ESM4::Survey survey({}, false);
        collect(survey, plugin, parseStatic);

        const ESM4::SurveyType& type = survey.getTypes().at("STAT");
        EXPECT_EQ(type.mTotal, 3u);
        EXPECT_EQ(type.mParsed, 1u);
        EXPECT_EQ(type.mFailed, 2u);
        EXPECT_EQ(type.mSurveyed, 3u);
        EXPECT_EQ(type.mSubRecords.at("EDID").mRecords, 3u);
        EXPECT_THAT(type.mOrders,
            UnorderedElementsAre(Pair("EDID MODL", 1u), Pair("EDID ZZZZ MODL", 1u), Pair("EDID ZZZZ", 1u)));
        EXPECT_THAT(written(survey), HasSubstr("STAT: 3 records, 1 parsed, 0 no parser, 2 failed\n"));
    }

    TEST(ESM4SurveyTest, canListOnlyTheRecordsALoaderRejects)
    {
        const std::string statics = record("STAT", 1, zString("EDID", "Good") + zString("MODL", "mesh"))
            + record("STAT", 2, zString("EDID", "Bad") + subRecord("ZZZZ", "1234"))
            + record("STAT", 3, zString("EDID", "Fine"));
        const std::string plugin = header() + topGroup("STAT", statics) + topGroup("ZZZZ", record("ZZZZ", 4, ""));

        ESM4::Survey survey({}, true);
        collect(survey, plugin, parseStatic);

        const ESM4::SurveyType& type = survey.getTypes().at("STAT");
        EXPECT_EQ(type.mTotal, 3u);
        EXPECT_EQ(type.mParsed, 2u);
        EXPECT_EQ(type.mFailed, 1u);
        EXPECT_EQ(type.mSurveyed, 1u);
        EXPECT_THAT(type.mOrders, UnorderedElementsAre(Pair("EDID ZZZZ", 1u)));
        EXPECT_THAT(type.mSubRecords, Not(Contains(Key("MODL"))));
        EXPECT_EQ(survey.getTypes().at("ZZZZ").mSurveyed, 0u);

        const std::string text = written(survey);
        EXPECT_THAT(text, HasSubstr("STAT: 3 records, 2 parsed, 0 no parser, 1 failed, 1 listed\n"));
        EXPECT_THAT(text, HasSubstr("only of records a loader rejects"));
    }

    TEST(ESM4SurveyTest, addsSeveralFilesToOneSurvey)
    {
        ESM4::Survey survey;
        collect(survey, header() + topGroup("ZZZZ", record("ZZZZ", 1, subRecord("AAAA", "1"))));
        collect(
            survey, header() + topGroup("ZZZZ", record("ZZZZ", 1, subRecord("AAAA", "1") + subRecord("BBBB", "2"))));

        const ESM4::SurveyType& type = survey.getTypes().at("ZZZZ");
        EXPECT_EQ(type.mTotal, 2u);
        EXPECT_EQ(type.mSubRecords.at("AAAA").mRecords, 2u);
        EXPECT_EQ(type.mSubRecords.at("BBBB").mRecords, 1u);
    }

    TEST(ESM4SurveyTest, reportsASubrecordThatRunsPastItsRecordAndKeepsReading)
    {
        std::string longSubRecord = subRecord("BBBB", "1234");
        longSubRecord[4] = 100; // the size field: 100 bytes promised, 4 there
        const std::string plugin = header()
            + topGroup("ZZZZ",
                record("ZZZZ", 1, subRecord("AAAA", "1") + longSubRecord) + record("ZZZZ", 2, subRecord("AAAA", "1")));
        ESM4::Survey survey;
        collect(survey, plugin);

        const ESM4::SurveyType& type = survey.getTypes().at("ZZZZ");
        EXPECT_EQ(type.mTotal, 2u);
        EXPECT_EQ(type.mSurveyed, 2u);
        EXPECT_EQ(type.mOverrunning, 1u);
        EXPECT_EQ(type.mSubRecords.at("AAAA").mRecords, 2u);
        EXPECT_THAT(survey.getFatalErrors(), IsEmpty());
        EXPECT_THAT(written(survey), HasSubstr("1 listed records have a sub-record that runs past the end"));
    }

    TEST(ESM4SurveyTest, listsAnEmptyRecord)
    {
        ESM4::Survey survey;
        collect(survey, header() + topGroup("ZZZZ", record("ZZZZ", 1, "")));

        EXPECT_THAT(survey.getTypes().at("ZZZZ").mOrders, UnorderedElementsAre(Pair("", 1u)));
        EXPECT_THAT(written(survey), HasSubstr("    1 x (none)\n"));
    }

    TEST(ESM4SurveyTest, shortensLongListsOfSizes)
    {
        std::string data;
        for (int size = 1; size <= 30; ++size)
            data += subRecord("AAAA", std::string(static_cast<std::size_t>(size), 'x'));
        ESM4::Survey survey;
        collect(survey, header() + topGroup("ZZZZ", record("ZZZZ", 1, data)));

        const std::string text = written(survey);
        EXPECT_THAT(text, HasSubstr("sizes 1:1 2:1"));
        EXPECT_THAT(text, HasSubstr(" 24:1 ... and 6 more sizes, the largest 30\n"));
        EXPECT_THAT(text, Not(HasSubstr("25:1")));
    }
}
