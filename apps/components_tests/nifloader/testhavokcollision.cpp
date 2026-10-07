#include "../nif/node.hpp"

#include <components/bullethelpers/processtrianglecallback.hpp>
#include <components/nif/data.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/node.hpp>
#include <components/nif/physics.hpp>
#include <components/nifbullet/bulletnifloader.hpp>
#include <components/nifbullet/havokshape.hpp>
#include <components/nifbullet/havoksurvey.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>

#include <gtest/gtest.h>

#include <limits>
#include <numbers>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace Nif::Testing;

    constexpr VFS::Path::NormalizedView testNif("test.nif");
    constexpr VFS::Path::NormalizedView xtestNif("xtest.nif");

    constexpr std::uint32_t fallout3Version = 34;
    constexpr std::uint32_t skyrimVersion = 83;

    constexpr std::uint8_t staticLayer = 1;
    constexpr std::uint8_t triggerLayer = 12;

    std::vector<btVector3> getTriangles(const btCollisionShape& shape)
    {
        const btBvhTriangleMeshShape* mesh = nullptr;
        if (shape.getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE)
            mesh = static_cast<const btScaledBvhTriangleMeshShape&>(shape).getChildShape();
        else
            mesh = static_cast<const btBvhTriangleMeshShape*>(&shape);

        std::vector<btVector3> result;
        auto callback = BulletHelpers::makeProcessTriangleCallback([&](btVector3* triangle, int, int) {
            for (std::size_t i = 0; i < 3; ++i)
                result.push_back(triangle[i]);
        });
        btVector3 aabbMin;
        btVector3 aabbMax;
        mesh->getAabb(btTransform::getIdentity(), aabbMin, aabbMax);
        mesh->processAllTriangles(&callback, aabbMin, aabbMax);
        return result;
    }

    std::pair<btVector3, btVector3> getBounds(const std::vector<btVector3>& points)
    {
        btVector3 min(1e9f, 1e9f, 1e9f);
        btVector3 max(-1e9f, -1e9f, -1e9f);
        for (const btVector3& point : points)
        {
            min.setMin(point);
            max.setMax(point);
        }
        return { min, max };
    }

    void expectNear(const btVector3& actual, const btVector3& expected, double tolerance = 1e-3)
    {
        EXPECT_NEAR(actual.x(), expected.x(), tolerance);
        EXPECT_NEAR(actual.y(), expected.y(), tolerance);
        EXPECT_NEAR(actual.z(), expected.z(), tolerance);
    }

    struct TestHavokCollision : Test
    {
        Nif::NiNode mRoot;
        Nif::NiIntegerExtraData mBsxFlags;
        Nif::bhkCollisionObject mObject;
        Nif::bhkRigidBody mBody;
        Nif::bhkBoxShape mBox;
        Nif::NiTriShapeData mData;
        Nif::NiTriShape mTriShape;
        Nif::NiTimeController mController;

        TestHavokCollision()
        {
            init(mRoot);
            init(mTriShape);
            init(mController);

            // 2 is "has collision"
            mBsxFlags.mRecordType = Nif::RC_BSXFlags;
            mBsxFlags.mData = 2;
            mRoot.mExtraList.push_back(Nif::ExtraPtr(&mBsxFlags));

            mBox.mRecordType = Nif::RC_bhkBoxShape;
            mBox.mRecordName = "bhkBoxShape";
            mBox.mExtents = osg::Vec3f(1, 2, 3);

            // The transform of a body is used by a bhkRigidBodyT only
            mBody.mRecordType = Nif::RC_bhkRigidBodyT;
            mBody.mRecordName = "bhkRigidBodyT";
            mBody.mShape = Nif::bhkShapePtr(&mBox);
            mBody.mHavokFilter.mLayer = staticLayer;
            mBody.mHavokFilter.mFlags = 0;
            mBody.mInfo.mResponseType = Nif::HkResponseType::Response_SimpleContact;
            mBody.mInfo.mTranslation = osg::Vec4f();
            mBody.mInfo.mRotation = osg::Quat();

            mObject.mRecordType = Nif::RC_bhkCollisionObject;
            mObject.mRecordName = "bhkCollisionObject";
            mObject.mFlags = 1;
            mObject.mBody = Nif::bhkWorldObjectPtr(&mBody);
            mRoot.mCollision = Nif::NiCollisionObjectPtr(&mObject);

            // A rendered triangle 100 units long, the size of a thing to hold a box that fits
            mData.mRecordType = Nif::RC_NiTriShapeData;
            mData.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(100, 0, 0), osg::Vec3f(100, 100, 0) };
            mData.mNumTriangles = 1;
            mData.mTriangles = { 0, 1, 2 };
            mTriShape.mData = Nif::NiGeometryDataPtr(&mData);
        }

        void addRenderedGeometry()
        {
            mTriShape.mParents.push_back(&mRoot);
            mRoot.mChildren = Nif::NiAVObjectList{ Nif::NiAVObjectPtr(&mTriShape) };
        }

        std::shared_ptr<Resource::BulletShape> load(bool havokCollision = true,
            std::uint32_t bethVersion = fallout3Version, bool animatedFile = false,
            NifBullet::HavokSurvey* survey = nullptr)
        {
            Nif::NIFFile file(animatedFile ? xtestNif : testNif);
            file.mRoots.push_back(&mRoot);
            file.mHash = "hash";
            file.mVersion = Nif::NIFStream::generateVersion(20, 2, 0, 7);
            file.mBethVersion = bethVersion;

            NifBullet::BulletNifLoader loader(havokCollision);
            loader.setHavokSurvey(survey);
            return loader.load(file);
        }

        static const btCompoundShape& compound(const Resource::BulletShape& shape)
        {
            return static_cast<const btCompoundShape&>(*shape.mCollisionShape);
        }
    };

    TEST_F(TestHavokCollision, a_box_is_made_in_game_units_and_placed_by_its_body)
    {
        mBody.mInfo.mTranslation = osg::Vec4f(1, 2, 3, 0);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        ASSERT_EQ(shape.getChildShape(0)->getShapeType(), BOX_SHAPE_PROXYTYPE);
        // A Havok unit is 7 game units, and the extents of a box are its half extents
        expectNear(
            static_cast<const btBoxShape*>(shape.getChildShape(0))->getHalfExtentsWithMargin(), btVector3(7, 14, 21));
        expectNear(shape.getChildTransform(0).getOrigin(), btVector3(7, 14, 21));
        EXPECT_TRUE(result->mAnimatedShapes.empty());
    }

    TEST_F(TestHavokCollision, the_transform_of_a_body_that_is_not_a_bhkrigidbodyt_is_not_used)
    {
        mBody.mRecordType = Nif::RC_bhkRigidBody;
        mBody.mInfo.mTranslation = osg::Vec4f(1, 2, 3, 0);
        mBody.mInfo.mRotation = osg::Quat(osg::PI_2, osg::Vec3f(0, 0, 1));

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        expectNear(shape.getChildTransform(0).getOrigin(), btVector3(0, 0, 0));
        // The box is not turned: its half extents (1, 2, 3) times 7 stay along x, y and z
        expectNear(
            static_cast<const btBoxShape*>(shape.getChildShape(0))->getHalfExtentsWithMargin(), btVector3(7, 14, 21));
        EXPECT_TRUE(shape.getChildTransform(0).getBasis() == btMatrix3x3::getIdentity());
    }

    TEST_F(TestHavokCollision, a_model_that_is_only_collision_has_collision)
    {
        // No rendered geometry: the wall that is not drawn
        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getNumChildShapes(), 1);
    }

    TEST_F(TestHavokCollision, the_setting_off_leaves_the_rendered_geometry_as_the_collision)
    {
        const auto withoutGeometry = load(false);
        EXPECT_EQ(withoutGeometry->mCollisionShape, nullptr);

        addRenderedGeometry();
        const auto withGeometry = load(false);
        ASSERT_NE(withGeometry->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*withGeometry);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        EXPECT_EQ(shape.getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_body_replaces_the_rendered_geometry_when_it_fits)
    {
        addRenderedGeometry();
        // The box is 14 by 28 by 42 around (50, 50, 0)
        mBody.mInfo.mTranslation = osg::Vec4f(50.f / NifBullet::sHavokScale, 50.f / NifBullet::sHavokScale, 0, 0);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        EXPECT_EQ(shape.getChildShape(0)->getShapeType(), BOX_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_shape_made_from_havok_data_has_a_hash_of_its_own)
    {
        addRenderedGeometry();
        mBody.mInfo.mTranslation = osg::Vec4f(50.f / NifBullet::sHavokScale, 50.f / NifBullet::sHavokScale, 0, 0);

        // The navigation mesh database names a shape by its file and hash, so the same file read with another kind of
        // collision must have another hash
        const std::string rendered = load(false)->mFileHash;
        const std::string havok = load(true)->mFileHash;

        EXPECT_EQ(rendered, "hash");
        EXPECT_NE(havok, rendered);
    }

    TEST_F(TestHavokCollision, a_shape_that_falls_back_to_the_rendered_geometry_keeps_the_hash_of_its_file)
    {
        addRenderedGeometry();
        mBody.mInfo.mTranslation = osg::Vec4f(-100, 0, 0, 0);

        EXPECT_EQ(load(true)->mFileHash, "hash");
    }

    TEST_F(TestHavokCollision, a_body_that_is_not_where_the_rendered_geometry_is_is_not_used)
    {
        addRenderedGeometry();
        mBody.mInfo.mTranslation = osg::Vec4f(-100, 0, 0, 0);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_body_that_is_a_seventh_of_the_size_of_the_rendered_geometry_is_not_used)
    {
        addRenderedGeometry();
        // A scale of 1 instead of 7 would look so
        mBox.mExtents = osg::Vec3f(1, 1, 1) / NifBullet::sHavokScale * 0.5f;
        mBody.mInfo.mTranslation = osg::Vec4f(50.f / NifBullet::sHavokScale, 50.f / NifBullet::sHavokScale, 0, 0);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, the_rotation_of_a_body_turns_its_shape)
    {
        // A quarter turn about z takes x to y
        mBody.mInfo.mRotation = osg::Quat(std::numbers::pi_v<float> / 2, osg::Vec3f(0, 0, 1));

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        expectNear(shape.getChildTransform(0).getBasis() * btVector3(1, 0, 0), btVector3(0, 1, 0));
        expectNear(shape.getChildTransform(0).getBasis() * btVector3(0, 1, 0), btVector3(-1, 0, 0));
    }

    TEST_F(TestHavokCollision, the_transform_and_scale_of_the_node_apply_to_its_body)
    {
        mRoot.mTransform.mTranslation = osg::Vec3f(10, 20, 30);
        mRoot.mTransform.mScale = 2.f;
        mBody.mInfo.mTranslation = osg::Vec4f(1, 0, 0, 0);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        expectNear(shape.getChildShape(0)->getLocalScaling(), btVector3(2, 2, 2));
        // The body is 7 units from the node, and the scale of the node makes that 14
        expectNear(shape.getChildTransform(0).getOrigin(), btVector3(24, 20, 30));
    }

    TEST_F(TestHavokCollision, bodies_of_child_nodes_are_made_where_their_nodes_are)
    {
        Nif::NiNode child;
        init(child);
        child.mParents.push_back(&mRoot);
        child.mTransform.mTranslation = osg::Vec3f(0, 100, 0);
        child.mCollision = Nif::NiCollisionObjectPtr(&mObject);
        mRoot.mCollision = Nif::NiCollisionObjectPtr(nullptr);
        mRoot.mChildren = Nif::NiAVObjectList{ Nif::NiAVObjectPtr(&child) };

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        expectNear(shape.getChildTransform(0).getOrigin(), btVector3(0, 100, 0));
    }

    TEST_F(TestHavokCollision, a_list_has_all_its_shapes_and_a_mopp_tree_is_looked_through)
    {
        Nif::bhkBoxShape second;
        second.mRecordType = Nif::RC_bhkBoxShape;
        second.mExtents = osg::Vec3f(2, 2, 2);
        Nif::bhkListShape list;
        list.mRecordType = Nif::RC_bhkListShape;
        list.mSubshapes = { Nif::bhkShapePtr(&mBox), Nif::bhkShapePtr(&second) };
        Nif::bhkMoppBvTreeShape mopp;
        mopp.mRecordType = Nif::RC_bhkMoppBvTreeShape;
        mopp.mShape = Nif::bhkShapePtr(&list);
        mBody.mShape = Nif::bhkShapePtr(&mopp);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getNumChildShapes(), 2);
    }

    TEST_F(TestHavokCollision, packed_triangle_strips_are_made_in_game_units)
    {
        Nif::hkPackedNiTriStripsData data;
        data.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) };
        data.mTriangles.push_back(Nif::TriangleData{ { 0, 1, 2 }, 0, osg::Vec3f() });
        Nif::bhkPackedNiTriStripsShape strips;
        strips.mRecordType = Nif::RC_bhkPackedNiTriStripsShape;
        strips.mScale = osg::Vec4f(1, 1, 1, 0);
        strips.mData = Nif::hkPackedNiTriStripsDataPtr(&data);
        Nif::bhkMoppBvTreeShape mopp;
        mopp.mRecordType = Nif::RC_bhkMoppBvTreeShape;
        mopp.mShape = Nif::bhkShapePtr(&strips);
        mBody.mShape = Nif::bhkShapePtr(&mopp);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        const std::vector<btVector3> triangles = getTriangles(*shape.getChildShape(0));
        ASSERT_EQ(triangles.size(), 3);
        expectNear(triangles[0], btVector3(0, 0, 0));
        expectNear(triangles[1], btVector3(7, 0, 0));
        expectNear(triangles[2], btVector3(0, 7, 0));
    }

    TEST_F(TestHavokCollision, a_subshape_of_packed_strips_that_stops_nothing_is_left_out)
    {
        Nif::hkPackedNiTriStripsData data;
        data.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0), osg::Vec3f(0, 0, 5),
            osg::Vec3f(1, 0, 5), osg::Vec3f(0, 1, 5), osg::Vec3f(0, 0, 9), osg::Vec3f(1, 0, 9), osg::Vec3f(0, 1, 9) };
        data.mTriangles.push_back(Nif::TriangleData{ { 0, 1, 2 }, 0, osg::Vec3f() });
        data.mTriangles.push_back(Nif::TriangleData{ { 3, 4, 5 }, 0, osg::Vec3f() });
        data.mTriangles.push_back(Nif::TriangleData{ { 6, 7, 8 }, 0, osg::Vec3f() });
        // One subshape in a layer that is not solid, one that has the flag for no collision, and a solid one
        Nif::hkSubPartData trigger{};
        trigger.mNumVertices = 3;
        trigger.mHavokFilter.mLayer = triggerLayer;
        trigger.mHavokFilter.mFlags = 0;
        Nif::hkSubPartData noCollision{};
        noCollision.mNumVertices = 3;
        noCollision.mHavokFilter.mLayer = staticLayer;
        noCollision.mHavokFilter.mFlags = 0x40;
        Nif::hkSubPartData solid{};
        solid.mNumVertices = 3;
        solid.mHavokFilter.mLayer = staticLayer;
        data.mSubshapes = { solid, trigger, noCollision };
        Nif::bhkPackedNiTriStripsShape strips;
        strips.mRecordType = Nif::RC_bhkPackedNiTriStripsShape;
        strips.mScale = osg::Vec4f(1, 1, 1, 0);
        strips.mData = Nif::hkPackedNiTriStripsDataPtr(&data);
        mBody.mShape = Nif::bhkShapePtr(&strips);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        // Only the first triangle, that of the solid subshape
        const std::vector<btVector3> triangles = getTriangles(*shape.getChildShape(0));
        ASSERT_EQ(triangles.size(), 3);
        expectNear(triangles[2], btVector3(0, 7, 0));
    }

    TEST_F(TestHavokCollision, subshapes_that_do_not_add_up_to_the_vertices_are_not_read)
    {
        Nif::hkPackedNiTriStripsData data;
        data.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) };
        data.mTriangles.push_back(Nif::TriangleData{ { 0, 1, 2 }, 0, osg::Vec3f() });
        Nif::hkSubPartData trigger{};
        trigger.mNumVertices = 2;
        trigger.mHavokFilter.mLayer = triggerLayer;
        trigger.mHavokFilter.mFlags = 0;
        data.mSubshapes = { trigger };
        Nif::bhkPackedNiTriStripsShape strips;
        strips.mRecordType = Nif::RC_bhkPackedNiTriStripsShape;
        strips.mScale = osg::Vec4f(1, 1, 1, 0);
        strips.mData = Nif::hkPackedNiTriStripsDataPtr(&data);
        mBody.mShape = Nif::bhkShapePtr(&strips);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(getTriangles(*compound(*result).getChildShape(0)).size(), 3);
    }

    TEST_F(TestHavokCollision, a_collision_object_that_is_not_active_makes_no_obstacle)
    {
        addRenderedGeometry();
        mObject.mFlags = 0;

        const auto result = load();

        // Nothing in the file is active: what the rendered geometry is stays
        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_body_with_the_flag_for_no_collision_makes_no_obstacle)
    {
        addRenderedGeometry();
        mBody.mHavokFilter.mFlags = 0x40;

        const auto result = load();

        // Nothing in the file stops what touches it: what the rendered geometry is stays
        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, the_vertices_of_a_convex_shape_make_its_hull)
    {
        Nif::bhkConvexVerticesShape convex;
        convex.mRecordType = Nif::RC_bhkConvexVerticesShape;
        for (float x : { -1.f, 1.f })
            for (float y : { -1.f, 1.f })
                for (float z : { -1.f, 1.f })
                    convex.mVertices.push_back(osg::Vec4f(x, y, z, 0));
        mBody.mShape = Nif::bhkShapePtr(&convex);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        // The six faces of a cube are twelve triangles, which span from -7 to 7
        const std::vector<btVector3> triangles = getTriangles(*shape.getChildShape(0));
        EXPECT_EQ(triangles.size(), 36);
        btVector3 min(1e9f, 1e9f, 1e9f);
        btVector3 max(-1e9f, -1e9f, -1e9f);
        for (const btVector3& vertex : triangles)
        {
            min.setMin(vertex);
            max.setMax(vertex);
        }
        expectNear(min, btVector3(-7, -7, -7));
        expectNear(max, btVector3(7, 7, 7));
    }

    TEST_F(TestHavokCollision, a_sphere_is_a_hull_with_its_radius_in_game_units)
    {
        Nif::bhkSphereShape sphere;
        sphere.mRecordType = Nif::RC_bhkSphereShape;
        sphere.mRadius = 2.f;
        mBody.mShape = Nif::bhkShapePtr(&sphere);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        const auto [min, max] = getBounds(getTriangles(*shape.getChildShape(0)));
        expectNear(min, btVector3(-14, -14, -14));
        expectNear(max, btVector3(14, 14, 14));
    }

    TEST_F(TestHavokCollision, a_sphere_with_no_radius_makes_no_obstacle)
    {
        Nif::bhkSphereShape sphere;
        sphere.mRecordType = Nif::RC_bhkSphereShape;
        sphere.mRadius = 0.f;
        mBody.mShape = Nif::bhkShapePtr(&sphere);

        EXPECT_EQ(load()->mCollisionShape, nullptr);
    }

    TEST_F(TestHavokCollision, a_capsule_is_a_hull_of_the_two_ends)
    {
        Nif::bhkCapsuleShape capsule;
        capsule.mRecordType = Nif::RC_bhkCapsuleShape;
        capsule.mPoint1 = osg::Vec3f(0, 0, 0);
        capsule.mPoint2 = osg::Vec3f(0, 0, 2);
        capsule.mRadius = 1.f;
        capsule.mRadius1 = 1.f;
        capsule.mRadius2 = 0.5f;
        mBody.mShape = Nif::bhkShapePtr(&capsule);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        // The ends are 14 apart in z; one has a radius of 7 and the other of 3.5. The hull computer rounds the points
        // to a grid, so the bounds are near and not exact.
        const auto [min, max] = getBounds(getTriangles(*shape.getChildShape(0)));
        expectNear(min, btVector3(-7, -7, -7), 0.01);
        expectNear(max, btVector3(7, 7, 14 + 3.5), 0.01);
    }

    TEST_F(TestHavokCollision, a_convex_list_has_all_its_shapes)
    {
        Nif::bhkBoxShape second;
        second.mRecordType = Nif::RC_bhkBoxShape;
        second.mExtents = osg::Vec3f(2, 2, 2);
        Nif::bhkConvexListShape list;
        list.mRecordType = Nif::RC_bhkConvexListShape;
        list.mSubShapes = { Nif::bhkShapePtr(&mBox), Nif::bhkShapePtr(&second) };
        mBody.mShape = Nif::bhkShapePtr(&list);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getNumChildShapes(), 2);
    }

    TEST_F(TestHavokCollision, a_transform_shape_places_its_shape_by_its_matrix_in_havok_units)
    {
        // Turned a quarter about z, then moved one Havok unit along x (the translation is in the last row)
        Nif::bhkConvexTransformShape transformShape;
        transformShape.mRecordType = Nif::RC_bhkConvexTransformShape;
        transformShape.mShape = Nif::bhkShapePtr(&mBox);
        transformShape.mTransform = osg::Matrixf::rotate(std::numbers::pi_v<float> / 2, osg::Vec3f(0, 0, 1))
            * osg::Matrixf::translate(1, 0, 0);
        mBody.mShape = Nif::bhkShapePtr(&transformShape);
        mBody.mInfo.mTranslation = osg::Vec4f(0, 2, 0, 0);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        ASSERT_EQ(shape.getChildShape(0)->getShapeType(), BOX_SHAPE_PROXYTYPE);
        // The matrix of the shape first, then the body: (1, 0, 0) and (0, 2, 0) Havok units
        expectNear(shape.getChildTransform(0).getOrigin(), btVector3(7, 14, 0));
        expectNear(shape.getChildTransform(0).getBasis() * btVector3(1, 0, 0), btVector3(0, 1, 0));
    }

    TEST_F(TestHavokCollision, triangle_strips_are_taken_as_they_are_and_a_data_that_stops_nothing_is_left_out)
    {
        // Two triangles of a strip of four vertices, and a strip of another data that is in a layer that is no obstacle
        Nif::NiTriStripsData wall;
        wall.mRecordType = Nif::RC_NiTriStripsData;
        wall.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(10, 0, 0), osg::Vec3f(0, 10, 0), osg::Vec3f(10, 10, 0) };
        wall.mStrips = { { 0, 1, 2, 3 } };
        Nif::NiTriStripsData trigger;
        trigger.mRecordType = Nif::RC_NiTriStripsData;
        trigger.mVertices = { osg::Vec3f(0, 0, 5), osg::Vec3f(1, 0, 5), osg::Vec3f(0, 1, 5) };
        trigger.mStrips = { { 0, 1, 2 } };
        Nif::bhkNiTriStripsShape strips;
        strips.mRecordType = Nif::RC_bhkNiTriStripsShape;
        strips.mData
            = { Nif::RecordPtrT<Nif::NiTriStripsData>(&wall), Nif::RecordPtrT<Nif::NiTriStripsData>(&trigger) };
        strips.mHavokFilters = { Nif::HavokFilter{ staticLayer, 0, 0 }, Nif::HavokFilter{ triggerLayer, 0, 0 } };
        mBody.mShape = Nif::bhkShapePtr(&strips);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        const std::vector<btVector3> triangles = getTriangles(*shape.getChildShape(0));
        ASSERT_EQ(triangles.size(), 6);
        const auto [min, max] = getBounds(triangles);
        expectNear(min, btVector3(0, 0, 0));
        expectNear(max, btVector3(10, 10, 0));
    }

    TEST_F(TestHavokCollision, triangle_strips_have_the_scale_of_their_shape)
    {
        Nif::NiTriStripsData data;
        data.mRecordType = Nif::RC_NiTriStripsData;
        data.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) };
        data.mStrips = { { 0, 1, 2 } };
        Nif::bhkNiTriStripsShape strips;
        strips.mRecordType = Nif::RC_bhkNiTriStripsShape;
        strips.mScale = osg::Vec4f(2, 3, 1, 0);
        strips.mData = { Nif::RecordPtrT<Nif::NiTriStripsData>(&data) };
        mBody.mShape = Nif::bhkShapePtr(&strips);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        const btCompoundShape& shape = compound(*result);
        ASSERT_EQ(shape.getNumChildShapes(), 1);
        const auto [min, max] = getBounds(getTriangles(*shape.getChildShape(0)));
        expectNear(min, btVector3(0, 0, 0));
        expectNear(max, btVector3(2, 3, 0));
    }

    TEST_F(TestHavokCollision, triangle_strips_leave_out_a_triangle_that_has_a_vertex_twice_or_not_at_all)
    {
        Nif::NiTriStripsData data;
        data.mRecordType = Nif::RC_NiTriStripsData;
        data.mVertices = { osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0) };
        // 0, 1, 2 is a triangle; 1, 2, 2 is none; 2, 2, 9 is none; 2, 9, 0 has a vertex that is not there
        data.mStrips = { { 0, 1, 2, 2, 9, 0 } };
        Nif::bhkNiTriStripsShape strips;
        strips.mRecordType = Nif::RC_bhkNiTriStripsShape;
        strips.mData = { Nif::RecordPtrT<Nif::NiTriStripsData>(&data) };
        mBody.mShape = Nif::bhkShapePtr(&strips);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        ASSERT_EQ(compound(*result).getNumChildShapes(), 1);
        EXPECT_EQ(getTriangles(*compound(*result).getChildShape(0)).size(), 3);
    }

    TEST_F(TestHavokCollision, a_shape_that_is_not_read_leaves_the_rendered_geometry)
    {
        addRenderedGeometry();
        Nif::bhkCylinderShape cylinder;
        cylinder.mRecordType = Nif::RC_bhkCylinderShape;
        cylinder.mRadius = 1.f;
        mBody.mShape = Nif::bhkShapePtr(&cylinder);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_list_that_has_a_shape_that_is_not_read_is_not_used_at_all)
    {
        Nif::bhkCylinderShape cylinder;
        cylinder.mRecordType = Nif::RC_bhkCylinderShape;
        Nif::bhkListShape list;
        list.mRecordType = Nif::RC_bhkListShape;
        list.mSubshapes = { Nif::bhkShapePtr(&mBox), Nif::bhkShapePtr(&cylinder) };
        mBody.mShape = Nif::bhkShapePtr(&list);

        const auto result = load();

        EXPECT_EQ(result->mCollisionShape, nullptr);
    }

    TEST_F(TestHavokCollision, a_layer_that_is_not_solid_makes_no_obstacle)
    {
        addRenderedGeometry();
        mBody.mHavokFilter.mLayer = triggerLayer;

        const auto result = load();

        // Nothing solid in the file: what the rendered geometry is stays
        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_body_that_only_reports_or_does_nothing_makes_no_obstacle)
    {
        addRenderedGeometry();

        for (const Nif::HkResponseType response :
            { Nif::HkResponseType::Response_Reporting, Nif::HkResponseType::Response_None })
        {
            mBody.mInfo.mResponseType = response;

            const auto result = load();

            // Nothing in the file stops what touches it: what the rendered geometry is stays
            ASSERT_NE(result->mCollisionShape, nullptr);
            EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE)
                << static_cast<int>(response);
        }
    }

    TEST_F(TestHavokCollision, a_body_with_no_response_set_is_solid)
    {
        // The response type of a body is unset (invalid) in some files, and what that body is is by its layer
        mBody.mInfo.mResponseType = Nif::HkResponseType::Response_Invalid;

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), BOX_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_solid_body_leaves_out_a_body_that_is_not_solid_of_the_same_file)
    {
        Nif::bhkRigidBody trigger;
        trigger.mRecordType = Nif::RC_bhkRigidBody;
        trigger.mHavokFilter.mLayer = triggerLayer;
        trigger.mHavokFilter.mFlags = 0;
        trigger.mInfo.mResponseType = Nif::HkResponseType::Response_SimpleContact;
        trigger.mInfo.mRotation = osg::Quat();
        trigger.mShape = Nif::bhkShapePtr(&mBox);
        Nif::bhkCollisionObject object;
        object.mRecordType = Nif::RC_bhkCollisionObject;
        object.mFlags = 1;
        object.mBody = Nif::bhkWorldObjectPtr(&trigger);
        Nif::NiNode child;
        init(child);
        child.mParents.push_back(&mRoot);
        child.mCollision = Nif::NiCollisionObjectPtr(&object);
        mRoot.mChildren = Nif::NiAVObjectList{ Nif::NiAVObjectPtr(&child) };

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getNumChildShapes(), 1);
    }

    TEST_F(TestHavokCollision, a_body_that_moves_is_not_used)
    {
        addRenderedGeometry();
        mController.mRecordType = Nif::RC_NiKeyframeController;
        mController.mFlags |= Nif::NiTimeController::Flag_Active;
        mRoot.mController = Nif::NiTimeControllerPtr(&mController);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_file_that_is_animated_as_a_whole_is_not_used)
    {
        addRenderedGeometry();

        const auto result = load(true, fallout3Version, true);

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, only_the_meshes_of_fallout_are_read)
    {
        const auto result = load(true, skyrimVersion);

        EXPECT_EQ(result->mCollisionShape, nullptr);
    }

    TEST_F(TestHavokCollision, a_file_with_no_collision_flag_has_no_collision)
    {
        mBsxFlags.mData = 0;

        const auto result = load();

        EXPECT_EQ(result->mCollisionShape, nullptr);
    }

    constexpr std::string_view filesSection = "Havok collision of the files";

    TEST_F(TestHavokCollision, a_survey_counts_a_file_that_gets_its_havok_collision)
    {
        NifBullet::HavokSurvey survey;

        load(true, fallout3Version, false, &survey);

        EXPECT_EQ(survey.count("Files of Fallout 3 and New Vegas", "collision flag set"), 1);
        EXPECT_EQ(survey.count(filesSection, "used, without geometry that is drawn"), 1);
        EXPECT_EQ(survey.count("Collision objects", "bhkCollisionObject, flags 01 (active)"), 1);
        EXPECT_EQ(survey.count("Bodies of the active collision objects", "bhkRigidBodyT"), 1);
        EXPECT_EQ(survey.count("Rigid bodies by layer", "01 (solid)"), 1);
        EXPECT_EQ(survey.count("Rigid bodies by response", "simple contact"), 1);
        EXPECT_EQ(survey.count("Rigid bodies by shape", "bhkBoxShape"), 1);
        EXPECT_EQ(survey.count("Rigid bodies that stop what walks", "yes"), 1);
    }

    TEST_F(TestHavokCollision, a_survey_measures_the_havok_shape_against_the_rendered_geometry)
    {
        addRenderedGeometry();
        // The box is 14 by 28 by 42 (42 is its longest side) around (50, 50, 0), the triangle 100 by 100
        mBody.mInfo.mTranslation = osg::Vec4f(50.f / NifBullet::sHavokScale, 50.f / NifBullet::sHavokScale, 0, 0);
        NifBullet::HavokSurvey survey;

        load(true, fallout3Version, false, &survey);

        EXPECT_EQ(
            survey.count("Size of the Havok shape over the size of the geometry that is drawn", "2: 0.25 to 0.5"), 1);
        EXPECT_EQ(survey.count("Distance of the centres, in sizes of the geometry that is drawn", "1: up to 0.1"), 1);
        EXPECT_EQ(survey.count(filesSection, "used, with geometry that is drawn"), 1);
    }

    TEST_F(TestHavokCollision, a_survey_counts_a_file_that_does_not_fit_with_its_size)
    {
        addRenderedGeometry();
        mBody.mInfo.mTranslation = osg::Vec4f(-100, 0, 0, 0);
        NifBullet::HavokSurvey survey;

        load(true, fallout3Version, false, &survey);

        EXPECT_EQ(survey.count(filesSection, "not used, its size and place are not those of the geometry"), 1);
        EXPECT_EQ(survey.count("Distance of the centres, in sizes of the geometry that is drawn", "4: over 0.5"), 1);
        EXPECT_EQ(survey.count(filesSection, "used, with geometry that is drawn"), 0);
    }

    TEST_F(TestHavokCollision, a_survey_counts_the_kind_of_shape_that_is_not_read)
    {
        Nif::bhkCylinderShape cylinder;
        cylinder.mRecordType = Nif::RC_bhkCylinderShape;
        cylinder.mRecordName = "bhkCylinderShape";
        cylinder.mRadius = 1.f;
        mBody.mShape = Nif::bhkShapePtr(&cylinder);
        NifBullet::HavokSurvey survey;

        load(true, fallout3Version, false, &survey);

        EXPECT_EQ(survey.count(filesSection, "not used, it has bhkCylinderShape"), 1);
        EXPECT_EQ(survey.count("Rigid bodies by shape", "bhkCylinderShape"), 1);
    }

    TEST_F(TestHavokCollision, a_survey_counts_a_layer_that_is_not_solid_and_a_file_without_the_collision_flag)
    {
        mBody.mHavokFilter.mLayer = triggerLayer;
        NifBullet::HavokSurvey survey;
        load(true, fallout3Version, false, &survey);

        EXPECT_EQ(survey.count("Rigid bodies by layer", "12 (not solid)"), 1);
        EXPECT_EQ(survey.count("Rigid bodies that stop what walks", "no"), 1);
        EXPECT_EQ(survey.count(filesSection, "not used, none of its bodies is solid"), 1);

        mBsxFlags.mData = 0;
        load(true, fallout3Version, false, &survey);
        EXPECT_EQ(survey.count("Files of Fallout 3 and New Vegas", "no collision flag"), 1);
    }

    TEST_F(TestHavokCollision, a_survey_tells_a_solid_body_that_has_no_pieces_from_one_that_is_not_solid)
    {
        Nif::bhkListShape list;
        list.mRecordType = Nif::RC_bhkListShape;
        mBody.mShape = Nif::bhkShapePtr(&list);
        NifBullet::HavokSurvey survey;

        const auto result = load(true, fallout3Version, false, &survey);

        EXPECT_EQ(result->mCollisionShape, nullptr);
        EXPECT_EQ(
            survey.count(filesSection, "not used, its solid bodies have no pieces (an empty or degenerate shape)"), 1);
        EXPECT_EQ(survey.count(filesSection, "not used, none of its bodies is solid"), 0);
    }

    TEST_F(TestHavokCollision, a_survey_counts_a_file_that_has_no_scene_root)
    {
        Nif::NIFFile file(testNif);
        file.mHash = "hash";
        file.mVersion = Nif::NIFStream::generateVersion(20, 2, 0, 7);
        file.mBethVersion = fallout3Version;
        NifBullet::HavokSurvey survey;
        NifBullet::BulletNifLoader loader(true);
        loader.setHavokSurvey(&survey);

        loader.load(file);

        EXPECT_EQ(survey.count("Files of Fallout 3 and New Vegas", "no scene root"), 1);
    }

    TEST_F(TestHavokCollision, a_survey_of_files_of_other_games_counts_nothing)
    {
        NifBullet::HavokSurvey survey;

        load(true, skyrimVersion, false, &survey);

        EXPECT_EQ(survey.count("Files of Fallout 3 and New Vegas", "collision flag set"), 0);
        EXPECT_EQ(survey.count(filesSection, "used, without geometry that is drawn"), 0);
    }

    TEST_F(TestHavokCollision, a_survey_goes_on_after_a_body_that_is_not_read)
    {
        Nif::bhkCylinderShape cylinder;
        cylinder.mRecordType = Nif::RC_bhkCylinderShape;
        cylinder.mRecordName = "bhkCylinderShape";
        cylinder.mRadius = 1.f;
        mBody.mShape = Nif::bhkShapePtr(&cylinder);
        // A second body in a child node, with the box
        Nif::bhkRigidBody second = mBody;
        second.mShape = Nif::bhkShapePtr(&mBox);
        Nif::bhkCollisionObject object;
        object.mRecordType = Nif::RC_bhkCollisionObject;
        object.mRecordName = "bhkCollisionObject";
        object.mFlags = 1;
        object.mBody = Nif::bhkWorldObjectPtr(&second);
        Nif::NiNode child;
        init(child);
        child.mParents.push_back(&mRoot);
        child.mCollision = Nif::NiCollisionObjectPtr(&object);
        mRoot.mChildren = Nif::NiAVObjectList{ Nif::NiAVObjectPtr(&child) };
        NifBullet::HavokSurvey survey;

        const auto result = load(true, fallout3Version, false, &survey);

        // The collision is not made from a file that has a body that is not read ...
        EXPECT_EQ(result->mCollisionShape, nullptr);
        EXPECT_EQ(survey.count("Havok collision of the files", "not used, it has bhkCylinderShape"), 1);
        // ... and the survey has both bodies
        EXPECT_EQ(survey.count("Rigid bodies by shape", "bhkCylinderShape"), 1);
        EXPECT_EQ(survey.count("Rigid bodies by shape", "bhkBoxShape"), 1);
        EXPECT_EQ(survey.count("Rigid bodies that stop what walks", "yes"), 2);
        EXPECT_EQ(load()->mCollisionShape, nullptr);
    }

    TEST_F(TestHavokCollision, a_survey_counts_the_bodies_after_one_that_moves)
    {
        Nif::bhkRigidBody second = mBody;
        Nif::bhkCollisionObject object;
        object.mRecordType = Nif::RC_bhkCollisionObject;
        object.mRecordName = "bhkCollisionObject";
        object.mFlags = 1;
        object.mBody = Nif::bhkWorldObjectPtr(&second);
        Nif::NiNode child;
        init(child);
        child.mParents.push_back(&mRoot);
        child.mCollision = Nif::NiCollisionObjectPtr(&object);
        mRoot.mChildren = Nif::NiAVObjectList{ Nif::NiAVObjectPtr(&child) };
        NifBullet::HavokSurvey survey;

        // The file is animated as a whole (its name starts with x)
        const auto result = load(true, fallout3Version, true, &survey);

        EXPECT_EQ(result->mCollisionShape, nullptr);
        EXPECT_EQ(survey.count("Havok collision of the files", "not used, it has a body that moves"), 1);
        EXPECT_EQ(survey.count("Rigid bodies by shape", "bhkBoxShape"), 2);
    }

    TEST_F(TestHavokCollision, a_survey_counts_a_file_with_several_roots_once)
    {
        Nif::NiNode secondRoot;
        init(secondRoot);
        Nif::NiIntegerExtraData noCollision;
        noCollision.mRecordType = Nif::RC_BSXFlags;
        noCollision.mData = 0;
        secondRoot.mExtraList.push_back(Nif::ExtraPtr(&noCollision));
        Nif::NIFFile file(testNif);
        file.mRoots.push_back(&mRoot);
        file.mRoots.push_back(&secondRoot);
        file.mHash = "hash";
        file.mVersion = Nif::NIFStream::generateVersion(20, 2, 0, 7);
        file.mBethVersion = fallout3Version;
        NifBullet::HavokSurvey survey;
        NifBullet::BulletNifLoader loader(true);
        loader.setHavokSurvey(&survey);

        loader.load(file);

        EXPECT_EQ(survey.count("Files of Fallout 3 and New Vegas", "collision flag set"), 1);
        EXPECT_EQ(survey.count("Files of Fallout 3 and New Vegas", "no collision flag"), 0);
    }

    TEST(TestHavokSurvey, the_answers_are_counted_with_a_few_files_for_each)
    {
        NifBullet::HavokSurvey survey(2);
        survey.add("Layers", "01", "a.nif");
        survey.add("Layers", "01", "a.nif");
        survey.add("Layers", "01", "b.nif");
        survey.add("Layers", "01", "c.nif");
        survey.add("Layers", "12", "d.nif");
        survey.add("Shapes", "box", "a.nif");

        EXPECT_EQ(survey.count("Layers", "01"), 4);
        EXPECT_EQ(survey.count("Layers", "12"), 1);
        EXPECT_EQ(survey.count("Layers", "02"), 0);
        EXPECT_EQ(survey.count("Nothing", "01"), 0);

        std::ostringstream out;
        survey.print(out);
        const std::string text = out.str();
        // Sections in the order they came, the answers sorted, two examples of the four, none repeated in a row
        EXPECT_LT(text.find("Layers (5)"), text.find("Shapes (1)"));
        EXPECT_NE(text.find("e.g. a.nif, b.nif\n"), std::string::npos) << text;
        EXPECT_EQ(text.find("c.nif"), std::string::npos) << text;
        EXPECT_NE(text.find("80.0%  01"), std::string::npos) << text;
        EXPECT_LT(text.find("01"), text.find("12"));
    }

    TEST(TestHavokSurvey, sizes_are_sorted_into_answers)
    {
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(0.1f), "1: under 0.25");
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(0.4f), "2: 0.25 to 0.5");
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(0.7f), "3: 0.5 to 0.8");
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(1.f), "4: 0.8 to 1.25");
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(1.5f), "5: 1.25 to 2");
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(3.f), "6: 2 to 4");
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(7.f), "7: over 4");
        EXPECT_EQ(NifBullet::havokSizeRatioAnswer(std::numeric_limits<float>::infinity()), "7: over 4");
        EXPECT_EQ(NifBullet::havokOffsetAnswer(0.f), "1: up to 0.1");
        EXPECT_EQ(NifBullet::havokOffsetAnswer(0.2f), "2: 0.1 to 0.25");
        EXPECT_EQ(NifBullet::havokOffsetAnswer(0.4f), "3: 0.25 to 0.5");
        EXPECT_EQ(NifBullet::havokOffsetAnswer(std::numeric_limits<float>::infinity()), "4: over 0.5");
        EXPECT_EQ(NifBullet::havokNumberAnswer(7), "07");
        EXPECT_EQ(NifBullet::havokNumberAnswer(43), "43");
        EXPECT_EQ(NifBullet::havokNumberAnswer(200), "200");
    }

    TEST(TestHavokLayers, the_layers_that_make_a_world_solid_are_solid)
    {
        // Static, animated static, transparent, clutter, trees, props, terrain, ground, invisible walls and collision
        // boxes
        for (std::uint8_t layer : { 1, 2, 3, 4, 9, 10, 13, 17, 27, 32 })
            EXPECT_TRUE(NifBullet::isSolidHavokLayer(layer)) << static_cast<int>(layer);
        // Weapons, projectiles, biped, water, triggers, non collidable, portals, zones, picking, line of sight and the
        // null layer
        for (std::uint8_t layer : { 5, 6, 8, 11, 12, 15, 18, 21, 22, 23, 35, 36, 37, 38, 43 })
            EXPECT_FALSE(NifBullet::isSolidHavokLayer(layer)) << static_cast<int>(layer);
        // A layer that is not known is solid
        EXPECT_TRUE(NifBullet::isSolidHavokLayer(200));
    }
}
