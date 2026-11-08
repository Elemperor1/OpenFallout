#include <components/esm4/facegen.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
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

    // A .egt file: the header of 64 bytes, then the modes; a mode is the scale and the red, green and blue pictures
    // (each one row after the other) of signed bytes. `planes` has the three pictures of each mode.
    std::string textureFile(std::uint32_t width, std::uint32_t height, const std::vector<float>& scales,
        const std::vector<std::vector<std::int8_t>>& planes, std::uint32_t asymmetric = 0)
    {
        std::string out("FREGT003");
        append<std::uint32_t>(out, width);
        append<std::uint32_t>(out, height);
        append<std::uint32_t>(out, static_cast<std::uint32_t>(scales.size()));
        append<std::uint32_t>(out, asymmetric);
        append<std::uint32_t>(out, 81);
        append<std::uint32_t>(out, 0);
        out.append(64 - out.size(), '\0');
        for (std::size_t mode = 0; mode < scales.size(); ++mode)
        {
            append<float>(out, scales[mode]);
            for (const std::int8_t value : planes[mode])
                append<std::int8_t>(out, value);
        }
        return out;
    }

    // The three pictures of a mode for pictures of width by height: the same byte everywhere in the red, the green
    // and the blue
    std::vector<std::int8_t> flatPlanes(std::size_t pixels, std::int8_t red, std::int8_t green, std::int8_t blue)
    {
        std::vector<std::int8_t> out;
        for (const std::int8_t value : { red, green, blue })
            out.insert(out.end(), pixels, value);
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

        // only the version 002 has the layout that is read
        for (const char version : { '1', '3', 'x' })
        {
            std::string other = morphFile(1, { { 1, 2, 3 } }, {});
            other[7] = version;
            EXPECT_FALSE(ESM4::readFaceMorphs(other, morphs, error)) << version;
            EXPECT_THAT(error, HasSubstr("not a FaceGen"));
        }

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

    TEST(ESM4FaceGenTest, addsTheCoefficientsOfACharacterToThoseOfItsRace)
    {
        EXPECT_THAT(ESM4::addFaceCoefficients({ 1.f, 2.f, 3.f }, { 0.5f, -2.f, 0.f }), ElementsAre(1.5f, 0.f, 3.f));
        // a character with none of its own has the face of the race, and a race with none leaves the character's
        EXPECT_THAT(ESM4::addFaceCoefficients({ 1.f, 2.f }, {}), ElementsAre(1.f, 2.f));
        EXPECT_THAT(ESM4::addFaceCoefficients({}, { 4.f }), ElementsAre(4.f));
        EXPECT_TRUE(ESM4::addFaceCoefficients({}, {}).empty());
        // numbers that only one of them has are kept
        EXPECT_THAT(ESM4::addFaceCoefficients({ 1.f }, { 1.f, 5.f }), ElementsAre(2.f, 5.f));
        EXPECT_THAT(ESM4::addFaceCoefficients({ 1.f, 7.f }, { 1.f }), ElementsAre(2.f, 7.f));
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

    TEST(ESM4FaceTextureIndexTest, findsTheTextureOfACharacterInARaceWhenItHasNoneOfItsOwn)
    {
        using Kind = ESM4::FaceTextureIndex::Kind;
        ESM4::FaceTextureIndex index;
        // the names the editor wrote for characters that get their race when the game makes them: the sex, the race,
        // the character
        index.add("textures/characters/facemods/falloutnv.esm/m00000019_0013d6a2_0.dds");
        index.add("textures/characters/facemods/falloutnv.esm/f000038e5_0013d6a2_0.dds");
        index.add("textures/characters/facemods/falloutnv.esm/f00000019_0013d6a2_0.dds");
        index.add("textures/characters/bodymods/falloutnv.esm/00000019_0013d6a2modbodymale.dds");
        index.add("textures/characters/bodymods/falloutnv.esm/00000019_0013d6a2modbodyfemale.dds");
        index.add("textures/characters/facemods/falloutnv.esm/x00000019_0013d6a2_0.dds");
        EXPECT_EQ(index.size(), 5u);

        ASSERT_NE(index.findForRace(Kind::Face, "falloutnv", 0x19, 0x0013d6a2, false), nullptr);
        EXPECT_EQ(*index.findForRace(Kind::Face, "falloutnv", 0x19, 0x0013d6a2, false),
            "textures/characters/facemods/falloutnv.esm/m00000019_0013d6a2_0.dds");
        EXPECT_EQ(*index.findForRace(Kind::Face, "falloutnv", 0x19, 0x0113d6a2, true),
            "textures/characters/facemods/falloutnv.esm/f00000019_0013d6a2_0.dds");
        EXPECT_EQ(*index.findForRace(Kind::BodyMale, "falloutnv", 0x19, 0x0013d6a2, false),
            "textures/characters/bodymods/falloutnv.esm/00000019_0013d6a2modbodymale.dds");
        EXPECT_EQ(*index.findForRace(Kind::BodyFemale, "falloutnv", 0x19, 0x0013d6a2, true),
            "textures/characters/bodymods/falloutnv.esm/00000019_0013d6a2modbodyfemale.dds");
        // another race, another character, the other sex of a race that has one
        EXPECT_EQ(index.findForRace(Kind::Face, "falloutnv", 0x38e5, 0x0013d6a2, false), nullptr);
        EXPECT_NE(index.findForRace(Kind::Face, "falloutnv", 0x38e5, 0x0013d6a2, true), nullptr);
        EXPECT_EQ(index.findForRace(Kind::Face, "falloutnv", 0x19, 0x0013d6a3, false), nullptr);
        EXPECT_EQ(index.findForRace(Kind::BodyMale, "falloutnv", 0x38e5, 0x0013d6a2, false), nullptr);
        // they are not the files of a character without a race
        EXPECT_EQ(index.find(Kind::Face, "falloutnv", 0x0013d6a2), nullptr);
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

    TEST(ESM4FaceTextureIndexTest, findsInTheFolderOfAPluginOnlyWhenAskedTo)
    {
        ESM4::FaceTextureIndex index;
        index.add("textures/characters/facemods/falloutnv.esm/00000001_0.dds");
        index.add("textures/characters/facemods/deadmoney.esm/00000001_0.dds");
        index.add("textures/characters/facemods/falloutnv.esm/00000002_0.dds");

        // a character that a plugin changed has its texture in the folder of that plugin, and the one of the plugin
        // that made it is another file
        EXPECT_EQ(*index.findIn(ESM4::FaceTextureIndex::Kind::Face, "deadmoney", 0x03000001),
            "textures/characters/facemods/deadmoney.esm/00000001_0.dds");
        EXPECT_EQ(*index.findIn(ESM4::FaceTextureIndex::Kind::Face, "falloutnv.esm", 1),
            "textures/characters/facemods/falloutnv.esm/00000001_0.dds");
        // no file of that plugin: nothing, where find would take the only other file
        EXPECT_EQ(index.findIn(ESM4::FaceTextureIndex::Kind::Face, "deadmoney", 2), nullptr);
        EXPECT_NE(index.find(ESM4::FaceTextureIndex::Kind::Face, "deadmoney", 2), nullptr);
        EXPECT_EQ(index.findIn(ESM4::FaceTextureIndex::Kind::Face, "", 1), nullptr);
        EXPECT_EQ(index.findIn(ESM4::FaceTextureIndex::Kind::BodyMale, "falloutnv", 1), nullptr);
    }

    TEST(ESM4FaceTextureIndexTest, leavesOutPathsThatAreNotTexturesOfCharacters)
    {
        ESM4::FaceTextureIndex index;
        index.add("textures/characters/facemods/falloutnv/x000038e5_001300a5_0.dds");
        index.add("textures/characters/facemods/falloutnv/m000038e5_001300a5_1.dds");
        index.add("textures/characters/facemods/falloutnv/m38e5_001300a5_0.dds");
        index.add("textures/characters/bodymods/falloutnv/000038e5_001300a5modbodyother.dds");
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
    // A block of S3TC colours: the two 5:6:5 colours, then 16 indices of 2 bits, a row to a byte
    std::vector<std::uint8_t> colourBlock(std::uint16_t first, std::uint16_t second, std::uint8_t rowIndices)
    {
        return { static_cast<std::uint8_t>(first & 255), static_cast<std::uint8_t>(first >> 8),
            static_cast<std::uint8_t>(second & 255), static_cast<std::uint8_t>(second >> 8), rowIndices, rowIndices,
            rowIndices, rowIndices };
    }

    ESM4::SkinImage flatImage(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b)
    {
        ESM4::SkinImage image;
        image.mWidth = width;
        image.mHeight = height;
        for (int i = 0; i < width * height; ++i)
            image.mPixels.insert(image.mPixels.end(), { r, g, b });
        return image;
    }

    TEST(ESM4SkinImageTest, readsTheChannelsOfUncompressedPicturesInTheOrderOfTheFormat)
    {
        const std::vector<std::uint8_t> bytes{ 1, 2, 3, 255, 4, 5, 6, 255 };
        ESM4::SkinImage image;
        ASSERT_TRUE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Bgra, bytes.data(), bytes.size(), 2, 1, image));
        EXPECT_THAT(image.mPixels, ElementsAre(3, 2, 1, 6, 5, 4));
        ASSERT_TRUE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Rgba, bytes.data(), bytes.size(), 2, 1, image));
        EXPECT_THAT(image.mPixels, ElementsAre(1, 2, 3, 4, 5, 6));
        ASSERT_TRUE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Rgb, bytes.data(), bytes.size(), 2, 1, image));
        EXPECT_THAT(image.mPixels, ElementsAre(1, 2, 3, 255, 4, 5));
    }

    TEST(ESM4SkinImageTest, refusesDataThatIsTooShortOrAnEmptyPicture)
    {
        const std::vector<std::uint8_t> bytes(8, 0);
        ESM4::SkinImage image;
        EXPECT_FALSE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Rgb, bytes.data(), bytes.size(), 2, 2, image));
        EXPECT_FALSE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt5, bytes.data(), bytes.size(), 4, 4, image));
        EXPECT_FALSE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt1, bytes.data(), bytes.size(), 0, 4, image));
        EXPECT_FALSE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt1, nullptr, 8, 4, 4, image));
        // A file that declares a huge picture and holds a few bytes is refused, and the picture stays as it was
        ESM4::SkinImage untouched;
        EXPECT_FALSE(
            ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Rgba, bytes.data(), bytes.size(), 8192, 8192, untouched));
        EXPECT_TRUE(untouched.mPixels.empty());
        EXPECT_FALSE(
            ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt5, bytes.data(), bytes.size(), 8192, 8192, untouched));
        EXPECT_TRUE(untouched.mPixels.empty());
    }

    TEST(ESM4SkinImageTest, readsTheFourColoursOfABlockOfDxt1)
    {
        // red and blue; the indices of each row 0, 1, 2, 3 for the four pixels: 0b11100100
        const std::vector<std::uint8_t> block = colourBlock(0xF800, 0x001F, 0xE4);
        ESM4::SkinImage image;
        ASSERT_TRUE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt1, block.data(), block.size(), 4, 4, image));
        ASSERT_EQ(image.mPixels.size(), 48u);
        EXPECT_THAT(std::vector<int>(image.mPixels.begin(), image.mPixels.begin() + 3), ElementsAre(255, 0, 0));
        EXPECT_THAT(std::vector<int>(image.mPixels.begin() + 3, image.mPixels.begin() + 6), ElementsAre(0, 0, 255));
        // two thirds of the first and a third of the second, then a third and two thirds
        EXPECT_THAT(std::vector<int>(image.mPixels.begin() + 6, image.mPixels.begin() + 9), ElementsAre(170, 0, 85));
        EXPECT_THAT(std::vector<int>(image.mPixels.begin() + 9, image.mPixels.begin() + 12), ElementsAre(85, 0, 170));
    }

    TEST(ESM4SkinImageTest, halvesBetweenTheColoursWhenTheFirstIsTheSmallerInDxt1Only)
    {
        // The first colour is the smaller: the third is halfway and the fourth is black
        const std::vector<std::uint8_t> block = colourBlock(0x001F, 0xF800, 0xE4);
        ESM4::SkinImage image;
        ASSERT_TRUE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt1, block.data(), block.size(), 4, 4, image));
        EXPECT_THAT(std::vector<int>(image.mPixels.begin() + 6, image.mPixels.begin() + 9), ElementsAre(127, 0, 127));
        EXPECT_THAT(std::vector<int>(image.mPixels.begin() + 9, image.mPixels.begin() + 12), ElementsAre(0, 0, 0));

        // the blocks of DXT5 have their alpha first and always the four colours
        std::vector<std::uint8_t> withAlpha(8, 0);
        withAlpha.insert(withAlpha.end(), block.begin(), block.end());
        ASSERT_TRUE(
            ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt5, withAlpha.data(), withAlpha.size(), 4, 4, image));
        EXPECT_THAT(std::vector<int>(image.mPixels.begin() + 6, image.mPixels.begin() + 9), ElementsAre(85, 0, 170));
        EXPECT_THAT(std::vector<int>(image.mPixels.begin() + 9, image.mPixels.begin() + 12), ElementsAre(170, 0, 85));
    }

    TEST(ESM4SkinImageTest, keepsOnlyThePixelsInsideAPictureThatIsNotAMultipleOfTheBlock)
    {
        const std::vector<std::uint8_t> block = colourBlock(0xF800, 0xF800, 0x00);
        ESM4::SkinImage image;
        ASSERT_TRUE(ESM4::decodeSkinImage(ESM4::SkinPixelFormat::Dxt1, block.data(), block.size(), 2, 3, image));
        EXPECT_EQ(image.mWidth, 2);
        EXPECT_EQ(image.mHeight, 3);
        EXPECT_EQ(image.mPixels.size(), 18u);
    }

    constexpr std::array<float, 3> noGain{ 1.f, 1.f, 1.f };

    TEST(ESM4SkinImageTest, aModAddsTwiceWhatItHasAboveHalfGreySoThatMidGreyChangesNothing)
    {
        const ESM4::SkinImage skin = flatImage(2, 2, 100, 150, 200);
        const ESM4::SkinImage grey = ESM4::addSkinMod(skin, flatImage(1, 1, 128, 128, 128), 2.f, noGain);
        ASSERT_EQ(grey.mPixels.size(), 12u);
        EXPECT_NEAR(grey.mPixels[0], 100, 1);
        EXPECT_NEAR(grey.mPixels[1], 150, 1);
        EXPECT_NEAR(grey.mPixels[2], 200, 1);

        const ESM4::SkinImage warm = ESM4::addSkinMod(skin, flatImage(1, 1, 160, 64, 255), 2.f, noGain);
        EXPECT_NEAR(warm.mPixels[0], 165, 1);
        EXPECT_NEAR(warm.mPixels[1], 23, 1);
        // held at 255
        EXPECT_EQ(warm.mPixels[2], 255);

        // and at 0
        EXPECT_EQ(ESM4::addSkinMod(skin, flatImage(1, 1, 0, 0, 0), 2.f, noGain).mPixels[0], 0);
    }

    TEST(ESM4SkinImageTest, aModOfADarkSkinMakesTheSkinDark)
    {
        // the numbers of a head: the picture of the head and the mod of a dark character
        const ESM4::SkinImage dark
            = ESM4::addSkinMod(flatImage(1, 1, 132, 103, 86), flatImage(1, 1, 103, 97, 93), 2.f, noGain);
        EXPECT_NEAR(dark.mPixels[0], 83, 1);
        EXPECT_NEAR(dark.mPixels[1], 42, 1);
        EXPECT_NEAR(dark.mPixels[2], 17, 1);
    }

    TEST(ESM4SkinImageTest, theGainMultipliesTheSumOfEachChannel)
    {
        const ESM4::SkinImage skin = flatImage(1, 1, 100, 100, 100);
        const std::array<float, 3> gain{ 0.5f, 1.f, 2.f };
        const ESM4::SkinImage result = ESM4::addSkinMod(skin, flatImage(1, 1, 128, 128, 128), 2.f, gain);
        // 101 (the mod of 128 adds one) times the gain
        EXPECT_NEAR(result.mPixels[0], 51, 1);
        EXPECT_NEAR(result.mPixels[1], 101, 1);
        EXPECT_NEAR(result.mPixels[2], 202, 1);
    }

    TEST(ESM4SkinImageTest, aModIsStretchedOverASmallerOrBiggerSkin)
    {
        ESM4::SkinImage mod;
        mod.mWidth = 2;
        mod.mHeight = 1;
        mod.mPixels = { 0, 0, 0, 255, 255, 255 };
        const ESM4::SkinImage result = ESM4::addSkinMod(flatImage(4, 1, 100, 100, 100), mod, 1.f, noGain);
        ASSERT_EQ(result.mWidth, 4);
        EXPECT_EQ(result.mPixels[0], 0);
        EXPECT_LT(result.mPixels[0], result.mPixels[3]);
        EXPECT_LT(result.mPixels[3], result.mPixels[6]);
        EXPECT_LT(result.mPixels[6], result.mPixels[9]);
        EXPECT_NEAR(result.mPixels[9], 228, 1);
    }

    TEST(ESM4SkinImageTest, aModByAColourAndTheAverageOfAPicture)
    {
        ESM4::SkinImage mixed;
        mixed.mWidth = 2;
        mixed.mHeight = 1;
        mixed.mPixels = { 0, 100, 200, 100, 200, 100 };
        const std::array<float, 3> mean = ESM4::averageColour(mixed);
        EXPECT_FLOAT_EQ(mean[0], 50.f);
        EXPECT_FLOAT_EQ(mean[1], 150.f);
        EXPECT_FLOAT_EQ(mean[2], 150.f);

        const ESM4::SkinImage result = ESM4::addSkinMod(flatImage(1, 1, 200, 200, 200), mean, 1.f, noGain);
        EXPECT_NEAR(result.mPixels[0], 123, 1);
        EXPECT_NEAR(result.mPixels[1], 222, 1);
        EXPECT_NEAR(result.mPixels[2], 222, 1);

        EXPECT_EQ(ESM4::averageColour({}), (std::array<float, 3>{ 0.f, 0.f, 0.f }));
        EXPECT_TRUE(ESM4::addSkinMod({}, mean, 2.f, noGain).mPixels.empty());
        EXPECT_TRUE(ESM4::addSkinMod(flatImage(1, 1, 1, 1, 1), ESM4::SkinImage{}, 2.f, noGain).mPixels.empty());
    }

    TEST(ESM4FaceGenTest, namesTheTextureFileOfAModel)
    {
        EXPECT_EQ(
            ESM4::faceTexturePath("meshes/characters/head/headhuman.nif"), "meshes/characters/head/headhuman.egt");
        EXPECT_EQ(ESM4::faceTexturePath("Meshes\\Characters\\Head\\HeadFemale.NIF"),
            "Meshes\\Characters\\Head\\HeadFemale.egt");
        EXPECT_EQ(ESM4::faceTexturePath("meshes/head"), "");
        EXPECT_EQ(ESM4::faceTexturePath(""), "");
        // A body has its file the same way (the women's is in the folder of the men's, as its model is)
        EXPECT_EQ(ESM4::faceTexturePath("meshes/characters/_male/upperbodyhumanfemale.nif"),
            "meshes/characters/_male/upperbodyhumanfemale.egt");
    }

    TEST(ESM4FaceToneTest, aBodyFileOfFlatPicturesMakesOneFlatColourOverTheWholeBody)
    {
        // The upper bodies of humans have the same byte over the whole picture for each colour, so every pixel of
        // the base mod of such a body is the same
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceTextureMorphs(
            textureFile(32, 32, { 2.f }, { flatPlanes(32 * 32, 40, -20, 10) }), morphs, error))
            << error;
        const ESM4::SkinImage tone = ESM4::makeFaceToneImage(morphs, { 0.5f }, true);
        ASSERT_EQ(tone.mWidth, 32);
        ASSERT_EQ(tone.mHeight, 32);
        ASSERT_EQ(tone.mPixels.size(), 32u * 32u * 3u);
        for (std::size_t i = 3; i < tone.mPixels.size(); ++i)
            EXPECT_EQ(tone.mPixels[i], tone.mPixels[i % 3]) << i;
        // Positive for the first colour, negative for the second: the base mod moves from mid grey the way the file
        // says
        EXPECT_GT(tone.mPixels[0], 128);
        EXPECT_LT(tone.mPixels[1], 128);
    }

    TEST(ESM4FaceGenTest, readsATextureFile)
    {
        const std::string bytes
            = textureFile(2, 2, { 0.5f, 2.f }, { flatPlanes(4, 1, 2, 3), flatPlanes(4, -4, -5, -6) });
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceTextureMorphs(bytes, morphs, error)) << error;
        EXPECT_EQ(morphs.mWidth, 2);
        EXPECT_EQ(morphs.mHeight, 2);
        EXPECT_THAT(morphs.mScales, ElementsAre(0.5f, 2.f));
        ASSERT_EQ(morphs.mPlanes.size(), 24u);
        EXPECT_EQ(morphs.mPlanes[0], 1);
        EXPECT_EQ(morphs.mPlanes[4], 2);
        EXPECT_EQ(morphs.mPlanes[8], 3);
        EXPECT_EQ(morphs.mPlanes[12], -4);
        EXPECT_EQ(morphs.mPlanes[23], -6);
    }

    TEST(ESM4FaceGenTest, refusesTextureFilesThatDoNotFitTogether)
    {
        const std::string good = textureFile(2, 2, { 1.f }, { flatPlanes(4, 1, 2, 3) });
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceTextureMorphs(good, morphs, error)) << error;

        EXPECT_FALSE(ESM4::readFaceTextureMorphs("FREGT", morphs, error));
        EXPECT_FALSE(ESM4::readFaceTextureMorphs("", morphs, error));
        std::string version = good;
        version[7] = '2';
        EXPECT_FALSE(ESM4::readFaceTextureMorphs(version, morphs, error));
        EXPECT_FALSE(ESM4::readFaceTextureMorphs(good.substr(0, good.size() - 1), morphs, error));
        EXPECT_FALSE(ESM4::readFaceTextureMorphs(good + "x", morphs, error));
        EXPECT_FALSE(ESM4::readFaceTextureMorphs(textureFile(0, 2, {}, {}), morphs, error));
        EXPECT_FALSE(ESM4::readFaceTextureMorphs(textureFile(2, 0, {}, {}), morphs, error));
        // A size that no file this long could hold is refused before anything is made for it
        EXPECT_FALSE(ESM4::readFaceTextureMorphs(textureFile(65536, 65536, { 1.f }, { { 1 } }), morphs, error));
        EXPECT_FALSE(error.empty());
        // Modes for FGTA that the file lists but does not hold
        EXPECT_FALSE(
            ESM4::readFaceTextureMorphs(textureFile(2, 2, { 1.f }, { flatPlanes(4, 1, 2, 3) }, 1), morphs, error));
    }

    TEST(ESM4FaceGenTest, keepsOnlyTheModesForFgtsOfATextureFile)
    {
        // A file that holds a mode for FGTA after the one for FGTS: the FGTA mode is skipped over, not used
        const std::string bytes = textureFile(1, 1, { 1.f, 3.f }, { flatPlanes(1, 7, 8, 9), flatPlanes(1, 1, 1, 1) });
        std::string asymmetric = bytes;
        std::uint32_t one = 1;
        std::memcpy(asymmetric.data() + 16, &one, sizeof(one));
        std::memcpy(asymmetric.data() + 20, &one, sizeof(one));
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceTextureMorphs(asymmetric, morphs, error)) << error;
        EXPECT_THAT(morphs.mScales, ElementsAre(1.f));
        EXPECT_EQ(morphs.mPlanes.size(), 3u);
        EXPECT_EQ(morphs.mPlanes[0], 7);
    }

    TEST(ESM4FaceToneTest, aFaceWithNoCoefficientsIsMidGreyThatChangesNothing)
    {
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(
            ESM4::readFaceTextureMorphs(textureFile(2, 1, { 1.f }, { flatPlanes(2, 50, -50, 100) }), morphs, error));
        for (const std::vector<float>& coefficients : { std::vector<float>{}, std::vector<float>{ 0.f } })
        {
            const ESM4::SkinImage tone = ESM4::makeFaceToneImage(morphs, coefficients, false);
            ASSERT_EQ(tone.mWidth, 2);
            ASSERT_EQ(tone.mHeight, 1);
            ASSERT_EQ(tone.mPixels.size(), 6u);
            for (const std::uint8_t value : tone.mPixels)
                EXPECT_EQ(value, 128);
        }
    }

    TEST(ESM4FaceToneTest, aCoefficientMovesEachColourByItsScaleAndByteAndTheModKeepsHalfOfThat)
    {
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(
            ESM4::readFaceTextureMorphs(textureFile(1, 1, { 1.f }, { flatPlanes(1, 40, -40, 0) }), morphs, error));
        const ESM4::SkinImage tone = ESM4::makeFaceToneImage(morphs, { 1.f }, false);
        ASSERT_EQ(tone.mPixels.size(), 3u);
        // 40 steps are 20 of the mod above its middle (127.5): the skin shader doubles that
        EXPECT_EQ(tone.mPixels[0], 148);
        EXPECT_EQ(tone.mPixels[1], 108);
        EXPECT_EQ(tone.mPixels[2], 128);

        // Twice the scale, half the coefficient: the same
        ASSERT_TRUE(
            ESM4::readFaceTextureMorphs(textureFile(1, 1, { 2.f }, { flatPlanes(1, 40, -40, 0) }), morphs, error));
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { 0.5f }, false).mPixels, tone.mPixels);
    }

    TEST(ESM4FaceToneTest, theModesAddAndTheStepsAreHeldBetweenMinusAndPlus255)
    {
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(ESM4::readFaceTextureMorphs(
            textureFile(1, 1, { 1.f, 1.f }, { flatPlanes(1, 20, 20, 20), flatPlanes(1, 20, -20, 120) }), morphs,
            error));
        const ESM4::SkinImage added = ESM4::makeFaceToneImage(morphs, { 1.f, 1.f }, false);
        EXPECT_EQ(added.mPixels[0], 148); // 40 steps
        EXPECT_EQ(added.mPixels[1], 128); // 0
        EXPECT_EQ(added.mPixels[2], 198); // 140 steps
        const ESM4::SkinImage held = ESM4::makeFaceToneImage(morphs, { 10.f, -10.f }, false);
        EXPECT_EQ(held.mPixels[0], 128); // 200 - 200 steps
        EXPECT_EQ(held.mPixels[1], 255); // 400 steps held at 255
        EXPECT_EQ(held.mPixels[2], 0); // -1000 steps held at -255
    }

    TEST(ESM4FaceToneTest, theExecutableCutsTheCoefficientAndTheScaleTowardsZeroAfterTimes256)
    {
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(
            ESM4::readFaceTextureMorphs(textureFile(1, 1, { 1.f }, { flatPlanes(1, 127, 127, 127) }), morphs, error));
        // Under 1/256 the whole number is 0 and nothing moves, though 0.0039 * 127 is about half a step
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { 0.0039f }, false).mPixels[0], 128);
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { -0.0039f }, false).mPixels[0], 128);
        // 1/256 is a whole 1: 127 / 256 = 0.496 of a step, half of it kept
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { 1.f / 256.f }, false).mPixels[0], 128);
        // 0.999 is 255, not 256: 127 * 255 / 256 = 126.5 steps, 63.25 above the middle
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { 0.999f }, false).mPixels[0], 191);
    }

    TEST(ESM4FaceToneTest, aNumberThatIsNotANumberMovesNothingAndCoefficientsBeyondTheModesAreLeftOut)
    {
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        ASSERT_TRUE(
            ESM4::readFaceTextureMorphs(textureFile(1, 1, { 1.f }, { flatPlanes(1, 40, 40, 40) }), morphs, error));
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { nan }, false).mPixels[0], 128);
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { infinity }, false).mPixels[0], 128);
        EXPECT_EQ(ESM4::makeFaceToneImage(morphs, { 1.f, 5.f, 7.f }, false).mPixels[0], 148);
    }

    TEST(ESM4FaceToneTest, theRowsCanBeTurnedOver)
    {
        ESM4::FaceTextureMorphs morphs;
        std::string error;
        // Two rows of one pixel: the red byte is 40 in the first and -40 in the second
        std::vector<std::int8_t> planes{ 40, -40, 0, 0, 0, 0 };
        ASSERT_TRUE(ESM4::readFaceTextureMorphs(textureFile(1, 2, { 1.f }, { planes }), morphs, error));
        const ESM4::SkinImage straight = ESM4::makeFaceToneImage(morphs, { 1.f }, false);
        const ESM4::SkinImage flipped = ESM4::makeFaceToneImage(morphs, { 1.f }, true);
        EXPECT_EQ(straight.mPixels[0], 148);
        EXPECT_EQ(straight.mPixels[3], 108);
        EXPECT_EQ(flipped.mPixels[0], 108);
        EXPECT_EQ(flipped.mPixels[3], 148);
    }

    TEST(ESM4FaceToneTest, aMorphSetThatDoesNotFitItsPicturesGivesNoPicture)
    {
        EXPECT_TRUE(ESM4::makeFaceToneImage({}, { 1.f }, false).mPixels.empty());
        ESM4::FaceTextureMorphs morphs;
        morphs.mWidth = 2;
        morphs.mHeight = 2;
        morphs.mScales = { 1.f };
        morphs.mPlanes.assign(5, 1);
        EXPECT_TRUE(ESM4::makeFaceToneImage(morphs, { 1.f }, false).mPixels.empty());
    }
}
