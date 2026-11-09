#include "niftestfile.hpp"

#include <components/nif/exception.hpp>
#include <components/nif/node.hpp>
#include <components/nif/physics.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

// The Havok records of the games: the collision objects, the rigid bodies, their shapes and the constraints that join
// them, as Oblivion, Fallout 3 and the games after it write them.
namespace
{
    using namespace NifTest;
    using namespace testing;

    /// The oldest files of Oblivion, which have an unknown number before the material of a shape.
    constexpr Profile oldOblivion{ 0x0A000102, 0, 11 };

    bool hasOldMaterial(const Builder& file)
    {
        return file.profile().mVersion <= 0x0A000102;
    }

    Bytes& havokFilter(Bytes& bytes, std::uint8_t layer = 1, std::uint8_t flags = 2, std::uint16_t group = 3)
    {
        return bytes.u8(layer).u8(flags).u16(group);
    }

    Bytes& havokMaterial(const Builder& file, Bytes& bytes, std::uint32_t material)
    {
        if (hasOldMaterial(file))
            bytes.u32(0);
        return bytes.u32(material);
    }

    Bytes& property(Bytes& bytes, std::uint32_t data = 1, std::uint32_t size = 2, std::uint32_t capacity = 3)
    {
        return bytes.u32(data).u32(size).u32(capacity);
    }

    Bytes propertyBytes(std::uint32_t data, std::uint32_t size, std::uint32_t capacity)
    {
        Bytes bytes;
        property(bytes, data, size, capacity);
        return bytes;
    }

    /// What every Havok object that is in the world has: the shape, the filter and the information of the world.
    Bytes worldObject(const Builder& file, std::int32_t shape)
    {
        Bytes bytes;
        bytes.ref(shape);
        if (hasOldMaterial(file))
            bytes.u32(0);
        havokFilter(bytes, 5, 6, 7);
        bytes.zeros(4).u8(2).zeros(3); // the broad phase is 2: a phantom
        return std::move(property(bytes, 10, 11, 12));
    }

    Bytes entity(const Builder& file, std::int32_t shape)
    {
        Bytes bytes = worldObject(file, shape);
        return std::move(bytes.u8(2).u8(0).u16(9)); // the response, unused and the delay of contacts
    }

    /// The information of a rigid body as the versions of the files write it, with a mass of 4 and a friction of 0.5.
    Bytes rigidBodyInfo(const Builder& file)
    {
        const std::uint32_t version = file.profile().mVersion;
        const std::uint32_t beth = file.profile().mBethVersion;
        Bytes bytes;
        if (version >= 0x0A010000)
        {
            bytes.zeros(4);
            havokFilter(bytes, 8, 9, 10);
            bytes.zeros(4);
            if (beth != 130)
            {
                if (beth >= 83)
                    bytes.zeros(4);
                bytes.u8(2).u8(0).u16(11);
            }
        }
        if (beth < 83)
            bytes.zeros(4);
        bytes.vec4(1, 2, 3, 0); // translation
        bytes.f32(0).f32(0).f32(0.6f).f32(0.8f); // rotation: x, y, z, w
        bytes.vec4(0, 0, 1, 0).vec4(0, 1, 0, 0); // the velocities
        for (int i = 0; i < 3; ++i)
            bytes.f32(1.f + i).f32(2.f + i).f32(3.f + i).f32(0); // the inertia tensor, with padding
        bytes.vec4(0.5f, 0.25f, 0.125f, 0); // the center
        bytes.f32(4).f32(0.1f).f32(0.2f); // mass and the damping
        if (beth >= 83)
        {
            if (beth != 130)
                bytes.f32(2); // time factor
            bytes.f32(3); // gravity factor
        }
        bytes.f32(0.5f); // friction
        if (beth >= 83)
            bytes.f32(1.5f); // rolling friction
        bytes.f32(0.25f); // restitution
        if (version >= 0x0A010000)
        {
            bytes.f32(100).f32(200);
            if (beth != 130)
                bytes.f32(0.5f); // penetration depth
        }
        bytes.u8(1); // motion type: dynamic
        if (beth < 83)
            bytes.u8(2); // deactivator
        else
            bytes.boolean(false); // deactivation enabled
        bytes.u8(3); // solver deactivation
        if (beth == 130)
        {
            bytes.u8(0).f32(0.5f).f32(2).zeros(4);
            bytes.u8(2).u8(0).u16(11);
        }
        bytes.u8(4); // quality
        if (beth >= 83)
            bytes.u8(5).u8(6).u8(7).boolean(true);
        return std::move(bytes.zeros(beth == 130 ? 3 : 12));
    }

    /// The bytes of a rigid body: the entity, the information and then the constraints and the flags.
    Bytes rigidBody(const Builder& file, std::int32_t shape, const std::vector<std::int32_t>& constraints = {})
    {
        Bytes bytes = entity(file, shape);
        bytes.raw(rigidBodyInfo(file).str()).refs(constraints);
        if (file.profile().mBethVersion < 76)
            bytes.u32(0xA1B2);
        else
            bytes.u16(0xA1B2);
        return bytes;
    }

    TEST(NifHavokTest, readsTheRigidBodiesOfEveryGame)
    {
        for (const Profile& profile : { oldOblivion, oblivion, fallout3, skyrim, fallout4 })
        {
            Builder file(profile);
            file.add("bhkBoxShape", [&] {
                Bytes bytes;
                havokMaterial(file, bytes, 7).f32(0.25f).zeros(8).vec3(1, 2, 3).zeros(4);
                return bytes;
            }());
            file.add("bhkRigidBody", rigidBody(file, 0));

            const Parsed parsed(file.build());

            const auto* body = parsed.as<Nif::bhkRigidBody>(1);
            ASSERT_NE(body, nullptr) << profile.mBethVersion;
            EXPECT_EQ(body->mShape.getPtr(), parsed.as<Nif::bhkBoxShape>(0)) << profile.mBethVersion;
            EXPECT_EQ(body->mHavokFilter.mLayer, 5);
            EXPECT_EQ(body->mHavokFilter.mFlags, 6);
            EXPECT_EQ(body->mHavokFilter.mGroup, 7);
            EXPECT_EQ(body->mWorldObjectInfo.mPhaseType, Nif::BroadPhaseType::BroadPhase_Phantom);
            EXPECT_EQ(body->mWorldObjectInfo.mProperty.mData, 10u);
            EXPECT_EQ(body->mWorldObjectInfo.mProperty.mSize, 11u);
            EXPECT_EQ(body->mWorldObjectInfo.mProperty.mCapacityAndFlags, 12u);
            EXPECT_EQ(body->mInfo.mTranslation, osg::Vec4f(1, 2, 3, 0));
            EXPECT_EQ(body->mInfo.mRotation.z(), 0.6f);
            EXPECT_EQ(body->mInfo.mRotation.w(), 0.8f);
            EXPECT_EQ(body->mInfo.mLinearVelocity, osg::Vec4f(0, 0, 1, 0));
            EXPECT_EQ(body->mInfo.mAngularVelocity, osg::Vec4f(0, 1, 0, 0));
            EXPECT_EQ(body->mInfo.mCenter, osg::Vec4f(0.5f, 0.25f, 0.125f, 0));
            EXPECT_EQ(body->mInfo.mMass, 4.f);
            EXPECT_EQ(body->mInfo.mLinearDamping, 0.1f);
            EXPECT_EQ(body->mInfo.mAngularDamping, 0.2f);
            EXPECT_EQ(body->mInfo.mFriction, 0.5f);
            EXPECT_EQ(body->mInfo.mRestitution, 0.25f);
            EXPECT_EQ(body->mInfo.mMotionType, Nif::HkMotionType::Motion_Dynamic);
            EXPECT_EQ(body->mInfo.mSolverDeactivation, Nif::HkSolverDeactivation::SolverDeactivation_Medium);
            EXPECT_EQ(body->mInfo.mQualityType, Nif::HkQualityType::Quality_Moving);
            EXPECT_EQ(body->mBodyFlags, 0xA1B2u);
            if (profile.mVersion >= 0x0A010000)
            {
                EXPECT_EQ(body->mInfo.mHavokFilter.mLayer, 8);
                EXPECT_EQ(body->mInfo.mHavokFilter.mGroup, 10);
                EXPECT_EQ(body->mInfo.mResponseType, Nif::HkResponseType::Response_Reporting);
                EXPECT_EQ(body->mInfo.mProcessContactDelay, 11u);
                EXPECT_EQ(body->mInfo.mMaxLinearVelocity, 100.f);
                EXPECT_EQ(body->mInfo.mMaxAngularVelocity, 200.f);
            }
            if (profile.mBethVersion < 83)
                EXPECT_EQ(body->mInfo.mDeactivatorType, Nif::HkDeactivatorType::Deactivator_Spatial);
            else
            {
                EXPECT_FALSE(body->mInfo.mEnableDeactivation);
                EXPECT_EQ(body->mInfo.mGravityFactor, 3.f);
                EXPECT_EQ(body->mInfo.mRollingFrictionMult, 1.5f);
                EXPECT_EQ(body->mInfo.mAutoRemoveLevel, 5);
                EXPECT_EQ(body->mInfo.mResponseModifierFlags, 6);
                EXPECT_EQ(body->mInfo.mNumContactPointShapeKeys, 7);
                EXPECT_TRUE(body->mInfo.mForceCollidedOntoPPU);
            }
            if (profile.mBethVersion >= 83)
                EXPECT_EQ(body->mInfo.mTimeFactor, 2.f);
        }
    }

    TEST(NifHavokTest, readsTheCollisionObjectsThatHoldABody)
    {
        Builder file(fallout3);
        file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
        file.add("NiNode", file.node({ .mName = "Child", .mCollision = 2 }));
        file.add("bhkCollisionObject", Bytes().ref(1).u16(0x81).ref(4));
        file.add("bhkBlendCollisionObject", Bytes().ref(1).u16(0x82).ref(4).f32(0.25f).f32(0.75f));
        file.add("bhkRigidBody", rigidBody(file, -1));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* object = parsed.as<Nif::bhkCollisionObject>(2);
        ASSERT_NE(object, nullptr);
        EXPECT_EQ(object->mTarget.getPtr(), parsed.as<Nif::NiNode>(1));
        EXPECT_EQ(object->mFlags, 0x81u);
        EXPECT_EQ(object->mBody.getPtr(), parsed.as<Nif::bhkRigidBody>(4));

        const auto* blend = parsed.as<Nif::bhkBlendCollisionObject>(3);
        ASSERT_NE(blend, nullptr);
        EXPECT_EQ(blend->mHeirGain, 0.25f);
        EXPECT_EQ(blend->mVelGain, 0.75f);
    }

    TEST(NifHavokTest, theBlendCollisionObjectOfTheOldestFilesHasAnExtraPair)
    {
        Builder file(Profile{ 0x14000005, 11, 8 });
        file.add("NiNode", file.node({ .mName = "Root" }, {}));
        file.add("bhkRigidBody", rigidBody(file, -1));
        file.add("bhkBlendCollisionObject", Bytes().ref(0).u16(1).ref(1).f32(1).f32(2).zeros(8));
        file.addRoot(0);

        const Parsed parsed(file.build());

        EXPECT_EQ(parsed.as<Nif::bhkBlendCollisionObject>(2)->mVelGain, 2.f);
    }

    TEST(NifHavokTest, readsTheCollisionObjectAndTheSystemsOfFallout4)
    {
        Builder file(fallout4);
        file.add("NiNode", file.node({ .mName = "Root" }));
        file.add("bhkNPCollisionObject", Bytes().ref(0).u16(0x80).ref(2).u32(77));
        file.add("bhkPhysicsSystem", Bytes().u32(3).u8(1).u8(2).u8(3));
        file.add("bhkRagdollSystem", Bytes().u32(2).u8(4).u8(5));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* object = parsed.as<Nif::bhkNPCollisionObject>(1);
        ASSERT_NE(object, nullptr);
        EXPECT_EQ(object->mTarget.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(object->mFlags, 0x80u);
        EXPECT_EQ(object->mData.getPtr(), parsed.as<Nif::bhkPhysicsSystem>(2));
        EXPECT_EQ(object->mBodyID, 77u);
        EXPECT_THAT(parsed.as<Nif::bhkPhysicsSystem>(2)->mData, ElementsAre(1, 2, 3));
        EXPECT_THAT(parsed.as<Nif::bhkRagdollSystem>(3)->mData, ElementsAre(4, 5));
    }

    TEST(NifHavokTest, readsTheSimpleShapes)
    {
        for (const Profile& profile : { oldOblivion, oblivion, fallout3 })
        {
            Builder file(profile);
            Bytes sphere;
            havokMaterial(file, sphere, 1).f32(0.5f);
            file.add("bhkSphereShape", sphere);
            Bytes box;
            havokMaterial(file, box, 2).f32(0.25f).zeros(8).vec3(1, 2, 3).zeros(4);
            file.add("bhkBoxShape", box);
            Bytes capsule;
            havokMaterial(file, capsule, 3).f32(0.125f).zeros(8).vec3(0, 0, 1).f32(0.5f).vec3(0, 0, 2).f32(0.75f);
            file.add("bhkCapsuleShape", capsule);
            Bytes cylinder;
            havokMaterial(file, cylinder, 4).f32(0.0625f).zeros(8).vec4(0, 0, 1, 0).vec4(0, 0, 2, 0).f32(3).zeros(12);
            file.add("bhkCylinderShape", cylinder);
            Bytes plane;
            havokMaterial(file, plane, 5).zeros(12).vec4(0, 0, 1, 2).vec4(3, 4, 5, 0).vec4(6, 7, 8, 0);
            file.add("bhkPlaneShape", plane);

            const Parsed parsed(file.build());

            const auto* sphereShape = parsed.as<Nif::bhkSphereShape>(0);
            ASSERT_NE(sphereShape, nullptr) << profile.mVersion;
            EXPECT_EQ(sphereShape->mHavokMaterial.mMaterial, 1u);
            EXPECT_EQ(sphereShape->mRadius, 0.5f);

            const auto* boxShape = parsed.as<Nif::bhkBoxShape>(1);
            ASSERT_NE(boxShape, nullptr);
            EXPECT_EQ(boxShape->mRadius, 0.25f);
            EXPECT_EQ(boxShape->mExtents, osg::Vec3f(1, 2, 3));

            const auto* capsuleShape = parsed.as<Nif::bhkCapsuleShape>(2);
            ASSERT_NE(capsuleShape, nullptr);
            EXPECT_EQ(capsuleShape->mPoint1, osg::Vec3f(0, 0, 1));
            EXPECT_EQ(capsuleShape->mRadius1, 0.5f);
            EXPECT_EQ(capsuleShape->mPoint2, osg::Vec3f(0, 0, 2));
            EXPECT_EQ(capsuleShape->mRadius2, 0.75f);

            const auto* cylinderShape = parsed.as<Nif::bhkCylinderShape>(3);
            ASSERT_NE(cylinderShape, nullptr);
            EXPECT_EQ(cylinderShape->mVertexA, osg::Vec4f(0, 0, 1, 0));
            EXPECT_EQ(cylinderShape->mVertexB, osg::Vec4f(0, 0, 2, 0));
            EXPECT_EQ(cylinderShape->mCylinderRadius, 3.f);

            const auto* planeShape = parsed.as<Nif::bhkPlaneShape>(4);
            ASSERT_NE(planeShape, nullptr);
            EXPECT_EQ(planeShape->mHavokMaterial.mMaterial, 5u);
            EXPECT_FLOAT_EQ(planeShape->mPlane[2], 1.f);
            EXPECT_FLOAT_EQ(planeShape->mPlane[3], 2.f);
            EXPECT_EQ(planeShape->mExtents, osg::Vec4f(3, 4, 5, 0));
            EXPECT_EQ(planeShape->mCenter, osg::Vec4f(6, 7, 8, 0));
        }
    }

    TEST(NifHavokTest, readsTheShapesThatHoldOtherShapes)
    {
        Builder file(fallout3);
        // 0 to 2: shapes with nothing in them
        file.add("bhkSphereShape", Bytes().u32(1).f32(0.5f));
        file.add("bhkBoxShape", Bytes().u32(2).f32(0.25f).zeros(8).vec3(1, 1, 1).zeros(4));
        file.add("bhkSphereShape", Bytes().u32(3).f32(0.75f));
        // 3: a list of the three
        file.add("bhkListShape",
            Bytes()
                .refs({ 0, 1, 2 })
                .u32(9)
                .raw(propertyBytes(1, 2, 3).str())
                .raw(propertyBytes(4, 5, 6).str())
                .u32(2)
                .u32(0x01020304)
                .u32(0x05060708));
        // 4: a tree that gives a shape its bounds, with the code that Havok makes
        file.add("bhkMoppBvTreeShape", Bytes().ref(3).zeros(12).f32(1.5f).u32(3).vec4(1, 2, 3, 4).u8(1).u8(2).u8(3));
        // 5: a convex shape with a transform
        file.add("bhkConvexTransformShape",
            Bytes()
                .ref(0)
                .u32(4)
                .f32(0.5f)
                .zeros(8)
                .f32(1)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(1)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(1)
                .f32(0)
                .f32(1)
                .f32(2)
                .f32(3)
                .f32(1));
        // 6: a convex list
        file.add("bhkConvexListShape",
            Bytes().refs({ 0, 2 }).u32(6).f32(0.5f).zeros(8).u32(1).u32(2).u32(3).boolean(true).f32(0.125f));
        // 7: the sweep of a convex shape
        file.add("bhkConvexSweepShape", Bytes().ref(0).u32(8).f32(0.5f).zeros(12));

        const Parsed parsed(file.build());

        const auto* list = parsed.as<Nif::bhkListShape>(3);
        ASSERT_NE(list, nullptr);
        ASSERT_EQ(list->mSubshapes.size(), 3u);
        EXPECT_EQ(list->mSubshapes[1].getPtr(), parsed.as<Nif::bhkBoxShape>(1));
        EXPECT_EQ(list->mHavokMaterial.mMaterial, 9u);
        EXPECT_EQ(list->mChildShapeProperty.mData, 1u);
        EXPECT_EQ(list->mChildFilterProperty.mCapacityAndFlags, 6u);
        ASSERT_EQ(list->mHavokFilters.size(), 2u);
        EXPECT_EQ(list->mHavokFilters[0].mLayer, 4);
        EXPECT_EQ(list->mHavokFilters[1].mGroup, 0x0506);

        const auto* mopp = parsed.as<Nif::bhkMoppBvTreeShape>(4);
        ASSERT_NE(mopp, nullptr);
        EXPECT_EQ(mopp->mShape.getPtr(), list);
        EXPECT_EQ(mopp->mScale, 1.5f);
        EXPECT_EQ(mopp->mMopp.mOffset, osg::Vec4f(1, 2, 3, 4));
        EXPECT_THAT(mopp->mMopp.mData, ElementsAre(1, 2, 3));

        const auto* transform = parsed.as<Nif::bhkConvexTransformShape>(5);
        ASSERT_NE(transform, nullptr);
        EXPECT_EQ(transform->mShape.getPtr(), parsed.as<Nif::bhkSphereShape>(0));
        EXPECT_EQ(transform->mHavokMaterial.mMaterial, 4u);
        EXPECT_EQ(transform->mTransform.getTrans(), osg::Vec3f(1, 2, 3));

        const auto* convexList = parsed.as<Nif::bhkConvexListShape>(6);
        ASSERT_NE(convexList, nullptr);
        ASSERT_EQ(convexList->mSubShapes.size(), 2u);
        EXPECT_EQ(convexList->mSubShapes[1].getPtr(), parsed.as<Nif::bhkSphereShape>(2));
        EXPECT_EQ(convexList->mMaterial.mMaterial, 6u);
        EXPECT_EQ(convexList->mRadius, 0.5f);
        EXPECT_EQ(convexList->mChildShapeProperty.mSize, 2u);
        EXPECT_TRUE(convexList->mUseCachedAABB);
        EXPECT_EQ(convexList->mClosestPointMinDistance, 0.125f);

        const auto* sweep = parsed.as<Nif::bhkConvexSweepShape>(7);
        ASSERT_NE(sweep, nullptr);
        EXPECT_EQ(sweep->mShape.getPtr(), parsed.as<Nif::bhkSphereShape>(0));
        EXPECT_EQ(sweep->mMaterial.mMaterial, 8u);
        EXPECT_EQ(sweep->mRadius, 0.5f);
    }

    TEST(NifHavokTest, readsTheConvexShapeThatIsMadeOfPoints)
    {
        Builder file(fallout3);
        Bytes bytes;
        bytes.u32(1).f32(0.5f);
        property(bytes, 1, 2, 3);
        property(bytes, 4, 5, 6);
        bytes.u32(2).vec4(0, 0, 0, 0).vec4(1, 1, 1, 0);
        bytes.u32(1).vec4(0, 0, 1, 2);
        file.add("bhkConvexVerticesShape", bytes);
        Bytes spheres;
        spheres.u32(2);
        property(spheres, 7, 8, 9);
        spheres.u32(2).vec3(1, 2, 3).f32(0.5f).vec3(4, 5, 6).f32(1.5f);
        file.add("bhkMultiSphereShape", spheres);

        const Parsed parsed(file.build());

        const auto* vertices = parsed.as<Nif::bhkConvexVerticesShape>(0);
        ASSERT_NE(vertices, nullptr);
        EXPECT_EQ(vertices->mVerticesProperty.mData, 1u);
        EXPECT_EQ(vertices->mNormalsProperty.mCapacityAndFlags, 6u);
        EXPECT_THAT(vertices->mVertices, ElementsAre(osg::Vec4f(0, 0, 0, 0), osg::Vec4f(1, 1, 1, 0)));
        EXPECT_THAT(vertices->mNormals, ElementsAre(osg::Vec4f(0, 0, 1, 2)));

        const auto* multi = parsed.as<Nif::bhkMultiSphereShape>(1);
        ASSERT_NE(multi, nullptr);
        EXPECT_EQ(multi->mHavokMaterial.mMaterial, 2u);
        EXPECT_EQ(multi->mShapeProperty.mSize, 8u);
        ASSERT_EQ(multi->mSpheres.size(), 2u);
        EXPECT_EQ(multi->mSpheres[1].center(), osg::Vec3f(4, 5, 6));
        EXPECT_EQ(multi->mSpheres[1].radius(), 1.5f);
    }

    TEST(NifHavokTest, readsTheShapesThatAreMadeOfTriangles)
    {
        for (const Profile& profile : { oblivion, fallout3 })
        {
            const bool oblivionFile = profile.mVersion <= 0x14000005;
            Builder file(profile);
            // 0: strips with a filter for each, none of the data
            Bytes strips;
            havokMaterial(file, strips, 3).f32(0.5f).zeros(20).u32(2).vec4(1, 1, 1, 0).refs({}).u32(2);
            havokFilter(strips, 1, 2, 3);
            havokFilter(strips, 4, 5, 6);
            file.add("bhkNiTriStripsShape", strips);
            // 1: the shape of packed triangles that points to its data in 2
            Bytes packed;
            if (oblivionFile)
            {
                packed.u16(1);
                havokFilter(packed, 7, 8, 9).u32(3);
                havokMaterial(file, packed, 4);
            }
            packed.u32(55).zeros(4).f32(0.5f).zeros(4).vec4(1, 2, 3, 0).zeros(20).ref(2);
            file.add("bhkPackedNiTriStripsShape", packed);
            // 2: the data: two triangles, three vertices
            Bytes data;
            data.u32(2);
            for (int i = 0; i < 2; ++i)
            {
                data.u16(0).u16(1).u16(2).u16(static_cast<std::uint16_t>(100 + i));
                if (oblivionFile)
                    data.vec3(0, 0, 1);
            }
            data.u32(3);
            if (!oblivionFile)
                data.boolean(false);
            data.vec3(0, 0, 0).vec3(1, 0, 0).vec3(0, 1, 0);
            if (!oblivionFile)
            {
                data.u16(1);
                havokFilter(data, 7, 8, 9).u32(3);
                havokMaterial(file, data, 4);
            }
            file.add("hkPackedNiTriStripsData", data);
            // 3: the shape of a mesh, which has data of its own
            Bytes mesh;
            mesh.zeros(8).f32(0.5f).zeros(8).vec4(2, 2, 2, 0).u32(1);
            property(mesh, 1, 2, 3).zeros(12).refs({});
            file.add("bhkMeshShape", mesh);

            const Parsed parsed(file.build());

            const auto* triStrips = parsed.as<Nif::bhkNiTriStripsShape>(0);
            ASSERT_NE(triStrips, nullptr);
            EXPECT_EQ(triStrips->mHavokMaterial.mMaterial, 3u);
            EXPECT_EQ(triStrips->mRadius, 0.5f);
            EXPECT_EQ(triStrips->mGrowBy, 2u);
            EXPECT_EQ(triStrips->mScale, osg::Vec4f(1, 1, 1, 0));
            EXPECT_TRUE(triStrips->mData.empty());
            ASSERT_EQ(triStrips->mHavokFilters.size(), 2u);
            EXPECT_EQ(triStrips->mHavokFilters[1].mLayer, 4);

            const auto* shape = parsed.as<Nif::bhkPackedNiTriStripsShape>(1);
            ASSERT_NE(shape, nullptr);
            EXPECT_EQ(shape->mUserData, 55u);
            EXPECT_EQ(shape->mRadius, 0.5f);
            EXPECT_EQ(shape->mScale, osg::Vec4f(1, 2, 3, 0));
            EXPECT_EQ(shape->mData.getPtr(), parsed.as<Nif::hkPackedNiTriStripsData>(2));
            EXPECT_EQ(shape->mSubshapes.size(), oblivionFile ? 1u : 0u);

            const auto* triangles = parsed.as<Nif::hkPackedNiTriStripsData>(2);
            ASSERT_NE(triangles, nullptr);
            ASSERT_EQ(triangles->mTriangles.size(), 2u);
            EXPECT_EQ(triangles->mTriangles[1].mTriangle, (std::array<std::uint16_t, 3>{ 0, 1, 2 }));
            EXPECT_EQ(triangles->mTriangles[1].mWeldingInfo, 101u);
            if (oblivionFile)
                EXPECT_EQ(triangles->mTriangles[0].mNormal, osg::Vec3f(0, 0, 1));
            EXPECT_THAT(
                triangles->mVertices, ElementsAre(osg::Vec3f(0, 0, 0), osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0)));
            ASSERT_EQ(triangles->mSubshapes.size(), oblivionFile ? 0u : 1u);
            if (!oblivionFile)
            {
                EXPECT_EQ(triangles->mSubshapes[0].mHavokFilter.mLayer, 7);
                EXPECT_EQ(triangles->mSubshapes[0].mNumVertices, 3u);
                EXPECT_EQ(triangles->mSubshapes[0].mHavokMaterial.mMaterial, 4u);
            }

            const auto* meshShape = parsed.as<Nif::bhkMeshShape>(3);
            ASSERT_NE(meshShape, nullptr);
            EXPECT_EQ(meshShape->mRadius, 0.5f);
            EXPECT_EQ(meshShape->mScale, osg::Vec4f(2, 2, 2, 0));
            ASSERT_EQ(meshShape->mShapeProperties.size(), 1u);
            EXPECT_EQ(meshShape->mShapeProperties[0].mSize, 2u);
            EXPECT_TRUE(meshShape->mDataList.empty());
        }
    }

    TEST(NifHavokTest, skipsThePointsOfAShapeThatHavokCompressed)
    {
        Builder file(fallout3);
        Bytes data;
        data.u32(0).u32(2).boolean(true).zeros(12).u16(0);
        file.add("hkPackedNiTriStripsData", data);

        const Parsed parsed(file.build());

        const auto* triangles = parsed.as<Nif::hkPackedNiTriStripsData>(0);
        ASSERT_NE(triangles, nullptr);
        EXPECT_TRUE(triangles->mVertices.empty());
        EXPECT_TRUE(triangles->mSubshapes.empty());
    }

    /// The data of a shape that Skyrim compresses: the bits, the bounds and then the lists.
    Bytes compressedMeshData(const Builder& file)
    {
        Bytes bytes;
        bytes.u32(17).u32(18).u32(0xFFFF).u32(0x1FFFF).f32(0.5f).vec4(0, 0, 0, 0).vec4(1, 2, 3, 0).u8(1).u8(2);
        bytes.u32(1).u32(0); // three lists that are skipped
        bytes.u32(0).u32(2).u32(0).u32(0);
        bytes.u32(2); // the materials
        havokMaterial(file, bytes, 5);
        havokFilter(bytes, 1, 2, 3);
        havokMaterial(file, bytes, 6);
        havokFilter(bytes, 4, 5, 6);
        bytes.u32(0); // unused
        bytes.u32(1).vec4(1, 2, 3, 0).quat(1, 0, 0, 0); // the transforms of the chunks
        bytes.u32(2).vec4(0, 0, 0, 0).vec4(1, 1, 1, 0); // the vertices that are not in a chunk
        bytes.u32(1).u16(0).u16(1).u16(1).u32(7).u16(5); // the triangles that are not in a chunk
        // the chunks
        bytes.u32(1).vec4(4, 5, 6, 0).u32(1).u16(2).u16(3);
        bytes.u32(2).u16(10).u16(11); // vertices
        bytes.u32(1).u16(12); // indices
        bytes.u32(1).u16(13); // strips
        bytes.u32(1).u16(14); // welding information
        return std::move(bytes.u32(0)); // unused
    }

    TEST(NifHavokTest, readsTheCompressedMeshShapeOfSkyrimAndFallout3)
    {
        for (const Profile& profile : { fallout3, skyrim })
        {
            Builder file(profile);
            file.add("NiNode", file.node({ .mName = "Root" }));
            file.add("bhkCompressedMeshShape",
                Bytes().ref(0).u32(9).f32(0.5f).zeros(4).vec4(1, 2, 3, 0).zeros(4).zeros(16).ref(2));
            file.add("bhkCompressedMeshShapeData", compressedMeshData(file));
            file.addRoot(0);

            const Parsed parsed(file.build());

            const auto* shape = parsed.as<Nif::bhkCompressedMeshShape>(1);
            ASSERT_NE(shape, nullptr);
            EXPECT_EQ(shape->mTarget.getPtr(), parsed.as<Nif::NiNode>(0));
            EXPECT_EQ(shape->mUserData, 9u);
            EXPECT_EQ(shape->mRadius, 0.5f);
            EXPECT_EQ(shape->mScale, osg::Vec4f(1, 2, 3, 0));
            EXPECT_EQ(shape->mData.getPtr(), parsed.as<Nif::bhkCompressedMeshShapeData>(2));

            const auto* data = parsed.as<Nif::bhkCompressedMeshShapeData>(2);
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->mBitsPerIndex, 17u);
            EXPECT_EQ(data->mBitsPerWIndex, 18u);
            EXPECT_EQ(data->mMaskWIndex, 0xFFFFu);
            EXPECT_EQ(data->mMaskIndex, 0x1FFFFu);
            EXPECT_EQ(data->mError, 0.5f);
            EXPECT_EQ(data->mAabbMax, osg::Vec4f(1, 2, 3, 0));
            EXPECT_EQ(data->mWeldingType, 1);
            EXPECT_EQ(data->mMaterialType, 2);
            ASSERT_EQ(data->mMaterials.size(), 2u);
            EXPECT_EQ(data->mMaterials[1].mHavokMaterial.mMaterial, 6u);
            EXPECT_EQ(data->mMaterials[1].mHavokFilter.mLayer, 4);
            ASSERT_EQ(data->mChunkTransforms.size(), 1u);
            EXPECT_EQ(data->mChunkTransforms[0].mTranslation, osg::Vec4f(1, 2, 3, 0));
            EXPECT_THAT(data->mBigVerts, ElementsAre(osg::Vec4f(0, 0, 0, 0), osg::Vec4f(1, 1, 1, 0)));
            ASSERT_EQ(data->mBigTris.size(), 1u);
            EXPECT_EQ(data->mBigTris[0].mTriangle, (std::array<std::uint16_t, 3>{ 0, 1, 1 }));
            EXPECT_EQ(data->mBigTris[0].mMaterial, 7u);
            EXPECT_EQ(data->mBigTris[0].mWeldingInfo, 5u);
            ASSERT_EQ(data->mChunks.size(), 1u);
            EXPECT_EQ(data->mChunks[0].mTranslation, osg::Vec4f(4, 5, 6, 0));
            EXPECT_EQ(data->mChunks[0].mMaterialIndex, 1u);
            EXPECT_EQ(data->mChunks[0].mReference, 2u);
            EXPECT_EQ(data->mChunks[0].mTransformIndex, 3u);
            EXPECT_THAT(data->mChunks[0].mVertices, ElementsAre(10, 11));
            EXPECT_THAT(data->mChunks[0].mIndices, ElementsAre(12));
            EXPECT_THAT(data->mChunks[0].mStrips, ElementsAre(13));
            EXPECT_THAT(data->mChunks[0].mWeldingInfos, ElementsAre(14));
        }
    }

    TEST(NifHavokTest, readsThePhantoms)
    {
        Builder file(fallout3);
        file.add("bhkSphereShape", Bytes().u32(1).f32(0.5f));
        Bytes aabb = worldObject(file, 0);
        aabb.zeros(8).vec4(-1, -2, -3, 0).vec4(1, 2, 3, 0);
        file.add("bhkAabbPhantom", aabb);
        Bytes simple = worldObject(file, 0);
        simple.zeros(8);
        for (const float value : { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 4.f, 5.f, 6.f, 1.f })
            simple.f32(value);
        file.add("bhkSimpleShapePhantom", simple);

        const Parsed parsed(file.build());

        const auto* phantom = parsed.as<Nif::bhkAabbPhantom>(1);
        ASSERT_NE(phantom, nullptr);
        EXPECT_EQ(phantom->mShape.getPtr(), parsed.as<Nif::bhkSphereShape>(0));
        EXPECT_EQ(phantom->mAabbMin, osg::Vec4f(-1, -2, -3, 0));
        EXPECT_EQ(phantom->mAabbMax, osg::Vec4f(1, 2, 3, 0));

        const auto* shapePhantom = parsed.as<Nif::bhkSimpleShapePhantom>(2);
        ASSERT_NE(shapePhantom, nullptr);
        EXPECT_EQ(shapePhantom->mTransform.getTrans(), osg::Vec3f(4, 5, 6));
    }

    /// The part that every constraint starts with: the entities and the priority.
    Bytes constraintInfo(std::int32_t a, std::int32_t b, std::uint32_t priority = 1)
    {
        return std::move(Bytes().u32(2).ref(a).ref(b).u32(priority));
    }

    Bytes& motor(Bytes& bytes, std::uint8_t type)
    {
        bytes.u8(type);
        switch (type)
        {
            case 1:
                return bytes.f32(1).f32(2).f32(3).f32(4).f32(5).f32(6).boolean(true);
            case 2:
                return bytes.f32(1).f32(2).f32(3).f32(4).boolean(true).boolean(false);
            case 3:
                return bytes.f32(1).f32(2).f32(3).f32(4).boolean(true);
            default:
                return bytes;
        }
    }

    /// The vectors of a constraint: four of them, numbered so that each can be told from the others.
    Bytes& vectors(Bytes& bytes, int count, float first = 1)
    {
        for (int i = 0; i < count; ++i)
            bytes.vec4(first + i, 0, 0, 0);
        return bytes;
    }

    TEST(NifHavokTest, readsTheConstraintsOfOblivion)
    {
        Builder file(oblivion);
        file.add("bhkRigidBody", rigidBody(file, -1));
        file.add("bhkRigidBody", rigidBody(file, -1));
        // 2: the ragdoll, with a plane, a twist and a pivot for each of the entities
        Bytes ragdoll = constraintInfo(0, 1, 3);
        vectors(ragdoll, 6).f32(1).f32(2).f32(3).f32(4).f32(5).f32(6);
        file.add("bhkRagdollConstraint", ragdoll);
        // 3: the hinge keeps the pivot and the perpendicular axes of the first and the pivot and the axis of the second
        Bytes hinge = constraintInfo(0, 1);
        vectors(hinge, 5);
        file.add("bhkHingeConstraint", hinge);
        // 4: the limited hinge
        Bytes limited = constraintInfo(0, 1);
        vectors(limited, 7).f32(0.5f).f32(1.5f).f32(2.5f);
        file.add("bhkLimitedHingeConstraint", limited);
        // 5, 6: the ball and socket and the stiff spring
        file.add("bhkBallAndSocketConstraint", constraintInfo(0, 1).vec4(1, 2, 3, 0).vec4(4, 5, 6, 0));
        file.add("bhkStiffSpringConstraint", constraintInfo(0, 1).vec4(1, 2, 3, 0).vec4(4, 5, 6, 0).f32(7));
        // 7: the prismatic
        Bytes prismatic = constraintInfo(0, 1);
        vectors(prismatic, 8).f32(1).f32(2).f32(3);
        file.add("bhkPrismaticConstraint", prismatic);
        // 8: the malleable constraint that wraps a ball and socket
        Bytes malleable = constraintInfo(0, 1);
        malleable.u32(0).raw(constraintInfo(0, 1).str()).vec4(1, 0, 0, 0).vec4(2, 0, 0, 0).f32(0.25f).f32(0.75f);
        file.add("bhkMalleableConstraint", malleable);
        // 9: the breakable constraint with a hinge in it
        Bytes breakable = constraintInfo(0, 1);
        breakable.u32(1).raw(constraintInfo(0, 1).str());
        vectors(breakable, 5).f32(3).boolean(true);
        file.add("bhkBreakableConstraint", breakable);

        const Parsed parsed(file.build());

        const auto* ragdollConstraint = parsed.as<Nif::bhkRagdollConstraint>(2);
        ASSERT_NE(ragdollConstraint, nullptr);
        EXPECT_EQ(ragdollConstraint->mInfo.mEntityA.getPtr(), parsed.as<Nif::bhkRigidBody>(0));
        EXPECT_EQ(ragdollConstraint->mInfo.mEntityB.getPtr(), parsed.as<Nif::bhkRigidBody>(1));
        EXPECT_EQ(ragdollConstraint->mInfo.mPriority, Nif::ConstraintPriority::Priority_TimeOfImpact);
        EXPECT_EQ(ragdollConstraint->mConstraint.mDataA.mPivot, osg::Vec4f(1, 0, 0, 0));
        EXPECT_EQ(ragdollConstraint->mConstraint.mDataA.mPlane, osg::Vec4f(2, 0, 0, 0));
        EXPECT_EQ(ragdollConstraint->mConstraint.mDataA.mTwist, osg::Vec4f(3, 0, 0, 0));
        EXPECT_EQ(ragdollConstraint->mConstraint.mDataB.mPivot, osg::Vec4f(4, 0, 0, 0));
        EXPECT_EQ(ragdollConstraint->mConstraint.mConeMaxAngle, 1.f);
        EXPECT_EQ(ragdollConstraint->mConstraint.mPlaneMinAngle, 2.f);
        EXPECT_EQ(ragdollConstraint->mConstraint.mPlaneMaxAngle, 3.f);
        EXPECT_EQ(ragdollConstraint->mConstraint.mTwistMinAngle, 4.f);
        EXPECT_EQ(ragdollConstraint->mConstraint.mTwistMaxAngle, 5.f);
        EXPECT_EQ(ragdollConstraint->mConstraint.mMaxFriction, 6.f);

        const auto* hingeConstraint = parsed.as<Nif::bhkHingeConstraint>(3);
        ASSERT_NE(hingeConstraint, nullptr);
        EXPECT_EQ(hingeConstraint->mConstraint.mDataA.mPivot, osg::Vec4f(1, 0, 0, 0));
        EXPECT_EQ(hingeConstraint->mConstraint.mDataA.mPerpAxis1, osg::Vec4f(2, 0, 0, 0));
        EXPECT_EQ(hingeConstraint->mConstraint.mDataA.mPerpAxis2, osg::Vec4f(3, 0, 0, 0));
        EXPECT_EQ(hingeConstraint->mConstraint.mDataB.mPivot, osg::Vec4f(4, 0, 0, 0));
        EXPECT_EQ(hingeConstraint->mConstraint.mDataB.mAxis, osg::Vec4f(5, 0, 0, 0));

        const auto* limitedConstraint = parsed.as<Nif::bhkLimitedHingeConstraint>(4);
        ASSERT_NE(limitedConstraint, nullptr);
        EXPECT_EQ(limitedConstraint->mConstraint.mDataA.mPivot, osg::Vec4f(1, 0, 0, 0));
        EXPECT_EQ(limitedConstraint->mConstraint.mDataA.mAxis, osg::Vec4f(2, 0, 0, 0));
        EXPECT_EQ(limitedConstraint->mConstraint.mDataB.mPivot, osg::Vec4f(5, 0, 0, 0));
        EXPECT_EQ(limitedConstraint->mConstraint.mDataB.mPerpAxis2, osg::Vec4f(7, 0, 0, 0));
        EXPECT_EQ(limitedConstraint->mConstraint.mMinAngle, 0.5f);
        EXPECT_EQ(limitedConstraint->mConstraint.mMaxAngle, 1.5f);
        EXPECT_EQ(limitedConstraint->mConstraint.mMaxFriction, 2.5f);

        EXPECT_EQ(parsed.as<Nif::bhkBallAndSocketConstraint>(5)->mConstraint.mPivotB, osg::Vec4f(4, 5, 6, 0));
        const auto* spring = parsed.as<Nif::bhkStiffSpringConstraint>(6);
        ASSERT_NE(spring, nullptr);
        EXPECT_EQ(spring->mConstraint.mPivotA, osg::Vec4f(1, 2, 3, 0));
        EXPECT_EQ(spring->mConstraint.mLength, 7.f);

        const auto* prismaticConstraint = parsed.as<Nif::bhkPrismaticConstraint>(7);
        ASSERT_NE(prismaticConstraint, nullptr);
        EXPECT_EQ(prismaticConstraint->mConstraint.mDataA.mPivot, osg::Vec4f(1, 0, 0, 0));
        EXPECT_EQ(prismaticConstraint->mConstraint.mDataA.mSliding, osg::Vec4f(4, 0, 0, 0));
        EXPECT_EQ(prismaticConstraint->mConstraint.mDataB.mSliding, osg::Vec4f(5, 0, 0, 0));
        EXPECT_EQ(prismaticConstraint->mConstraint.mMinDistance, 1.f);
        EXPECT_EQ(prismaticConstraint->mConstraint.mMaxDistance, 2.f);
        EXPECT_EQ(prismaticConstraint->mConstraint.mFriction, 3.f);

        const auto* malleableConstraint = parsed.as<Nif::bhkMalleableConstraint>(8);
        ASSERT_NE(malleableConstraint, nullptr);
        EXPECT_EQ(malleableConstraint->mConstraint.mType, Nif::HkConstraintType::BallAndSocket);
        EXPECT_EQ(malleableConstraint->mConstraint.mBallAndSocketInfo.mPivotB, osg::Vec4f(2, 0, 0, 0));
        EXPECT_EQ(malleableConstraint->mConstraint.mTau, 0.25f);
        EXPECT_EQ(malleableConstraint->mConstraint.mDamping, 0.75f);

        const auto* breakableConstraint = parsed.as<Nif::bhkBreakableConstraint>(9);
        ASSERT_NE(breakableConstraint, nullptr);
        EXPECT_EQ(breakableConstraint->mConstraint.mType, Nif::HkConstraintType::Hinge);
        EXPECT_EQ(breakableConstraint->mConstraint.mHingeInfo.mDataB.mAxis, osg::Vec4f(5, 0, 0, 0));
        EXPECT_EQ(breakableConstraint->mThreshold, 3.f);
        EXPECT_TRUE(breakableConstraint->mRemoveWhenBroken);
    }

    TEST(NifHavokTest, readsTheConstraintsOfFallout3WithTheirMotors)
    {
        for (const std::uint8_t motorType : { 0, 1, 2, 3 })
        {
            Builder file(fallout3);
            file.add("bhkRigidBody", rigidBody(file, -1));
            file.add("bhkRigidBody", rigidBody(file, -1));
            // 2: the ragdoll: the twist, the plane, the motor and the pivot of each entity
            Bytes ragdoll = constraintInfo(0, 1);
            vectors(ragdoll, 8).f32(1).f32(2).f32(3).f32(4).f32(5).f32(6);
            motor(ragdoll, motorType);
            file.add("bhkRagdollConstraint", ragdoll);
            // 3: the limited hinge: the axis, the two perpendicular axes and the pivot of each
            Bytes limited = constraintInfo(0, 1);
            vectors(limited, 8).f32(0.5f).f32(1.5f).f32(2.5f);
            motor(limited, motorType);
            file.add("bhkLimitedHingeConstraint", limited);
            // 4: the prismatic with the motor
            Bytes prismatic = constraintInfo(0, 1);
            vectors(prismatic, 8).f32(1).f32(2).f32(3);
            motor(prismatic, motorType);
            file.add("bhkPrismaticConstraint", prismatic);
            // 5: the hinge, which keeps the eight vectors
            Bytes hinge = constraintInfo(0, 1);
            vectors(hinge, 8);
            file.add("bhkHingeConstraint", hinge);

            const Parsed parsed(file.build());

            const auto* ragdollConstraint = parsed.as<Nif::bhkRagdollConstraint>(2);
            ASSERT_NE(ragdollConstraint, nullptr) << int(motorType);
            EXPECT_EQ(ragdollConstraint->mConstraint.mDataA.mTwist, osg::Vec4f(1, 0, 0, 0));
            EXPECT_EQ(ragdollConstraint->mConstraint.mDataA.mPlane, osg::Vec4f(2, 0, 0, 0));
            EXPECT_EQ(ragdollConstraint->mConstraint.mDataA.mMotor, osg::Vec4f(3, 0, 0, 0));
            EXPECT_EQ(ragdollConstraint->mConstraint.mDataA.mPivot, osg::Vec4f(4, 0, 0, 0));
            EXPECT_EQ(ragdollConstraint->mConstraint.mDataB.mPivot, osg::Vec4f(8, 0, 0, 0));
            EXPECT_EQ(ragdollConstraint->mConstraint.mMaxFriction, 6.f);
            EXPECT_EQ(ragdollConstraint->mConstraint.mMotor.mType, static_cast<Nif::HkMotorType>(motorType));
            const auto& ragdollMotor = ragdollConstraint->mConstraint.mMotor;
            if (motorType == 1)
            {
                EXPECT_EQ(ragdollMotor.mPositionMotor.mMinForce, 1.f);
                EXPECT_EQ(ragdollMotor.mPositionMotor.mMaxForce, 2.f);
                EXPECT_EQ(ragdollMotor.mPositionMotor.mTau, 3.f);
                EXPECT_EQ(ragdollMotor.mPositionMotor.mDamping, 4.f);
                EXPECT_EQ(ragdollMotor.mPositionMotor.mProportionalRecoveryVelocity, 5.f);
                EXPECT_EQ(ragdollMotor.mPositionMotor.mConstantRecoveryVelocity, 6.f);
                EXPECT_TRUE(ragdollMotor.mPositionMotor.mEnabled);
            }
            else if (motorType == 2)
            {
                EXPECT_EQ(ragdollMotor.mVelocityMotor.mTargetVelocity, 4.f);
                EXPECT_TRUE(ragdollMotor.mVelocityMotor.mUseVelocityTarget);
                EXPECT_FALSE(ragdollMotor.mVelocityMotor.mEnabled);
            }
            else if (motorType == 3)
            {
                EXPECT_EQ(ragdollMotor.mSpringDamperMotor.mSpringConstant, 3.f);
                EXPECT_EQ(ragdollMotor.mSpringDamperMotor.mSpringDamping, 4.f);
                EXPECT_TRUE(ragdollMotor.mSpringDamperMotor.mEnabled);
            }

            const auto* limitedConstraint = parsed.as<Nif::bhkLimitedHingeConstraint>(3);
            ASSERT_NE(limitedConstraint, nullptr);
            EXPECT_EQ(limitedConstraint->mConstraint.mDataA.mAxis, osg::Vec4f(1, 0, 0, 0));
            EXPECT_EQ(limitedConstraint->mConstraint.mDataA.mPivot, osg::Vec4f(4, 0, 0, 0));
            EXPECT_EQ(limitedConstraint->mConstraint.mDataB.mPerpAxis1, osg::Vec4f(6, 0, 0, 0));
            EXPECT_EQ(limitedConstraint->mConstraint.mMinAngle, 0.5f);
            EXPECT_EQ(limitedConstraint->mConstraint.mMotor.mType, static_cast<Nif::HkMotorType>(motorType));

            const auto* prismaticConstraint = parsed.as<Nif::bhkPrismaticConstraint>(4);
            ASSERT_NE(prismaticConstraint, nullptr);
            EXPECT_EQ(prismaticConstraint->mConstraint.mDataA.mSliding, osg::Vec4f(1, 0, 0, 0));
            EXPECT_EQ(prismaticConstraint->mConstraint.mDataA.mPivot, osg::Vec4f(4, 0, 0, 0));
            EXPECT_EQ(prismaticConstraint->mConstraint.mFriction, 3.f);
            EXPECT_EQ(prismaticConstraint->mConstraint.mMotor.mType, static_cast<Nif::HkMotorType>(motorType));

            const auto* hingeConstraint = parsed.as<Nif::bhkHingeConstraint>(5);
            ASSERT_NE(hingeConstraint, nullptr);
            EXPECT_EQ(hingeConstraint->mConstraint.mDataA.mAxis, osg::Vec4f(1, 0, 0, 0));
            EXPECT_EQ(hingeConstraint->mConstraint.mDataB.mPivot, osg::Vec4f(8, 0, 0, 0));
        }
    }

    TEST(NifHavokTest, readsTheWrappedConstraintsOfEveryKind)
    {
        // each kind of constraint that a breakable or a malleable one can hold, with the bytes that it writes
        const auto body = [](std::uint32_t type) {
            Bytes bytes;
            switch (type)
            {
                case 0:
                    return std::move(bytes.vec4(1, 0, 0, 0).vec4(2, 0, 0, 0));
                case 1:
                    return std::move(vectors(bytes, 8));
                case 2:
                case 6:
                    return std::move(vectors(bytes, 8).f32(1).f32(2).f32(3));
                case 7:
                    return std::move(vectors(bytes, 8).f32(1).f32(2).f32(3).f32(4).f32(5).f32(6));
                default:
                    return std::move(bytes.vec4(1, 0, 0, 0).vec4(2, 0, 0, 0).f32(9));
            }
        };

        for (const std::uint32_t type : { 0u, 1u, 2u, 6u, 7u, 8u })
        {
            Builder file(fallout3);
            file.add("bhkRigidBody", rigidBody(file, -1));
            // the constraint of Fallout 3 has a motor after the data of the types that have one
            Bytes bytes = constraintInfo(0, 0);
            bytes.u32(type).raw(constraintInfo(0, 0, 3).str()).raw(body(type).str());
            if (type == 2 || type == 6 || type == 7)
                bytes.u8(0);
            bytes.f32(4).boolean(false);
            file.add("bhkBreakableConstraint", bytes);

            const Parsed parsed(file.build());

            const auto* breakable = parsed.as<Nif::bhkBreakableConstraint>(1);
            ASSERT_NE(breakable, nullptr) << type;
            EXPECT_EQ(breakable->mConstraint.mType, static_cast<Nif::HkConstraintType>(type));
            EXPECT_EQ(breakable->mConstraint.mInfo.mPriority, Nif::ConstraintPriority::Priority_TimeOfImpact);
            EXPECT_EQ(breakable->mThreshold, 4.f);
            EXPECT_FALSE(breakable->mRemoveWhenBroken);
        }
    }

    TEST(NifHavokTest, aMalleableConstraintOfFallout3HasAStrengthAndAnotherConstraintInIt)
    {
        Builder file(fallout3);
        file.add("bhkRigidBody", rigidBody(file, -1));
        // a ragdoll in a malleable constraint in a breakable one
        Bytes bytes = constraintInfo(0, 0);
        bytes.u32(13).raw(constraintInfo(0, 0).str());
        // the malleable constraint: the type, the entities, the ragdoll and the strength
        bytes.u32(7).raw(constraintInfo(0, 0).str());
        vectors(bytes, 8).f32(1).f32(2).f32(3).f32(4).f32(5).f32(6).u8(0);
        bytes.f32(0.5f);
        bytes.f32(4).boolean(true);
        file.add("bhkBreakableConstraint", bytes);

        const Parsed parsed(file.build());

        const auto* breakable = parsed.as<Nif::bhkBreakableConstraint>(1);
        ASSERT_NE(breakable, nullptr);
        EXPECT_EQ(breakable->mConstraint.mType, Nif::HkConstraintType::Malleable);
        EXPECT_EQ(breakable->mConstraint.mMalleableInfo.mType, Nif::HkConstraintType::Ragdoll);
        EXPECT_EQ(breakable->mConstraint.mMalleableInfo.mRagdollInfo.mConeMaxAngle, 1.f);
        EXPECT_EQ(breakable->mConstraint.mMalleableInfo.mStrength, 0.5f);
        EXPECT_EQ(breakable->mThreshold, 4.f);
        EXPECT_TRUE(breakable->mRemoveWhenBroken);
    }

    TEST(NifHavokTest, refusesAConstraintThatItDoesNotKnow)
    {
        for (const char* type : { "bhkBreakableConstraint", "bhkMalleableConstraint" })
        {
            Builder file(oblivion);
            file.add("bhkRigidBody", rigidBody(file, -1));
            Bytes bytes = constraintInfo(0, 0);
            bytes.u32(5).raw(constraintInfo(0, 0).str()).zeros(64);
            file.add(type, bytes);

            EXPECT_THROW(Parsed(file.build()), std::exception) << type;
        }
        // the malleable constraint cannot hold another one
        Builder file(oblivion);
        file.add("bhkRigidBody", rigidBody(file, -1));
        file.add("bhkMalleableConstraint", constraintInfo(0, 0).u32(13).raw(constraintInfo(0, 0).str()).zeros(64));
        EXPECT_THROW(Parsed(file.build()), std::exception);
    }

    TEST(NifHavokTest, readsAChainOfBallAndSocketConstraints)
    {
        Builder file(oblivion);
        file.add("bhkRigidBody", rigidBody(file, -1));
        file.add("bhkRigidBody", rigidBody(file, -1));
        Bytes chain;
        chain.u32(4); // pivots, two for each constraint
        chain.vec4(1, 0, 0, 0).vec4(2, 0, 0, 0).vec4(3, 0, 0, 0).vec4(4, 0, 0, 0);
        chain.f32(0.5f).f32(0.25f).f32(0.125f).f32(0.0625f);
        chain.refs({ 0, 1 }).raw(constraintInfo(0, 1, 3).str());
        file.add("bhkBallSocketConstraintChain", chain);

        const Parsed parsed(file.build());

        const auto* result = parsed.as<Nif::bhkBallSocketConstraintChain>(2);
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(result->mConstraints.size(), 2u);
        EXPECT_EQ(result->mConstraints[1].mPivotA, osg::Vec4f(3, 0, 0, 0));
        EXPECT_EQ(result->mTau, 0.5f);
        EXPECT_EQ(result->mDamping, 0.25f);
        EXPECT_EQ(result->mConstraintForceMixing, 0.125f);
        EXPECT_EQ(result->mMaxErrorDistance, 0.0625f);
        ASSERT_EQ(result->mConstraintChainInfo.mEntities.size(), 2u);
        EXPECT_EQ(result->mConstraintChainInfo.mEntities[1].getPtr(), parsed.as<Nif::bhkRigidBody>(1));
        EXPECT_EQ(result->mConstraintChainInfo.mInfo.mPriority, Nif::ConstraintPriority::Priority_TimeOfImpact);
    }

    TEST(NifHavokTest, refusesAChainWithAnOddNumberOfPivots)
    {
        Builder file(oblivion);
        file.add("bhkBallSocketConstraintChain", Bytes().u32(3).zeros(200));

        EXPECT_THROW(Parsed(file.build()), std::exception);
    }

    TEST(NifHavokTest, readsTheActionsOnABody)
    {
        Builder file(oblivion);
        file.add("bhkRigidBody", rigidBody(file, -1));
        file.add("bhkLiquidAction", Bytes().zeros(12).f32(1).f32(2).f32(3).f32(4));
        file.add("bhkOrientHingedBodyAction",
            Bytes().ref(0).zeros(8).zeros(8).vec4(0, 0, 1, 0).vec4(1, 0, 0, 0).f32(0.5f).f32(0.25f).zeros(8));

        const Parsed parsed(file.build());

        const auto* liquid = parsed.as<Nif::bhkLiquidAction>(1);
        ASSERT_NE(liquid, nullptr);
        EXPECT_EQ(liquid->mInitialStickForce, 1.f);
        EXPECT_EQ(liquid->mStickStrength, 2.f);
        EXPECT_EQ(liquid->mNeighborDistance, 3.f);
        EXPECT_EQ(liquid->mNeighborStrength, 4.f);

        const auto* hinged = parsed.as<Nif::bhkOrientHingedBodyAction>(2);
        ASSERT_NE(hinged, nullptr);
        EXPECT_EQ(hinged->mEntity.getPtr(), parsed.as<Nif::bhkRigidBody>(0));
        EXPECT_EQ(hinged->mHingeAxisLS, osg::Vec4f(0, 0, 1, 0));
        EXPECT_EQ(hinged->mForwardLS, osg::Vec4f(1, 0, 0, 0));
        EXPECT_EQ(hinged->mStrength, 0.5f);
        EXPECT_EQ(hinged->mDamping, 0.25f);
    }

    TEST(NifHavokTest, readsTheRagdollTemplateAndItsPoses)
    {
        Builder file(fallout3);
        // 0: the template names its bones: 1 and 2
        Bytes ragdollTemplate;
        file.string(ragdollTemplate, "Template");
        ragdollTemplate.refs({ 1, 2 });
        file.add("bhkRagdollTemplate", ragdollTemplate);
        for (const char* name : { "Spine", "Head" })
        {
            Bytes bone;
            file.string(bone, name);
            bone.f32(5).f32(0.5f).f32(0.25f).f32(0.125f).u32(9);
            bone.u32(1);
            // a ball and socket constraint
            bone.u32(0).raw(constraintInfo(-1, -1).str()).vec4(1, 0, 0, 0).vec4(2, 0, 0, 0);
            file.add("bhkRagdollTemplateData", bone);
        }
        // 3: the poses: two bones with two poses
        Bytes poses;
        poses.u32(2);
        file.string(poses, "Spine");
        file.string(poses, "Head");
        poses.u32(2);
        for (int pose = 0; pose < 2; ++pose)
        {
            poses.u32(2);
            for (int bone = 0; bone < 2; ++bone)
                poses.vec3(static_cast<float>(pose), static_cast<float>(bone), 0).quat(1, 0, 0, 0).vec3(1, 1, 1);
        }
        file.add("bhkPoseArray", poses);

        const Parsed parsed(file.build());

        const auto* result = parsed.as<Nif::bhkRagdollTemplate>(0);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->mName, "Template");
        ASSERT_EQ(result->mBones.size(), 2u);
        EXPECT_EQ(result->mBones[1].getPtr(), parsed.as<Nif::bhkRagdollTemplateData>(2));

        const auto* spine = parsed.as<Nif::bhkRagdollTemplateData>(1);
        ASSERT_NE(spine, nullptr);
        EXPECT_EQ(spine->mName, "Spine");
        EXPECT_EQ(spine->mMass, 5.f);
        EXPECT_EQ(spine->mRestitution, 0.5f);
        EXPECT_EQ(spine->mFriction, 0.25f);
        EXPECT_EQ(spine->mRadius, 0.125f);
        EXPECT_EQ(spine->mHavokMaterial.mMaterial, 9u);
        ASSERT_EQ(spine->mConstraints.size(), 1u);
        EXPECT_EQ(spine->mConstraints[0].mType, Nif::HkConstraintType::BallAndSocket);

        const auto* pose = parsed.as<Nif::bhkPoseArray>(3);
        ASSERT_NE(pose, nullptr);
        EXPECT_THAT(pose->mBones, ElementsAre("Spine", "Head"));
        ASSERT_EQ(pose->mPoses.size(), 2u);
        ASSERT_EQ(pose->mPoses[1].size(), 2u);
        EXPECT_EQ(pose->mPoses[1][1].mTranslation, osg::Vec3f(1, 1, 0));
        EXPECT_EQ(pose->mPoses[1][1].mScale, osg::Vec3f(1, 1, 1));
    }
}
