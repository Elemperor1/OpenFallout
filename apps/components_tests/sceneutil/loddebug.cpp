#include <components/sceneutil/loddebug.hpp>
#include <components/sceneutil/material.hpp>

#include <osg/Geometry>
#include <osg/Group>
#include <osg/Texture2D>

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
    using namespace SceneUtil;

    osg::ref_ptr<osg::Geometry> makeTileWithSkirt()
    {
        // A square of two triangles on the ground and one triangle that hangs from its edge to far below
        osg::ref_ptr<osg::Geometry> geometry(new osg::Geometry);
        osg::ref_ptr<osg::Vec3Array> points(new osg::Vec3Array);
        points->push_back(osg::Vec3f(0, 0, 0));
        points->push_back(osg::Vec3f(10, 0, 0));
        points->push_back(osg::Vec3f(10, 10, 0));
        points->push_back(osg::Vec3f(0, 10, 0));
        points->push_back(osg::Vec3f(0, 0, -14098));
        geometry->setVertexArray(points);
        osg::ref_ptr<osg::DrawElementsUShort> triangles(new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES));
        for (unsigned short index : { 0, 1, 2, 0, 2, 3, 0, 4, 1 })
            triangles->push_back(index);
        geometry->addPrimitiveSet(triangles);
        return geometry;
    }

    TEST(SceneUtilLodDebug, readsTheWordsSeparatedBySpacesAndCommas)
    {
        std::vector<std::string> unknown;
        const LodDebug debug = parseLodDebug(" flat,no-skirt  objects-only, sparkle", &unknown);
        EXPECT_TRUE(debug.mFlat);
        EXPECT_TRUE(debug.mNoSkirt);
        EXPECT_TRUE(debug.mObjectsOnly);
        EXPECT_FALSE(debug.mWireframe);
        EXPECT_FALSE(debug.mNoHigh);
        ASSERT_EQ(unknown.size(), 1u);
        EXPECT_EQ(unknown[0], "sparkle");
    }

    TEST(SceneUtilLodDebug, noWordsChangeNothing)
    {
        const LodDebug debug = parseLodDebug("");
        EXPECT_FALSE(debug.changesFiles());
        EXPECT_FALSE(debug.mObjectsOnly);
        EXPECT_FALSE(debug.mNoHigh);
        // What is drawn is not changed by the words that choose what is loaded
        EXPECT_FALSE(parseLodDebug("objects-only no-high").changesFiles());
    }

    TEST(SceneUtilLodDebug, leavesOutTheSkirtOfATile)
    {
        osg::ref_ptr<osg::Geometry> geometry = makeTileWithSkirt();
        osg::ref_ptr<osg::Group> root(new osg::Group);
        root->addChild(geometry);
        applyLodDebug(*root, "meshes/landscape/lod/w/w.level8.x0.y0.nif", parseLodDebug("no-skirt"), 8192.f);

        ASSERT_EQ(geometry->getNumPrimitiveSets(), 1u);
        const osg::DrawElementsUInt* kept = dynamic_cast<const osg::DrawElementsUInt*>(geometry->getPrimitiveSet(0));
        ASSERT_NE(kept, nullptr);
        EXPECT_EQ(kept->getNumIndices(), 6u);
        for (unsigned int i = 0; i < kept->getNumIndices(); ++i)
            EXPECT_NE(kept->index(i), 4u);
    }

    TEST(SceneUtilLodDebug, keepsTheSkirtUnlessAskedToLeaveItOut)
    {
        osg::ref_ptr<osg::Geometry> geometry = makeTileWithSkirt();
        applyLodDebug(*geometry, "meshes/landscape/lod/w/w.level8.x0.y0.nif", parseLodDebug("flat"), 8192.f);
        ASSERT_EQ(geometry->getNumPrimitiveSets(), 1u);
        EXPECT_EQ(geometry->getPrimitiveSet(0)->getNumIndices(), 9u);
    }

    TEST(SceneUtilLodDebug, takesTheTexturesAwayAndColoursByLevel)
    {
        osg::ref_ptr<osg::Geometry> geometry = makeTileWithSkirt();
        geometry->getOrCreateStateSet()->setTextureAttributeAndModes(0, new osg::Texture2D, osg::StateAttribute::ON);
        osg::ref_ptr<osg::Group> root(new osg::Group);
        root->addChild(geometry);
        applyLodDebug(*root, "meshes/landscape/lod/w/w.level16.x0.y0.nif", parseLodDebug("flat"), 8192.f);

        EXPECT_TRUE(geometry->getStateSet()->getTextureAttributeList().empty());
        EXPECT_TRUE(geometry->getStateSet()->getTextureModeList().empty());
        const Material* material
            = dynamic_cast<const Material*>(root->getStateSet()->getAttribute(osg::StateAttribute::MATERIAL));
        ASSERT_NE(material, nullptr);
        EXPECT_EQ(material->getDiffuse(), osg::Vec4f(0.3f, 0.4f, 1.f, 1.f));
        EXPECT_EQ(material->getAmbient(), material->getDiffuse());
    }

    TEST(SceneUtilLodDebug, blocksOfObjectsHaveTheirOwnColours)
    {
        const auto colorOf = [](std::string_view path) {
            osg::ref_ptr<osg::Group> root(new osg::Group);
            applyLodDebug(*root, path, parseLodDebug("flat"), 8192.f);
            return static_cast<const Material*>(root->getStateSet()->getAttribute(osg::StateAttribute::MATERIAL))
                ->getDiffuse();
        };
        EXPECT_EQ(colorOf("meshes/landscape/lod/w/blocks/w.level4.x0.y0.nif"), osg::Vec4f(1.f, 0.2f, 1.f, 1.f));
        EXPECT_EQ(colorOf("meshes/landscape/lod/w/blocks/w.level4.high.x0.y0.nif"), osg::Vec4f(0.2f, 1.f, 1.f, 1.f));
        EXPECT_EQ(colorOf("meshes/landscape/lod/w/w.level4.x0.y0.nif"), osg::Vec4f(1.f, 0.2f, 0.2f, 1.f));
        EXPECT_EQ(colorOf("meshes/landscape/lod/w/w.level32.x0.y0.nif"), osg::Vec4f(1.f, 1.f, 0.2f, 1.f));
    }
}
