#include <components/esm4/facegen.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    using namespace testing;

    template <class T>
    void append(std::string& out, T value)
    {
        out.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    // A .egm file: the header, then the morphs; a morph is the scale and three deltas for each vertex
    std::string morphFile(std::uint32_t vertices, const std::vector<std::vector<std::int16_t>>& symmetric,
        const std::vector<std::vector<std::int16_t>>& asymmetric, float scale = 0.5f, std::size_t reserved = 40)
    {
        std::string out("FREGM002");
        append<std::uint32_t>(out, vertices);
        append<std::uint32_t>(out, static_cast<std::uint32_t>(symmetric.size()));
        append<std::uint32_t>(out, static_cast<std::uint32_t>(asymmetric.size()));
        append<std::uint32_t>(out, 0x01020304);
        out.append(reserved, '\0');
        for (const auto* group : { &symmetric, &asymmetric })
            for (const std::vector<std::int16_t>& deltas : *group)
            {
                append<float>(out, scale);
                for (const std::int16_t delta : deltas)
                    append<std::int16_t>(out, delta);
            }
        return out;
    }

    TEST(ESM4FaceGenTest, readsAMorphFile)
    {
        // two vertices; two symmetric morphs and one asymmetric
        const std::string bytes
            = morphFile(2, { { 1, 2, 3, 4, 5, 6 }, { -1, -2, -3, 0, 0, 0 } }, { { 10, 0, 0, 0, 0, 20 } });

        ESM4::FaceMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceMorphs(bytes, morphs, error)) << error;
        EXPECT_EQ(morphs.mVertexCount, 2u);
        ASSERT_EQ(morphs.mSymmetric.size(), 2u);
        ASSERT_EQ(morphs.mAsymmetric.size(), 1u);
        EXPECT_EQ(morphs.mSymmetric[0].mScale, 0.5f);
        EXPECT_THAT(morphs.mSymmetric[0].mDeltas, ElementsAre(1, 2, 3, 4, 5, 6));
        EXPECT_THAT(morphs.mSymmetric[1].mDeltas, ElementsAre(-1, -2, -3, 0, 0, 0));
        EXPECT_THAT(morphs.mAsymmetric[0].mDeltas, ElementsAre(10, 0, 0, 0, 0, 20));
    }

    TEST(ESM4FaceGenTest, refusesFilesThatDoNotFitTogether)
    {
        ESM4::FaceMorphs morphs;
        std::string error;

        EXPECT_FALSE(ESM4::readFaceMorphs("FREGM", morphs, error));
        EXPECT_THAT(error, HasSubstr("shorter"));

        std::string notMorphs = morphFile(1, { { 1, 2, 3 } }, {});
        notMorphs[0] = 'X';
        EXPECT_FALSE(ESM4::readFaceMorphs(notMorphs, morphs, error));
        EXPECT_THAT(error, HasSubstr("not a FaceGen"));

        // the file ends in the middle of the last morph
        std::string cut = morphFile(2, { { 1, 2, 3, 4, 5, 6 } }, {});
        cut.resize(cut.size() - 2);
        EXPECT_FALSE(ESM4::readFaceMorphs(cut, morphs, error));
        EXPECT_THAT(error, HasSubstr("need"));

        // a header that is not 64 bytes: the sizes do not add up
        const std::string wrong = morphFile(1, { { 1, 2, 3 } }, {}, 1.f, 60);
        EXPECT_FALSE(ESM4::readFaceMorphs(wrong, morphs, error));
        EXPECT_THAT(error, HasSubstr("94 bytes, but 1 vertices and 1 morphs need 74"));

        EXPECT_FALSE(ESM4::readFaceMorphs(morphFile(0, {}, {}), morphs, error));
        EXPECT_THAT(error, HasSubstr("0 vertices"));
    }

    TEST(ESM4FaceGenTest, movesTheVerticesByTheScaledCoefficients)
    {
        // morph 0 moves the first vertex by (2, 0, 0) and the second by (0, 4, 0), at scale 0.5
        const std::string bytes
            = morphFile(2, { { 2, 0, 0, 0, 4, 0 }, { 0, 0, 10, 0, 0, 10 } }, { { 0, 0, 0, 100, 0, 0 } });
        ESM4::FaceMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceMorphs(bytes, morphs, error)) << error;

        std::vector<float> positions{ 1.f, 1.f, 1.f, 2.f, 2.f, 2.f };
        // the second symmetric morph has coefficient 0, the first 1 and the asymmetric one -0.1
        const std::size_t moved = ESM4::applyFaceMorphs(morphs, { 1.f, 0.f }, { -0.1f }, positions.data(), 2);

        EXPECT_EQ(moved, 2u);
        EXPECT_THAT(positions, Pointwise(FloatEq(), { 2.f, 1.f, 1.f, 2.f - 5.f, 4.f, 2.f }));
    }

    TEST(ESM4FaceGenTest, movesOnlyTheVerticesItIsGivenFromTheOneItIsToldToStartAt)
    {
        // three vertices in the file; a mesh of the last two
        const std::string bytes = morphFile(3, { { 2, 0, 0, 4, 0, 0, 6, 0, 0 } }, {}, 1.f);
        ESM4::FaceMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceMorphs(bytes, morphs, error)) << error;

        std::vector<float> positions{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
        EXPECT_EQ(ESM4::applyFaceMorphs(morphs, { 1.f }, {}, positions.data(), 2, 1), 1u);
        EXPECT_THAT(positions, Pointwise(FloatEq(), { 4.f, 0.f, 0.f, 6.f, 0.f, 0.f }));

        // a mesh with more vertices than are left only moves the ones that have deltas
        std::vector<float> longer(9, 0.f);
        ESM4::applyFaceMorphs(morphs, { 1.f }, {}, longer.data(), 3, 2);
        EXPECT_THAT(longer, Pointwise(FloatEq(), { 6.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }));
        EXPECT_EQ(ESM4::applyFaceMorphs(morphs, { 1.f }, {}, longer.data(), 3, 3), 0u);
    }

    TEST(ESM4FaceGenTest, ignoresCoefficientsWithNoMorphAndMorphsWithNoCoefficient)
    {
        const std::string bytes = morphFile(1, { { 2, 0, 0 } }, {}, 1.f);
        ESM4::FaceMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceMorphs(bytes, morphs, error)) << error;

        std::vector<float> positions{ 0.f, 0.f, 0.f };
        // a character with no coefficients stays as the race made it
        EXPECT_EQ(ESM4::applyFaceMorphs(morphs, {}, {}, positions.data(), 1), 0u);
        // coefficients beyond the morphs of the file are left out
        EXPECT_EQ(ESM4::applyFaceMorphs(morphs, { 1.f, 5.f, 5.f }, { 3.f }, positions.data(), 1), 1u);
        EXPECT_THAT(positions, Pointwise(FloatEq(), { 2.f, 0.f, 0.f }));
    }

    TEST(ESM4FaceGenTest, namesTheMorphFileOfAModel)
    {
        EXPECT_EQ(ESM4::faceMorphPath("meshes/characters/head/headhuman.nif"), "meshes/characters/head/headhuman.egm");
        EXPECT_EQ(ESM4::faceMorphPath("Meshes\\Characters\\Hair\\Hair01.NIF"), "Meshes\\Characters\\Hair\\Hair01.egm");
        EXPECT_EQ(ESM4::faceMorphPath("meshes/a.b/hair"), "");
        EXPECT_EQ(ESM4::faceMorphPath("meshes/head"), "");
        EXPECT_EQ(ESM4::faceMorphPath("meshes/.nif"), "");
        EXPECT_EQ(ESM4::faceMorphPath(".nif"), "");
        EXPECT_EQ(ESM4::faceMorphPath(""), "");
    }

    TEST(ESM4FaceTextureIndexTest, findsTheTextureOfACharacterByThePluginAndTheLastSixDigits)
    {
        ESM4::FaceTextureIndex index;
        index.add("textures/characters/facemods/falloutnv/000f1234_0.dds");
        index.add("textures/characters/facemods/falloutnv/00aabbcc_0.dds");
        index.add("textures/characters/bodymods/falloutnv/000f1234modbodymale.dds");
        index.add("textures/characters/bodymods/falloutnv/000f1234modbodyfemale.dds");
        EXPECT_EQ(index.size(), 4u);

        // the digits for the load order are the editor's, not the game's
        ASSERT_NE(index.find(ESM4::FaceTextureIndex::Kind::Face, "falloutnv", 0x0100f1234), nullptr);
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::Face, "falloutnv", 0x0100f1234),
            "textures/characters/facemods/falloutnv/000f1234_0.dds");
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::Face, "falloutnv", 0x00aabbcc),
            "textures/characters/facemods/falloutnv/00aabbcc_0.dds");
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::BodyMale, "falloutnv", 0x000f1234),
            "textures/characters/bodymods/falloutnv/000f1234modbodymale.dds");
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::BodyFemale, "falloutnv", 0x000f1234),
            "textures/characters/bodymods/falloutnv/000f1234modbodyfemale.dds");

        EXPECT_EQ(index.find(ESM4::FaceTextureIndex::Kind::Face, "falloutnv", 0x000f1235), nullptr);
        EXPECT_EQ(index.find(ESM4::FaceTextureIndex::Kind::BodyMale, "falloutnv", 0x00aabbcc), nullptr);
    }

    TEST(ESM4FaceTextureIndexTest, takesTheFolderOfAPluginWithOrWithoutItsExtension)
    {
        // the editor names the folder of a plugin with the extension of its file
        ESM4::FaceTextureIndex index;
        index.add("textures/characters/facemods/falloutnv.esm/00000001_0.dds");
        index.add("textures/characters/facemods/deadmoney.esm/00000001_0.dds");
        index.add("textures/characters/bodymods/honesthearts.esp/00000001modbodymale.dds");
        EXPECT_EQ(index.size(), 3u);

        for (const char* plugin : { "falloutnv", "falloutnv.esm" })
            EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::Face, plugin, 1),
                "textures/characters/facemods/falloutnv.esm/00000001_0.dds");
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::Face, "deadmoney", 1),
            "textures/characters/facemods/deadmoney.esm/00000001_0.dds");
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::BodyMale, "honesthearts.esp", 1),
            "textures/characters/bodymods/honesthearts.esp/00000001modbodymale.dds");
        // a plugin with no folder of its own, and two other folders with a file of those digits
        EXPECT_EQ(index.find(ESM4::FaceTextureIndex::Kind::Face, "honesthearts", 1), nullptr);
    }

    TEST(ESM4FaceTextureIndexTest, takesAnotherPluginsFolderOnlyWhenThereIsNoOtherChoice)
    {
        ESM4::FaceTextureIndex index;
        index.add("textures/characters/facemods/falloutnv/00000001_0.dds");
        index.add("textures/characters/facemods/deadmoney/00000001_0.dds");
        index.add("textures/characters/facemods/deadmoney/00000002_0.dds");

        // the plugin's own folder first
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::Face, "deadmoney", 1),
            "textures/characters/facemods/deadmoney/00000001_0.dds");
        // a character that its plugin has no file for, and one file elsewhere
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::Face, "falloutnv", 2),
            "textures/characters/facemods/deadmoney/00000002_0.dds");
        // two other folders have a file with those digits: no way to tell whose it is
        EXPECT_EQ(index.find(ESM4::FaceTextureIndex::Kind::Face, "honesthearts", 1), nullptr);
    }

    TEST(ESM4FaceTextureIndexTest, leavesOutPathsThatAreNotTexturesOfCharacters)
    {
        ESM4::FaceTextureIndex index;
        index.add("textures/characters/facemods/falloutnv/m000038e5_001300a5_0.dds");
        index.add("textures/characters/facemods/falloutnv/000038e5_1.dds");
        index.add("textures/characters/facemods/falloutnv/000038e5_0.png");
        index.add("textures/characters/facemods/000038e5_0.dds");
        index.add("textures/characters/facemods/falloutnv/extra/000038e5_0.dds");
        index.add("textures/characters/facemods/falloutnv/000038E5_0.dds");
        index.add("textures/characters/head/headhuman.dds");
        index.add("textures/characters/bodymods/falloutnv/000038e5modbodyother.dds");
        index.add("textures/characters/other/falloutnv/000038e5_0.dds");
        index.add("textures/clutter/falloutnv/000038e5_0.dds");
        index.add("");
        EXPECT_EQ(index.size(), 0u);
    }

    TEST(ESM4FaceTextureIndexTest, keepsTheFirstOfTwoFilesWithTheSameLastSixDigits)
    {
        ESM4::FaceTextureIndex index;
        index.add("textures/characters/facemods/anchorage/00001234_0.dds");
        index.add("textures/characters/facemods/anchorage/01001234_0.dds");
        EXPECT_EQ(index.size(), 1u);
        EXPECT_EQ(*index.find(ESM4::FaceTextureIndex::Kind::Face, "anchorage", 0x1234),
            "textures/characters/facemods/anchorage/00001234_0.dds");
    }
}
