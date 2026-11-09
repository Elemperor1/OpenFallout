#include <components/nif/extra.hpp>

#include "niftestfile.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <osg/Vec3f>

namespace Nif
{
    namespace
    {
        using namespace testing;
        using namespace NifTest;

        // The start of every extra data record of the files from 10.0.1.0 on: its name
        Bytes extra(Builder& file, std::string_view name)
        {
            Bytes bytes;
            file.string(bytes, name);
            return bytes;
        }

        TEST(NifExtraDataTest, readsTheSimpleExtraDataOfFallout)
        {
            Builder file(fallout3);
            std::int32_t flags = file.add("BSXFlags", extra(file, "BSX").u32(0x23));
            std::int32_t attach = file.add("NiStringExtraData", file.string(extra(file, "Prn"), "Bip01 R Hand"));
            std::int32_t boolean = file.add("NiBooleanExtraData", extra(file, "Flag").boolean(true));
            std::int32_t number = file.add("NiFloatExtraData", extra(file, "Number").f32(2.5f));
            std::int32_t vector = file.add("NiVectorExtraData", extra(file, "Vector").vec4(1, 2, 3, 4));
            std::int32_t binary = file.add("NiBinaryExtraData", extra(file, "Binary").u32(3).raw("abc"));
            std::int32_t floats = file.add("NiFloatsExtraData", extra(file, "Floats").u32(2).f32(0.5f).f32(1.5f));
            std::int32_t integers = file.add("NiIntegersExtraData", extra(file, "Integers").u32(2).u32(7).u32(9));
            std::int32_t strings
                = file.add("NiStringsExtraData", extra(file, "Strings").u32(2).sized("first").sized("second"));
            std::int32_t generic = file.add("NiExtraData", extra(file, "Generic"));
            std::int32_t base = file.add("BSExtraData", Bytes());
            std::int32_t weights = file.add("NiVertWeightsExtraData", extra(file, "Weights").u16(2).f32(1).f32(2));
            std::int32_t distant = file.add("BSDistantObjectExtraData", extra(file, "Distant").u32(5));
            std::int32_t large = file.add("BSDistantObjectLargeRefExtraData", extra(file, "Large").boolean(true));
            std::int32_t eyes = file.add("BSEyeCenterExtraData", extra(file, "Eyes").u32(2).f32(3).f32(4));
            std::int32_t positions = file.add("BSPositionData", extra(file, "Positions").u32(2).u16(0x3C00).u16(0x4000));

            const Parsed parsed(file.build());

            ASSERT_EQ(parsed.size(), 16u);
            const auto* bsx = parsed.as<NiIntegerExtraData>(flags);
            ASSERT_NE(bsx, nullptr);
            EXPECT_EQ(bsx->mName, "BSX");
            EXPECT_EQ(bsx->mData, 0x23u);
            EXPECT_EQ(bsx->mRecordType, RC_BSXFlags);
            ASSERT_NE(parsed.as<NiStringExtraData>(attach), nullptr);
            EXPECT_EQ(parsed.as<NiStringExtraData>(attach)->mName, "Prn");
            EXPECT_EQ(parsed.as<NiStringExtraData>(attach)->mData, "Bip01 R Hand");
            EXPECT_TRUE(parsed.as<NiBooleanExtraData>(boolean)->mData);
            EXPECT_EQ(parsed.as<NiFloatExtraData>(number)->mData, 2.5f);
            EXPECT_EQ(parsed.as<NiVectorExtraData>(vector)->mData, osg::Vec4f(1, 2, 3, 4));
            EXPECT_THAT(parsed.as<NiBinaryExtraData>(binary)->mData, ElementsAre('a', 'b', 'c'));
            EXPECT_THAT(parsed.as<NiFloatsExtraData>(floats)->mData, ElementsAre(0.5f, 1.5f));
            EXPECT_THAT(parsed.as<NiIntegersExtraData>(integers)->mData, ElementsAre(7u, 9u));
            EXPECT_THAT(parsed.as<NiStringsExtraData>(strings)->mData, ElementsAre("first", "second"));
            EXPECT_EQ(parsed.as<NiExtraData>(generic)->mName, "Generic");
            EXPECT_TRUE(parsed.as<NiExtraData>(generic)->mData.empty());
            EXPECT_NE(parsed.as<BSExtraData>(base), nullptr);
            EXPECT_EQ(parsed.as<NiVertWeightsExtraData>(weights)->mName, "Weights");
            EXPECT_EQ(parsed.as<BSDistantObjectExtraData>(distant)->mData, 5u);
            EXPECT_TRUE(parsed.as<BSDistantObjectLargeRefExtraData>(large)->mData);
            EXPECT_THAT(parsed.as<BSEyeCenterExtraData>(eyes)->mData, ElementsAre(3.f, 4.f));
            ASSERT_EQ(parsed.as<BSPositionData>(positions)->mData.size(), 2u);
        }

        TEST(NifExtraDataTest, readsTheTextKeysOfAnAnimation)
        {
            Builder file(fallout3);
            Bytes keys = extra(file, "textkeys");
            keys.u32(2);
            keys.f32(0.f);
            file.string(keys, "start");
            keys.f32(1.5f);
            file.string(keys, "Sound: Footstep");
            file.add("NiTextKeyExtraData", keys);

            const Parsed parsed(file.build());

            const auto* data = parsed.as<NiTextKeyExtraData>(0);
            ASSERT_NE(data, nullptr);
            ASSERT_EQ(data->mList.size(), 2u);
            EXPECT_EQ(data->mList[0].mTime, 0.f);
            EXPECT_EQ(data->mList[0].mText, "start");
            EXPECT_EQ(data->mList[1].mTime, 1.5f);
            EXPECT_EQ(data->mList[1].mText, "Sound: Footstep");
        }

        TEST(NifExtraDataTest, readsTheBoundsAndTheMarkersOfFallout)
        {
            Builder file(fallout3);
            file.add("BSBound", extra(file, "Bounds").vec3(1, 2, 3).vec3(4, 5, 6));
            // a position with the offset, the orientation, the reference to the position and a byte that is not used
            file.add("BSFurnitureMarker",
                extra(file, "FRN").u32(2).vec3(1, 2, 3).u16(90).u8(4).u8(0).vec3(4, 5, 6).u16(180).u8(2).u8(0));
            // each angle comes in thousandths of a radian
            file.add("BSInvMarker", extra(file, "INV").u16(1000).u16(0).u16(500).f32(1.25f));

            const Parsed parsed(file.build());

            const auto* bounds = parsed.as<BSBound>(0);
            ASSERT_NE(bounds, nullptr);
            EXPECT_EQ(bounds->mCenter, osg::Vec3f(1, 2, 3));
            EXPECT_EQ(bounds->mExtents, osg::Vec3f(4, 5, 6));
            const auto* furniture = parsed.as<BSFurnitureMarker>(1);
            ASSERT_NE(furniture, nullptr);
            ASSERT_EQ(furniture->mLegacyMarkers.size(), 2u);
            EXPECT_TRUE(furniture->mMarkers.empty());
            EXPECT_EQ(furniture->mLegacyMarkers[0].mOffset, osg::Vec3f(1, 2, 3));
            EXPECT_EQ(furniture->mLegacyMarkers[0].mOrientation, 90);
            EXPECT_EQ(furniture->mLegacyMarkers[0].mPositionRef, 4);
            EXPECT_EQ(furniture->mLegacyMarkers[1].mOrientation, 180);
            const auto* inventory = parsed.as<BSInvMarker>(2);
            ASSERT_NE(inventory, nullptr);
            EXPECT_EQ(inventory->mScale, 1.25f);
            EXPECT_NEAR(inventory->mRotation.x(), osg::Quat(1.0, osg::X_AXIS, 0.0, osg::Y_AXIS, 0.5, osg::Z_AXIS).x(), 1e-6);
        }

        TEST(NifExtraDataTest, readsTheMarkersOfFurnitureAsSkyrimWritesThem)
        {
            Builder file(skyrim);
            file.add("BSFurnitureMarkerNode", extra(file, "FRN").u32(1).vec3(1, 2, 3).f32(1.5f).u16(2).u16(3));

            const Parsed parsed(file.build());

            const auto* furniture = parsed.as<BSFurnitureMarker>(0);
            ASSERT_NE(furniture, nullptr);
            EXPECT_TRUE(furniture->mLegacyMarkers.empty());
            ASSERT_EQ(furniture->mMarkers.size(), 1u);
            EXPECT_EQ(furniture->mMarkers[0].mOffset, osg::Vec3f(1, 2, 3));
            EXPECT_EQ(furniture->mMarkers[0].mHeading, 1.5f);
            EXPECT_EQ(furniture->mMarkers[0].mType, 2);
            EXPECT_EQ(furniture->mMarkers[0].mEntryPoint, 3);
        }

        TEST(NifExtraDataTest, readsTheOtherExtraDataOfTheBethesdaGames)
        {
            Builder file(fallout3);
            file.add("BSBehaviorGraphExtraData", file.string(extra(file, "BGED"), "graph.hkx").boolean(true));
            file.add("BSBoneLODExtraData",
                file.string(file.string(extra(file, "BSBoneLOD").u32(2).u32(100), "Bone1").u32(200), "Bone2"));
            file.add("BSDecalPlacementVectorExtraData",
                extra(file, "Decal").f32(0.5f).u16(1).u16(2).vec3(1, 1, 1).vec3(2, 2, 2).vec3(0, 0, 1).vec3(0, 1, 0));
            file.add("BSClothExtraData", Bytes().u32(4).raw("data"));
            file.add("BSCollisionQueryProxyExtraData", Bytes().u32(2).raw("ab"));

            const Parsed parsed(file.build());

            ASSERT_EQ(parsed.size(), 5u);
            EXPECT_EQ(parsed.as<BSBehaviorGraphExtraData>(0)->mFile, "graph.hkx");
            EXPECT_TRUE(parsed.as<BSBehaviorGraphExtraData>(0)->mControlsBaseSkeleton);
            const auto* lod = parsed.as<BSBoneLODExtraData>(1);
            ASSERT_NE(lod, nullptr);
            ASSERT_EQ(lod->mData.size(), 2u);
            EXPECT_EQ(lod->mData[0].mDistance, 100u);
            EXPECT_EQ(lod->mData[0].mBone, "Bone1");
            EXPECT_EQ(lod->mData[1].mDistance, 200u);
            EXPECT_EQ(lod->mData[1].mBone, "Bone2");
            const auto* decal = parsed.as<BSDecalPlacementVectorExtraData>(2);
            ASSERT_NE(decal, nullptr);
            EXPECT_EQ(decal->mData, 0.5f);
            ASSERT_EQ(decal->mBlocks.size(), 1u);
            ASSERT_EQ(decal->mBlocks[0].mPoints.size(), 2u);
            EXPECT_EQ(decal->mBlocks[0].mPoints[1], osg::Vec3f(2, 2, 2));
            EXPECT_EQ(decal->mBlocks[0].mNormals[1], osg::Vec3f(0, 1, 0));
            EXPECT_THAT(parsed.as<BSClothExtraData>(3)->mData, ElementsAre('d', 'a', 't', 'a'));
            EXPECT_THAT(parsed.as<BSCollisionQueryProxyExtraData>(4)->mData, ElementsAre('a', 'b'));
        }

        TEST(NifExtraDataTest, readsTheConnectPointsOfFallout4)
        {
            Builder file(fallout4);
            Bytes parents = extra(file, "CPA");
            parents.u32(1);
            parents.sized("parent").sized("child").quat(1, 0, 0, 0).vec3(1, 2, 3).f32(2.f);
            file.add("BSConnectPoint::Parents", parents);
            Bytes children = extra(file, "CPC");
            children.boolean(true).u32(2).sized("first").sized("second");
            file.add("BSConnectPoint::Children", children);

            const Parsed parsed(file.build());

            const auto* parentPoints = parsed.as<BSConnectPoint::Parents>(0);
            ASSERT_NE(parentPoints, nullptr);
            ASSERT_EQ(parentPoints->mPoints.size(), 1u);
            EXPECT_EQ(parentPoints->mPoints[0].mParent, "parent");
            EXPECT_EQ(parentPoints->mPoints[0].mName, "child");
            EXPECT_EQ(parentPoints->mPoints[0].mTransform.mTranslation, osg::Vec3f(1, 2, 3));
            EXPECT_EQ(parentPoints->mPoints[0].mTransform.mScale, 2.f);
            const auto* childPoints = parsed.as<BSConnectPoint::Children>(1);
            ASSERT_NE(childPoints, nullptr);
            EXPECT_TRUE(childPoints->mSkinned);
            EXPECT_THAT(childPoints->mPointNames, ElementsAre("first", "second"));
        }

        TEST(NifExtraDataTest, readsTheCombinedGeometryOfTheDistantObjectsOfFallout4)
        {
            Builder file(fallout4);
            Bytes data = extra(file, "Combined");
            data.u32(0).u32(0); // the vertex description of the whole
            data.u32(100).u32(50).u32(1).u32(2); // vertices, triangles, flags
            data.u32(1).u32(0xABCD).u32(16); // one object with its hash and offset
            // the data of that object: counts and offsets, then one combined entry and the vertex description
            for (const std::uint32_t value : { 4u, 3u, 10u, 0u, 5u, 40u, 2u, 60u })
                data.u32(value);
            data.u32(1).f32(0.25f).transform(1, 2, 3, 2).vec3(1, 1, 1).f32(5.f);
            data.u32(0).u32(0);
            file.add("BSPackedCombinedSharedGeomDataExtra", data);

            const Parsed parsed(file.build());

            const auto* combined = parsed.as<BSPackedCombinedSharedGeomDataExtra>(0);
            ASSERT_NE(combined, nullptr);
            EXPECT_EQ(combined->mNumVertices, 100u);
            EXPECT_EQ(combined->mNumTriangles, 50u);
            EXPECT_EQ(combined->mFlags2, 2u);
            ASSERT_EQ(combined->mObjects.size(), 1u);
            EXPECT_EQ(combined->mObjects[0].mFileHash, 0xABCDu);
            EXPECT_EQ(combined->mObjects[0].mDataOffset, 16u);
            ASSERT_EQ(combined->mObjectData.size(), 1u);
        }

        TEST(NifExtraDataTest, readsTheExtraDataOfMorrowindWithTheNextRecordAndTheSizeBeforeTheData)
        {
            Builder file(morrowind);
            // 4.0.0.2: the next extra data, the size of the record, then the data
            file.add("NiStringExtraData", Bytes().ref(1).u32(8).sized("Prn"));
            file.add("NiTextKeyExtraData", Bytes().ref(-1).u32(0).u32(1).f32(0.5f).sized("hit"));
            file.add("NiExtraData", Bytes().ref(-1).u32(3).raw("xyz"));

            const Parsed parsed(file.build());

            ASSERT_EQ(parsed.size(), 3u);
            const auto* text = parsed.as<NiStringExtraData>(0);
            ASSERT_NE(text, nullptr);
            EXPECT_EQ(text->mData, "Prn");
            EXPECT_EQ(text->mRecordSize, 8u);
            EXPECT_EQ(text->mNext.getPtr(), parsed.record(1));
            const auto* keys = parsed.as<NiTextKeyExtraData>(1);
            ASSERT_EQ(keys->mList.size(), 1u);
            EXPECT_EQ(keys->mList[0].mText, "hit");
            EXPECT_THAT(parsed.as<NiExtraData>(2)->mData, ElementsAre('x', 'y', 'z'));
        }

        TEST(NifExtraDataTest, doesNotReadARecordOfATypeItDoesNotKnow)
        {
            Builder file(fallout3);
            file.add("NiNotARecord", Bytes());

            EXPECT_THROW(Parsed(file.build()), std::runtime_error);
        }
    }
}
