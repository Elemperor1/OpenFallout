#include "../nif/node.hpp"

#include <components/bullethelpers/processtrianglecallback.hpp>
#include <components/nif/data.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/node.hpp>
#include <components/nif/physics.hpp>
#include <components/nifbullet/bulletnifloader.hpp>
#include <components/nifbullet/havokshape.hpp>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>

#include <gtest/gtest.h>

#include <numbers>
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

    void expectNear(const btVector3& actual, const btVector3& expected)
    {
        EXPECT_NEAR(actual.x(), expected.x(), 1e-3);
        EXPECT_NEAR(actual.y(), expected.y(), 1e-3);
        EXPECT_NEAR(actual.z(), expected.z(), 1e-3);
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
            mBox.mExtents = osg::Vec3f(1, 2, 3);

            // The transform of a body is used by a bhkRigidBodyT only
            mBody.mRecordType = Nif::RC_bhkRigidBodyT;
            mBody.mShape = Nif::bhkShapePtr(&mBox);
            mBody.mHavokFilter.mLayer = staticLayer;
            mBody.mInfo.mResponseType = Nif::HkResponseType::Response_SimpleContact;
            mBody.mInfo.mTranslation = osg::Vec4f();
            mBody.mInfo.mRotation = osg::Quat();

            mObject.mRecordType = Nif::RC_bhkCollisionObject;
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

        std::shared_ptr<Resource::BulletShape> load(
            bool havokCollision = true, std::uint32_t bethVersion = fallout3Version, bool animatedFile = false)
        {
            Nif::NIFFile file(animatedFile ? xtestNif : testNif);
            file.mRoots.push_back(&mRoot);
            file.mHash = "hash";
            file.mVersion = Nif::NIFStream::generateVersion(20, 2, 0, 7);
            file.mBethVersion = bethVersion;

            NifBullet::BulletNifLoader loader(havokCollision);
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

    TEST_F(TestHavokCollision, a_shape_that_is_not_read_leaves_the_rendered_geometry)
    {
        addRenderedGeometry();
        Nif::bhkSphereShape sphere;
        sphere.mRecordType = Nif::RC_bhkSphereShape;
        sphere.mRadius = 1.f;
        mBody.mShape = Nif::bhkShapePtr(&sphere);

        const auto result = load();

        ASSERT_NE(result->mCollisionShape, nullptr);
        EXPECT_EQ(compound(*result).getChildShape(0)->getShapeType(), SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE);
    }

    TEST_F(TestHavokCollision, a_list_that_has_a_shape_that_is_not_read_is_not_used_at_all)
    {
        Nif::bhkSphereShape sphere;
        sphere.mRecordType = Nif::RC_bhkSphereShape;
        Nif::bhkListShape list;
        list.mRecordType = Nif::RC_bhkListShape;
        list.mSubshapes = { Nif::bhkShapePtr(&mBox), Nif::bhkShapePtr(&sphere) };
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
        trigger.mInfo.mResponseType = Nif::HkResponseType::Response_SimpleContact;
        trigger.mInfo.mRotation = osg::Quat();
        trigger.mShape = Nif::bhkShapePtr(&mBox);
        Nif::bhkCollisionObject object;
        object.mRecordType = Nif::RC_bhkCollisionObject;
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

    TEST(TestHavokLayers, the_layers_that_make_a_world_solid_are_solid)
    {
        // Static, animated static, transparent, clutter, trees, props, terrain, ground and invisible walls
        for (std::uint8_t layer : { 1, 2, 3, 4, 9, 10, 13, 17, 27 })
            EXPECT_TRUE(NifBullet::isSolidHavokLayer(layer)) << static_cast<int>(layer);
        // Weapons, projectiles, biped, water, triggers, non collidable, portals, zones, picking, line of sight and the
        // null layer
        for (std::uint8_t layer : { 5, 6, 8, 11, 12, 15, 18, 21, 22, 23, 35, 36, 37, 38, 43 })
            EXPECT_FALSE(NifBullet::isSolidHavokLayer(layer)) << static_cast<int>(layer);
        // A layer that is not known is solid
        EXPECT_TRUE(NifBullet::isSolidHavokLayer(200));
    }
}
