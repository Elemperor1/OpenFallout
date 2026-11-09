#include <components/esm4/loadland.hpp>
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

    constexpr unsigned sVerts = ESM4::Land::sLandNumVerts;

    std::string baseTexture(std::uint32_t form, std::uint8_t quadrant)
    {
        std::string data;
        append(data, form);
        append(data, quadrant);
        append<std::uint8_t>(data, 0);
        append<std::uint16_t>(data, 0);
        return subRecord("BTXT", data);
    }

    std::string additionalTexture(std::uint32_t form, std::uint8_t quadrant, std::uint16_t layer)
    {
        std::string data;
        append(data, form);
        append(data, quadrant);
        append<std::uint8_t>(data, 0);
        append(data, layer);
        return subRecord("ATXT", data);
    }

    std::string alphaSpots(const std::vector<std::pair<std::uint16_t, float>>& spots)
    {
        std::string data;
        for (const auto& [position, opacity] : spots)
        {
            append(data, position);
            append<std::uint16_t>(data, 0);
            append(data, opacity);
        }
        return subRecord("VTXT", data);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Land>("LAND", record("LAND", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4LandTest, readsTheHeightsNormalsAndColoursOfACell)
    {
        std::string height;
        append<float>(height, 12.5f);
        height += bytePattern(sVerts, 1);
        height += std::string(3, '\0');
        ASSERT_EQ(height.size(), sizeof(ESM4::Land::VHGT));

        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>("LAND",
            record("LAND", 0x1234,
                valueSubRecord<std::uint32_t>("DATA", 7) + subRecord("VNML", bytePattern(sVerts * 3, 10))
                    + subRecord("VHGT", height) + subRecord("VCLR", bytePattern(sVerts * 3, 20))));

        ASSERT_EQ(lands.size(), 1u);
        const ESM4::Land& land = lands[0];
        EXPECT_EQ(land.mId.toUint32(), 0x1234u);
        EXPECT_EQ(land.mLandFlags, 7u);
        EXPECT_EQ(land.mDataTypes, ESM4::Land::LAND_VNML | ESM4::Land::LAND_VHGT | ESM4::Land::LAND_VCLR);
        EXPECT_EQ(land.mHeightMap.heightOffset, 12.5f);
        EXPECT_EQ(land.mHeightMap.gradientData[0], 1);
        EXPECT_EQ(land.mHeightMap.gradientData[1], 2);
        EXPECT_EQ(land.mHeightMap.gradientData[sVerts - 1], static_cast<std::int8_t>(1 + (sVerts - 1)));
        EXPECT_EQ(land.mVertNorm[0], 10);
        EXPECT_EQ(land.mVertNorm[sVerts * 3 - 1], static_cast<std::int8_t>(10 + (sVerts * 3 - 1)));
        EXPECT_EQ(land.mVertColr[0], 20);
        EXPECT_EQ(land.mVertColr[sVerts * 3 - 1], static_cast<std::uint8_t>(20 + (sVerts * 3 - 1)));
    }

    TEST(ESM4LandTest, tellsWhichDataTypesAreThere)
    {
        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>("LAND",
            record("LAND", 1, subRecord("VNML", bytePattern(sVerts * 3, 0)))
                + record("LAND", 2, subRecord("VCLR", bytePattern(sVerts * 3, 0)))
                + record("LAND", 3, valueSubRecord<std::uint32_t>("DATA", 1)));

        ASSERT_EQ(lands.size(), 3u);
        EXPECT_EQ(lands[0].mDataTypes, ESM4::Land::LAND_VNML);
        EXPECT_EQ(lands[1].mDataTypes, ESM4::Land::LAND_VCLR);
        EXPECT_EQ(lands[2].mDataTypes, 0);
    }

    TEST(ESM4LandTest, takesTheBaseTextureOfEachQuadrantOfTheCell)
    {
        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>("LAND",
            record("LAND", 1,
                baseTexture(0x00000101, 0) + baseTexture(0x00000102, 1) + baseTexture(0, 2)
                    + baseTexture(0x00000104, 3)),
            0, nullptr, 3);

        ASSERT_EQ(lands.size(), 1u);
        // the texture of a form of the plugin itself is taken to the load order index of the plugin; none is not
        EXPECT_EQ(lands[0].mTextures[0].base.formId, 0x03000101u);
        EXPECT_EQ(lands[0].mTextures[1].base.formId, 0x03000102u);
        EXPECT_EQ(lands[0].mTextures[2].base.formId, 0u);
        EXPECT_EQ(lands[0].mTextures[3].base.formId, 0x03000104u);
        EXPECT_EQ(lands[0].mTextures[1].base.quadrant, 1);
    }

    TEST(ESM4LandTest, doesNotTakeABaseTextureOfAQuadrantThatDoesNotExist)
    {
        EXPECT_THAT(loadFailure(baseTexture(0x00000101, 4)), HasSubstr("base texture quadrant index error"));
    }

    TEST(ESM4LandTest, ignoresABaseTextureThatIsTooShort)
    {
        const std::vector<ESM4::Land> lands
            = loadRecords<ESM4::Land>("LAND", record("LAND", 1, subRecord("BTXT", bytePattern(4, 0))));

        ASSERT_EQ(lands.size(), 1u);
        EXPECT_EQ(lands[0].mTextures[0].base.formId, 0u);
    }

    TEST(ESM4LandTest, collectsTheAdditionalTexturesAndTheirAlphaOfEachQuadrantInOrder)
    {
        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>("LAND",
            record("LAND", 1,
                additionalTexture(0x00000201, 1, 0) + alphaSpots({ { 17, 0.5f }, { 18, 1.f } })
                    + additionalTexture(0x00000202, 1, 1) + alphaSpots({ { 3, 0.25f } })
                    + additionalTexture(0x00000203, 2, 0) + alphaSpots({ { 4, 0.75f } })),
            0, nullptr, 2);

        ASSERT_EQ(lands.size(), 1u);
        const ESM4::Land& land = lands[0];
        ASSERT_EQ(land.mTextures[1].layers.size(), 2u);
        EXPECT_EQ(land.mTextures[1].layers[0].texture.formId, 0x02000201u);
        EXPECT_EQ(land.mTextures[1].layers[0].texture.layerIndex, 0);
        ASSERT_EQ(land.mTextures[1].layers[0].data.size(), 2u);
        EXPECT_EQ(land.mTextures[1].layers[0].data[0].position, 17);
        EXPECT_EQ(land.mTextures[1].layers[0].data[0].opacity, 0.5f);
        EXPECT_EQ(land.mTextures[1].layers[0].data[1].position, 18);
        EXPECT_EQ(land.mTextures[1].layers[0].data[1].opacity, 1.f);
        EXPECT_EQ(land.mTextures[1].layers[1].texture.formId, 0x02000202u);
        ASSERT_EQ(land.mTextures[1].layers[1].data.size(), 1u);
        EXPECT_EQ(land.mTextures[1].layers[1].data[0].opacity, 0.25f);
        ASSERT_EQ(land.mTextures[2].layers.size(), 1u);
        EXPECT_EQ(land.mTextures[2].layers[0].data[0].position, 4);
        EXPECT_TRUE(land.mTextures[0].layers.empty());
        EXPECT_TRUE(land.mTextures[3].layers.empty());
    }

    TEST(ESM4LandTest, givesALayerWithoutAlphaOneTransparentSpotInTheCorner)
    {
        // the second ATXT comes before the VTXT of the first, so the first layer is closed without alpha
        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>("LAND",
            record("LAND", 1,
                additionalTexture(0x00000201, 0, 0) + additionalTexture(0x00000202, 0, 1)
                    + alphaSpots({ { 9, 1.f } })));

        ASSERT_EQ(lands.size(), 1u);
        ASSERT_EQ(lands[0].mTextures[0].layers.size(), 2u);
        ASSERT_EQ(lands[0].mTextures[0].layers[0].data.size(), 1u);
        EXPECT_EQ(lands[0].mTextures[0].layers[0].data[0].position, 0);
        EXPECT_EQ(lands[0].mTextures[0].layers[0].data[0].opacity, 0.f);
        ASSERT_EQ(lands[0].mTextures[0].layers[1].data.size(), 1u);
        EXPECT_EQ(lands[0].mTextures[0].layers[1].data[0].position, 9);
    }

    TEST(ESM4LandTest, losesTheLastAdditionalTextureThatHasNoAlphaAtTheEndOfTheRecord)
    {
        const std::vector<ESM4::Land> lands
            = loadRecords<ESM4::Land>("LAND", record("LAND", 1, additionalTexture(0x00000201, 3, 0)));

        ASSERT_EQ(lands.size(), 1u);
        EXPECT_TRUE(lands[0].mTextures[3].layers.empty());
    }

    TEST(ESM4LandTest, takesALayerWithAnEmptyAlphaSubrecord)
    {
        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>(
            "LAND", record("LAND", 1, additionalTexture(0x00000201, 0, 0) + subRecord("VTXT", "")));

        ASSERT_EQ(lands.size(), 1u);
        ASSERT_EQ(lands[0].mTextures[0].layers.size(), 1u);
        EXPECT_TRUE(lands[0].mTextures[0].layers[0].data.empty());
    }

    TEST(ESM4LandTest, doesNotTakeAnAdditionalTextureThatSkipsALayer)
    {
        // the first layer of a quadrant has the index 0, not 1
        EXPECT_THAT(loadFailure(additionalTexture(0x00000201, 0, 1) + alphaSpots({ { 1, 1.f } })),
            HasSubstr("additional texture skipping layer"));
        // and the second layer one after the first
        EXPECT_THAT(loadFailure(additionalTexture(0x00000201, 0, 0) + alphaSpots({ { 1, 1.f } })
                        + additionalTexture(0x00000202, 0, 2) + additionalTexture(0x00000203, 0, 3)),
            HasSubstr("additional texture skipping layer"));
    }

    TEST(ESM4LandTest, doesNotTakeAnAdditionalTextureOfAQuadrantThatDoesNotExist)
    {
        EXPECT_THAT(loadFailure(additionalTexture(0x00000201, 4, 0)), HasSubstr("quadrant index error"));
    }

    TEST(ESM4LandTest, doesNotTakeAlphaThatNoAdditionalTextureAnnounced)
    {
        EXPECT_THAT(loadFailure(alphaSpots({ { 1, 1.f } })), HasSubstr("VTXT without ATXT"));
        // the one VTXT belongs to the one ATXT before it
        EXPECT_THAT(
            loadFailure(additionalTexture(0x00000201, 0, 0) + alphaSpots({ { 1, 1.f } }) + alphaSpots({ { 2, 1.f } })),
            HasSubstr("VTXT without ATXT"));
    }

    TEST(ESM4LandTest, doesNotTakeAlphaThatIsNotAWholeNumberOfSpots)
    {
        EXPECT_THAT(loadFailure(additionalTexture(0x00000201, 0, 0) + subRecord("VTXT", bytePattern(9, 0))),
            HasSubstr("VTXT data size error"));
    }

    TEST(ESM4LandTest, readsTheTextureListOfOblivion)
    {
        std::string ids;
        append<std::uint32_t>(ids, 0x00000301);
        append<std::uint32_t>(ids, 0x00000302);
        append<std::uint32_t>(ids, 0x00000303);

        const std::vector<ESM4::Land> lands
            = loadRecords<ESM4::Land>("LAND", record("LAND", 1, subRecord("VTEX", ids)), 0, nullptr, 4);

        ASSERT_EQ(lands.size(), 1u);
        ASSERT_EQ(lands[0].mIds.size(), 3u);
        EXPECT_EQ(lands[0].mIds[0].toUint32(), 0x04000301u);
        EXPECT_EQ(lands[0].mIds[2].toUint32(), 0x04000303u);
        EXPECT_THAT(loadFailure(subRecord("VTEX", bytePattern(6, 0))), HasSubstr("VTEX data size error"));
    }

    TEST(ESM4LandTest, skipsThePathCoverageOfFallout4AndNothingElse)
    {
        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>(
            "LAND", record("LAND", 1, subRecord("MPCD", bytePattern(8, 0)) + valueSubRecord<std::uint32_t>("DATA", 5)));

        ASSERT_EQ(lands.size(), 1u);
        EXPECT_EQ(lands[0].mLandFlags, 5u);
        EXPECT_THAT(loadFailure(zString("EDID", "Land")), HasSubstr("Unknown subrecord EDID"));
    }

    TEST(ESM4LandTest, fillsInTheTextureOfTheTerrainThatHasNone)
    {
        struct Game
        {
            float mHeaderVersion;
            std::uint16_t mFormVersion;
            const char* mDiffuse;
            const char* mNormal;
        };
        const Game games[] = {
            { 0.94f, 15, "textures/landscape/dirtwasteland01.dds", "textures/landscape/dirtwasteland01_n.dds" },
            { 1.34f, 15, "textures/landscape/dirtwasteland01.dds", "textures/landscape/dirtwasteland01_n.dds" },
            { 1.32f, 15, "textures/landscape/dirtwasteland01.dds", "textures/landscape/dirtwasteland01_n.dds" },
            { 0.94f, 43, "textures/landscape/dirt02.dds", "textures/landscape/dirt02_n.dds" },
            { 1.7f, 44, "textures/landscape/dirt02.dds", "textures/landscape/dirt02_n.dds" },
            { 0.95f, 131, "textures/landscape/ground/commonwealthdefault01_d.dds",
                "textures/landscape/ground/commonwealthdefault01_n.dds" },
        };
        for (const Game& game : games)
        {
            const ESM4::Land land = loadVersionedRecord<ESM4::Land>("LAND", game.mHeaderVersion, game.mFormVersion, "");

            EXPECT_EQ(land.mDefaultDiffuseMap.value(), game.mDiffuse) << game.mHeaderVersion;
            EXPECT_EQ(land.mDefaultNormalMap.value(), game.mNormal) << game.mHeaderVersion;
        }
    }

    TEST(ESM4LandTest, fillsInTheTextureOfTheTerrainOfOblivion)
    {
        const std::vector<ESM4::Land> lands = loadRecords<ESM4::Land>("LAND", record("LAND", 1, ""));

        ASSERT_EQ(lands.size(), 1u);
        EXPECT_EQ(lands[0].mDefaultDiffuseMap.value(), "textures/landscape/terrainhddirt01.dds");
        EXPECT_EQ(lands[0].mDefaultNormalMap.value(), "textures/landscape/terrainhddirt01_n.dds");
    }

    TEST(ESM4LandTest, doesNotReadTheTerrainOfAFileOfAnUnknownVersion)
    {
        EXPECT_THROW(loadVersionedRecord<ESM4::Land>("LAND", 3.f, 15, ""), std::runtime_error);
    }
}
