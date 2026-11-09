#include "geometry.hpp"
#include "niftestfile.hpp"

#include <components/nif/data.hpp>
#include <components/nif/exception.hpp>
#include <components/nif/node.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <BulletCollision/CollisionShapes/btCollisionShape.h>

#include <vector>

// The scene graph of the files of the games: nodes, shapes and the data of their geometry. The records are built in
// memory by NifTest::Builder with the layout of the version of each game, then read back.
namespace
{
    using namespace NifTest;
    using namespace testing;

    TEST(NifNodeTest, readsTheNodesAndTheShapeOfAFallout3File)
    {
        Builder file(fallout3);
        // 0: the root, 1: a node with a shape, 2: the shape, 3: its data, 4: a collision node
        file.add("NiNode", file.node({ .mName = "Scene Root" }, { 1, 4 }));
        file.add("NiNode", file.node({ .mName = "Weapon", .mFlags = 0x21, .mX = 4, .mY = 5, .mZ = 6, .mScale = 2.f }, { 2 }));
        file.add("NiTriShape", geometry(file, { .mName = "Barrel", .mX = 1 }, 3));
        file.add("NiTriShapeData", triShapeData(file, 4, 0x1001, { 0, 1, 2, 1, 2, 3 }));
        file.add("RootCollisionNode", file.node({ .mName = "Collision" }));
        file.addRoot(0);

        const Parsed parsed(file.build());

        ASSERT_EQ(parsed.size(), 5u);
        ASSERT_EQ(parsed.roots().size(), 1u);
        const auto* root = parsed.as<Nif::NiNode>(0);
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(parsed.roots()[0], root);
        EXPECT_EQ(root->mName, "Scene Root");
        ASSERT_EQ(root->mChildren.size(), 2u);
        const auto* weapon = parsed.as<Nif::NiNode>(1);
        ASSERT_NE(weapon, nullptr);
        EXPECT_EQ(weapon->mFlags, 0x21u);
        EXPECT_EQ(weapon->mTransform.mTranslation, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(weapon->mTransform.mScale, 2.f);
        EXPECT_THAT(weapon->mParents, ElementsAre(root));
        EXPECT_EQ(root->findRootCollisionNode(false), parsed.as<Nif::NiNode>(4));
        EXPECT_EQ(root->findRootCollisionNode(true), parsed.as<Nif::NiNode>(4));
        EXPECT_EQ(weapon->findRootCollisionNode(true), nullptr);

        const auto* shape = parsed.as<Nif::NiTriShape>(2);
        ASSERT_NE(shape, nullptr);
        EXPECT_EQ(shape->mName, "Barrel");
        EXPECT_EQ(shape->mTransform.mTranslation, osg::Vec3f(1, 0, 0));
        ASSERT_FALSE(shape->mData.empty());
        const auto* data = parsed.as<Nif::NiTriShapeData>(3);
        EXPECT_EQ(shape->mData.getPtr(), data);
        EXPECT_EQ(data->mNumVertices, 4u);
        ASSERT_EQ(data->mVertices.size(), 4u);
        EXPECT_EQ(data->mVertices[3], osg::Vec3f(3, 3.5f, 3.25f));
        ASSERT_EQ(data->mNormals.size(), 4u);
        EXPECT_EQ(data->mNormals[0], osg::Vec3f(0, 0, 1));
        EXPECT_EQ(data->mTangents.size(), 4u);
        EXPECT_EQ(data->mBitangents.size(), 4u);
        EXPECT_EQ(data->mBoundingSphere.center(), osg::Vec3f(1, 2, 3));
        EXPECT_EQ(data->mBoundingSphere.radius(), 10.f);
        ASSERT_EQ(data->mColors.size(), 4u);
        EXPECT_EQ(data->mColors[1], osg::Vec4f(1, 0.5f, 0.25f, 1));
        ASSERT_EQ(data->mUVList.size(), 1u);
        EXPECT_EQ(data->mUVList[0][2], osg::Vec2f(1, 0));
        EXPECT_EQ(data->mNumTriangles, 2u);
        EXPECT_THAT(data->mTriangles, ElementsAre(0, 1, 2, 1, 2, 3));
    }

    TEST(NifNodeTest, theTransformOfTheRootNodeIsDiscardedExceptForTheBip01)
    {
        for (const std::string_view name : { "Scene Root", "Bip01" })
        {
            Builder file(fallout3);
            file.add("NiNode", file.node({ .mName = std::string(name), .mX = 7, .mY = 8, .mZ = 9, .mScale = 3.f }));
            file.addRoot(0);

            const Parsed parsed(file.build());

            const auto* root = parsed.as<Nif::NiNode>(0);
            ASSERT_NE(root, nullptr);
            if (name == "Bip01")
            {
                EXPECT_EQ(root->mTransform.mTranslation, osg::Vec3f(7, 8, 9));
                EXPECT_EQ(root->mTransform.mScale, 3.f);
            }
            else
            {
                EXPECT_EQ(root->mTransform.mTranslation, osg::Vec3f(0, 0, 0));
                EXPECT_EQ(root->mTransform.mScale, 1.f);
            }
        }
    }

    TEST(NifNodeTest, theTransformOfTheRootNodeOfOblivionIsKept)
    {
        Builder file(oblivion);
        file.add("NiNode", file.node({ .mName = "Scene Root", .mX = 7, .mY = 8, .mZ = 9 }));
        file.addRoot(0);

        const Parsed parsed(file.build());

        EXPECT_EQ(parsed.as<Nif::NiNode>(0)->mTransform.mTranslation, osg::Vec3f(7, 8, 9));
    }

    TEST(NifNodeTest, readsTheNodesOfMorrowindWithTheirBoundsAndVelocity)
    {
        Builder file(morrowind);
        Bytes root;
        file.place(root, { .mName = "root", .mX = 1, .mY = 2, .mZ = 3 });
        root.refs({ 1 }).refs({});
        file.add("NiNode", root);
        // a child with a bounding box: the type, the centre, the axes and the extents
        Bytes child;
        file.named(child, "child");
        child.u16(0).vec3(1, 2, 3).identity3().f32(1).vec3(0.5f, 0.5f, 0.5f);
        child.refs({}).boolean32(true).u32(1).vec3(1, 2, 3).identity3().vec3(4, 5, 6);
        child.refs({}).refs({});
        file.add("NiNode", child);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* node = parsed.as<Nif::NiNode>(1);
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->mName, "child");
        EXPECT_EQ(node->mVelocity, osg::Vec3f(0.5f, 0.5f, 0.5f));
        EXPECT_EQ(node->mBounds.mType, Nif::BoundingVolume::BOX_BV);
        EXPECT_EQ(node->mBounds.mBox.mCenter, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(node->mBounds.mBox.mExtents, osg::Vec3f(4, 5, 6));
    }

    TEST(NifNodeTest, readsEveryKindOfBoundingVolume)
    {
        // The volumes of Morrowind: the lozenge has no extents and the half space has no origin before 4.2.1.0.
        struct Volume
        {
            std::uint32_t mType;
            Bytes mBytes;
        };
        std::vector<Volume> volumes;
        volumes.push_back({ Nif::BoundingVolume::BASE_BV, Bytes() });
        volumes.push_back({ Nif::BoundingVolume::SPHERE_BV, Bytes().vec3(1, 2, 3).f32(4) });
        volumes.push_back({ Nif::BoundingVolume::BOX_BV, Bytes().vec3(1, 2, 3).identity3().vec3(4, 5, 6) });
        volumes.push_back({ Nif::BoundingVolume::CAPSULE_BV, Bytes().vec3(1, 2, 3).vec3(0, 0, 1).f32(5).f32(6) });
        volumes.push_back(
            { Nif::BoundingVolume::LOZENGE_BV, Bytes().f32(3).vec3(1, 2, 3).vec3(1, 0, 0).vec3(0, 1, 0) });
        volumes.push_back(
            { Nif::BoundingVolume::UNION_BV, Bytes().u32(1).u32(Nif::BoundingVolume::SPHERE_BV).vec3(0, 0, 0).f32(2) });
        volumes.push_back({ Nif::BoundingVolume::HALFSPACE_BV, Bytes().vec4(0, 0, 1, -2) });

        for (const Volume& volume : volumes)
        {
            Builder file(morrowind);
            Bytes node;
            file.named(node, "root");
            node.u16(0).vec3(0, 0, 0).identity3().f32(1).vec3(0, 0, 0).refs({});
            node.boolean32(true).u32(volume.mType).raw(volume.mBytes.str());
            node.refs({}).refs({});
            file.add("NiNode", node);
            file.addRoot(0);

            const Parsed parsed(file.build());

            const Nif::BoundingVolume& bounds = parsed.as<Nif::NiNode>(0)->mBounds;
            EXPECT_EQ(bounds.mType, volume.mType);
            switch (volume.mType)
            {
                case Nif::BoundingVolume::SPHERE_BV:
                    EXPECT_EQ(bounds.mSphere.radius(), 4.f);
                    break;
                case Nif::BoundingVolume::CAPSULE_BV:
                    EXPECT_EQ(bounds.mCapsule.mExtent, 5.f);
                    EXPECT_EQ(bounds.mCapsule.mRadius, 6.f);
                    break;
                case Nif::BoundingVolume::LOZENGE_BV:
                    EXPECT_EQ(bounds.mLozenge.mRadius, 3.f);
                    EXPECT_EQ(bounds.mLozenge.mAxis1, osg::Vec3f(0, 1, 0));
                    break;
                case Nif::BoundingVolume::UNION_BV:
                    ASSERT_EQ(bounds.mChildren.size(), 1u);
                    EXPECT_EQ(bounds.mChildren[0].mSphere.radius(), 2.f);
                    break;
                case Nif::BoundingVolume::HALFSPACE_BV:
                    EXPECT_EQ(bounds.mHalfSpace.mPlane.getNormal(), osg::Vec3f(0, 0, 1));
                    break;
                default:
                    break;
            }
        }
    }

    TEST(NifNodeTest, refusesABoundingVolumeOfAKindItDoesNotKnow)
    {
        Builder file(morrowind);
        Bytes bytes;
        file.named(bytes, "root");
        bytes.u16(0).vec3(0, 0, 0).identity3().f32(1).vec3(0, 0, 0).refs({}).boolean32(true).u32(77);
        bytes.refs({}).refs({});
        file.add("NiNode", bytes);
        file.addRoot(0);

        EXPECT_THROW(Parsed(file.build()), Nif::Exception);
    }

    TEST(NifNodeTest, readsTheShapeOfMorrowind)
    {
        Builder file(morrowind);
        Bytes bytes;
        file.place(bytes, { .mName = "shape" });
        bytes.ref(1).ref(-1);
        file.add("NiTriShape", bytes);
        file.add("NiTriShapeData", triShapeData(file, 3, 1, { 0, 1, 2 }, true, false));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* data = parsed.as<Nif::NiTriShapeData>(1);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(data->mNumVertices, 3u);
        ASSERT_EQ(data->mUVList.size(), 1u);
        EXPECT_THAT(data->mTriangles, ElementsAre(0, 1, 2));
        EXPECT_TRUE(data->mColors.empty());
    }

    TEST(NifNodeTest, theCollisionShapeOfAShapeIsAMeshOfItsTriangles)
    {
        Builder file(fallout3);
        file.add("NiTriShape", geometry(file, { .mName = "Mesh" }, 1));
        file.add("NiTriShapeData", triShapeData(file, 4, 0, { 0, 1, 2, 1, 2, 3 }, false, false));
        file.add("NiTriShape", geometry(file, { .mName = "Empty" }, 3));
        file.add("NiTriShapeData", triShapeData(file, 0, 0, {}, false, false));
        file.add("NiTriShape", geometry(file, { .mName = "Without data" }, -1));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const std::unique_ptr<btCollisionShape> shape = parsed.as<Nif::NiTriShape>(0)->getCollisionShape();
        EXPECT_NE(shape, nullptr);
        EXPECT_EQ(parsed.as<Nif::NiTriShape>(2)->getCollisionShape(), nullptr);
        EXPECT_EQ(parsed.as<Nif::NiTriShape>(4)->getCollisionShape(), nullptr);
    }

    TEST(NifNodeTest, aShapeKeepsTheDataOfItsKindOnly)
    {
        Builder file(fallout3);
        // The data of a strips shape is not what a triangles shape asks for, so the shape forgets it.
        file.add("NiTriShape", geometry(file, { .mName = "Wrong" }, 1));
        Bytes strips = geometryData(file, 3, 0, false, false);
        strips.u16(1).u16(1).u16(3).boolean(true).u16(0).u16(1).u16(2);
        file.add("NiTriStripsData", strips);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* shape = parsed.as<Nif::NiTriShape>(0);
        ASSERT_NE(shape, nullptr);
        EXPECT_TRUE(shape->mData.empty());
    }

    TEST(NifNodeTest, readsTheStripsOfAShape)
    {
        Builder file(fallout3);
        file.add("NiTriStrips", geometry(file, { .mName = "Strips" }, 1));
        Bytes data = geometryData(file, 5, 0, false, false);
        data.u16(3).u16(2);
        data.u16(4).u16(3); // the lengths of the strips
        data.boolean(true);
        data.u16(0).u16(1).u16(2).u16(3);
        data.u16(2).u16(3).u16(4);
        file.add("NiTriStripsData", data);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* strips = parsed.as<Nif::NiTriStripsData>(1);
        ASSERT_NE(strips, nullptr);
        ASSERT_EQ(strips->mStrips.size(), 2u);
        EXPECT_THAT(strips->mStrips[0], ElementsAre(0, 1, 2, 3));
        EXPECT_THAT(strips->mStrips[1], ElementsAre(2, 3, 4));
        const auto* shape = parsed.as<Nif::NiTriStrips>(0);
        ASSERT_NE(shape, nullptr);
        EXPECT_FALSE(shape->mData.empty());
        // the strips make the triangles that are not degenerate
        EXPECT_NE(shape->getCollisionShape(), nullptr);
    }

    TEST(NifNodeTest, readsTheLinesOfAShapeFromTheirConnectivityFlags)
    {
        Builder file(fallout3);
        file.add("NiLines", geometry(file, { .mName = "Lines" }, 1));
        Bytes data = geometryData(file, 4, 0, false, false);
        data.u8(1).u8(0).u8(1).u8(1);
        file.add("NiLinesData", data);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* lines = parsed.as<Nif::NiLinesData>(1);
        ASSERT_NE(lines, nullptr);
        // the first vertex goes to the second, the third to the fourth, and the last closes the loop to the first
        EXPECT_THAT(lines->mLines, ElementsAre(0, 1, 2, 3, 3, 0));
        EXPECT_EQ(parsed.as<Nif::NiLines>(0)->getCollisionShape(), nullptr);
    }

    TEST(NifNodeTest, aLineWithoutASecondVertexHasNoLines)
    {
        Builder file(fallout3);
        Bytes data = geometryData(file, 1, 0, false, false);
        data.u8(1);
        file.add("NiLinesData", data);
        file.addRoot(-1);

        const Parsed parsed(file.build());

        EXPECT_TRUE(parsed.as<Nif::NiLinesData>(0)->mLines.empty());
        EXPECT_EQ(parsed.roots().size(), 1u);
        EXPECT_EQ(parsed.roots()[0], nullptr);
    }

    TEST(NifNodeTest, readsTheSpecialShapes)
    {
        Builder file(fallout3);
        Bytes segmented = geometry(file, { .mName = "Segmented" }, -1);
        segmented.u32(2).u8(1).u32(0).u32(10).u8(2).u32(10).u32(20);
        file.add("BSSegmentedTriShape", segmented);
        Bytes lod = geometry(file, { .mName = "LOD" }, -1);
        lod.u32(10).u32(20).u32(30);
        file.add("BSLODTriShape", lod);
        file.add("NiParticles", geometry(file, { .mName = "Particles" }, -1));
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* shape = parsed.as<Nif::BSSegmentedTriShape>(0);
        ASSERT_NE(shape, nullptr);
        ASSERT_EQ(shape->mSegments.size(), 2u);
        EXPECT_EQ(shape->mSegments[0].mFlags, 1);
        EXPECT_EQ(shape->mSegments[0].mNumTriangles, 10u);
        EXPECT_EQ(shape->mSegments[1].mStartIndex, 10u);
        EXPECT_EQ(shape->mSegments[1].mNumTriangles, 20u);
        EXPECT_THAT(parsed.as<Nif::BSLODTriShape>(1)->mLOD, ElementsAre(10u, 20u, 30u));
        EXPECT_EQ(parsed.as<Nif::NiParticles>(2)->getCollisionShape(), nullptr);
    }

    TEST(NifNodeTest, readsTheNodesThatChooseWhichChildToShow)
    {
        Builder file(fallout3);
        Bytes lodNode = file.node({ .mName = "LOD" });
        lodNode.u16(1).u32(1).i32(-1); // the flags, the first child shown, the LOD data
        file.add("NiLODNode", lodNode);
        Bytes switchNode = file.node({ .mName = "Switch" });
        switchNode.u16(2).u32(3);
        file.add("NiSwitchNode", switchNode);
        Bytes animation = file.node({ .mName = "Animation" });
        animation.u16(0).u32(0).f32(2.5f);
        file.add("NiFltAnimationNode", animation);
        Bytes billboard = file.node({ .mName = "Billboard" });
        billboard.u16(0x0A);
        file.add("NiBillboardNode", billboard);
        Bytes sorting = file.node({ .mName = "Sort" });
        sorting.u32(2);
        file.add("NiSortAdjustNode", sorting);
        file.addRoot(0);

        const Parsed parsed(file.build());

        EXPECT_EQ(parsed.as<Nif::NiLODNode>(0)->mSwitchFlags, 1);
        EXPECT_EQ(parsed.as<Nif::NiLODNode>(0)->mInitialIndex, 1u);
        EXPECT_EQ(parsed.as<Nif::NiSwitchNode>(1)->mSwitchFlags, 2);
        EXPECT_EQ(parsed.as<Nif::NiSwitchNode>(1)->mInitialIndex, 3u);
        EXPECT_EQ(parsed.as<Nif::NiFltAnimationNode>(2)->mDuration, 2.5f);
        EXPECT_EQ(parsed.as<Nif::NiBillboardNode>(3)->mMode, Nif::NiBillboardNode::BillboardMode::RigidFaceCamera);
        EXPECT_EQ(parsed.as<Nif::NiSortAdjustNode>(4)->mMode, Nif::NiSortAdjustNode::SortingMode::Subsort);
    }

    TEST(NifNodeTest, readsTheLevelsOfDetailOfAnOldLODNode)
    {
        Builder file(morrowind);
        Bytes bytes;
        file.place(bytes, { .mName = "LOD" });
        bytes.refs({}).refs({});
        bytes.u32(0); // the index of the child shown
        bytes.vec3(1, 2, 3).u32(2).f32(0).f32(100).f32(100).f32(500);
        file.add("NiLODNode", bytes);
        Bytes billboard;
        file.place(billboard, { .mName = "Billboard", .mFlags = 0x40 });
        billboard.refs({}).refs({});
        file.add("NiBillboardNode", billboard);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* node = parsed.as<Nif::NiLODNode>(0);
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->mLODCenter, osg::Vec3f(1, 2, 3));
        ASSERT_EQ(node->mLODLevels.size(), 2u);
        EXPECT_EQ(node->mLODLevels[1].mMinRange, 100.f);
        EXPECT_EQ(node->mLODLevels[1].mMaxRange, 500.f);
        // before the mode was written, the mode was in the flags
        EXPECT_EQ(parsed.as<Nif::NiBillboardNode>(1)->mMode, Nif::NiBillboardNode::BillboardMode::RigidFaceCamera);
    }

    TEST(NifNodeTest, readsACameraWithItsFrustumAndItsViewport)
    {
        Builder file(fallout3);
        Bytes camera;
        file.place(camera, { .mName = "Camera" });
        camera.u16(1).f32(-1).f32(1).f32(0.75f).f32(-0.75f).f32(1).f32(1000).boolean(true);
        camera.f32(0).f32(1).f32(1).f32(0).f32(0.5f).ref(-1).u32(0).u32(0);
        file.add("NiCamera", camera);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* result = parsed.as<Nif::NiCamera>(0);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->mCameraFlags, 1);
        EXPECT_EQ(result->mLeft, -1.f);
        EXPECT_EQ(result->mTop, 0.75f);
        EXPECT_EQ(result->mFarDist, 1000.f);
        EXPECT_TRUE(result->mOrthographic);
        EXPECT_EQ(result->mVRight, 1.f);
        EXPECT_EQ(result->mLODAdjust, 0.5f);
        EXPECT_TRUE(result->mScene.empty());
    }

    TEST(NifNodeTest, readsTheNodesOfTheGamesThatKeepSomethingOfTheirOwn)
    {
        Builder file(fallout3);
        Bytes value = file.node({ .mName = "Value" });
        value.u32(7).u8(1);
        file.add("BSValueNode", value);
        Bytes ordered = file.node({ .mName = "Ordered" });
        ordered.vec4(1, 2, 3, 4).u8(1);
        file.add("BSOrderedNode", ordered);
        Bytes range = file.node({ .mName = "Range" });
        range.u8(1).u8(2).u8(3);
        file.add("BSBlastNode", range);
        Bytes tree = file.node({ .mName = "Tree" });
        tree.refs({}).refs({});
        file.add("BSTreeNode", tree);
        file.add("BSFadeNode", file.node({ .mName = "Fade" }));
        file.addRoot(0);

        const Parsed parsed(file.build());

        EXPECT_EQ(parsed.as<Nif::BSValueNode>(0)->mValue, 7u);
        EXPECT_EQ(parsed.as<Nif::BSOrderedNode>(1)->mAlphaSortBound, osg::Vec4f(1, 2, 3, 4));
        EXPECT_EQ(parsed.as<Nif::BSRangeNode>(2)->mMax, 2);
        EXPECT_NE(parsed.as<Nif::BSTreeNode>(3), nullptr);
        EXPECT_NE(parsed.as<Nif::NiNode>(4), nullptr);
    }

    TEST(NifNodeTest, refusesARecordTypeThatItDoesNotKnow)
    {
        Builder file(fallout3);
        file.add("NiNotARecord", Bytes().zeros(8));

        EXPECT_THROW(Parsed(file.build()), Nif::Exception);
    }

    TEST(NifNodeTest, refusesARecordWithoutAType)
    {
        Builder file(fallout3);
        file.add("", Bytes().zeros(8));

        EXPECT_THROW(Parsed(file.build()), Nif::Exception);
    }

    TEST(NifNodeTest, refusesAReferenceToARecordOfAnotherKind)
    {
        Builder file(fallout3);
        // the children of a node are objects of the scene, and the data of a shape is not
        file.add("NiNode", file.node({ .mName = "root" }, { 1 }));
        file.add("NiTriShapeData", triShapeData(file, 1, 0, {}, false, false));

        EXPECT_THROW(Parsed(file.build()), std::runtime_error);
    }

    TEST(NifNodeTest, aFileThatIsNotANifIsRefused)
    {
        EXPECT_THROW(Parsed("this is not a nif file\n"), Nif::Exception);
    }

    TEST(NifNodeTest, readsTheShapeOfFallout4WithoutItsVertices)
    {
        Builder file(fallout4);
        Bytes shape;
        file.place(shape, { .mName = "Shape" });
        shape.vec3(0, 0, 0).f32(5); // the bounding sphere
        shape.ref(-1).ref(-1).ref(-1); // the skin, the shader and the alpha
        shape.u32(0).u32(0); // the vertex description, as one 64-bit number
        shape.u32(2).u16(0).u32(0); // two triangles, no vertices, no data
        file.add("BSTriShape", shape);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* result = parsed.as<Nif::BSTriShape>(0);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->mBoundingSphere.radius(), 5.f);
        EXPECT_EQ(result->mNumVertices, 0u);
        EXPECT_EQ(result->mDataSize, 0u);
        EXPECT_TRUE(result->mTriangles.empty());
    }

    TEST(NifNodeTest, readsTheVerticesOfAShapeOfFallout4)
    {
        Builder file(fallout4);
        Bytes shape;
        file.place(shape, { .mName = "Shape" });
        shape.vec3(0, 0, 0).f32(5);
        shape.ref(-1).ref(-1).ref(-1);
        // positions with full precision, UVs, normals and colours
        const std::uint64_t desc = static_cast<std::uint64_t>(0x042B) << 0x2C;
        shape.u32(static_cast<std::uint32_t>(desc & 0xFFFFFFFF)).u32(static_cast<std::uint32_t>(desc >> 32));
        shape.u32(1).u16(2).u32(100);
        for (int vertex = 0; vertex < 2; ++vertex)
        {
            shape.vec3(static_cast<float>(vertex), 2, 3).f32(0); // the position and the bitangent X
            shape.u16(0x3C00).u16(0x4000); // UV as halves
            shape.u8(128).u8(128).u8(255).u8(128); // the normal
            shape.u8(10).u8(20).u8(30).u8(40); // the colour
        }
        shape.u16(0).u16(1).u16(0);
        file.add("BSTriShape", shape);
        file.addRoot(0);

        const Parsed parsed(file.build());

        const auto* result = parsed.as<Nif::BSTriShape>(0);
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(result->mVertData.size(), 2u);
        EXPECT_EQ(result->mVertData[1].mVertex.x(), 1.f);
        EXPECT_EQ(result->mVertData[1].mVertex.y(), 2.f);
        EXPECT_THAT(result->mTriangles, ElementsAre(0, 1, 0));
    }
}
