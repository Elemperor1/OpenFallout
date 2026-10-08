#include <apps/openfallout/mwrender/falloutface.hpp>

#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/texturetype.hpp>

#include <gtest/gtest.h>

#include <osg/Geometry>
#include <osg/Group>
#include <osg/Texture2D>

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

    TEST(OpenFalloutRenderMorphFaceMeshes, leavesAPartAloneWhoseVerticesAreNotThoseOfTheFile)
    {
        const osg::ref_ptr<osg::Geometry> shared = makeGeometry(3);
        osg::ref_ptr<osg::Group> part = new osg::Group;
        part->addChild(shared);

        const ESM4::FaceMorphs morphs = makeMorphs(5, { 2 });
        EXPECT_EQ(morphFaceMeshes(*part, morphs, { 1.f }, {}), 0u);
        EXPECT_EQ(part->getChild(0), shared.get());
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
}
