#include <components/esm4/loadpack.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string scriptHeader(std::uint32_t references, std::uint32_t compiledSize, std::uint32_t variables)
    {
        std::string data;
        append<std::uint32_t>(data, 0);
        append(data, references);
        append(data, compiledSize);
        append(data, variables);
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 1);
        return data;
    }

    std::string formIdData(std::uint32_t id)
    {
        std::string data;
        append(data, id);
        return data;
    }

    std::string basics()
    {
        return zString("EDID", "Text of EDID") + subRecord("PKDT", bytePattern(8, 1))
            + subRecord("PLDT", bytePattern(12, 10)) + subRecord("PSDT", bytePattern(8, 20));
    }

    TEST(ESM4PackageTest, readsTheScriptsThatRunWhenAPackageBeginsEndsAndChanges)
    {
        const std::string data = basics() + subRecord("POBA", "") + subRecord("INAM", formIdData(0x00010001))
            + subRecord("SCHR", scriptHeader(0, 0, 0)) + subRecord("TNAM", formIdData(0x00010002))
            + subRecord("POEA", "") + subRecord("INAM", formIdData(0x00010003))
            + subRecord("SCHR", scriptHeader(1, 3, 0)) + subRecord("SCDA", bytePattern(3, 60))
            + subRecord("SCTX", "source") + subRecord("SCRO", formIdData(0x00010004))
            + subRecord("TNAM", formIdData(0x00010005)) + subRecord("POCA", "")
            + subRecord("INAM", formIdData(0x00010006)) + subRecord("SCHR", scriptHeader(0, 0, 0))
            + subRecord("TNAM", formIdData(0x00010007));
        const std::vector<ESM4::AIPackage> result = loadRecords<ESM4::AIPackage>("PACK", record("PACK", 1, data));

        ASSERT_EQ(result.size(), 1u);
        const ESM4::AIPackage& package = result[0];
        EXPECT_EQ(package.mEditorId, "Text of EDID");

        EXPECT_TRUE(package.mBegin.mPresent);
        EXPECT_EQ(package.mBegin.mIdle, ESM::FormId::fromUint32(0x00010001));
        EXPECT_EQ(package.mBegin.mTopic, ESM::FormId::fromUint32(0x00010002));
        EXPECT_THAT(package.mBegin.mScript.compiledScript, IsEmpty());

        EXPECT_TRUE(package.mEnd.mPresent);
        EXPECT_EQ(package.mEnd.mIdle, ESM::FormId::fromUint32(0x00010003));
        EXPECT_EQ(package.mEnd.mTopic, ESM::FormId::fromUint32(0x00010005));
        EXPECT_THAT(package.mEnd.mScript.compiledScript, ElementsAre(60, 61, 62));
        EXPECT_EQ(package.mEnd.mScript.scriptSource, "source");
        ASSERT_EQ(package.mEnd.mScript.references.size(), 1u);
        EXPECT_EQ(package.mEnd.mScript.references[0].formId, ESM::FormId::fromUint32(0x00010004));
        EXPECT_TRUE(package.mEnd.mScript.isConsistent());

        EXPECT_TRUE(package.mChange.mPresent);
        EXPECT_EQ(package.mChange.mIdle, ESM::FormId::fromUint32(0x00010006));
        EXPECT_EQ(package.mChange.mTopic, ESM::FormId::fromUint32(0x00010007));
    }

    TEST(ESM4PackageTest, marksOnlyTheEventsThatArePresent)
    {
        const std::string data = basics() + subRecord("POEA", "");
        const std::vector<ESM4::AIPackage> result = loadRecords<ESM4::AIPackage>("PACK", record("PACK", 1, data));

        ASSERT_EQ(result.size(), 1u);
        EXPECT_FALSE(result[0].mBegin.mPresent);
        EXPECT_TRUE(result[0].mEnd.mPresent);
        EXPECT_FALSE(result[0].mChange.mPresent);
    }

    TEST(ESM4PackageTest, readsConditionsOfTwentyAndTwentyEightBytesAsTargetConditions)
    {
        const std::string longCondition = conditionData(0x40, 2.5f, 14, 0x00010001);
        const std::string shortCondition = longCondition.substr(0, 20);
        const std::string data = basics() + subRecord("CTDA", longCondition) + subRecord("CTDA", shortCondition)
            + subRecord("CTDA", bytePattern(24, 5)) + subRecord("CTDA", "12345");
        const std::vector<ESM4::AIPackage> result = loadRecords<ESM4::AIPackage>("PACK", record("PACK", 1, data));

        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0].mTargetConditions.size(), 2u);
        EXPECT_EQ(result[0].mTargetConditions[0].functionIndex, 14u);
        EXPECT_EQ(result[0].mTargetConditions[0].runOn, 1u);
        EXPECT_EQ(result[0].mTargetConditions[0].reference, 0x00010001u);
        EXPECT_EQ(result[0].mTargetConditions[1].functionIndex, 14u);
        EXPECT_EQ(result[0].mTargetConditions[1].param2, 8u);
        EXPECT_EQ(result[0].mTargetConditions[1].runOn, 0u);
        EXPECT_EQ(result[0].mTargetConditions[1].reference, 0u);
        EXPECT_EQ(result[0].mConditions.size(), 1u);
    }

    // Loads the one package of a plugin that is a Fallout 3 file, by its record only: the group headers of Fallout have
    // another size than the synthetic plugins have.
    ESM4::AIPackage loadFromAFalloutFile(const std::string& data)
    {
        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string plugin
            = versionedRecord("TES4", 0, subRecord("HEDR", hedr), 15) + versionedRecord("PACK", 1, data, 15);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_TRUE(reader.isFalloutFile());
        reader.getRecordData();
        ESM4::AIPackage result;
        result.load(reader);
        return result;
    }

    TEST(ESM4PackageTest, readsAConditionOfTwentyFourBytesAsATargetConditionInAFalloutFile)
    {
        // Run on and no reference, which the format reference allows. The first is a global, so it is adjusted.
        std::string condition;
        append<std::uint32_t>(condition, 0x44);
        append<std::uint32_t>(condition, 0x00000123);
        for (const std::uint32_t value : { 14u, 7u, 8u, 3u })
            append(condition, value);
        const ESM4::AIPackage result = loadFromAFalloutFile(basics() + subRecord("CTDA", condition));

        ASSERT_EQ(result.mTargetConditions.size(), 1u);
        EXPECT_EQ(result.mTargetConditions[0].functionIndex, 14u);
        EXPECT_EQ(result.mTargetConditions[0].runOn, 3u);
        EXPECT_EQ(result.mTargetConditions[0].reference, 0u);
        EXPECT_THAT(result.mConditions, IsEmpty());
    }

    // PKDT of Fallout 3 (8 bytes) and New Vegas (12): the flags, the type as one byte, a byte, flags of the behaviour
    // and, in New Vegas, flags of the type and padding
    std::string falloutPackageData(std::uint8_t type, std::size_t size)
    {
        std::string data;
        append<std::uint32_t>(data, 0x00000011);
        append(data, type);
        append<std::uint8_t>(data, 0xAA);
        append<std::uint16_t>(data, 0x1234);
        data.resize(size, '\x77');
        return subRecord("PKDT", data);
    }

    TEST(ESM4PackageTest, readsTheTypeOfAPackageOfFallout3AsOneByte)
    {
        const ESM4::AIPackage result = loadFromAFalloutFile(
            zString("EDID", "Wander") + falloutPackageData(5, 8) + subRecord("PLDT", bytePattern(12, 10)));

        EXPECT_EQ(result.mData.flags, 0x11u);
        EXPECT_EQ(result.mData.type, 5);
        // the sub-record after it is read from the right place
        EXPECT_EQ(result.mLocation.type, 0x0D0C0B0A);
    }

    TEST(ESM4PackageTest, readsTheTypeOfAPackageOfNewVegasAsOneByte)
    {
        const ESM4::AIPackage result = loadFromAFalloutFile(
            zString("EDID", "Sandbox") + falloutPackageData(12, 12) + subRecord("PLDT", bytePattern(12, 10)));

        EXPECT_EQ(result.mData.flags, 0x11u);
        EXPECT_EQ(result.mData.type, 12);
        EXPECT_EQ(result.mLocation.type, 0x0D0C0B0A);
    }

    TEST(ESM4PackageTest, readsTheTypeOfAPackageOfOblivionAsAWord)
    {
        std::string pkdt;
        append<std::uint32_t>(pkdt, 0x22);
        append<std::int32_t>(pkdt, 9);
        const std::vector<ESM4::AIPackage> result = loadRecords<ESM4::AIPackage>(
            "PACK", record("PACK", 1, zString("EDID", "Ambush") + subRecord("PKDT", pkdt)));

        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0].mData.flags, 0x22u);
        EXPECT_EQ(result[0].mData.type, 9);
    }

    TEST(ESM4PackageTest, adjustsTheTargetOfAPackageByTheTypeOfTheTarget)
    {
        const auto target = [](std::int32_t type) {
            std::string data;
            append(data, type);
            append<std::uint32_t>(data, 0x00000123);
            append<std::int32_t>(data, 5);
            return subRecord("PTDT", data);
        };
        const std::vector<ESM4::AIPackage> result = loadRecords<ESM4::AIPackage>("PACK",
            record("PACK", 1, basics() + target(0)) + record("PACK", 2, basics() + target(1))
                + record("PACK", 3, basics() + target(2)),
            0, nullptr, 3);

        ASSERT_EQ(result.size(), 3u);
        EXPECT_EQ(result[0].mTarget.target, 0x03000123u);
        EXPECT_EQ(result[1].mTarget.target, 0x03000123u);
        EXPECT_EQ(result[2].mTarget.target, 0x00000123u); // an object type is a number
    }

    TEST(ESM4PackageTest, readsTheTargetOfAPackageOfFallout)
    {
        // type, target, distance and 4 bytes more (a float)
        std::string data;
        append<std::int32_t>(data, 0);
        append<std::uint32_t>(data, 0x00000014);
        append<std::int32_t>(data, 300);
        append<float>(data, 1.5f);
        const ESM4::AIPackage result = loadFromAFalloutFile(zString("EDID", "Follow") + falloutPackageData(1, 8)
            + subRecord("PTDT", data) + subRecord("PLDT", bytePattern(12, 10)));

        EXPECT_EQ(result.mData.type, 1);
        EXPECT_EQ(result.mTarget.type, 0);
        EXPECT_EQ(result.mTarget.target, 0x00000014u);
        EXPECT_EQ(result.mTarget.distance, 300);
        // the sub-record after it is read from the right place
        EXPECT_EQ(result.mLocation.type, 0x0D0C0B0A);
    }

    TEST(ESM4PackageTest, ignoresSubrecordsThatNoEventClaims)
    {
        // The loader is shared with games whose packages have no event markers.
        const std::string data = basics() + subRecord("INAM", formIdData(1)) + subRecord("TNAM", formIdData(2))
            + subRecord("SCHR", scriptHeader(0, 0, 0)) + subRecord("POBA", "") + subRecord("INAM", "123")
            + subRecord("TNAM", "");
        const std::vector<ESM4::AIPackage> result = loadRecords<ESM4::AIPackage>("PACK", record("PACK", 1, data));

        ASSERT_EQ(result.size(), 1u);
        EXPECT_TRUE(result[0].mBegin.mPresent);
        EXPECT_EQ(result[0].mBegin.mIdle, ESM::FormId());
        EXPECT_EQ(result[0].mBegin.mTopic, ESM::FormId());
    }

    TEST(ESM4PackageTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_THROW(
            loadRecords<ESM4::AIPackage>("PACK", record("PACK", 1, subRecord("ZZZZ", "1"))), std::runtime_error);
    }
}
