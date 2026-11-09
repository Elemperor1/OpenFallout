#include "geometry.hpp"
#include "niftestfile.hpp"

#include <components/nif/controller.hpp>
#include <components/nif/data.hpp>
#include <components/nif/exception.hpp>
#include <components/nif/node.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <vector>

// The data records of a file: the keys of the animations, the skin of a mesh, the pixels of a texture and the like.
namespace
{
    using namespace NifTest;
    using namespace testing;

    constexpr std::uint32_t linear = 1;
    constexpr std::uint32_t quadratic = 2;
    constexpr std::uint32_t tcb = 3;
    constexpr std::uint32_t xyz = 4;
    constexpr std::uint32_t constant = 5;

    TEST(NifDataTest, readsTheKeysOfTheDataOfAnimations)
    {
        Builder file(oblivion);
        file.add("NiFloatData", Bytes().floatKeys({ { 0.f, 1.f }, { 1.f, 3.f } }));
        file.add("NiFloatData", Bytes().u32(2).u32(constant).f32(0).f32(5).f32(1).f32(6));
        file.add("NiFloatData", Bytes().u32(1).u32(quadratic).f32(0).f32(1).f32(2).f32(3));
        // the tangents of a TCB key come from its neighbours
        file.add("NiFloatData",
            Bytes()
                .u32(3)
                .u32(tcb)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(1)
                .f32(1)
                .f32(0)
                .f32(0)
                .f32(0)
                .f32(2)
                .f32(3)
                .f32(0)
                .f32(0)
                .f32(0));
        file.add("NiFloatData", Bytes().u32(0));
        file.add("NiPosData", Bytes().vec3Keys({ { 0.f, { 1, 2, 3 } } }));
        file.add("NiPosData",
            Bytes().u32(2).u32(tcb).f32(0).vec3(0, 0, 0).f32(0).f32(0).f32(0).f32(1).vec3(2, 4, 6).f32(0).f32(0).f32(
                0));
        file.add("NiColorData", Bytes().u32(1).u32(linear).f32(0).vec4(1, 0.5f, 0.25f, 1));
        file.add("NiBoolData", Bytes().u32(2).u32(linear).f32(0).u8(1).f32(1).u8(0));
        file.add("NiBoolData", Bytes().u32(1).u32(tcb).f32(0).u8(1).f32(0).f32(0).f32(0));
        file.add("NiVisData", Bytes().u32(2).f32(0).u8(1).f32(0.5f).u8(0));
        file.add("NiUVData", Bytes().floatKeys({ { 0.f, 1.f } }).u32(0).floatKeys({ { 0.f, 2.f } }).u32(0));

        const Parsed parsed(file.build());

        const auto& linearKeys = parsed.as<Nif::NiFloatData>(0)->mKeyList->mKeys;
        ASSERT_EQ(linearKeys.size(), 2u);
        EXPECT_EQ(linearKeys[1].first, 1.f);
        EXPECT_EQ(linearKeys[1].second.mValue, 3.f);
        EXPECT_EQ(parsed.as<Nif::NiFloatData>(1)->mKeyList->mInterpolationType, constant);
        const auto& quadraticKeys = parsed.as<Nif::NiFloatData>(2)->mKeyList->mKeys;
        ASSERT_EQ(quadraticKeys.size(), 1u);
        EXPECT_EQ(quadraticKeys[0].second.mInTan, 2.f);
        EXPECT_EQ(quadraticKeys[0].second.mOutTan, 3.f);
        const auto& tcbKeys = parsed.as<Nif::NiFloatData>(3)->mKeyList->mKeys;
        ASSERT_EQ(tcbKeys.size(), 3u);
        EXPECT_EQ(tcbKeys[0].second.mOutTan, 1.f);
        EXPECT_EQ(tcbKeys[1].second.mInTan, 1.5f);
        EXPECT_EQ(tcbKeys[1].second.mOutTan, 1.5f);
        EXPECT_EQ(tcbKeys[2].second.mInTan, 2.f);
        EXPECT_TRUE(parsed.as<Nif::NiFloatData>(4)->mKeyList->mKeys.empty());
        EXPECT_EQ(parsed.as<Nif::NiPosData>(5)->mKeyList->mKeys[0].second.mValue, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(parsed.as<Nif::NiPosData>(6)->mKeyList->mKeys[0].second.mOutTan, osg::Vec3f(2, 4, 6));
        EXPECT_EQ(parsed.as<Nif::NiColorData>(7)->mKeyMap->mKeys[0].second.mValue, osg::Vec4f(1, 0.5f, 0.25f, 1));
        const auto& boolKeys = parsed.as<Nif::NiBoolData>(8)->mKeyList->mKeys;
        ASSERT_EQ(boolKeys.size(), 2u);
        EXPECT_TRUE(boolKeys[0].second.mValue);
        EXPECT_FALSE(boolKeys[1].second.mValue);
        EXPECT_EQ(parsed.as<Nif::NiBoolData>(9)->mKeyList->mKeys.size(), 1u);
        const auto& visibility = *parsed.as<Nif::NiVisData>(10)->mKeys;
        ASSERT_EQ(visibility.size(), 2u);
        EXPECT_TRUE(visibility[0].second);
        EXPECT_FALSE(visibility[1].second);
        const auto* uv = parsed.as<Nif::NiUVData>(11);
        EXPECT_EQ(uv->mKeyList[0]->mKeys[0].second.mValue, 1.f);
        EXPECT_TRUE(uv->mKeyList[1]->mKeys.empty());
        EXPECT_EQ(uv->mKeyList[2]->mKeys[0].second.mValue, 2.f);
    }

    TEST(NifDataTest, refusesAnInterpolationOfKeysThatItDoesNotKnow)
    {
        Builder file(oblivion);
        file.add("NiFloatData", Bytes().u32(1).u32(77).f32(0).f32(1));

        EXPECT_THROW(Parsed(file.build()), Nif::Exception);
    }

    TEST(NifDataTest, readsTheKeysOfTheRotationOfANode)
    {
        for (const Profile& profile : { oblivion, morrowind })
        {
            Builder file(profile);
            // 0: the rotations as quaternions, then the translations and the scales
            file.add("NiKeyframeData",
                Bytes()
                    .quatKeys({ { 0.f, { 1, 0, 0, 0 } }, { 1.f, { 0, 1, 0, 0 } } })
                    .vec3Keys({ { 0.f, { 1, 2, 3 } } })
                    .floatKeys({ { 0.f, 2.f } }));
            // 1: the rotation as the angles of three axes
            Bytes angles;
            angles.u32(1).u32(xyz);
            if (profile.mVersion <= 0x0A010000)
                angles.u32(1); // the order of the axes
            angles.floatKeys({ { 0.f, 0.5f } }).floatKeys({}).floatKeys({ { 1.f, 1.5f } });
            angles.u32(0).u32(0);
            file.add("NiTransformData", angles);

            const Parsed parsed(file.build());

            const auto* data = parsed.as<Nif::NiKeyframeData>(0);
            ASSERT_NE(data, nullptr);
            ASSERT_EQ(data->mRotations->mKeys.size(), 2u);
            EXPECT_EQ(data->mRotations->mKeys[1].second.mValue, osg::Quat(1, 0, 0, 0));
            EXPECT_EQ(data->mTranslations->mKeys[0].second.mValue, osg::Vec3f(1, 2, 3));
            EXPECT_EQ(data->mScales->mKeys[0].second.mValue, 2.f);
            EXPECT_EQ(data->mXRotations, nullptr);
            const auto* euler = parsed.as<Nif::NiKeyframeData>(1);
            ASSERT_NE(euler, nullptr);
            EXPECT_EQ(euler->mRotations->mInterpolationType, xyz);
            ASSERT_NE(euler->mXRotations, nullptr);
            EXPECT_EQ(euler->mXRotations->mKeys[0].second.mValue, 0.5f);
            EXPECT_TRUE(euler->mYRotations->mKeys.empty());
            EXPECT_EQ(euler->mZRotations->mKeys[0].second.mValue, 1.5f);
            if (profile.mVersion <= 0x0A010000)
                EXPECT_EQ(euler->mAxisOrder, Nif::NiKeyframeData::AxisOrder::Order_XZY);
        }
    }

    TEST(NifDataTest, readsTheMorphsOfAMesh)
    {
        {
            Builder file(oblivion);
            // two morphs of three vertices: the name of the morph, its keys and its offsets
            Bytes bytes;
            bytes.u32(2).u32(3).u8(1);
            for (const std::string_view name : { "Smile", "Frown" })
            {
                file.string(bytes, name);
                for (int i = 0; i < 3; ++i)
                    bytes.vec3(static_cast<float>(i), 0, 0);
            }
            file.add("NiMorphData", bytes);

            const Parsed parsed(file.build());

            const auto* data = parsed.as<Nif::NiMorphData>(0);
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->mRelativeTargets, 1u);
            ASSERT_EQ(data->mMorphs.size(), 2u);
            ASSERT_EQ(data->mMorphs[1].mVertices.size(), 3u);
            EXPECT_EQ(data->mMorphs[1].mVertices[2], osg::Vec3f(2, 0, 0));
            EXPECT_EQ(data->mMorphs[1].mKeyFrames->mFrameName, "Frown");
        }
        {
            // The oldest files have the keys of a morph with it, and the files of before 20.1.0.3 keep a weight of it.
            Builder file(Profile{ 0x0A01006A, 0, 0 });
            Bytes bytes;
            bytes.u32(1).u32(2).u8(0);
            bytes.sized("Morph").f32(0.75f);
            bytes.vec3(1, 1, 1).vec3(2, 2, 2);
            file.add("NiMorphData", bytes);

            const Parsed parsed(file.build());

            EXPECT_EQ(parsed.as<Nif::NiMorphData>(0)->mMorphs.at(0).mKeyFrames->mLegacyWeight, 0.75f);
        }
        {
            Builder file(morrowind);
            file.add(
                "NiMorphData", Bytes().u32(1).u32(1).u8(1).floatKeys({ { 0.f, 0.f }, { 1.f, 1.f } }).vec3(5, 6, 7));

            const Parsed parsed(file.build());

            const auto* data = parsed.as<Nif::NiMorphData>(0);
            ASSERT_EQ(data->mMorphs.size(), 1u);
            EXPECT_EQ(data->mMorphs[0].mKeyFrames->mKeys.size(), 2u);
            EXPECT_EQ(data->mMorphs[0].mVertices[0], osg::Vec3f(5, 6, 7));
        }
    }

    TEST(NifDataTest, readsThePixelsOfATextureWithAPalette)
    {
        Builder file(morrowind);
        file.add("NiPalette", Bytes().u8(0).u32(3).u32(0x00112233).u32(0x00445566).u32(0x00778899));
        file.add("NiPalette", Bytes().u8(1).u32(1).u32(0x80112233));
        Bytes pixels;
        // an RGBA format with the masks of the channels, 32 bits to a pixel and the bits that are compared
        pixels.u32(1).u32(0xFF).u32(0xFF00).u32(0xFF0000).u32(0xFF000000).u32(32).u32(0).u32(0);
        pixels.ref(0).u32(2).u32(4);
        pixels.u32(2).u32(2).u32(0).u32(1).u32(1).u32(16);
        pixels.u32(20);
        for (int i = 0; i < 20; ++i)
            pixels.u8(static_cast<std::uint8_t>(i));
        file.add("NiPixelData", pixels);

        const Parsed parsed(file.build());

        const auto* palette = parsed.as<Nif::NiPalette>(0);
        EXPECT_EQ(palette->mColors[1], 0xFF445566u);
        EXPECT_EQ(palette->mColors[3], 0u);
        EXPECT_EQ(parsed.as<Nif::NiPalette>(1)->mColors[0], 0x80112233u);
        const auto* data = parsed.as<Nif::NiPixelData>(2);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(data->mPixelFormat.mFormat, Nif::NiPixelFormat::Format::RGBA);
        EXPECT_EQ(data->mPixelFormat.mColorMasks[3], 0xFF000000u);
        EXPECT_EQ(data->mPixelFormat.mBitsPerPixel, 32u);
        EXPECT_EQ(data->mPalette.getPtr(), palette);
        ASSERT_EQ(data->mMipmaps.size(), 2u);
        EXPECT_EQ(data->mMipmaps[1].mOffset, 16u);
        EXPECT_EQ(data->mNumFaces, 1u);
        ASSERT_EQ(data->mData.size(), 20u);
        EXPECT_EQ(data->mData[19], 19u);
    }

    TEST(NifDataTest, readsThePixelsOfTheTexturesOfOblivionAndOfTheLaterVersions)
    {
        // The format of the pixels is a list of channels from 10.4.0.2 on, and the sRGB flag comes in 20.3.0.4.
        for (const Profile& profile : { oblivion, Profile{ 0x14030004, 0, 0 } })
        {
            Builder file(profile);
            Bytes pixels;
            pixels.u32(8).u8(8).u32(1).u32(2).u8(3).u32(1);
            if (profile.mVersion >= 0x14030004)
                pixels.boolean(true);
            for (std::uint32_t channel = 0; channel < 4; ++channel)
                pixels.u32(channel).u32(4).u8(8).boolean(channel == 3);
            pixels.ref(-1).u32(1).u32(1);
            pixels.u32(2).u32(2).u32(0);
            pixels.u32(4).u32(6); // four bytes in each of the six faces of a cube
            for (int i = 0; i < 24; ++i)
                pixels.u8(1);
            file.add("NiPixelData", pixels);

            const Parsed parsed(file.build());

            const auto* data = parsed.as<Nif::NiPixelData>(0);
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->mPixelFormat.mFormat, Nif::NiPixelFormat::Format::DXT5);
            EXPECT_EQ(data->mPixelFormat.mBitsPerPixel, 8u);
            EXPECT_EQ(data->mPixelFormat.mRendererHint, 1u);
            EXPECT_EQ(data->mPixelFormat.mFlags, 3u);
            EXPECT_EQ(data->mPixelFormat.mUseSrgb, profile.mVersion >= 0x14030004);
            EXPECT_EQ(data->mPixelFormat.mChannels[1].mType, Nif::NiPixelFormat::ChannelData::Type::Green);
            EXPECT_EQ(
                data->mPixelFormat.mChannels[1].mConvention, Nif::NiPixelFormat::ChannelData::Convention::Compressed);
            EXPECT_FALSE(data->mPixelFormat.mChannels[2].mSigned);
            EXPECT_TRUE(data->mPixelFormat.mChannels[3].mSigned);
            EXPECT_EQ(data->mNumFaces, 6u);
            EXPECT_EQ(data->mData.size(), 24u);
        }
    }

    TEST(NifDataTest, readsTheSkinOfAMeshAndTheBonesThatItUses)
    {
        Builder file(oblivion);
        // 0: the root with a bone, 1: the bone
        file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
        file.add("NiNode", file.node({ .mName = "Bone" }));
        // 2: two vertices of the mesh follow the one bone, with the weights
        Bytes skin;
        skin.transform(1, 2, 3, 1.5f).u32(1).boolean(true);
        skin.transform(4, 5, 6).vec3(0, 1, 0).f32(7).u16(2).u16(0).f32(0.25f).u16(1).f32(0.75f);
        file.add("NiSkinData", skin);
        file.add("NiSkinInstance", Bytes().ref(2).ref(-1).ref(0).refs({ 1 }));

        const Parsed parsed(file.build());

        const auto* data = parsed.as<Nif::NiSkinData>(2);
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(data->mTransform.mTranslation, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(data->mTransform.mScale, 1.5f);
        ASSERT_EQ(data->mBones.size(), 1u);
        EXPECT_EQ(data->mBones[0].mTransform.mTranslation, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(data->mBones[0].mBoundSphere.radius(), 7.f);
        ASSERT_EQ(data->mBones[0].mWeights.size(), 2u);
        EXPECT_EQ(data->mBones[0].mWeights[1].first, 1);
        EXPECT_EQ(data->mBones[0].mWeights[1].second, 0.75f);
        const auto* instance = parsed.as<Nif::NiSkinInstance>(3);
        ASSERT_NE(instance, nullptr);
        EXPECT_EQ(instance->mData.getPtr(), data);
        EXPECT_EQ(instance->mRoot.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(instance->getPartitions(), nullptr);
        EXPECT_TRUE(parsed.as<Nif::NiNode>(1)->mIsBone);
        EXPECT_FALSE(parsed.as<Nif::NiNode>(0)->mIsBone);
    }

    TEST(NifDataTest, readsTheSkinOfMorrowindWithItsPartitionsAndWithoutWeights)
    {
        {
            Builder file(morrowind);
            file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
            file.add("NiNode", file.node({ .mName = "Bone" }));
            // the skin of Morrowind has no flag for the weights and has the partitions of its own
            file.add(
                "NiSkinData", Bytes().transform().u32(1).ref(-1).transform().vec3(0, 0, 0).f32(1).u16(1).u16(5).f32(1));
            file.add("NiSkinInstance", Bytes().ref(2).ref(0).refs({ 1 }));

            const Parsed parsed(file.build());

            EXPECT_EQ(parsed.as<Nif::NiSkinData>(2)->mBones.at(0).mWeights.size(), 1u);
        }
        {
            Builder file(oblivion);
            file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
            file.add("NiNode", file.node({ .mName = "Bone" }));
            // a bone that has no weights lists no vertices
            file.add("NiSkinData", Bytes().transform().u32(1).boolean(false).transform().vec3(0, 0, 0).f32(1).u16(4));
            file.add("NiSkinInstance", Bytes().ref(2).ref(-1).ref(0).refs({ 1 }));

            const Parsed parsed(file.build());

            EXPECT_TRUE(parsed.as<Nif::NiSkinData>(2)->mBones.at(0).mWeights.empty());
        }
    }

    TEST(NifDataTest, refusesASkinThatIsMissingItsPartsOrDoesNotAgreeWithItself)
    {
        const auto build = [](std::int32_t data, std::int32_t root, const std::vector<std::int32_t>& bones,
                               std::uint32_t skinnedBones) {
            Builder file(oblivion);
            file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
            file.add("NiNode", file.node({ .mName = "Bone" }));
            Bytes skin;
            skin.transform().u32(skinnedBones).boolean(false);
            for (std::uint32_t i = 0; i < skinnedBones; ++i)
                skin.transform().vec3(0, 0, 0).f32(1).u16(0);
            file.add("NiSkinData", skin);
            file.add("NiSkinInstance", Bytes().ref(data).ref(-1).ref(root).refs(bones));
            return file.build();
        };

        EXPECT_NO_THROW(Parsed(build(2, 0, { 1 }, 1)));
        EXPECT_THROW(Parsed(build(-1, 0, { 1 }, 1)), Nif::Exception);
        EXPECT_THROW(Parsed(build(2, -1, { 1 }, 1)), Nif::Exception);
        EXPECT_THROW(Parsed(build(2, 0, { 1, 1 }, 1)), Nif::Exception);
        EXPECT_THROW(Parsed(build(2, 0, { -1 }, 1)), Nif::Exception);
    }

    TEST(NifDataTest, readsThePartitionsOfASkin)
    {
        const auto partition = [](Builder& file, bool strips) {
            Bytes bytes;
            bytes.u32(1);
            // 4 vertices, 2 triangles or one strip, 2 bones, 2 bones to a vertex
            bytes.u16(4).u16(2).u16(2).u16(strips ? 1 : 0).u16(2);
            bytes.u16(5).u16(6);
            if (file.profile().mVersion >= 0x0A010000)
                bytes.boolean(true);
            bytes.u16(10).u16(11).u16(12).u16(13);
            if (file.profile().mVersion >= 0x0A010000)
                bytes.boolean(true);
            for (int i = 0; i < 8; ++i)
                bytes.f32(0.5f);
            if (strips)
                bytes.u16(4);
            if (file.profile().mVersion >= 0x0A010000)
                bytes.boolean(true);
            if (strips)
                bytes.u16(0).u16(1).u16(2).u16(3);
            else
                bytes.u16(0).u16(1).u16(2).u16(1).u16(2).u16(3);
            bytes.u8(1);
            for (int i = 0; i < 8; ++i)
                bytes.u8(1);
            if (file.profile().mBethVersion > 34)
                bytes.u8(2).boolean(true);
            return bytes;
        };
        for (const Profile& profile : { oblivion, Profile{ 0x0A000100, 0, 0 }, skyrim })
        {
            for (const bool strips : { false, true })
            {
                Builder file(profile);
                file.add("NiSkinPartition", partition(file, strips));

                const Parsed parsed(file.build());

                const auto* skin = parsed.as<Nif::NiSkinPartition>(0);
                ASSERT_NE(skin, nullptr);
                ASSERT_EQ(skin->mPartitions.size(), 1u);
                const auto& part = skin->mPartitions[0];
                EXPECT_THAT(part.mBones, ElementsAre(5, 6));
                EXPECT_THAT(part.mVertexMap, ElementsAre(10, 11, 12, 13));
                EXPECT_EQ(part.mWeights.size(), 8u);
                EXPECT_EQ(part.mBoneIndices.size(), 8u);
                // the vertices of the triangles are those of the map
                if (strips)
                {
                    ASSERT_EQ(part.mTrueStrips.size(), 1u);
                    EXPECT_THAT(part.mTrueStrips[0], ElementsAre(10, 11, 12, 13));
                }
                else
                {
                    EXPECT_THAT(part.mTrueTriangles, ElementsAre(10, 11, 12, 11, 12, 13));
                }
                if (profile.mBethVersion > 34)
                {
                    EXPECT_EQ(part.mLODLevel, 2u);
                    EXPECT_TRUE(part.mGlobalVB);
                }
            }
        }
    }

    TEST(NifDataTest, readsTheDismemberPartsOfAnActor)
    {
        Builder file(fallout3);
        file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
        file.add("NiNode", file.node({ .mName = "Bone" }));
        file.add("NiSkinData", Bytes().transform().u32(1).boolean(false).transform().vec3(0, 0, 0).f32(1).u16(0));
        file.add("NiSkinPartition", Bytes().u32(0));
        file.add("BSDismemberSkinInstance",
            Bytes().ref(2).ref(3).ref(0).refs({ 1 }).u32(2).u16(1).u16(4).u16(0x101).u16(30));

        const Parsed parsed(file.build());

        const auto* instance = parsed.as<Nif::BSDismemberSkinInstance>(4);
        ASSERT_NE(instance, nullptr);
        ASSERT_EQ(instance->mParts.size(), 2u);
        EXPECT_EQ(instance->mParts[0].mFlags, 1u);
        EXPECT_EQ(instance->mParts[0].mType, 4u);
        EXPECT_EQ(instance->mParts[1].mFlags, 0x101u);
        EXPECT_EQ(instance->getPartitions(), parsed.as<Nif::NiSkinPartition>(3));
    }

    TEST(NifDataTest, readsTheSkinOfFallout4)
    {
        const auto build = [](std::uint32_t bones) {
            Builder file(fallout4);
            file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
            file.add("NiNode", file.node({ .mName = "Bone" }));
            // the data lists the bound sphere and the transform of each bone
            file.add("BSSkin::BoneData", Bytes().u32(1).vec3(1, 2, 3).f32(4).transform(5, 6, 7));
            file.add("BSSkin::Instance", Bytes().ref(0).ref(2).refs({ 1 }).u32(bones).vec3(1, 1, 1));
            return file.build();
        };

        const Parsed parsed(build(1));

        const auto* data = parsed.as<Nif::BSSkinBoneData>(2);
        ASSERT_NE(data, nullptr);
        ASSERT_EQ(data->mBones.size(), 1u);
        EXPECT_EQ(data->mBones[0].mBoundSphere.radius(), 4.f);
        EXPECT_EQ(data->mBones[0].mTransform.mTranslation, osg::Vec3f(5, 6, 7));
        const auto* instance = parsed.as<Nif::BSSkinInstance>(3);
        ASSERT_NE(instance, nullptr);
        EXPECT_EQ(instance->mRoot.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_THAT(instance->mScales, ElementsAre(osg::Vec3f(1, 1, 1)));
        EXPECT_TRUE(parsed.as<Nif::NiNode>(1)->mIsBone);
    }

    TEST(NifDataTest, refusesTheSkinOfFallout4ThatDoesNotAgreeWithItself)
    {
        Builder file(fallout4);
        file.add("NiNode", file.node({ .mName = "Root" }, { 1 }));
        file.add("NiNode", file.node({ .mName = "Bone" }));
        file.add("BSSkin::BoneData", Bytes().u32(2).vec3(1, 2, 3).f32(4).transform().vec3(1, 2, 3).f32(4).transform());
        file.add("BSSkin::Instance", Bytes().ref(0).ref(2).refs({ 1 }).u32(0));

        EXPECT_THROW(Parsed(file.build()), Nif::Exception);
    }

    TEST(NifDataTest, readsTheAdditionalDataOfAGeometry)
    {
        for (const std::string_view type : { "NiAdditionalGeometryData", "BSPackedAdditionalGeometryData" })
        {
            Builder file(fallout3);
            Bytes bytes;
            bytes.u16(3).u32(1);
            bytes.u32(1).u32(4).u32(12).u32(12).u32(0).u32(0).u8(1); // a stream of the data
            bytes.u32(2);
            // a valid block of three vertices of 12 bytes, and one that is not valid
            bytes.boolean(true).u32(12).u32(1).u32(0).u32(3).u32(12).u32(12).u32(12);
            for (int i = 0; i < 36; ++i)
                bytes.u8(static_cast<std::uint8_t>(i));
            if (type == "BSPackedAdditionalGeometryData")
                bytes.u32(7).u32(36);
            bytes.boolean(false);
            file.add(type, bytes);

            const Parsed parsed(file.build());

            const auto* data = parsed.as<Nif::NiAdditionalGeometryData>(0);
            ASSERT_NE(data, nullptr);
            EXPECT_EQ(data->mNumVertices, 3u);
            ASSERT_EQ(data->mBlockInfos.size(), 1u);
            EXPECT_EQ(data->mBlockInfos[0].mStride, 12u);
            ASSERT_EQ(data->mBlocks.size(), 2u);
            EXPECT_TRUE(data->mBlocks[0].mValid);
            EXPECT_EQ(data->mBlocks[0].mData.size(), 36u);
            EXPECT_FALSE(data->mBlocks[1].mValid);
            if (type == "BSPackedAdditionalGeometryData")
                EXPECT_EQ(data->mBlocks[0].mShaderIndex, 7u);
        }
    }

    TEST(NifDataTest, readsTheBoundsOfTheRoomsOfACell)
    {
        Builder file(fallout3);
        file.add("BSMultiBound", Bytes().ref(1));
        file.add("BSMultiBoundAABB", Bytes().vec3(1, 2, 3).vec3(4, 5, 6));
        file.add("BSMultiBound", Bytes().ref(3));
        file.add("BSMultiBoundOBB", Bytes().vec3(1, 2, 3).vec3(4, 5, 6).identity3());
        file.add("BSMultiBound", Bytes().ref(5));
        file.add("BSMultiBoundSphere", Bytes().vec3(1, 2, 3).f32(9));

        const Parsed parsed(file.build());

        EXPECT_EQ(parsed.as<Nif::BSMultiBoundAABB>(1)->mExtents, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(parsed.as<Nif::BSMultiBound>(0)->mData.getPtr(), parsed.as<Nif::BSMultiBoundAABB>(1));
        EXPECT_EQ(parsed.as<Nif::BSMultiBoundOBB>(3)->mSize, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(parsed.as<Nif::BSMultiBoundSphere>(5)->mSphere.radius(), 9.f);
    }

    TEST(NifDataTest, readsTheLinesOfAGeometry)
    {
        Builder file(fallout3);
        // the flags say which vertices are joined to the next: the last joins to the first
        Bytes bytes = geometryData(file, 4, 0, false, false);
        bytes.u8(1).u8(0).u8(1).u8(1);
        file.add("NiLinesData", bytes);
        Bytes single = geometryData(file, 1, 0, false, false);
        single.u8(1);
        file.add("NiLinesData", single);

        const Parsed parsed(file.build());

        EXPECT_THAT(parsed.as<Nif::NiLinesData>(0)->mLines, ElementsAre(0, 1, 2, 3, 3, 0));
        EXPECT_TRUE(parsed.as<Nif::NiLinesData>(1)->mLines.empty());
    }
}
