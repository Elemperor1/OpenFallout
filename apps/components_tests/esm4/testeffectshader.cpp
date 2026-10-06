#include <components/esm4/loadefsh.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
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

    TEST(ESM4EffectShaderTest, readsDataThatEndsAfterAnyMemberAndAdjustsTheDebris)
    {
        // The format reference lets DATA end after any member from the 224th byte on. The debris of the addon models
        // is at 244 when DATA goes that far.
        std::string longData = bytePattern(284, 1);
        const std::uint32_t debris = 0x00000123;
        longData.replace(244, 4, std::string(reinterpret_cast<const char*>(&debris), 4));
        for (const std::size_t size : { 224u, 228u, 244u, 248u, 252u, 284u })
        {
            const std::vector<ESM4::EffectShader> result = loadRecords<ESM4::EffectShader>(
                "EFSH", record("EFSH", 1, subRecord("DATA", longData.substr(0, size))), 0, nullptr, 3);
            ASSERT_EQ(result.size(), 1u) << size;
            ASSERT_EQ(result[0].mData.size(), size);
            std::uint32_t id = 0;
            if (size >= 248)
            {
                std::memcpy(&id, result[0].mData.data() + 244, sizeof(id));
                EXPECT_EQ(id, 0x03000123u) << size;
            }
            else
                EXPECT_EQ(std::string(result[0].mData.begin(), result[0].mData.end()), longData.substr(0, size));
        }
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(223, 'x'))), "ESM4::EFSH::load - DATA has an unexpected size");
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
