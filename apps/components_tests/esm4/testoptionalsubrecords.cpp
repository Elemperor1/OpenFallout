#include <components/esm4/loadccrd.hpp>
#include <components/esm4/loadchal.hpp>
#include <components/esm4/loadchip.hpp>
#include <components/esm4/loadcmny.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadperk.hpp>
#include <components/esm4/loadrepu.hpp>
#include <components/esm4/loadterm.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Sub-records that the format references list for these records and the game files in use do not show.
namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::vector<TextureEntry> textureEntries()
    {
        return { { "Part", 0x00010001, 4 }, { "Other", 0x00010002, -1 } };
    }

    void expectTextureEntries(const std::vector<ESM4::AlternateTexture>& result)
    {
        const std::vector<TextureEntry> expected = textureEntries();
        ASSERT_EQ(result.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            EXPECT_EQ(result[i].mName, expected[i].mName);
            EXPECT_EQ(result[i].mTexture.toUint32(), expected[i].mTexture);
            EXPECT_EQ(result[i].mIndex, expected[i].mIndex);
        }
    }

    std::string modelData()
    {
        return zString("MODL", "Text of MODL") + valueSubRecord<float>("MODB", 2.5f)
            + subRecord("MODT", bytePattern(5, 10)) + subRecord("MODS", alternateTextureData(textureEntries()))
            + valueSubRecord<std::uint8_t>("MODD", 3);
    }

    template <class T>
    T loadOne(const char* type, const std::string& data)
    {
        std::vector<T> result = loadRecords<T>(type, record(type, 1, data));
        EXPECT_EQ(result.size(), 1u);
        return result.at(0);
    }

    template <class T>
    void expectModelData(const T& result)
    {
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(result.mBoundRadius, 2.5f);
        EXPECT_EQ(std::string(result.mModelTextures.begin(), result.mModelTextures.end()), bytePattern(5, 10));
        expectTextureEntries(result.mModelAlternateTextures);
        EXPECT_EQ(result.mModelFlags, 3);
    }

    std::string icons()
    {
        return zString("ICON", "Text of ICON") + zString("MICO", "Text of MICO");
    }

    std::string sounds()
    {
        return valueSubRecord<std::uint32_t>("YNAM", 0x00010001) + valueSubRecord<std::uint32_t>("ZNAM", 0x00010002);
    }

    TEST(ESM4OptionalSubrecordTest, readsTheIconsAndTheModelOfAMagicEffect)
    {
        const ESM4::MagicEffect result
            = loadOne<ESM4::MagicEffect>("MGEF", zString("EDID", "x") + icons() + modelData());
        EXPECT_EQ(result.mIcon, "Text of ICON");
        EXPECT_EQ(result.mSmallIcon, "Text of MICO");
        expectModelData(result);
    }

    TEST(ESM4OptionalSubrecordTest, readsTheSmallIconOfPerksChallengesAndReputations)
    {
        EXPECT_EQ(loadOne<ESM4::Perk>("PERK", zString("EDID", "x") + zString("MICO", "Text of MICO")).mSmallIcon,
            "Text of MICO");
        const ESM4::Challenge challenge = loadOne<ESM4::Challenge>("CHAL", zString("EDID", "x") + icons());
        EXPECT_EQ(challenge.mIcon, "Text of ICON");
        EXPECT_EQ(challenge.mSmallIcon, "Text of MICO");
        EXPECT_EQ(loadOne<ESM4::Reputation>("REPU", zString("EDID", "x") + icons()).mSmallIcon, "Text of MICO");
    }

    TEST(ESM4OptionalSubrecordTest, readsTheModelAndTheSoundsOfCaravanCardsAndMoney)
    {
        const ESM4::CaravanCard card
            = loadOne<ESM4::CaravanCard>("CCRD", zString("EDID", "x") + modelData() + icons() + sounds());
        expectModelData(card);
        EXPECT_EQ(card.mSmallIcon, "Text of MICO");
        EXPECT_EQ(card.mPickUpSound.toUint32(), 0x00010001u);
        EXPECT_EQ(card.mDropSound.toUint32(), 0x00010002u);

        const ESM4::CaravanMoney money
            = loadOne<ESM4::CaravanMoney>("CMNY", zString("EDID", "x") + modelData() + sounds());
        expectModelData(money);
        EXPECT_EQ(money.mPickUpSound.toUint32(), 0x00010001u);
        EXPECT_EQ(money.mDropSound.toUint32(), 0x00010002u);
    }

    TEST(ESM4OptionalSubrecordTest, readsTheModelAndTheDestructionOfAPokerChip)
    {
        std::string header;
        append<std::int32_t>(header, 150);
        append<std::uint8_t>(header, 2);
        append<std::uint8_t>(header, 1);
        header += std::string(2, '\0');
        std::string stage;
        append<std::uint8_t>(stage, 50);
        append<std::uint8_t>(stage, 1);
        append<std::uint8_t>(stage, 2);
        append<std::uint8_t>(stage, 4);
        append<std::int32_t>(stage, -7);
        append<ESM::FormId32>(stage, 0x00010003);
        append<ESM::FormId32>(stage, 0x00010004);
        append<std::int32_t>(stage, 6);

        const ESM4::PokerChip chip = loadOne<ESM4::PokerChip>("CHIP",
            zString("EDID", "x") + modelData() + icons() + subRecord("DEST", header) + subRecord("DSTD", stage)
                + zString("DMDL", "Stage model") + subRecord("DMDT", bytePattern(3, 1)) + subRecord("DSTF", "")
                + subRecord("DSTD", stage) + subRecord("DSTF", ""));
        expectModelData(chip);
        EXPECT_EQ(chip.mSmallIcon, "Text of MICO");
        EXPECT_TRUE(chip.mDestruction.mPresent);
        EXPECT_EQ(chip.mDestruction.mHeader.mHealth, 150);
        EXPECT_EQ(chip.mDestruction.mHeader.mStageCount, 2);
        ASSERT_EQ(chip.mDestruction.mStages.size(), 2u);
        EXPECT_EQ(chip.mDestruction.mStages[0].mHealthPercent, 50);
        EXPECT_EQ(chip.mDestruction.mStages[0].mExplosion, 0x00010003u);
        EXPECT_EQ(chip.mDestruction.mStages[0].mDebrisCount, 6);
        ASSERT_EQ(chip.mDestruction.mStageModels.size(), 2u);
        EXPECT_EQ(chip.mDestruction.mStageModels[0].mModel.getOriginal(), "Stage model");
        EXPECT_THAT(chip.mDestruction.mStageModels[0].mTextures, ElementsAre(1, 2, 3));
        EXPECT_EQ(chip.mDestruction.mStageModels[1].mModel.getOriginal(), "");
    }

    TEST(ESM4OptionalSubrecordTest, rejectsAStageModelBeforeTheFirstStage)
    {
        try
        {
            loadOne<ESM4::PokerChip>("CHIP", zString("DMDL", "Model"));
            FAIL() << "no exception";
        }
        catch (const std::exception& e)
        {
            EXPECT_STREQ(e.what(), "ESM4::CHIP::load - DMDL comes before DSTD");
        }
    }

    TEST(ESM4OptionalSubrecordTest, skipsTheModelAndDestructionSubrecordsOfATerminal)
    {
        const ESM4::Terminal terminal = loadOne<ESM4::Terminal>("TERM",
            zString("EDID", "x") + subRecord("MODB", "1234") + subRecord("MODD", "1") + subRecord("DEST", "12345678")
                + subRecord("DSTD", std::string(20, '\0')) + zString("DMDL", "m") + subRecord("DSTF", "")
                + zString("FULL", "Text of FULL"));
        EXPECT_EQ(terminal.mFullName, "Text of FULL");
    }
}
