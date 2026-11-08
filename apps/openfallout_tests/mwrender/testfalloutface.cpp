#include <apps/openfallout/mwrender/falloutface.hpp>

#include <components/misc/osguservalues.hpp>
#include <components/sceneutil/material.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/texturetype.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/manager.hpp>

#include <gtest/gtest.h>

#include <osg/Geometry>
#include <osg/Group>
#include <osg/Matrix>
#include <osg/Quat>
#include <osg/Texture2D>
#include <osg/Uniform>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace OFRender;

    // A file of morphs for the given number of vertices; every symmetric morph moves the vertex i by (deltas[m], 0, 0)
    // times its scale (1), and there is no asymmetric one
    ESM4::FaceMorphs makeMorphs(std::uint32_t vertices, const std::vector<std::int16_t>& deltaOfMorph)
    {
        ESM4::FaceMorphs morphs;
        morphs.mVertexCount = vertices;
        for (const std::int16_t delta : deltaOfMorph)
        {
            ESM4::FaceMorph morph;
            morph.mScale = 1.f;
            for (std::uint32_t i = 0; i < vertices; ++i)
            {
                morph.mDeltas.push_back(delta);
                morph.mDeltas.push_back(0);
                morph.mDeltas.push_back(static_cast<std::int16_t>(i));
            }
            morphs.mSymmetric.push_back(std::move(morph));
        }
        return morphs;
    }

    osg::ref_ptr<osg::Geometry> makeGeometry(std::size_t vertices)
    {
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> positions = new osg::Vec3Array;
        for (std::size_t i = 0; i < vertices; ++i)
            positions->push_back(osg::Vec3f(static_cast<float>(i), 10.f, 20.f));
        geometry->setVertexArray(positions);
        geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices)));
        return geometry;
    }

    const osg::Vec3Array& positionsOf(const osg::Geometry& geometry)
    {
        return *static_cast<const osg::Vec3Array*>(geometry.getVertexArray());
    }

    osg::Geometry& childGeometry(osg::Group& part, unsigned int index)
    {
        return *part.getChild(index)->asGeometry();
    }

    TEST(OpenFalloutRenderMorphFaceMeshes, movesTheMeshOfThisPartAndNoOtherThatSharesIt)
    {
        const osg::ref_ptr<osg::Geometry> shared = makeGeometry(3);
        osg::ref_ptr<osg::Group> mine = new osg::Group;
        osg::ref_ptr<osg::Group> theirs = new osg::Group;
        mine->addChild(shared);
        theirs->addChild(shared);

        // the bounds of the mesh are known before, as they are once it has been drawn
        EXPECT_FLOAT_EQ(shared->getBoundingBox().xMax(), 2.f);

        const ESM4::FaceMorphs morphs = makeMorphs(3, { 2 });
        EXPECT_EQ(morphFaceMeshes(*mine, morphs, { 1.5f }, {}), 1u);

        // and those of the moved mesh are where its vertices are
        EXPECT_FLOAT_EQ(childGeometry(*mine, 0).getBoundingBox().xMax(), 5.f);
        EXPECT_FLOAT_EQ(shared->getBoundingBox().xMax(), 2.f);

        // x moves by 2 * 1.5 and z by i * 1.5
        const osg::Vec3Array& moved = positionsOf(childGeometry(*mine, 0));
        ASSERT_EQ(moved.size(), 3u);
        EXPECT_FLOAT_EQ(moved[0].x(), 3.f);
        EXPECT_FLOAT_EQ(moved[2].x(), 5.f);
        EXPECT_FLOAT_EQ(moved[2].z(), 23.f);
        EXPECT_FLOAT_EQ(moved[0].y(), 10.f);

        // the mesh of the other character, and the one it was made from, are as they were
        EXPECT_EQ(theirs->getChild(0), shared.get());
        EXPECT_FLOAT_EQ(positionsOf(*shared)[2].x(), 2.f);
        EXPECT_FLOAT_EQ(positionsOf(*shared)[2].z(), 20.f);
        EXPECT_NE(mine->getChild(0), shared.get());
        // what the mesh is drawn with is kept
        EXPECT_EQ(childGeometry(*mine, 0).getNumPrimitiveSets(), 1u);
    }

    TEST(OpenFalloutRenderMorphFaceMeshes, movesTheSourceOfASkinnedMesh)
    {
        const osg::ref_ptr<osg::Geometry> source = makeGeometry(2);
        osg::ref_ptr<SceneUtil::RigGeometry> rig = new SceneUtil::RigGeometry;
        rig->setSourceGeometry(source);
        osg::ref_ptr<osg::Group> part = new osg::Group;
        part->addChild(rig);

        const ESM4::FaceMorphs morphs = makeMorphs(2, { 4 });
        EXPECT_EQ(morphFaceMeshes(*part, morphs, { 1.f }, {}), 1u);

        EXPECT_NE(rig->getSourceGeometry().get(), source.get());
        EXPECT_FLOAT_EQ(positionsOf(*rig->getSourceGeometry())[1].x(), 5.f);
        EXPECT_FLOAT_EQ(positionsOf(*source)[1].x(), 1.f);
    }

    TEST(OpenFalloutRenderMorphFaceMeshes, makesTheBoundsOfASkinnedMeshLargerByTheFarthestMove)
    {
        osg::ref_ptr<SceneUtil::RigGeometry> rig = new SceneUtil::RigGeometry;
        rig->setSourceGeometry(makeGeometry(2));
        osg::ref_ptr<osg::Group> part = new osg::Group;
        part->addChild(rig);
        EXPECT_FLOAT_EQ(rig->getBoundsMargin(), 0.f);

        // the vertex 1 moves by (4, 0, 1)
        EXPECT_EQ(morphFaceMeshes(*part, makeMorphs(2, { 4 }), { 1.f }, {}), 1u);
        EXPECT_NEAR(rig->getBoundsMargin(), std::sqrt(17.f), 1e-4f);

        // a copy of the mesh, as the scene manager makes for the next character, keeps the margin of its source
        osg::ref_ptr<SceneUtil::RigGeometry> copy = new SceneUtil::RigGeometry(*rig, osg::CopyOp::SHALLOW_COPY);
        EXPECT_NEAR(copy->getBoundsMargin(), std::sqrt(17.f), 1e-4f);

        // and a mesh that no morph moves keeps a margin of none
        osg::ref_ptr<SceneUtil::RigGeometry> other = new SceneUtil::RigGeometry;
        other->setSourceGeometry(makeGeometry(3));
        osg::ref_ptr<osg::Group> otherPart = new osg::Group;
        otherPart->addChild(other);
        EXPECT_EQ(morphFaceMeshes(*otherPart, makeMorphs(5, { 4 }), { 1.f }, {}), 0u);
        EXPECT_FLOAT_EQ(other->getBoundsMargin(), 0.f);
    }

    TEST(OpenFalloutRenderMorphFaceMeshes, leavesAPartAloneWhoseVerticesAreNotThoseOfTheFile)
    {
        const osg::ref_ptr<osg::Geometry> shared = makeGeometry(3);
        osg::ref_ptr<osg::Group> part = new osg::Group;
        part->addChild(shared);

        const ESM4::FaceMorphs morphs = makeMorphs(5, { 2 });
        EXPECT_EQ(morphFaceMeshes(*part, morphs, { 1.f }, {}), 0u);
        EXPECT_EQ(part->getChild(0), shared.get());
    }

    TEST(OpenFalloutRenderMorphFaceMeshes, movesTheFirstVerticesOfALongerFileWhenToldTo)
    {
        // a head of 3 vertices and a file of 5: the head is the first three
        const ESM4::FaceMorphs morphs = makeMorphs(5, { 2 });
        {
            osg::ref_ptr<osg::Group> part = new osg::Group;
            part->addChild(makeGeometry(3));
            EXPECT_EQ(morphFaceMeshes(*part, morphs, { 1.f }, {}, true), 1u);
            const osg::Vec3Array& moved = positionsOf(childGeometry(*part, 0));
            ASSERT_EQ(moved.size(), 3u);
            // x moves by 2 and z by the number of the vertex, so no vertex got the morph of one after the third
            EXPECT_FLOAT_EQ(moved[0].x(), 2.f);
            EXPECT_FLOAT_EQ(moved[2].x(), 4.f);
            EXPECT_FLOAT_EQ(moved[2].z(), 22.f);
        }
        {
            // not unless told to
            osg::ref_ptr<osg::Group> part = new osg::Group;
            part->addChild(makeGeometry(3));
            EXPECT_EQ(morphFaceMeshes(*part, morphs, { 1.f }, {}), 0u);
            EXPECT_EQ(morphFaceMeshes(*part, morphs, { 1.f }, {}, false), 0u);
        }
        {
            // not for several meshes, and not for a part with more vertices than the file
            osg::ref_ptr<osg::Group> several = new osg::Group;
            several->addChild(makeGeometry(2));
            several->addChild(makeGeometry(2));
            EXPECT_EQ(morphFaceMeshes(*several, morphs, { 1.f }, {}, true), 0u);

            osg::ref_ptr<osg::Group> bigger = new osg::Group;
            bigger->addChild(makeGeometry(7));
            EXPECT_EQ(morphFaceMeshes(*bigger, morphs, { 1.f }, {}, true), 0u);
        }
    }

    TEST(OpenFalloutRenderMorphFaceMeshes, movesTheMeshesOfAFileForSeveralOneAfterTheOther)
    {
        osg::ref_ptr<osg::Group> part = new osg::Group;
        part->addChild(makeGeometry(2));
        part->addChild(makeGeometry(3));

        // five vertices in the file; the z delta of a vertex is its number in the file
        const ESM4::FaceMorphs morphs = makeMorphs(5, { 0 });
        EXPECT_EQ(morphFaceMeshes(*part, morphs, { 1.f }, {}), 2u);

        EXPECT_FLOAT_EQ(positionsOf(childGeometry(*part, 0))[1].z(), 21.f);
        EXPECT_FLOAT_EQ(positionsOf(childGeometry(*part, 1))[0].z(), 22.f);
        EXPECT_FLOAT_EQ(positionsOf(childGeometry(*part, 1))[2].z(), 24.f);
    }

    TEST(OpenFalloutRenderMorphFaceMeshes, movesOnlyTheMeshesThatHaveTheVerticesOfTheFileWhenOneDoes)
    {
        osg::ref_ptr<osg::Group> part = new osg::Group;
        const osg::ref_ptr<osg::Geometry> other = makeGeometry(2);
        part->addChild(other);
        part->addChild(makeGeometry(3));

        EXPECT_EQ(morphFaceMeshes(*part, makeMorphs(3, { 1 }), { 1.f }, {}), 1u);
        EXPECT_EQ(part->getChild(0), other.get());
        EXPECT_FLOAT_EQ(positionsOf(childGeometry(*part, 1))[0].x(), 1.f);
    }

    osg::ref_ptr<osg::Image> makeImage(int size)
    {
        osg::ref_ptr<osg::Image> image = new osg::Image;
        image->allocateImage(size, size, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        return image;
    }

    TEST(OpenFalloutRenderReplaceDiffuseMap, givesThePartItsOwnTextureAndLeavesTheSharedOneAlone)
    {
        const osg::ref_ptr<osg::Image> original = makeImage(2);
        const osg::ref_ptr<osg::Image> replacement = makeImage(4);

        osg::ref_ptr<osg::StateSet> shared = new osg::StateSet;
        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(original);
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        shared->setTextureAttribute(0, texture, osg::StateAttribute::ON);
        shared->setTextureAttribute(0, new SceneUtil::TextureType("diffuseMap"), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Texture2D> normal = new osg::Texture2D(original);
        shared->setTextureAttribute(1, normal, osg::StateAttribute::ON);
        shared->setTextureAttribute(1, new SceneUtil::TextureType("normalMap"), osg::StateAttribute::ON);

        osg::ref_ptr<osg::Group> mine = new osg::Group;
        mine->setStateSet(shared);
        osg::ref_ptr<osg::Group> theirs = new osg::Group;
        theirs->setStateSet(shared);

        EXPECT_EQ(replaceDiffuseMap(*mine, replacement), 1u);

        EXPECT_EQ(theirs->getStateSet(), shared.get());
        EXPECT_EQ(static_cast<const osg::Texture2D*>(shared->getTextureAttribute(0, osg::StateAttribute::TEXTURE))
                      ->getImage(),
            original.get());

        ASSERT_NE(mine->getStateSet(), shared.get());
        const osg::Texture2D* own = static_cast<const osg::Texture2D*>(
            mine->getStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
        EXPECT_EQ(own->getImage(), replacement.get());
        EXPECT_EQ(own->getWrap(osg::Texture::WRAP_S), osg::Texture::CLAMP_TO_EDGE);
        // the other maps, and the names of the units, stay
        EXPECT_EQ(mine->getStateSet()->getTextureAttribute(1, osg::StateAttribute::TEXTURE), normal.get());
        EXPECT_NE(mine->getStateSet()->getTextureAttribute(0, SceneUtil::TextureType::AttributeType), nullptr);
    }

    TEST(OpenFalloutRenderReplaceDiffuseMap, takesTheFirstTextureForTheDiffuseMapWhenItHasNoName)
    {
        osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;
        stateset->setTextureAttribute(0, new osg::Texture2D(makeImage(2)), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Group> part = new osg::Group;
        osg::ref_ptr<osg::Group> child = new osg::Group;
        child->setStateSet(stateset);
        part->addChild(child);

        EXPECT_EQ(replaceDiffuseMap(*part, makeImage(8)), 1u);
        EXPECT_NE(child->getStateSet(), stateset.get());
    }

    TEST(OpenFalloutRenderReplaceDiffuseMap, doesNothingForAPartWithoutADiffuseMapOrWithoutAnImage)
    {
        osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;
        stateset->setTextureAttribute(1, new osg::Texture2D(makeImage(2)), osg::StateAttribute::ON);
        stateset->setTextureAttribute(1, new SceneUtil::TextureType("normalMap"), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Group> part = new osg::Group;
        part->setStateSet(stateset);

        EXPECT_EQ(replaceDiffuseMap(*part, makeImage(8)), 0u);
        EXPECT_EQ(part->getStateSet(), stateset.get());
        EXPECT_EQ(replaceDiffuseMap(*part, nullptr), 0u);
    }
    osg::ref_ptr<osg::Image> makeRgb(
        int width, int height, unsigned char r, unsigned char g, unsigned char b, const std::string& name)
    {
        osg::ref_ptr<osg::Image> image = new osg::Image;
        image->allocateImage(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE);
        for (int i = 0; i < width * height; ++i)
        {
            image->data()[i * 3] = r;
            image->data()[i * 3 + 1] = g;
            image->data()[i * 3 + 2] = b;
        }
        image->setFileName(name);
        return image;
    }

    TEST(OpenFalloutRenderReplaceDiffuseMap, asksTheChooserForEachMapWithThePictureItHasAndLeavesItWhenTheAnswerIsNone)
    {
        const osg::ref_ptr<osg::Image> original = makeImage(2);
        osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;
        stateset->setTextureAttribute(0, new osg::Texture2D(original), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Group> part = new osg::Group;
        part->setStateSet(stateset);

        const osg::Image* asked = nullptr;
        EXPECT_EQ(replaceDiffuseMap(*part,
                      [&](const osg::Image* image) {
                          asked = image;
                          return osg::ref_ptr<osg::Image>();
                      }),
            0u);
        EXPECT_EQ(asked, original.get());
        EXPECT_EQ(part->getStateSet(), stateset.get());

        const osg::ref_ptr<osg::Image> other = makeImage(8);
        EXPECT_EQ(replaceDiffuseMap(*part, [&](const osg::Image*) { return other; }), 1u);
        const osg::Texture2D* own = static_cast<const osg::Texture2D*>(
            part->getStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
        EXPECT_EQ(own->getImage(), other.get());
        EXPECT_EQ(own->getInternalFormatMode(), osg::Texture::USE_IMAGE_DATA_FORMAT);
        EXPECT_EQ(replaceDiffuseMap(*part, DiffuseChooser()), 0u);
    }

    TEST(OpenFalloutRenderTintSkinImage, addsTwiceWhatTheTextureHasAboveHalfGreyToTheSkin)
    {
        const osg::ref_ptr<osg::Image> skin = makeRgb(4, 4, 100, 150, 200, "textures/characters/male/skin.dds");
        const osg::ref_ptr<osg::Image> tint
            = makeRgb(2, 2, 128, 160, 64, "textures/characters/facemods/a/00000001_0.dds");

        const osg::ref_ptr<osg::Image> tinted = tintSkinImage(*skin, *tint);
        ASSERT_NE(tinted, nullptr);
        EXPECT_EQ(tinted->s(), 4);
        EXPECT_EQ(tinted->t(), 4);
        EXPECT_EQ(tinted->getPixelFormat(), static_cast<GLenum>(GL_RGB));
        // (the skin + 2 * (texture - 127.5)) times the default of the detail mod, (0.9725, 1.0196, 0.9725)
        EXPECT_NEAR(tinted->data()[0], 98, 1);
        EXPECT_NEAR(tinted->data()[1], 219, 1);
        EXPECT_NEAR(tinted->data()[2], 71, 1);
        // the skin of the race is not changed
        EXPECT_EQ(skin->data()[0], 100);
        // and the same two files give the same picture while it is in use
        EXPECT_EQ(tintSkinImage(*skin, *tint), tinted);
    }

    TEST(OpenFalloutRenderTintSkinImage, byTheAverageOfATintThatIsNotLaidOutLikeTheSkin)
    {
        const osg::ref_ptr<osg::Image> skin = makeRgb(2, 2, 200, 200, 200, "textures/characters/male/hand.dds");
        osg::ref_ptr<osg::Image> tint
            = makeRgb(2, 1, 0, 0, 0, "textures/characters/bodymods/a/00000001modbodymale.dds");
        tint->data()[3] = 255;
        tint->data()[4] = 255;
        tint->data()[5] = 255;

        const osg::ref_ptr<osg::Image> tinted = tintSkinImageByAverage(*skin, *tint);
        ASSERT_NE(tinted, nullptr);
        // the average, 127.5 of 255, changes nothing but the default of the detail mod, 0.9725 for red and blue
        EXPECT_NEAR(tinted->data()[0], 195, 1);
        EXPECT_NEAR(tinted->data()[11], 195, 1);
    }

    TEST(OpenFalloutRenderTintSkinImage, isNothingForAPictureInAFormatThatCannotBeRead)
    {
        const osg::ref_ptr<osg::Image> skin = makeRgb(2, 2, 1, 2, 3, "a");
        osg::ref_ptr<osg::Image> odd = new osg::Image;
        odd->allocateImage(2, 2, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE);
        EXPECT_EQ(tintSkinImage(*skin, *odd), nullptr);
        EXPECT_EQ(tintSkinImage(*odd, *skin), nullptr);
    }

    osg::ref_ptr<osg::Group> makeHair()
    {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        root->setName("HairMessy01");
        osg::ref_ptr<osg::Group> noHat = new osg::Group;
        noHat->setName("Nohat");
        osg::ref_ptr<osg::Group> hat = new osg::Group;
        hat->setName("HAT");
        root->addChild(noHat);
        root->addChild(hat);
        return root;
    }

    // A .egt file of one mode (scale 1) with pictures of one pixel: the given bytes for red, green and blue
    std::string makeToneFile(std::int8_t red, std::int8_t green, std::int8_t blue)
    {
        std::string out("FREGT003");
        for (const std::uint32_t value : { 1u, 1u, 1u, 0u, 81u, 0u })
            out.append(reinterpret_cast<const char*>(&value), sizeof(value));
        out.append(64 - out.size(), '\0');
        const float scale = 1.f;
        out.append(reinterpret_cast<const char*>(&scale), sizeof(scale));
        for (const std::int8_t value : { red, green, blue })
            out.push_back(static_cast<char>(value));
        return out;
    }

    constexpr VFS::Path::NormalizedView sOwnFile("meshes/characters/_male/bodyofarace.egt");
    constexpr VFS::Path::NormalizedView sWomanFile("meshes/characters/_male/upperbodyhumanfemale.egt");
    constexpr VFS::Path::NormalizedView sManFile("meshes/characters/_male/upperbodyhumanmale.egt");

    TEST(OpenFalloutRenderMakeBodyToneImage, takesTheFileOfTheModelOfTheBodyWhenItHasOne)
    {
        TestingOpenMW::VFSTestFile own(makeToneFile(50, 0, 0));
        TestingOpenMW::VFSTestFile human(makeToneFile(-50, 0, 0));
        const auto vfs = TestingOpenMW::createTestVFS({ { sOwnFile, &own }, { sWomanFile, &human } });

        const osg::ref_ptr<osg::Image> image
            = makeBodyToneImage(*vfs, "Characters\\_Male\\BodyOfARace.nif", true, { 1.f });
        ASSERT_NE(image, nullptr);
        EXPECT_GT(image->data()[0], 128);
    }

    TEST(OpenFalloutRenderMakeBodyToneImage, takesTheHumanBodyOfTheSexWhenTheModelHasNoFile)
    {
        // The model of the human woman's skin, as a race names it (femaleupperbody.nif has no .egt in the games)
        TestingOpenMW::VFSTestFile woman(makeToneFile(50, 0, 0));
        TestingOpenMW::VFSTestFile man(makeToneFile(-50, 0, 0));
        const auto vfs = TestingOpenMW::createTestVFS({ { sWomanFile, &woman }, { sManFile, &man } });

        const osg::ref_ptr<osg::Image> forWoman
            = makeBodyToneImage(*vfs, "Characters\\_Male\\FemaleUpperBody.nif", true, { 1.f });
        ASSERT_NE(forWoman, nullptr);
        EXPECT_GT(forWoman->data()[0], 128);

        const osg::ref_ptr<osg::Image> forMan
            = makeBodyToneImage(*vfs, "Characters\\_Male\\MaleUpperBody.nif", false, { 1.f });
        ASSERT_NE(forMan, nullptr);
        EXPECT_LT(forMan->data()[0], 128);

        // a race that names no body at all has the human one too (the files are kept by their path for the whole run,
        // so what a missing file gives cannot be tested after these, which have made the human files)
        EXPECT_NE(makeBodyToneImage(*vfs, std::string(), true, { 1.f }), nullptr);
    }

    TEST(OpenFalloutRenderChooseHairShape, keepsTheShapeForABareHeadOrForAHat)
    {
        osg::ref_ptr<osg::Group> bare = makeHair();
        EXPECT_EQ(chooseHairShape(*bare, false), 1u);
        ASSERT_EQ(bare->getNumChildren(), 1u);
        EXPECT_EQ(bare->getChild(0)->getName(), "Nohat");

        osg::ref_ptr<osg::Group> hatted = makeHair();
        EXPECT_EQ(chooseHairShape(*hatted, true), 1u);
        ASSERT_EQ(hatted->getNumChildren(), 1u);
        EXPECT_EQ(hatted->getChild(0)->getName(), "HAT");
    }

    TEST(OpenFalloutRenderChooseHairShape, takesOutAShapeWhoseOnlyOwnerIsItsParentAndSharedOnesFromEveryParent)
    {
        // the group is freed with its last parent: it must not be asked anything after that
        osg::ref_ptr<osg::Group> root = new osg::Group;
        osg::ref_ptr<osg::Group> other = new osg::Group;
        root->addChild(new osg::Group);
        root->getChild(0)->setName("NoHat");
        root->addChild(new osg::Group);
        root->getChild(1)->setName("Hat");
        osg::ref_ptr<osg::Group> shared = new osg::Group;
        shared->setName("hat");
        root->addChild(shared);
        other->addChild(shared);
        shared = nullptr;
        EXPECT_EQ(chooseHairShape(*root, false), 3u);
        EXPECT_EQ(root->getNumChildren(), 1u);
        EXPECT_EQ(other->getNumChildren(), 0u);
    }

    TEST(OpenFalloutRenderChooseHairShape, leavesAModelThatHasOnlyOneOfThemAlone)
    {
        osg::ref_ptr<osg::Group> beard = new osg::Group;
        osg::ref_ptr<osg::Group> shape = new osg::Group;
        shape->setName("BeardThin:0");
        beard->addChild(shape);
        EXPECT_EQ(chooseHairShape(*beard, true), 0u);
        EXPECT_EQ(chooseHairShape(*beard, false), 0u);
        EXPECT_EQ(beard->getNumChildren(), 1u);

        osg::ref_ptr<osg::Group> onlyHat = new osg::Group;
        osg::ref_ptr<osg::Group> hat = new osg::Group;
        hat->setName("Hat");
        onlyHat->addChild(hat);
        EXPECT_EQ(chooseHairShape(*onlyHat, false), 0u);
        EXPECT_EQ(onlyHat->getNumChildren(), 1u);
    }
    TEST(OpenFalloutRenderTintHair, givesTheShapesTheirOwnMaterialOfTheColourWithoutTheColoursOfTheVertices)
    {
        SceneUtil::MaterialConfig config;
        config.mVertexColorMode = SceneUtil::VertexColorModes::AmbientAndDiffuse;
        osg::ref_ptr<SceneUtil::Material> material = new SceneUtil::Material(config);
        osg::ref_ptr<osg::StateSet> shared = new osg::StateSet;
        shared->setTextureAttribute(0, new osg::Texture2D(makeImage(2)), osg::StateAttribute::ON);
        shared->setAttributeAndModes(material, osg::StateAttribute::ON);
        // what the setup of the shaders made of the material
        material->setStateSet(shared);
        const osg::Uniform* sharedDiffuse = shared->getUniform("material.diffuse");
        ASSERT_NE(sharedDiffuse, nullptr);

        osg::ref_ptr<osg::Group> mine = new osg::Group;
        mine->setStateSet(shared);
        osg::ref_ptr<osg::Group> theirs = new osg::Group;
        theirs->setStateSet(shared);
        osg::ref_ptr<osg::Group> root = new osg::Group;
        root->addChild(mine);

        EXPECT_EQ(tintHair(*root, osg::Vec3f(0.5f, 0.25f, 0.1f)), 1u);

        EXPECT_EQ(theirs->getStateSet(), shared.get());
        EXPECT_EQ(shared->getAttribute(osg::StateAttribute::MATERIAL), material.get());
        EXPECT_EQ(material->getVertexColorMode(), SceneUtil::VertexColorModes::AmbientAndDiffuse);

        ASSERT_NE(mine->getStateSet(), shared.get());
        const SceneUtil::Material* own = dynamic_cast<const SceneUtil::Material*>(
            mine->getStateSet()->getAttribute(osg::StateAttribute::MATERIAL));
        ASSERT_NE(own, nullptr);
        EXPECT_EQ(own->getVertexColorMode(), SceneUtil::VertexColorModes::None);
        // twice the colour of the character
        EXPECT_FLOAT_EQ(own->getDiffuse().r(), 1.f);
        EXPECT_FLOAT_EQ(own->getDiffuse().g(), 0.5f);
        EXPECT_FLOAT_EQ(own->getAmbient().b(), 0.2f);
        // and the picture stays
        EXPECT_NE(mine->getStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXTURE), nullptr);

        // the shaders read uniforms: this shape has its own, of the colour, and those of the others do not change
        const osg::Uniform* ownDiffuse = mine->getStateSet()->getUniform("material.diffuse");
        ASSERT_NE(ownDiffuse, nullptr);
        EXPECT_NE(ownDiffuse, sharedDiffuse);
        osg::Vec4f value;
        ASSERT_TRUE(ownDiffuse->get(value));
        EXPECT_FLOAT_EQ(value.r(), 1.f);
        EXPECT_FLOAT_EQ(value.g(), 0.5f);
        int mode = -1;
        ASSERT_TRUE(mine->getStateSet()->getUniform("material.vertexColorMode")->get(mode));
        EXPECT_EQ(mode, static_cast<int>(SceneUtil::VertexColorModes::None));
        ASSERT_TRUE(sharedDiffuse->get(value));
        EXPECT_FLOAT_EQ(value.r(), 1.f);
        EXPECT_FLOAT_EQ(value.g(), 1.f);
        ASSERT_TRUE(shared->getUniform("material.vertexColorMode")->get(mode));
        EXPECT_EQ(mode, static_cast<int>(SceneUtil::VertexColorModes::AmbientAndDiffuse));
    }

    TEST(OpenFalloutRenderTintHair, makesAMaterialForAShapeThatHasNoneAndSkipsNodesWithoutAPicture)
    {
        osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;
        stateset->setTextureAttribute(0, new osg::Texture2D(makeImage(2)), osg::StateAttribute::ON);
        osg::ref_ptr<osg::Group> shape = new osg::Group;
        shape->setStateSet(stateset);
        osg::ref_ptr<osg::Group> plain = new osg::Group;
        osg::ref_ptr<osg::Group> root = new osg::Group;
        root->addChild(shape);
        root->addChild(plain);

        EXPECT_EQ(tintHair(*root, osg::Vec3f(1.f, 1.f, 1.f)), 1u);
        EXPECT_NE(shape->getStateSet()->getAttribute(osg::StateAttribute::MATERIAL), nullptr);
        EXPECT_EQ(plain->getStateSet(), nullptr);
    }

    TEST(OpenFalloutRenderAttachmentName, givesTheBoneThatTheModelNames)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        osg::ref_ptr<osg::Group> inner = new osg::Group;
        root->addChild(inner);
        inner->setUserValue(Misc::OsgUserValues::sAttachTo, std::string("Bip01 Head"));

        EXPECT_EQ(attachmentName(*root), "Bip01 Head");
        EXPECT_EQ(attachmentName(*inner), "Bip01 Head");
    }

    TEST(OpenFalloutRenderAttachmentName, isEmptyForAModelThatNamesNone)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group;
        root->addChild(new osg::Group);
        osg::ref_ptr<osg::Group> blank = new osg::Group;
        blank->setUserValue(Misc::OsgUserValues::sAttachTo, std::string());

        EXPECT_TRUE(attachmentName(*root).empty());
        EXPECT_TRUE(attachmentName(*blank).empty());
    }
    namespace
    {
        // A turn as the files write it: nine numbers, row by row, acting on column vectors
        osg::Quat turnOfFile(const float (&m)[9])
        {
            osg::Matrix matrix;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    matrix(i, j) = m[j * 3 + i];
            return matrix.getRotate();
        }

        // The rotation of Bip01 Head in the rest pose of the skeleton, and the root of the eye files, that holds the
        // turn back (the numbers of the files of New Vegas)
        constexpr float sHeadAtRest[9]
            = { 0.f, 0.000001f, -1.f, 0.002847f, 0.999996f, 0.000001f, 0.999996f, -0.002847f, 0.f };
        constexpr float sEyeRoot[9]
            = { 0.000002f, 0.002413f, 0.999997f, 0.f, 0.999997f, -0.002413f, -1.f, 0.f, 0.000002f };

        void expectNear(const osg::Vec3d& actual, const osg::Vec3d& expected, double tolerance)
        {
            EXPECT_NEAR(actual.x(), expected.x(), tolerance);
            EXPECT_NEAR(actual.y(), expected.y(), tolerance);
            EXPECT_NEAR(actual.z(), expected.z(), tolerance);
        }
    }

    TEST(OpenFalloutRenderAttachmentFrame, leavesAnEyeAloneThatHasTheTurnOfTheBoneUndoneInItsRoot)
    {
        const osg::Quat bone = turnOfFile(sHeadAtRest);
        const osg::Quat root = turnOfFile(sEyeRoot);
        const osg::Quat frame = attachmentFrame(bone, root);

        // Nothing to put in between
        EXPECT_GT(std::abs(frame.w()), 0.99999);
        // The left eye of the head model is where the head model has it: to the left, in front, above the bone
        const osg::Vec3d eye(-2.193, 6.608, 7.106);
        expectNear(bone * (frame * (root * eye)), eye, 0.01);
    }

    TEST(OpenFalloutRenderAttachmentFrame, putsAPartBackThatTurnsAboutAnAxisOtherThanThatOfTheBone)
    {
        // The female hair of the games: a half turn about (0, 1, 1) in the file (root and shape together), on a bone
        // that has a quarter turn about y. The vertices of the file are upright as they are stored; the file turns them
        // onto their side, as it is the meshes of the part that hold the turn once loaded
        const osg::Quat bone = turnOfFile(sHeadAtRest);
        const osg::Quat file(osg::PI, osg::Vec3d(0.0, 1.0, 1.0));
        const osg::Quat frame = attachmentFrame(bone, file);

        const osg::Vec3d stored(3.0, 5.0, 7.0);
        const osg::Vec3d inTheMesh = file * stored;
        // The mesh is on its side (y and z are exchanged, x mirrored) ...
        expectNear(inTheMesh, osg::Vec3d(-3.0, 7.0, 5.0), 0.01);
        // ... and the frame and the bone bring it back, whichever of the two is meant to come first in the file
        expectNear(bone * (frame * inTheMesh), stored, 0.01);
        // The turn that the part ends up with in the character is the inverse of the one of the file (a half turn, so
        // the same one): what is up in the mesh is up in the character
        expectNear(bone * (frame * osg::Vec3d(0.0, 1.0, 0.0)), osg::Vec3d(0.0, 0.0, 1.0), 0.01);
    }

    TEST(OpenFalloutRenderAttachmentFrame, turnsAPartThatHasNoTurnInItsRootBackIntoTheHeadModel)
    {
        const osg::Quat bone = turnOfFile(sHeadAtRest);
        const osg::Quat frame = attachmentFrame(bone, osg::Quat());

        // A quarter of a turn: the hair of the files is in the frame of the head model, which the bone turns away
        EXPECT_NEAR(std::abs(frame.w()), std::sqrt(0.5), 0.001);
        // Hair at 8.3 units above the origin of the head stays above it instead of landing at the side
        const osg::Vec3d hair(0.004, 0.561, 8.331);
        expectNear(bone * (frame * hair), hair, 0.01);
        expectNear(bone * hair, osg::Vec3d(-8.331, 0.561 + 0.0237, 0.0), 0.05);
    }
}
