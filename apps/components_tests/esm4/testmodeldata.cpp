#include <components/esm4/loadaddn.hpp>
#include <components/esm4/loadavif.hpp>
#include <components/esm4/loadcams.hpp>
#include <components/esm4/loadclmt.hpp>
#include <components/esm4/loadexpl.hpp>
#include <components/esm4/loadipct.hpp>
#include <components/esm4/loadmicn.hpp>
#include <components/esm4/loadproj.hpp>
#include <components/esm4/loadwthr.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// The records of Fallout 3 and New Vegas that have a model hold all of its sub-records, which the game files in use
// do not always show: MODL, MODB, MODT, MODS and MODD.
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

    TEST(ESM4ModelDataTest, readsAllTheModelSubrecordsOfEveryRecordThatHasAModel)
    {
        expectModelData(loadOne<ESM4::AddonNode>("ADDN", zString("EDID", "x") + modelData()));
        expectModelData(loadOne<ESM4::CameraShot>(
            "CAMS", zString("EDID", "x") + modelData() + subRecord("DATA", bytePattern(40, 1))));
        expectModelData(loadOne<ESM4::Climate>("CLMT", zString("EDID", "x") + modelData()));
        expectModelData(loadOne<ESM4::Explosion>("EXPL", zString("EDID", "x") + modelData()));
        expectModelData(loadOne<ESM4::ImpactData>("IPCT", zString("EDID", "x") + modelData()));
        expectModelData(loadOne<ESM4::Projectile>("PROJ", zString("EDID", "x") + modelData()));
        expectModelData(loadOne<ESM4::Weather>("WTHR", zString("EDID", "x") + modelData()));
    }

    TEST(ESM4ModelDataTest, readsTheSmallIconOfRecordsThatHaveAnIcon)
    {
        EXPECT_EQ(loadOne<ESM4::ActorValueInfo>(
                      "AVIF", zString("EDID", "x") + zString("ICON", "Text of ICON") + zString("MICO", "Text of MICO"))
                      .mSmallIcon,
            "Text of MICO");
        EXPECT_EQ(loadOne<ESM4::MenuIcon>(
                      "MICN", zString("EDID", "x") + zString("ICON", "Text of ICON") + zString("MICO", "Text of MICO"))
                      .mSmallIcon,
            "Text of MICO");
    }

    TEST(ESM4ModelDataTest, keepsTheModelOfEachDestructionStage)
    {
        std::string stage(20, '\0');
        const ESM4::Projectile result = loadOne<ESM4::Projectile>("PROJ",
            zString("EDID", "x") + subRecord("DSTD", stage) + zString("DMDL", "First model")
                + subRecord("DMDT", bytePattern(3, 1)) + subRecord("DMDS", alternateTextureData(textureEntries()))
                + subRecord("DSTF", "") + subRecord("DSTD", stage) + subRecord("DSTD", stage)
                + zString("DMDL", "Third model") + subRecord("DSTF", ""));

        ASSERT_EQ(result.mStages.size(), 3u);
        ASSERT_EQ(result.mStageModels.size(), 3u);
        EXPECT_EQ(result.mStageModels[0].mModel.getOriginal(), "First model");
        EXPECT_EQ(std::string(result.mStageModels[0].mTextures.begin(), result.mStageModels[0].mTextures.end()),
            bytePattern(3, 1));
        expectTextureEntries(result.mStageModels[0].mAlternateTextures);
        EXPECT_EQ(result.mStageModels[1].mModel.getOriginal(), "");
        EXPECT_THAT(result.mStageModels[1].mTextures, IsEmpty());
        EXPECT_EQ(result.mStageModels[2].mModel.getOriginal(), "Third model");
    }

    TEST(ESM4ModelDataTest, rejectsADestructionStageModelBeforeTheFirstStage)
    {
        try
        {
            loadOne<ESM4::Projectile>("PROJ", zString("DMDL", "Model"));
            FAIL() << "no exception";
        }
        catch (const std::exception& e)
        {
            EXPECT_STREQ(e.what(), "ESM4::PROJ::load - DMDL comes before DSTD");
        }
    }
}
