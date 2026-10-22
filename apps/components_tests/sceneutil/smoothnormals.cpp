#include <components/sceneutil/smoothnormals.hpp>

#include <osg/Geometry>
#include <osg/Group>

#include <vector>

#include <gtest/gtest.h>

namespace
{
    using namespace SceneUtil;

    osg::ref_ptr<osg::Geometry> makeGeometry(
        const std::vector<osg::Vec3f>& points, const std::vector<unsigned int>& indices)
    {
        osg::ref_ptr<osg::Geometry> geometry(new osg::Geometry);
        geometry->setVertexArray(new osg::Vec3Array(points.begin(), points.end()));
        geometry->addPrimitiveSet(new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, indices.begin(), indices.end()));
        return geometry;
    }

    TEST(SceneUtilSmoothNormals, shouldPointUpForAFlatCounterClockwiseTriangleOnTheGround)
    {
        osg::ref_ptr<osg::Geometry> geometry
            = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) }, { 0, 1, 2 });
        EXPECT_TRUE(generateSmoothNormals(*geometry));
        const osg::Vec3Array* normals = dynamic_cast<const osg::Vec3Array*>(geometry->getNormalArray());
        ASSERT_NE(normals, nullptr);
        ASSERT_EQ(normals->size(), 3u);
        for (const osg::Vec3f& normal : *normals)
            EXPECT_NEAR((normal - osg::Vec3f(0, 0, 1)).length(), 0.f, 1e-5f);
        EXPECT_EQ(geometry->getNormalArray()->getBinding(), osg::Array::BIND_PER_VERTEX);
    }

    TEST(SceneUtilSmoothNormals, shouldAverageTheTrianglesAroundAVertexByTheirArea)
    {
        // A big triangle on the ground and a small one standing up share the vertex 0
        osg::ref_ptr<osg::Geometry> geometry = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(10, 0, 0),
                                                                osg::Vec3f(0, 10, 0), osg::Vec3f(0, 0, 1) },
            { 0, 1, 2, 0, 3, 1 });
        EXPECT_TRUE(generateSmoothNormals(*geometry));
        const osg::Vec3Array* normals = dynamic_cast<const osg::Vec3Array*>(geometry->getNormalArray());
        ASSERT_NE(normals, nullptr);
        const osg::Vec3f& shared = (*normals)[0];
        EXPECT_NEAR(shared.length(), 1.f, 1e-5f);
        // The big triangle (area 50) counts more than the small one (area 5), which leans toward +y
        EXPECT_GT(shared.z(), 0.9f);
        EXPECT_GT(shared.y(), 0.f);
        EXPECT_NEAR((*normals)[2].z(), 1.f, 1e-5f);
    }

    TEST(SceneUtilSmoothNormals, shouldLeaveGeometryThatHasNormalsAlone)
    {
        osg::ref_ptr<osg::Geometry> geometry
            = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) }, { 0, 1, 2 });
        osg::ref_ptr<osg::Vec3Array> own = new osg::Vec3Array(3);
        for (osg::Vec3f& normal : *own)
            normal = osg::Vec3f(1, 0, 0);
        geometry->setNormalArray(own, osg::Array::BIND_PER_VERTEX);
        EXPECT_FALSE(generateSmoothNormals(*geometry));
        EXPECT_EQ(geometry->getNormalArray(), own.get());
    }

    TEST(SceneUtilSmoothNormals, shouldIgnoreIndicesOutsideTheVertices)
    {
        osg::ref_ptr<osg::Geometry> geometry
            = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) }, { 0, 1, 2, 0, 1, 7 });
        EXPECT_TRUE(generateSmoothNormals(*geometry));
        const osg::Vec3Array* normals = dynamic_cast<const osg::Vec3Array*>(geometry->getNormalArray());
        ASSERT_NE(normals, nullptr);
        EXPECT_NEAR((*normals)[0].z(), 1.f, 1e-5f);
    }

    TEST(SceneUtilSmoothNormals, shouldFindTheGeometryBelowANode)
    {
        osg::ref_ptr<osg::Geometry> geometry
            = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) }, { 0, 1, 2 });
        osg::ref_ptr<osg::Group> inner(new osg::Group);
        inner->addChild(geometry);
        osg::ref_ptr<osg::Group> root(new osg::Group);
        root->addChild(inner);
        generateMissingNormals(*root);
        EXPECT_NE(geometry->getNormalArray(), nullptr);
    }

    TEST(SceneUtilSmoothNormals, shouldNotMakeNormalsOfNothing)
    {
        osg::ref_ptr<osg::Geometry> geometry = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0) }, { 0, 1, 1 });
        EXPECT_FALSE(generateSmoothNormals(*geometry));
        EXPECT_EQ(geometry->getNormalArray(), nullptr);
    }
    TEST(SceneUtilSmoothNormals, shouldNotLetASkirtTiltTheEdgeOfTheLand)
    {
        // A patch of land on the ground whose far edge has a wall hanging down to a floor far below
        osg::ref_ptr<osg::Geometry> geometry = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(10, 0, 0),
                                                                osg::Vec3f(0, 10, 0), osg::Vec3f(10, 10, 0),
                                                                osg::Vec3f(0, 10, -14000), osg::Vec3f(10, 10, -14000) },
            { 0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5 });
        EXPECT_TRUE(generateSmoothNormals(*geometry, 8192.f));
        const osg::Vec3Array* normals = dynamic_cast<const osg::Vec3Array*>(geometry->getNormalArray());
        ASSERT_NE(normals, nullptr);
        // The vertices of the land on the edge are lit as the land is
        EXPECT_NEAR((*normals)[2].z(), 1.f, 1e-5f);
        EXPECT_NEAR((*normals)[3].z(), 1.f, 1e-5f);
        // The vertices of the skirt only have the skirt: they get the default
        EXPECT_NEAR((*normals)[4].z(), 1.f, 1e-5f);

        // Counted, the wall would tilt them
        osg::ref_ptr<osg::Geometry> counted = makeGeometry({ osg::Vec3f(0, 0, 0), osg::Vec3f(10, 0, 0),
                                                               osg::Vec3f(0, 10, 0), osg::Vec3f(10, 10, 0),
                                                               osg::Vec3f(0, 10, -14000), osg::Vec3f(10, 10, -14000) },
            { 0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5 });
        EXPECT_TRUE(generateSmoothNormals(*counted));
        const osg::Vec3Array* tilted = dynamic_cast<const osg::Vec3Array*>(counted->getNormalArray());
        ASSERT_NE(tilted, nullptr);
        EXPECT_LT((*tilted)[2].z(), 0.5f);
    }
}
