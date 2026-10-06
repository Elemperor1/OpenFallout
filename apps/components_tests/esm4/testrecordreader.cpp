#include <components/esm4/recordreader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    // A made-up record that reads every kind of sub-record the helper offers.
    struct Sample
    {
        struct Pair
        {
            std::int32_t mFirst = 0;
            float mSecond = 0.f;
        };

        std::string mEditorId;
        std::string mName;
        Pair mPair;
        ESM::FormId mTarget;
        std::array<std::uint8_t, 3> mTriple{};
        std::vector<std::uint8_t> mBlob;

        void load(ESM4::Reader& reader)
        {
            ESM4::RecordReader in(reader, "SMPL");
            while (in.next())
            {
                switch (in.type())
                {
                    case ESM::fourCC("EDID"):
                        in.string(mEditorId);
                        break;
                    case ESM::fourCC("FULL"):
                        in.string(mName);
                        break;
                    case ESM::fourCC("DATA"):
                        in.value(mPair);
                        break;
                    case ESM::fourCC("TRGT"):
                        in.formId(mTarget);
                        break;
                    case ESM::fourCC("TRPL"):
                        in.value(mTriple);
                        break;
                    case ESM::fourCC("BLOB"):
                        in.bytes(mBlob);
                        break;
                    default:
                        in.unknown();
                }
            }
            in.finish();
        }
    };

    std::string pairData(std::int32_t first, float second)
    {
        std::string data;
        append(data, first);
        append(data, second);
        return subRecord("DATA", data);
    }

    std::string loadFailure(const std::string& data, std::size_t cutBytes = 0)
    {
        try
        {
            loadRecords<Sample>("SMPL", record("SMPL", 1, data), cutBytes);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4RecordReaderTest, readsEveryKindOfSubrecordInFileOrder)
    {
        const std::string data = zString("EDID", "Sample") + zString("FULL", "A Sample") + pairData(-7, 2.5f)
            + valueSubRecord<std::uint32_t>("TRGT", 0x00010002) + subRecord("TRPL", std::string("\1\2\3", 3))
            + subRecord("BLOB", "abcde");

        const std::vector<Sample> samples = loadRecords<Sample>("SMPL", record("SMPL", 1, data));

        ASSERT_EQ(samples.size(), 1u);
        EXPECT_EQ(samples[0].mEditorId, "Sample");
        EXPECT_EQ(samples[0].mName, "A Sample");
        EXPECT_EQ(samples[0].mPair.mFirst, -7);
        EXPECT_EQ(samples[0].mPair.mSecond, 2.5f);
        EXPECT_EQ(samples[0].mTarget.toUint32(), 0x00010002u);
        EXPECT_THAT(samples[0].mTriple, ElementsAre(1, 2, 3));
        EXPECT_THAT(samples[0].mBlob, ElementsAre('a', 'b', 'c', 'd', 'e'));
    }

    TEST(ESM4RecordReaderTest, readsEmptyStringsAndBlobsAndAnEmptyRecord)
    {
        const std::vector<Sample> samples = loadRecords<Sample>(
            "SMPL", record("SMPL", 1, subRecord("EDID", "") + subRecord("BLOB", "")) + record("SMPL", 2, ""));

        ASSERT_EQ(samples.size(), 2u);
        EXPECT_EQ(samples[0].mEditorId, "");
        EXPECT_THAT(samples[0].mBlob, IsEmpty());
    }

    TEST(ESM4RecordReaderTest, rejectsASizeTheLoaderDoesNotKnowAndNamesTheRecordAndTheSubrecord)
    {
        EXPECT_EQ(loadFailure(subRecord("DATA", "1234567")), "ESM4::SMPL::load - DATA has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("DATA", "123456789")), "ESM4::SMPL::load - DATA has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("TRGT", "123")), "ESM4::SMPL::load - TRGT has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("TRPL", "12")), "ESM4::SMPL::load - TRPL has an unexpected size");
    }

    TEST(ESM4RecordReaderTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::SMPL::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4RecordReaderTest, rejectsASubrecordThatRunsPastItsRecord)
    {
        std::string longSubRecord = subRecord("BLOB", "1234");
        longSubRecord[4] = 100;
        EXPECT_EQ(loadFailure(zString("EDID", "x") + longSubRecord),
            "ESM4::SMPL::load - sub-record is longer than its record");
    }

    TEST(ESM4RecordReaderTest, rejectsBytesNoSubrecordAccountsFor)
    {
        for (const std::size_t extra : { 1u, 3u, 5u })
            EXPECT_EQ(loadFailure(zString("EDID", "x") + std::string(extra, '\0')),
                "ESM4::SMPL::load - record has unread bytes after its last sub-record")
                << extra;
    }

    TEST(ESM4RecordReaderTest, rejectsAFileThatEndsInsideAField)
    {
        // The last bytes of the file are cut off, so the headers still promise them.
        for (const std::size_t cut : { 1u, 4u })
        {
            EXPECT_EQ(loadFailure(pairData(1, 2.f), cut), "ESM4::SMPL::load - sub-record is shorter than its size")
                << cut;
            EXPECT_EQ(loadFailure(valueSubRecord<std::uint32_t>("TRGT", 1), cut),
                "ESM4::SMPL::load - sub-record is shorter than its size")
                << cut;
            EXPECT_EQ(
                loadFailure(subRecord("BLOB", "abcde"), cut), "ESM4::SMPL::load - sub-record is shorter than its size")
                << cut;
            EXPECT_EQ(
                loadFailure(zString("EDID", "abcde"), cut), "ESM4::SMPL::load - sub-record is shorter than its size")
                << cut;
        }
    }

    TEST(ESM4RecordReaderTest, readsTheLastShortSubrecordOfACompressedRecord)
    {
        const std::string data = zString("EDID", "Compressed") + subRecord("BLOB", "ab");
        const std::vector<Sample> samples = loadRecords<Sample>("SMPL", compressedRecord("SMPL", 1, data));

        ASSERT_EQ(samples.size(), 1u);
        EXPECT_EQ(samples[0].mEditorId, "Compressed");
        EXPECT_THAT(samples[0].mBlob, ElementsAre('a', 'b'));
    }
}
