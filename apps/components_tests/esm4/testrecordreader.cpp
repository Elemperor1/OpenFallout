#include <components/esm4/recordreader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
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

#pragma pack(push, 1)
        struct Link
        {
            std::uint8_t mKind = 0;
            ESM::FormId32 mTarget = 0;
        };
#pragma pack(pop)

        std::string mEditorId;
        std::string mName;
        Pair mPair;
        ESM::FormId mTarget;
        std::array<std::uint8_t, 3> mTriple{};
        std::vector<std::uint8_t> mBlob;
        Link mLink;
        std::vector<Link> mLinks;
        std::vector<ESM::FormId> mTargets;
        ESM4::TargetCondition mCondition{};
        std::vector<std::uint8_t> mSized;
        ESM4::RawSubRecord mRaw;
        std::vector<ESM4::AlternateTexture> mTextures;

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
                    case ESM::fourCC("LINK"):
                        in.value(mLink, &Link::mTarget);
                        break;
                    case ESM::fourCC("LNKS"):
                        in.values(mLinks, &Link::mTarget);
                        break;
                    case ESM::fourCC("TRGS"):
                        in.formIds(mTargets);
                        break;
                    case ESM::fourCC("CTDA"):
                        in.condition(mCondition);
                        break;
                    case ESM::fourCC("SIZD"):
                        in.bytes(mSized, { 2, 4 });
                        break;
                    case ESM::fourCC("RAWS"):
                        in.raw(mRaw);
                        break;
                    case ESM::fourCC("MODS"):
                        in.alternateTextures(mTextures);
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

    std::string link(std::uint8_t kind, std::uint32_t target)
    {
        std::string data;
        append(data, kind);
        append(data, target);
        return data;
    }

    TEST(ESM4RecordReaderTest, readsStructsListsConditionsAndRawSubrecords)
    {
        std::string condition;
        append<std::uint32_t>(condition, 0x40);
        append<float>(condition, 2.5f);
        for (const std::uint32_t value : { 14u, 7u, 8u, 1u, 0x00010003u })
            append(condition, value);

        const std::string data = subRecord("LINK", link(3, 0x00010004)) + subRecord("LNKS", link(1, 5) + link(2, 6))
            + subRecord("TRGS", std::string("\1\0\0\0\2\0\0\0", 8)) + subRecord("CTDA", condition)
            + subRecord("SIZD", "abcd") + subRecord("RAWS", "xyz");

        const std::vector<Sample> samples = loadRecords<Sample>("SMPL", record("SMPL", 1, data));

        ASSERT_EQ(samples.size(), 1u);
        EXPECT_EQ(samples[0].mLink.mKind, 3);
        EXPECT_EQ(samples[0].mLink.mTarget, 0x00010004u);
        ASSERT_EQ(samples[0].mLinks.size(), 2u);
        EXPECT_EQ(samples[0].mLinks[1].mKind, 2);
        EXPECT_EQ(samples[0].mLinks[1].mTarget, 6u);
        ASSERT_EQ(samples[0].mTargets.size(), 2u);
        EXPECT_EQ(samples[0].mTargets[1].toUint32(), 2u);
        EXPECT_EQ(samples[0].mCondition.condition, 0x40u);
        EXPECT_EQ(samples[0].mCondition.comparison, 2.5f);
        EXPECT_EQ(samples[0].mCondition.functionIndex, 14u);
        EXPECT_EQ(samples[0].mCondition.reference, 0x00010003u);
        EXPECT_THAT(samples[0].mSized, ElementsAre('a', 'b', 'c', 'd'));
        EXPECT_EQ(samples[0].mRaw.mType, ESM::fourCC("RAWS"));
        EXPECT_THAT(samples[0].mRaw.mData, ElementsAre('x', 'y', 'z'));
    }

    // Loads the record of a plugin that has the load order index 3 and no masters.
    Sample loadAsThirdPlugin(const std::string& data)
    {
        const std::string plugin = header() + topGroup("SMPL", record("SMPL", 1, data));
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        reader.setModIndex(3);
        Sample result;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                result.load(r);
                return true;
            },
            [](ESM4::Reader&) {});
        return result;
    }

    TEST(ESM4RecordReaderTest, adjustsFormIdsToTheLoadOrder)
    {
        std::string comparison;
        append<std::uint32_t>(comparison, 0x00000456);
        std::string condition;
        append<std::uint32_t>(condition, ESM4::CTF_UseGlobal);
        condition.append(comparison);
        for (const std::uint32_t value : { 14u, 7u, 8u, 1u, 0x00000789u })
            append(condition, value);

        const Sample sample = loadAsThirdPlugin(valueSubRecord<std::uint32_t>("TRGT", 0x00000123)
            + subRecord("LINK", link(1, 0x00000124)) + subRecord("LNKS", link(1, 0x00000125))
            + subRecord("TRGS", std::string("\x26\x01\0\0", 4)) + subRecord("CTDA", condition));

        EXPECT_EQ(sample.mTarget, (ESM::FormId{ 0x123, 3 }));
        EXPECT_EQ(sample.mLink.mTarget, 0x03000124u);
        ASSERT_EQ(sample.mLinks.size(), 1u);
        EXPECT_EQ(sample.mLinks[0].mTarget, 0x03000125u);
        ASSERT_EQ(sample.mTargets.size(), 1u);
        EXPECT_EQ(sample.mTargets[0], (ESM::FormId{ 0x126, 3 }));
        std::uint32_t global = 0;
        std::memcpy(&global, &sample.mCondition.comparison, sizeof(global));
        EXPECT_EQ(global, 0x03000456u);
        EXPECT_EQ(sample.mCondition.reference, 0x03000789u);
    }

    TEST(ESM4RecordReaderTest, keepsNullReferencesNull)
    {
        // The comparison value of a condition is a global variable only when the flag says so. This one is a zero
        // reference to one.
        std::string condition;
        append<std::uint32_t>(condition, ESM4::CTF_UseGlobal);
        append<std::uint32_t>(condition, 0);
        for (const std::uint32_t value : { 14u, 7u, 8u, 1u, 0u })
            append(condition, value);

        const Sample sample = loadAsThirdPlugin(valueSubRecord<std::uint32_t>("TRGT", 0) + subRecord("LINK", link(1, 0))
            + subRecord("LNKS", link(1, 0) + link(2, 0x00000125)) + subRecord("TRGS", std::string(8, '\0'))
            + subRecord("CTDA", condition));

        EXPECT_TRUE(sample.mTarget.isZeroOrUnset());
        EXPECT_EQ(sample.mTarget.toUint32(), 0u);
        EXPECT_EQ(sample.mLink.mTarget, 0u);
        ASSERT_EQ(sample.mLinks.size(), 2u);
        EXPECT_EQ(sample.mLinks[0].mTarget, 0u);
        EXPECT_EQ(sample.mLinks[1].mTarget, 0x03000125u);
        ASSERT_EQ(sample.mTargets.size(), 2u);
        EXPECT_EQ(sample.mTargets[0].toUint32(), 0u);
        EXPECT_EQ(sample.mTargets[1].toUint32(), 0u);
        EXPECT_EQ(sample.mCondition.reference, 0u);
        std::uint32_t global = 1;
        std::memcpy(&global, &sample.mCondition.comparison, sizeof(global));
        EXPECT_EQ(global, 0u);
    }

    TEST(ESM4RecordReaderTest, readsAlternateTexturesAndAdjustsTheirTextures)
    {
        const std::vector<Sample> samples = loadRecords<Sample>("SMPL",
            record("SMPL", 1,
                subRecord("MODS", alternateTextureData({ { "Part", 0x00000123, 4 }, { "", 0, -1 } }))
                    + subRecord("MODS", alternateTextureData({}))),
            0, nullptr, 3);

        ASSERT_EQ(samples.size(), 1u);
        ASSERT_EQ(samples[0].mTextures.size(), 2u);
        EXPECT_EQ(samples[0].mTextures[0].mName, "Part");
        EXPECT_EQ(samples[0].mTextures[0].mTexture, (ESM::FormId{ 0x123, 3 }));
        EXPECT_EQ(samples[0].mTextures[0].mIndex, 4);
        // A texture that is null stays null.
        EXPECT_EQ(samples[0].mTextures[1].mName, "");
        EXPECT_EQ(samples[0].mTextures[1].mTexture.toUint32(), 0u);
        EXPECT_EQ(samples[0].mTextures[1].mIndex, -1);
    }

    TEST(ESM4RecordReaderTest, rejectsAlternateTexturesWhoseSizesDoNotAddUp)
    {
        const std::string good = alternateTextureData({ { "Part", 0x00000123, 4 } });
        EXPECT_EQ(loadFailure(subRecord("MODS", "")), "ESM4::SMPL::load - MODS has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("MODS", good.substr(0, good.size() - 1))),
            "ESM4::SMPL::load - MODS has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("MODS", good + "x")), "ESM4::SMPL::load - MODS has an unexpected size");

        // A count that the sub-record is too short for.
        std::string tooMany = good;
        tooMany[0] = 2;
        EXPECT_EQ(loadFailure(subRecord("MODS", tooMany)), "ESM4::SMPL::load - MODS has an unexpected size");

        // A name whose length runs past the sub-record.
        std::string longName = good;
        longName[4] = 100;
        EXPECT_EQ(loadFailure(subRecord("MODS", longName)), "ESM4::SMPL::load - MODS has an unexpected size");
    }

    TEST(ESM4RecordReaderTest, readsConditionsWithoutTheRunOnAndTheReference)
    {
        const std::string full = conditionData(0x42, 2.5f, 14, 0x00000009); // run on target, so 0x02 is set
        const std::string withoutReference = full.substr(0, 24);
        const std::string withoutRunOn = full.substr(0, 20);
        std::string global;
        append<std::uint32_t>(global, ESM4::CTF_UseGlobal);
        append<std::uint32_t>(global, 0x00000456);
        for (const std::uint32_t value : { 14u, 7u, 8u })
            append(global, value);

        std::vector<Sample> samples;
        for (const std::string& condition : { full, withoutReference, withoutRunOn, global })
            samples.push_back(loadAsThirdPlugin(subRecord("CTDA", condition)));

        EXPECT_EQ(samples[0].mCondition.runOn, 1u);
        EXPECT_EQ(samples[0].mCondition.reference, 0x03000009u);
        // The run on of the record is 1 (target) in conditionData, and the flag says the same.
        EXPECT_EQ(samples[1].mCondition.runOn, 1u);
        EXPECT_EQ(samples[1].mCondition.reference, 0u);
        EXPECT_EQ(samples[1].mCondition.param2, 8u);
        EXPECT_EQ(samples[2].mCondition.runOn, 1u); // from the flag
        EXPECT_EQ(samples[2].mCondition.reference, 0u);
        // Without the flag, a condition without a run on is about the subject.
        EXPECT_EQ(samples[3].mCondition.runOn, 0u);
        EXPECT_EQ(samples[3].mCondition.reference, 0u);
        std::uint32_t id = 0;
        std::memcpy(&id, &samples[3].mCondition.comparison, sizeof(id));
        EXPECT_EQ(id, 0x03000456u);
    }

    TEST(ESM4RecordReaderTest, rejectsAListWhoseSizeIsNotAWholeNumberOfItems)
    {
        EXPECT_EQ(
            loadFailure(subRecord("LNKS", std::string(9, 'x'))), "ESM4::SMPL::load - LNKS has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("TRGS", std::string(6, 'x'))), "ESM4::SMPL::load - TRGS has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("LINK", std::string(6, 'x'))), "ESM4::SMPL::load - LINK has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(21, 'x'))), "ESM4::SMPL::load - CTDA has an unexpected size");
    }

    TEST(ESM4RecordReaderTest, rejectsASizeThatIsNotInTheListOfSizes)
    {
        EXPECT_EQ(loadFailure(subRecord("SIZD", "abc")), "ESM4::SMPL::load - SIZD has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("SIZD", "")), "ESM4::SMPL::load - SIZD has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("SIZD", "abcde")), "ESM4::SMPL::load - SIZD has an unexpected size");
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

    TEST(ESM4RecordReaderTest, writesTheBytesOfACodeThatAreNotPrintableAsEscapes)
    {
        // Image space modifiers have sub-records whose code starts with a byte below a space.
        EXPECT_EQ(
            loadFailure(subRecord(std::string("\1IAD", 4), "1")), "ESM4::SMPL::load - Unknown subrecord \\x01IAD");
        EXPECT_EQ(
            loadFailure(subRecord(std::string("\0IAD", 4), "1")), "ESM4::SMPL::load - Unknown subrecord \\x00IAD");
        EXPECT_EQ(
            loadFailure(subRecord(std::string("A\\BC", 4), "1")), "ESM4::SMPL::load - Unknown subrecord A\\x5cBC");
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

    TEST(ESM4RecordReaderTest, rejectsASubrecordWithAnExtendedSizeInsteadOfLosingIt)
    {
        const std::string longData(70000, 'x');
        EXPECT_EQ(loadFailure(zString("EDID", "x") + extendedSubRecord("BLOB", longData) + zString("FULL", "y")),
            "ESM4::SMPL::load - sub-record with an extended size, which the reader does not read");
        EXPECT_EQ(loadFailure(zString("EDID", "x") + extendedSubRecord("BLOB", longData)),
            "ESM4::SMPL::load - sub-record with an extended size, which the reader does not read");
    }
}
