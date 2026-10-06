#include <components/esm4/loadefsh.hpp>

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

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("ICON", "Text of ICON") + zString("ICO2", "Text of ICO2")
            + zString("NAM7", "Text of NAM7") + subRecord("DATA", bytePattern(224, 15));
    }

    void expectEverySubRecord(const ESM4::EffectShader& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFillTexture, "Text of ICON");
        EXPECT_EQ(result.mParticleTexture, "Text of ICO2");
        EXPECT_EQ(result.mHolesTexture, "Text of NAM7");
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(224, 15));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::EffectShader>("EFSH", record("EFSH", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4EffectShaderTest, readsEverySubrecord)
    {
        const std::vector<ESM4::EffectShader> records
            = loadRecords<ESM4::EffectShader>("EFSH", record("EFSH", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4EffectShaderTest, readsACompressedRecord)
    {
        const std::vector<ESM4::EffectShader> records
            = loadRecords<ESM4::EffectShader>("EFSH", compressedRecord("EFSH", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4EffectShaderTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(309, 'x'))), "ESM4::EFSH::load - DATA has an unexpected size");
    }

    TEST(ESM4EffectShaderTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::EFSH::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4EffectShaderTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::EFSH::load - record has unread bytes after its last sub-record");
    }

}
