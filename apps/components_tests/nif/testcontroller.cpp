#include "niftestfile.hpp"

#include <components/nif/controller.hpp>
#include <components/nif/data.hpp>
#include <components/nif/exception.hpp>
#include <components/nif/node.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

// The controllers and the interpolators that animate a scene: what each game writes for them, from the inline key data
// of Morrowind to the interpolators of Oblivion and the notes of Fallout 3.
namespace
{
    using namespace NifTest;
    using namespace testing;

    /// The start of every controller: the next one, the flags, the timing and the target.
    Bytes timeController(
        const Builder& file, std::int32_t next = -1, std::uint16_t flags = 0x8, std::int32_t target = -1)
    {
        Bytes bytes;
        bytes.ref(next).u16(flags).f32(1.5f).f32(0.25f).f32(0.5f).f32(2.f);
        if (file.profile().mVersion >= 0x03030D0D)
            bytes.ref(target);
        return bytes;
    }

    /// A controller with one interpolator: the interpolator comes after the timing, or the data in the old files.
    Bytes singleInterpolator(const Builder& file, std::int32_t interpolator = -1, std::int32_t data = -1)
    {
        Bytes bytes = timeController(file);
        if (file.profile().mVersion >= 0x0A010068)
            bytes.ref(interpolator);
        else
            bytes.ref(data);
        return bytes;
    }

    Bytes withRef(Bytes&& bytes, std::int32_t ref)
    {
        return std::move(bytes.ref(ref));
    }

    std::int32_t addRoot(Builder& file)
    {
        const std::int32_t index = file.add("NiNode", file.node({ .mName = "Root" }));
        file.addRoot(index);
        return index;
    }

    TEST(NifControllerTest, readsTheControllersOfMorrowindWithTheirInlineData)
    {
        Builder file(morrowind);
        addRoot(file);
        // 1: a controller that has a next one and a target
        file.add("NiKeyframeController", withRef(timeController(file, 2, 0x8, 0), -1));
        file.add("NiAlphaController", singleInterpolator(file));
        file.add("NiRollController", singleInterpolator(file));
        file.add("NiVisController", singleInterpolator(file));
        // the colour that is controlled is in the flags
        file.add("NiMaterialColorController", withRef(timeController(file, -1, 0x38), -1));
        file.add("NiLightColorController", withRef(timeController(file, -1, 0x18), -1));
        file.add("NiLookAtController", withRef(timeController(file, -1, 0x48), 0));
        file.add(
            "NiPathController", timeController(file, -1, 0x28).i32(-1).f32(0.5f).f32(0.25f).u16(2).ref(-1).ref(-1));
        file.add("NiUVController", timeController(file).u16(3).ref(-1));
        file.add("NiGeomMorpherController", timeController(file).ref(-1).u8(1));
        file.add("NiFlipController", timeController(file).u32(2).f32(0.1f).f32(0.2f).refs({}));
        file.add("NiTextureTransformController", timeController(file).boolean32(true).u32(1).u32(3).ref(-1));

        const Parsed parsed(file.build());

        const auto* keyframe = parsed.as<Nif::NiKeyframeController>(1);
        ASSERT_NE(keyframe, nullptr);
        EXPECT_EQ(keyframe->mNext.getPtr(), parsed.as<Nif::NiAlphaController>(2));
        EXPECT_EQ(keyframe->mTarget.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(keyframe->mFrequency, 1.5f);
        EXPECT_EQ(keyframe->mPhase, 0.25f);
        EXPECT_EQ(keyframe->mTimeStart, 0.5f);
        EXPECT_EQ(keyframe->mTimeStop, 2.f);
        EXPECT_TRUE(keyframe->isActive());
        EXPECT_EQ(parsed.as<Nif::NiMaterialColorController>(5)->mTargetColor,
            Nif::NiMaterialColorController::TargetColor::Emissive);
        EXPECT_EQ(parsed.as<Nif::NiLightColorController>(6)->mMode, Nif::NiLightColorController::Mode::Ambient);
        const auto* lookAt = parsed.as<Nif::NiLookAtController>(7);
        EXPECT_EQ(lookAt->mLookAtFlags, 4u);
        EXPECT_EQ(lookAt->mLookAt.getPtr(), parsed.as<Nif::NiNode>(0));
        const auto* path = parsed.as<Nif::NiPathController>(8);
        EXPECT_EQ(path->mPathFlags, 2u);
        EXPECT_EQ(path->mMaxBankAngle, 0.5f);
        EXPECT_EQ(path->mSmoothing, 0.25f);
        EXPECT_EQ(path->mFollowAxis, 2u);
        EXPECT_EQ(parsed.as<Nif::NiUVController>(9)->mUvSet, 3u);
        EXPECT_TRUE(parsed.as<Nif::NiGeomMorpherController>(10)->mAlwaysActive);
        const auto* flip = parsed.as<Nif::NiFlipController>(11);
        EXPECT_EQ(flip->mTexSlot, Nif::NiTexturingProperty::DetailTexture);
        EXPECT_EQ(flip->mDelta, 0.2f);
        const auto* transform = parsed.as<Nif::NiTextureTransformController>(12);
        EXPECT_TRUE(transform->mShaderMap);
        EXPECT_EQ(transform->mTransformMember, 3u);
    }

    TEST(NifControllerTest, readsAParticleSystemControllerOfMorrowind)
    {
        Builder file(morrowind);
        addRoot(file);
        Bytes bytes = timeController(file);
        bytes.f32(10).f32(1).f32(0.5f).f32(0.25f).f32(3).f32(0.75f); // speed and the angles
        bytes.vec3(0, 0, 1).vec4(1, 0.5f, 0.25f, 1).f32(2); // normal, colour, size
        bytes.f32(0).f32(8); // emit start and stop
        bytes.u8(1).f32(12.5f); // reset, birth rate
        bytes.f32(4).f32(1); // lifetime
        bytes.u16(0x3).vec3(5, 6, 7).ref(0); // emit flags, dimensions, emitter
        bytes.u16(2).f32(0.5f).u16(3).f32(0.1f).f32(0.2f); // spawning
        bytes.u16(2).u16(1); // particles and valid ones
        for (int i = 0; i < 2; ++i)
            bytes.vec3(1, 2, 3).vec3(0, 0, 1).f32(0.5f).f32(5).f32(0.25f).u16(1).u16(7);
        bytes.ref(-1); // the emitter modifier
        bytes.ref(-1).ref(-1).u8(1); // modifier, collider, static bound
        file.add("NiParticleSystemController", bytes);

        const Parsed parsed(file.build());

        const auto* controller = parsed.as<Nif::NiParticleSystemController>(1);
        ASSERT_NE(controller, nullptr);
        EXPECT_EQ(controller->mSpeed, 10.f);
        EXPECT_EQ(controller->mPlanarAngleVariation, 0.75f);
        EXPECT_EQ(controller->mInitialNormal, osg::Vec3f(0, 0, 1));
        EXPECT_EQ(controller->mInitialColor, osg::Vec4f(1, 0.5f, 0.25f, 1));
        EXPECT_TRUE(controller->mResetParticleSystem);
        EXPECT_EQ(controller->mBirthRate, 12.5f);
        EXPECT_EQ(controller->mEmitFlags, 3u);
        EXPECT_EQ(controller->mEmitterDimensions, osg::Vec3f(5, 6, 7));
        EXPECT_EQ(controller->mEmitter.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(controller->mNumSpawnGenerations, 2u);
        EXPECT_EQ(controller->mSpawnMultiplier, 3u);
        EXPECT_EQ(controller->mNumValid, 1u);
        ASSERT_EQ(controller->mParticles.size(), 2u);
        EXPECT_EQ(controller->mParticles[1].mVelocity, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(controller->mParticles[1].mLifespan, 5.f);
        EXPECT_EQ(controller->mParticles[1].mCode, 7u);
        EXPECT_EQ(controller->mStaticTargetBound, 1u);
    }

    TEST(NifControllerTest, readsTheGroupsOfABoneLevelOfDetailController)
    {
        // The skinned shapes follow the groups of nodes only where the file has no Bethesda header.
        for (const Profile& profile : { morrowind, Profile{ 0x04020200, 0, 0 }, oblivion })
        {
            Builder file(profile);
            addRoot(file);
            file.add("NiBoneLODController", [&] {
                Bytes bytes = timeController(file);
                bytes.u32(1).u32(2).u32(2);
                bytes.refs({ 0 }).refs({});
                if (profile.mBethVersion == 0 && profile.mVersion >= 0x04020200)
                    bytes.u32(0).refs({});
                return bytes;
            }());

            const Parsed parsed(file.build());

            const auto* controller = parsed.as<Nif::NiBoneLODController>(1);
            ASSERT_NE(controller, nullptr);
            EXPECT_EQ(controller->mLOD, 1u);
            EXPECT_EQ(controller->mNumNodeGroups, 2u);
            ASSERT_EQ(controller->mNodeGroups.size(), 2u);
            EXPECT_EQ(controller->mNodeGroups[0].size(), 1u);
            EXPECT_TRUE(controller->mNodeGroups[1].empty());
        }
    }

    TEST(NifControllerTest, readsTheInterpolatorsOfOblivion)
    {
        Builder file(oblivion);
        addRoot(file);
        // 1: a controller of the transform of its target with an interpolator after it
        file.add("NiTransformController", withRef(timeController(file, -1, 0x8, 0), 2));
        file.add("NiTransformInterpolator", Bytes().vec3(1, 2, 3).quat(1, 0, 0, 0).f32(2).ref(-1));
        file.add("NiFloatInterpolator", Bytes().f32(0.5f).ref(-1));
        file.add("NiBoolInterpolator", Bytes().boolean(true).ref(-1));
        file.add("NiPoint3Interpolator", Bytes().vec3(4, 5, 6).ref(-1));
        file.add("NiColorInterpolator", Bytes().vec4(1, 0, 0, 1).ref(-1));
        file.add("NiPathInterpolator", Bytes().u16(3).i32(1).f32(0.5f).f32(0.25f).u16(1).ref(-1).ref(-1));
        file.add("NiLookAtInterpolator", [&] {
            Bytes bytes;
            bytes.u16(2).ref(0);
            file.string(bytes, "Look");
            bytes.vec3(1, 1, 1).quat(1, 0, 0, 0).f32(1);
            return bytes.ref(2).ref(2).ref(2);
        }());
        // 8: a blend with two items
        file.add("NiBlendFloatInterpolator",
            Bytes()
                .u8(0)
                .u8(2)
                .f32(0.25f)
                .u8(2)
                .u8(1)
                .u8(5)
                .u8(4)
                .f32(0.5f)
                .f32(1)
                .f32(2)
                .f32(3)
                .ref(2)
                .f32(1)
                .f32(0.5f)
                .u8(7)
                .f32(0.1f)
                .ref(-1)
                .f32(0)
                .f32(0)
                .u8(2)
                .f32(0)
                .f32(9));
        // 9: a blend that the manager of the controllers drives has no items of its own
        file.add("NiBlendTransformInterpolator", Bytes().u8(1).u8(0).f32(0.75f));
        file.add("NiBlendBoolInterpolator", Bytes().u8(1).u8(0).f32(0).u8(1));
        file.add("NiBlendPoint3Interpolator", Bytes().u8(1).u8(0).f32(0).vec3(1, 2, 3));
        file.add("NiLightDimmerController", singleInterpolator(file, 3));

        const Parsed parsed(file.build());

        const auto* controller = parsed.as<Nif::NiKeyframeController>(1);
        ASSERT_NE(controller, nullptr);
        EXPECT_EQ(controller->mInterpolator.getPtr(), parsed.as<Nif::NiTransformInterpolator>(2));
        const auto* transform = parsed.as<Nif::NiTransformInterpolator>(2);
        EXPECT_EQ(transform->mDefaultValue.mTranslation, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(transform->mDefaultValue.mScale, 2.f);
        EXPECT_EQ(parsed.as<Nif::NiFloatInterpolator>(3)->mDefaultValue, 0.5f);
        EXPECT_TRUE(parsed.as<Nif::NiBoolInterpolator>(4)->mDefaultValue);
        EXPECT_EQ(parsed.as<Nif::NiPoint3Interpolator>(5)->mDefaultValue, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(parsed.as<Nif::NiColorInterpolator>(6)->mDefaultValue, osg::Vec4f(1, 0, 0, 1));
        const auto* path = parsed.as<Nif::NiPathInterpolator>(7);
        EXPECT_EQ(path->mFlags, 3u);
        EXPECT_EQ(path->mBankDirection, 1);
        EXPECT_EQ(path->mFollowAxis, 1u);
        const auto* lookAt = parsed.as<Nif::NiLookAtInterpolator>(8);
        EXPECT_EQ(lookAt->mLookAtFlags, 2u);
        EXPECT_EQ(lookAt->mLookAtName, "Look");
        EXPECT_EQ(lookAt->mLookAt.getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(lookAt->mTranslation.getPtr(), parsed.as<Nif::NiTransformInterpolator>(2));
        const auto* blend = parsed.as<Nif::NiBlendFloatInterpolator>(9);
        EXPECT_EQ(blend->mFlags, 0);
        EXPECT_EQ(blend->mWeightThreshold, 0.25f);
        EXPECT_EQ(blend->mInterpCount, 2u);
        EXPECT_EQ(blend->mSingleIndex, 1u);
        EXPECT_EQ(blend->mHighPriority, 5);
        EXPECT_EQ(blend->mSingleTime, 0.5f);
        ASSERT_EQ(blend->mItems.size(), 2u);
        EXPECT_EQ(blend->mItems[0].mInterpolator.getPtr(), parsed.as<Nif::NiTransformInterpolator>(2));
        EXPECT_EQ(blend->mItems[0].mPriority, 7);
        EXPECT_EQ(blend->mValue, 9.f);
        const auto* manager = parsed.as<Nif::NiBlendTransformInterpolator>(10);
        EXPECT_EQ(manager->mFlags, Nif::NiBlendInterpolator::Flag_ManagerControlled);
        EXPECT_TRUE(manager->mItems.empty());
        EXPECT_EQ(parsed.as<Nif::NiBlendPoint3Interpolator>(12)->mValue, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(parsed.as<Nif::NiFloatInterpController>(13)->mInterpolator.getPtr(),
            parsed.as<Nif::NiFloatInterpolator>(3));
    }

    TEST(NifControllerTest, readsTheBlendInterpolatorsOfTheFirstGamebryoVersions)
    {
        // The layout of a blend changed three times in the versions of Gamebryo before the one of Oblivion.
        for (const std::uint32_t version : { 0x0A01006Eu, 0x0A01006Cu, 0x0A01006Au })
        {
            Builder file(Profile{ version, 0, 0 });
            file.add("NiFloatInterpolator", Bytes().f32(1).ref(-1));
            Bytes bytes;
            if (version >= 0x0A01006E)
            {
                bytes.u8(1).ref(0).f32(1).f32(0.5f).u8(7).f32(0.1f); // one item
                bytes.boolean(true).f32(0.5f).boolean(true); // managed, threshold, highest only
                bytes.u8(3).u8(1).ref(0).f32(2).u8(4).u8(5);
            }
            else
            {
                bytes.u16(1).u16(4); // the count and the growth of the array
                bytes.ref(0).f32(1).f32(0.5f).i32(7).f32(0.1f);
                bytes.boolean(true).f32(0.5f).boolean(true);
                bytes.u16(3).u16(1);
                if (version >= 0x0A01006C)
                    bytes.ref(0).f32(2);
                bytes.i32(4).i32(5);
            }
            file.add("NiBlendFloatInterpolator", bytes.f32(8.f));

            const Parsed parsed(file.build());

            const auto* blend = parsed.as<Nif::NiBlendFloatInterpolator>(1);
            ASSERT_NE(blend, nullptr);
            ASSERT_EQ(blend->mItems.size(), 1u);
            EXPECT_EQ(blend->mItems[0].mPriority, 7);
            EXPECT_EQ(blend->mFlags,
                Nif::NiBlendInterpolator::Flag_ManagerControlled | Nif::NiBlendInterpolator::Flag_OnlyUseHighestWeight);
            EXPECT_EQ(blend->mWeightThreshold, 0.5f);
            EXPECT_EQ(blend->mInterpCount, 3u);
            EXPECT_EQ(blend->mSingleIndex, 1u);
            EXPECT_EQ(blend->mHighPriority, 4);
            EXPECT_EQ(blend->mNextHighPriority, 5);
            EXPECT_EQ(blend->mValue, 8.f);
            if (version >= 0x0A01006C)
                EXPECT_EQ(blend->mSingleInterpolator.getPtr(), parsed.as<Nif::NiFloatInterpolator>(0));
            else
                EXPECT_TRUE(blend->mSingleInterpolator.empty());
        }
    }

    TEST(NifControllerTest, readsTheOldTransformInterpolatorsWithTheirFlags)
    {
        // Before 10.1.0.110 a transform says with a flag for each part whether the part is there.
        Builder file(Profile{ 0x0A01006A, 0, 0 });
        file.add("NiTransformInterpolator",
            Bytes().vec3(1, 2, 3).quat(1, 0, 0, 0).f32(2).boolean(true).boolean(false).boolean(true).ref(-1));
        file.add("NiBlendTransformInterpolator",
            Bytes()
                .u16(0)
                .u16(1)
                .boolean(false)
                .f32(0)
                .boolean(false)
                .u16(0)
                .u16(0)
                .i32(0)
                .i32(0)
                .vec3(4, 5, 6)
                .quat(1, 0, 0, 0)
                .f32(3)
                .boolean(true)
                .boolean(true)
                .boolean(false));

        const Parsed parsed(file.build());

        const auto* transform = parsed.as<Nif::NiTransformInterpolator>(0);
        ASSERT_NE(transform, nullptr);
        EXPECT_EQ(transform->mDefaultValue.mTranslation, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(transform->mDefaultValue.mRotation, osg::Quat());
        EXPECT_EQ(transform->mDefaultValue.mScale, 2.f);
        const auto* blend = parsed.as<Nif::NiBlendTransformInterpolator>(1);
        ASSERT_NE(blend, nullptr);
        EXPECT_EQ(blend->mValue.mTranslation, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(blend->mValue.mScale, 1.f);
    }

    TEST(NifControllerTest, readsTheControllersAndTheExtraDataControllersOfOblivion)
    {
        Builder file(oblivion);
        addRoot(file);
        file.add("NiFloatInterpolator", Bytes().f32(0.5f).ref(-1));
        file.add("NiMultiTargetTransformController", timeController(file, -1, 0x8, 0).u16(2).ref(0).ref(0));
        // the morphs of a mesh, with the interpolators of the targets and what is not known
        file.add("NiGeomMorpherController", timeController(file).u16(1).ref(-1).u8(1).refs({ 1, 1 }).u32(1).u32(77));
        auto extraData = [&](std::string_view name) {
            Bytes bytes = singleInterpolator(file, 1);
            return file.string(bytes, name);
        };
        file.add("NiFloatExtraDataController", extraData("Scale"));
        file.add("NiFloatsExtraDataController", extraData("Floats").i32(4));
        file.add("NiFloatsExtraDataPoint3Controller", extraData("Point").i32(5));
        file.add("NiColorExtraDataController", extraData("Color"));

        const Parsed parsed(file.build());

        const auto* multi = parsed.as<Nif::NiMultiTargetTransformController>(2);
        ASSERT_NE(multi, nullptr);
        ASSERT_EQ(multi->mExtraTargets.size(), 2u);
        EXPECT_EQ(multi->mExtraTargets[0].getPtr(), parsed.as<Nif::NiNode>(0));
        const auto* morpher = parsed.as<Nif::NiGeomMorpherController>(3);
        ASSERT_NE(morpher, nullptr);
        EXPECT_TRUE(morpher->mUpdateNormals);
        EXPECT_TRUE(morpher->mAlwaysActive);
        EXPECT_EQ(morpher->mInterpolators.size(), 2u);
        EXPECT_EQ(parsed.as<Nif::NiFloatExtraDataController>(4)->mExtraDataName, "Scale");
        EXPECT_EQ(parsed.as<Nif::NiFloatExtraDataController>(4)->mInterpolator.getPtr(),
            parsed.as<Nif::NiFloatInterpolator>(1));
        EXPECT_EQ(parsed.as<Nif::NiFloatsExtraDataController>(5)->mFloatsExtraDataIndex, 4);
        EXPECT_EQ(parsed.as<Nif::NiFloatsExtraDataPoint3Controller>(6)->mFloatsExtraDataIndex, 5);
        EXPECT_EQ(parsed.as<Nif::NiExtraDataController>(7)->mExtraDataName, "Color");
    }

    TEST(NifControllerTest, readsTheExtraDataControllersOfTheOldGamebryoWithTheirInlineData)
    {
        Builder file(Profile{ 0x0A010000, 0, 0 });
        addRoot(file);
        // before 10.1.0.104 the controller has no interpolator and has the data of its own
        Bytes floatController = timeController(file);
        floatController.u8(2).raw(std::string(7, '\0')).raw("xy").ref(-1);
        file.add("NiFloatExtraDataController", floatController);
        file.add("NiFloatsExtraDataController", timeController(file).i32(2).ref(-1));
        file.add("NiKeyframeController", timeController(file).ref(-1));

        const Parsed parsed(file.build());

        EXPECT_NE(parsed.as<Nif::NiFloatExtraDataController>(1), nullptr);
        EXPECT_EQ(parsed.as<Nif::NiFloatsExtraDataController>(2)->mFloatsExtraDataIndex, 2);
        EXPECT_NE(parsed.as<Nif::NiKeyframeController>(3), nullptr);
    }

    TEST(NifControllerTest, refusesTheKeysOfABlendController)
    {
        {
            Builder file(oblivion);
            file.add("bhkBlendController", timeController(file).u32(0));
            EXPECT_NO_THROW(Parsed(file.build()));
        }
        Builder file(oblivion);
        file.add("bhkBlendController", timeController(file).u32(3));

        EXPECT_THROW(Parsed(file.build()), Nif::Exception);
    }

    TEST(NifControllerTest, readsTheControllersOfFallout3)
    {
        Builder file(fallout3);
        addRoot(file);
        file.add("NiFloatInterpolator", Bytes().f32(0.5f).ref(-1));
        file.add("BSKeyframeController", singleInterpolator(file, 1).ref(-1));
        file.add("BSLagBoneController", timeController(file).f32(1).f32(2).f32(3));
        Bytes lightning = timeController(file);
        for (int i = 0; i < 9; ++i)
            lightning.ref(1);
        lightning.u16(1).u16(2).u16(3).f32(4).f32(5).f32(6).f32(7).f32(8).boolean(true).boolean(false).boolean(true);
        file.add("BSProceduralLightningController", lightning.ref(-1));
        file.add("BSEffectShaderPropertyFloatController", singleInterpolator(file, 1).u32(6));
        file.add("BSEffectShaderPropertyColorController", singleInterpolator(file, 1).u32(2));
        file.add("BSLightingShaderPropertyFloatController", singleInterpolator(file, 1).u32(3));
        // a geometry morph has a weight for each of its interpolators
        file.add("NiGeomMorpherController",
            timeController(file).u16(0).ref(-1).u8(0).u32(2).ref(1).f32(0.25f).ref(1).f32(0.75f));
        file.add("BSFrustumFOVController", singleInterpolator(file, 1));
        file.add("BSMaterialEmittanceMultController", singleInterpolator(file, 1));

        const Parsed parsed(file.build());

        const auto* lag = parsed.as<Nif::BSLagBoneController>(3);
        ASSERT_NE(lag, nullptr);
        EXPECT_EQ(lag->mLinearVelocity, 1.f);
        EXPECT_EQ(lag->mLinearRotation, 2.f);
        EXPECT_EQ(lag->mMaximumDistance, 3.f);
        const auto* bolt = parsed.as<Nif::BSProceduralLightningController>(4);
        ASSERT_NE(bolt, nullptr);
        EXPECT_EQ(bolt->mGenerationInterp.getPtr(), parsed.as<Nif::NiFloatInterpolator>(1));
        EXPECT_EQ(bolt->mSubdivisions, 1u);
        EXPECT_EQ(bolt->mNumBranchesVar, 3u);
        EXPECT_EQ(bolt->mArcOffset, 8.f);
        EXPECT_TRUE(bolt->mFadeMainBolt);
        EXPECT_FALSE(bolt->mFadeChildBolts);
        EXPECT_TRUE(bolt->mAnimateArcOffset);
        EXPECT_EQ(parsed.as<Nif::BSEffectShaderPropertyFloatController>(5)->mControlledVariable, 6u);
        EXPECT_EQ(parsed.as<Nif::BSEffectShaderPropertyColorController>(6)->mControlledColor, 2u);
        const auto* morpher = parsed.as<Nif::NiGeomMorpherController>(8);
        ASSERT_NE(morpher, nullptr);
        EXPECT_FALSE(morpher->mUpdateNormals);
        EXPECT_THAT(morpher->mWeights, ElementsAre(0.25f, 0.75f));
        EXPECT_EQ(morpher->mInterpolators.size(), 2u);
    }

    TEST(NifControllerTest, readsAControllerManagerWithItsSequencesOfOblivion)
    {
        Builder file(oblivion);
        addRoot(file);
        file.add("NiFloatInterpolator", Bytes().f32(0.5f).ref(-1));
        // 2: the manager with one sequence and the palette of the objects of the scene
        file.add("NiControllerManager", timeController(file, -1, 0x8, 0).boolean(true).refs({ 3 }).ref(4));
        Bytes sequence;
        file.string(sequence, "Idle");
        sequence.u32(2).u32(4); // two blocks, the array grows by four
        // the names of a block are offsets into the string palette
        sequence.ref(1).ref(-1).u8(30).ref(5).u32(0).u32(6).u32(11).u32(0xFFFFFFFF).u32(0xFFFFFFFF);
        sequence.ref(-1)
            .ref(-1)
            .u8(0)
            .ref(5)
            .u32(0xFFFFFFFF)
            .u32(0xFFFFFFFF)
            .u32(0xFFFFFFFF)
            .u32(0xFFFFFFFF)
            .u32(0xFFFFFFFF);
        sequence.f32(0.5f).ref(-1).u32(1).f32(2).f32(0).f32(3).ref(2);
        file.string(sequence, "Bip01");
        sequence.ref(5);
        file.add("NiControllerSequence", sequence);
        file.add("NiDefaultAVObjectPalette", Bytes().ref(0).u32(1).sized("Bip01").ref(0));
        const std::string text("Bip01\0NiTransformController\0", 28);
        file.add("NiStringPalette", Bytes().u32(28).raw(text).u32(28));

        const Parsed parsed(file.build());

        const auto* manager = parsed.as<Nif::NiControllerManager>(2);
        ASSERT_NE(manager, nullptr);
        EXPECT_TRUE(manager->mCumulative);
        ASSERT_EQ(manager->mSequences.size(), 1u);
        EXPECT_EQ(manager->mObjectPalette.getPtr(), parsed.as<Nif::NiDefaultAVObjectPalette>(4));
        const auto* seq = parsed.as<Nif::NiControllerSequence>(3);
        ASSERT_NE(seq, nullptr);
        EXPECT_EQ(manager->mSequences[0].getPtr(), seq);
        EXPECT_EQ(seq->mName, "Idle");
        ASSERT_EQ(seq->mControlledBlocks.size(), 2u);
        EXPECT_EQ(seq->mControlledBlocks[0].mInterpolator.getPtr(), parsed.as<Nif::NiFloatInterpolator>(1));
        EXPECT_EQ(seq->mControlledBlocks[0].mPriority, 30u);
        EXPECT_EQ(seq->mControlledBlocks[0].mPropertyTypeOffset, 6u);
        EXPECT_EQ(seq->mControlledBlocks[0].mControllerTypeOffset, 11u);
        EXPECT_EQ(seq->mControlledBlocks[0].mStringPalette.getPtr(), parsed.as<Nif::NiStringPalette>(5));
        EXPECT_EQ(seq->mWeight, 0.5f);
        EXPECT_EQ(seq->mExtrapolationMode, Nif::NiTimeController::ExtrapolationMode::Reverse);
        EXPECT_EQ(seq->mFrequency, 2.f);
        EXPECT_EQ(seq->mStopTime, 3.f);
        EXPECT_EQ(seq->mManager.getPtr(), manager);
        EXPECT_EQ(seq->mAccumRootName, "Bip01");
        EXPECT_EQ(seq->mStringPalette.getPtr(), parsed.as<Nif::NiStringPalette>(5));
        const auto* palette = parsed.as<Nif::NiDefaultAVObjectPalette>(4);
        ASSERT_EQ(palette->mObjects.count("Bip01"), 1u);
        EXPECT_EQ(palette->mObjects.at("Bip01").getPtr(), parsed.as<Nif::NiNode>(0));
        EXPECT_EQ(parsed.as<Nif::NiStringPalette>(5)->mPalette, text);
    }

    TEST(NifControllerTest, readsTheSequencesOfFallout3WithTheNamesInTheFileAndTheNotesOfTheAnimation)
    {
        // Fallout 3 has a list of notes, and its first files have one note.
        for (const std::uint32_t bethVersion : { 34u, 26u })
        {
            Builder file(Profile{ 0x14020007, 11, bethVersion });
            addRoot(file);
            file.add("NiFloatInterpolator", Bytes().f32(0.5f).ref(-1));
            Bytes sequence;
            file.string(sequence, "Walk");
            sequence.u32(1).u32(2);
            sequence.ref(1).ref(-1).u8(5);
            for (const std::string_view text : { "Bip01 Head", "", "NiTransformController", "", "" })
                file.string(sequence, text);
            sequence.f32(1).ref(-1).u32(2).f32(1).f32(0).f32(1.5f).ref(-1);
            file.string(sequence, "Bip01");
            if (bethVersion >= 29)
                sequence.u16(1);
            sequence.ref(3);
            file.add("NiControllerSequence", sequence);
            file.add("BSAnimNotes", Bytes().u16(3).ref(4).ref(5).ref(6));
            file.add("BSAnimNote", Bytes().u32(1).f32(0.5f).u32(1));
            file.add("BSAnimNote", Bytes().u32(2).f32(0.75f).f32(2).u32(3));
            file.add("BSAnimNote", Bytes().u32(0).f32(1));

            const Parsed parsed(file.build());

            const auto* seq = parsed.as<Nif::NiControllerSequence>(2);
            ASSERT_NE(seq, nullptr);
            EXPECT_EQ(seq->mExtrapolationMode, Nif::NiTimeController::ExtrapolationMode::Constant);
            ASSERT_EQ(seq->mControlledBlocks.size(), 1u);
            EXPECT_EQ(seq->mControlledBlocks[0].mNodeName, "Bip01 Head");
            EXPECT_EQ(seq->mControlledBlocks[0].mControllerType, "NiTransformController");
            ASSERT_EQ(seq->mAnimNotesList.size(), 1u);
            const auto* notes = parsed.as<Nif::BSAnimNotes>(3);
            ASSERT_NE(notes, nullptr);
            ASSERT_EQ(notes->mList.size(), 3u);
            const auto* grab = parsed.as<Nif::BSAnimNote>(4);
            EXPECT_EQ(grab->mType, Nif::BSAnimNote::Type::GrabIK);
            EXPECT_EQ(grab->mArm, 1u);
            const auto* look = parsed.as<Nif::BSAnimNote>(5);
            EXPECT_EQ(look->mGain, 2.f);
            EXPECT_EQ(look->mState, 3u);
        }
    }

    TEST(NifControllerTest, readsTheSequencesOfTheFirstGamebryoVersions)
    {
        // Before 10.1.0.104 a sequence names the target of each block and has its text keys, its root and nothing more.
        {
            Builder file(Profile{ 0x0A010000, 0, 0 });
            file.add("NiControllerSequence",
                Bytes().sized("Run").sized("Bip01").ref(-1).u32(1).sized("Bip01 Spine").ref(-1));
            const Parsed parsed(file.build());

            const auto* seq = parsed.as<Nif::NiControllerSequence>(0);
            ASSERT_NE(seq, nullptr);
            EXPECT_EQ(seq->mAccumRootName, "Bip01");
            ASSERT_EQ(seq->mControlledBlocks.size(), 1u);
            EXPECT_EQ(seq->mControlledBlocks[0].mTargetName, "Bip01 Spine");
        }
        // At 10.1.0.106 the sequence can play backwards and a block has the blend interpolator and its index.
        {
            Builder file(Profile{ 0x0A01006A, 0, 0 });
            file.add("NiFloatInterpolator", Bytes().f32(0.5f).ref(-1));
            Bytes sequence;
            sequence.sized("Run").u32(1).u32(2);
            sequence.ref(0).ref(-1).ref(-1).u16(3);
            for (int i = 0; i < 5; ++i)
                sequence.sized("x");
            sequence.f32(1).ref(-1).u32(0).f32(1).f32(0).f32(0).f32(2).boolean(true).ref(-1).sized("Bip01");
            file.add("NiControllerSequence", sequence);

            const Parsed parsed(file.build());

            const auto* seq = parsed.as<Nif::NiControllerSequence>(1);
            ASSERT_NE(seq, nullptr);
            EXPECT_TRUE(seq->mPlayBackwards);
            EXPECT_EQ(seq->mExtrapolationMode, Nif::NiTimeController::ExtrapolationMode::Cycle);
            ASSERT_EQ(seq->mControlledBlocks.size(), 1u);
            EXPECT_EQ(seq->mControlledBlocks[0].mBlendIndex, 3u);
            EXPECT_EQ(seq->mControlledBlocks[0].mNodeName, "x");
        }
    }

    TEST(NifControllerTest, readsTheTreadsOfAnInterpolatorOfTracks)
    {
        Builder file(fallout3);
        file.add("NiFloatData", Bytes().floatKeys({ { 0.f, 1.f }, { 1.f, 2.f } }));
        Bytes treads;
        treads.u32(2);
        for (const std::string_view name : { "Left", "Right" })
        {
            file.string(treads, name);
            treads.vec3(1, 2, 3).quat(1, 0, 0, 0).f32(1).vec3(4, 5, 6).quat(1, 0, 0, 0).f32(2);
        }
        file.add("BSTreadTransfInterpolator", treads.ref(0));

        const Parsed parsed(file.build());

        const auto* interpolator = parsed.as<Nif::BSTreadTransfInterpolator>(1);
        ASSERT_NE(interpolator, nullptr);
        ASSERT_EQ(interpolator->mTransforms.size(), 2u);
        EXPECT_EQ(interpolator->mTransforms[1].mName, "Right");
        EXPECT_EQ(interpolator->mTransforms[1].mTransform2.mScale, 2.f);
        EXPECT_EQ(interpolator->mData.getPtr(), parsed.as<Nif::NiFloatData>(0));
    }
}
