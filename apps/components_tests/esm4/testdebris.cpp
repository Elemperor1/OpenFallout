#include <components/esm4/loaddebr.hpp>

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
        return zString("EDID", "Text of EDID")
            + subRecord("DATA",
                std::string("\x19"
                            "meshes\\a.nif\0"
                            "\x01",
                    15))
            + subRecord("MODT", bytePattern(24, 30))
            + subRecord("DATA",
                std::string("\x32"
                            "b.nif\0"
                            "\x00",
                    8))
            + subRecord("MODT", bytePattern(48, 60));
    }

    void expectEverySubRecord(const ESM4::Debris& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        ASSERT_EQ(result.mModels.size(), 2u);
        EXPECT_EQ(result.mModels[0].mPercentage, 25);
        EXPECT_EQ(result.mModels[0].mModel.getOriginal(), "meshes\\a.nif");
        EXPECT_EQ(result.mModels[0].mFlags, 1);
        EXPECT_EQ(
            std::string(result.mModels[0].mTextures.begin(), result.mModels[0].mTextures.end()), bytePattern(24, 30));
        EXPECT_EQ(result.mModels[1].mPercentage, 50);
        EXPECT_EQ(result.mModels[1].mModel.getOriginal(), "b.nif");
        EXPECT_EQ(result.mModels[1].mFlags, 0);
        EXPECT_EQ(
            std::string(result.mModels[1].mTextures.begin(), result.mModels[1].mTextures.end()), bytePattern(48, 60));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Debris>("DEBR", record("DEBR", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4DebrisTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Debris> records
            = loadRecords<ESM4::Debris>("DEBR", record("DEBR", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4DebrisTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Debris> records
            = loadRecords<ESM4::Debris>("DEBR", compressedRecord("DEBR", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4DebrisTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::DEBR::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4DebrisTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::DEBR::load - record has unread bytes after its last sub-record");
    }

    TEST(ESM4DebrisTest, rejectsAModelThatIsNotAPathBetweenAPercentageAndFlags)
    {
        EXPECT_EQ(loadFailure(subRecord("DATA", "ab")), "ESM4::DEBR::load - DATA has an unexpected layout");
        EXPECT_EQ(loadFailure(subRecord("DATA",
                      std::string("\x19"
                                  "abc"
                                  "\x01",
                          5))),
            "ESM4::DEBR::load - DATA has an unexpected layout");
        EXPECT_EQ(loadFailure(subRecord("DATA",
                      std::string("\x19"
                                  "a\0b\0"
                                  "\x01",
                          6))),
            "ESM4::DEBR::load - DATA has an unexpected layout");
    }

    TEST(ESM4DebrisTest, rejectsTexturesThatComeBeforeAModel)
    {
        EXPECT_EQ(loadFailure(subRecord("MODT", bytePattern(24, 1))), "ESM4::DEBR::load - MODT comes before DATA");
    }

}
