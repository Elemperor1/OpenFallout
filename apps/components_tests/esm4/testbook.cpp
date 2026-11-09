#include <components/esm4/loadbook.hpp>
#include <components/esm4/reader.hpp>

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

    ESM4::Book load(const std::string& data)
    {
        const std::vector<ESM4::Book> result = loadRecords<ESM4::Book>("BOOK", record("BOOK", 0x801, data));
        EXPECT_EQ(result.size(), 1u);
        return result.empty() ? ESM4::Book{} : result[0];
    }

    TEST(ESM4BookTest, readsTheTextsModelsAndFormsOfTheBook)
    {
        const std::string data = zString("EDID", "BookEdid") + zString("FULL", "Full name")
            + zString("DESC", "The text of the book") + zString("ICON", "icons/book.dds")
            + zString("MODL", "Meshes\\Book.nif") + valueSubRecord<float>("MODB", 1.5f)
            + valueSubRecord<std::uint32_t>("SCRI", 0x00000811) + valueSubRecord<std::uint16_t>("ANAM", 30)
            + valueSubRecord<std::uint32_t>("ENAM", 0x00000812) + valueSubRecord<std::uint32_t>("YNAM", 0x00000813)
            + valueSubRecord<std::uint32_t>("ZNAM", 0x00000814);

        const ESM4::Book book = load(data);

        EXPECT_EQ(book.mId.toUint32(), 0x801u);
        EXPECT_EQ(book.mEditorId, "BookEdid");
        EXPECT_EQ(book.mFullName, "Full name");
        EXPECT_EQ(book.mText, "The text of the book");
        EXPECT_EQ(book.mIcon, "icons/book.dds");
        EXPECT_EQ(book.mModel.getOriginal(), "Meshes\\Book.nif");
        EXPECT_EQ(book.mBoundRadius, 1.5f);
        EXPECT_EQ(book.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(book.mEnchantmentPoints, 30);
        EXPECT_EQ(book.mEnchantment.toUint32(), 0x812u);
        EXPECT_EQ(book.mPickUpSound.toUint32(), 0x813u);
        EXPECT_EQ(book.mDropSound.toUint32(), 0x814u);
    }

    TEST(ESM4BookTest, readsTheDataOfOblivionAndFalloutWithTheSkillItTeaches)
    {
        std::string data;
        append<std::uint8_t>(data, ESM4::Book::Flag_Scroll | ESM4::Book::Flag_NoTake);
        append<std::int8_t>(data, ESM4::Book::BookSkill_Sneak);
        append<std::uint32_t>(data, 120); // value
        append<float>(data, 0.5f); // weight

        const ESM4::Book book = load(subRecord("DATA", data));

        EXPECT_EQ(book.mData.flags, 3);
        EXPECT_EQ(book.mData.bookSkill, ESM4::Book::BookSkill_Sneak);
        EXPECT_EQ(book.mData.value, 120u);
        EXPECT_EQ(book.mData.weight, 0.5f);
    }

    TEST(ESM4BookTest, readsTheDataOfSkyrimWithTheSpellItTeaches)
    {
        std::string data;
        append<std::uint8_t>(data, 4); // flags
        append<std::uint8_t>(data, 1); // type
        append<std::uint16_t>(data, 0); // unused
        append<std::uint32_t>(data, 0x00000820); // what it teaches
        append<std::uint32_t>(data, 77); // value
        append<float>(data, 2.f); // weight

        const ESM4::Book book = load(subRecord("DATA", data));

        EXPECT_EQ(book.mData.flags, 4);
        EXPECT_EQ(book.mData.type, 1);
        EXPECT_EQ(book.mData.teaches, 0x820u);
        EXPECT_EQ(book.mData.value, 77u);
        EXPECT_EQ(book.mData.weight, 2.f);
    }

    TEST(ESM4BookTest, readsTheDataOfFallout4WithJustTheValueAndTheWeight)
    {
        std::string data;
        append<std::uint32_t>(data, 55);
        append<float>(data, 0.25f);

        const ESM4::Book book = load(subRecord("DATA", data));

        EXPECT_EQ(book.mData.value, 55u);
        EXPECT_EQ(book.mData.weight, 0.25f);
    }

    TEST(ESM4BookTest, skipsDataOfAnUnknownSize)
    {
        const ESM4::Book book = load(subRecord("DATA", bytePattern(3, 1)) + zString("EDID", "AfterTheData"));

        EXPECT_EQ(book.mData.value, 0u);
        EXPECT_EQ(book.mEditorId, "AfterTheData");
    }

    TEST(ESM4BookTest, skipsTheSubrecordsItDoesNotUse)
    {
        std::string skipped;
        for (const char* type : { "MODT", "MODC", "MODS", "MODF", "OBND", "KSIZ", "KWDA", "CNAM", "INAM", "VMAD",
                 "DAMC", "DEST", "DMDC", "DMDL", "DMDT", "DMDS", "DSTA", "DSTD", "DSTF", "DNAM", "FIMD", "MICO", "PTRN" })
            skipped += subRecord(type, bytePattern(4, 1));

        EXPECT_EQ(load(skipped + zString("EDID", "Last")).mEditorId, "Last");
    }

    TEST(ESM4BookTest, doesNotReadASubrecordItDoesNotKnow)
    {
        try
        {
            load(zString("EDID", "Book") + subRecord("XXYZ", ""));
            FAIL() << "No exception";
        }
        catch (const std::runtime_error& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("Unknown subrecord XXYZ"));
        }
    }
}
