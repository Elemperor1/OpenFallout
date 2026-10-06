#include <components/esm4/loadamef.hpp>

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

    std::string dataData(int offset)
    {
        std::string data;
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 0)));
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 1)));
        append<float>(data, static_cast<float>(offset + 2) + 0.5f);
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + subRecord("DATA", dataData(3));
    }

    void expectEverySubRecord(const ESM4::AmmoEffect& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mData.mType, 100003u);
        EXPECT_EQ(result.mData.mOperation, 100004u);
        EXPECT_EQ(result.mData.mValue, 5.5f);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::AmmoEffect>("AMEF", record("AMEF", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4AmmoEffectTest, readsEverySubrecord)
    {
        const std::vector<ESM4::AmmoEffect> records
            = loadRecords<ESM4::AmmoEffect>("AMEF", record("AMEF", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4AmmoEffectTest, readsACompressedRecord)
    {
        const std::vector<ESM4::AmmoEffect> records
            = loadRecords<ESM4::AmmoEffect>("AMEF", compressedRecord("AMEF", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4AmmoEffectTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(11, 'x'))), "ESM4::AMEF::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(13, 'x'))), "ESM4::AMEF::load - DATA has an unexpected size");
    }

    TEST(ESM4AmmoEffectTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::AMEF::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4AmmoEffectTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::AMEF::load - record has unread bytes after its last sub-record");
    }

}
