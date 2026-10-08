#include "../nif/sequence.hpp"

#include <components/nifosg/nifloader.hpp>
#include <components/sceneutil/controller.hpp>
#include <components/sceneutil/keyframe.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <osg/Quat>
#include <osg/Vec3f>

#include <limits>
#include <memory>
#include <set>
#include <string>

namespace
{
    using namespace testing;
    using namespace Nif::Testing;

    constexpr VFS::Path::NormalizedView idleKf("meshes/characters/_male/mtidle.kf");

    class ConstantSource : public SceneUtil::ControllerSource
    {
    public:
        float getValue(osg::NodeVisitor* nv) override { return 0.f; }
    };

    class IdentityFunction : public SceneUtil::ControllerFunction
    {
    public:
        float calculate(float input) const override { return input; }
        float getMaximum() const override { return 1.f; }
    };

    // The transform that the controller of a node gives at the first moment
    SceneUtil::KeyframeController::KfTransform transformAtStart(const SceneUtil::KeyframeController& controller)
    {
        auto& ctrl = const_cast<SceneUtil::KeyframeController&>(controller);
        ctrl.setSource(std::make_shared<ConstantSource>());
        ctrl.setFunction(std::make_shared<IdentityFunction>());
        osg::NodeVisitor visitor;
        return ctrl.getCurrentTransformation(&visitor);
    }

    constexpr float notSet = std::numeric_limits<float>::lowest();

    std::set<std::string> textKeys(const SceneUtil::TextKeyMap& map)
    {
        std::set<std::string> result;
        for (const auto& [time, key] : map)
            result.insert(std::to_string(time) + " " + key);
        return result;
    }

    // The animation of a file of Oblivion and later is a sequence of the nodes it drives.
    TEST(NifOsgLoadKfControllerSequence, makesAGroupOfTheNameOfTheFile)
    {
        Nif::NIFFile file(idleKf);
        addSequence(file, "Idle", 0.f, 2.f, Nif::NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01 Pelvis", { { 0.f, osg::Vec3f(0, 0, 0) }, { 2.f, osg::Vec3f(0, 0, 10) } } } });

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        EXPECT_THAT(holder.mTextKeys.getGroups(), ElementsAre("mtidle"));
        EXPECT_TRUE(holder.mTextKeys.hasGroupStart("mtidle"));
        EXPECT_THAT(textKeys(holder.mTextKeys),
            UnorderedElementsAre("0.000000 mtidle: start", "0.000000 mtidle: loop start", "2.000000 mtidle: stop",
                "2.000000 mtidle: loop stop"));
    }

    TEST(NifOsgLoadKfControllerSequence, doesNotLoopASequenceThatClamps)
    {
        Nif::NIFFile file(idleKf);
        addSequence(file, "Idle", 1.f, 3.f, Nif::NiTimeController::ExtrapolationMode::Constant, {});

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        EXPECT_THAT(
            textKeys(holder.mTextKeys), UnorderedElementsAre("1.000000 mtidle: start", "3.000000 mtidle: stop"));
    }

    TEST(NifOsgLoadKfControllerSequence, keepsTheTextKeysOfTheSequenceInLowerCase)
    {
        Nif::NIFFile file(idleKf);
        addSequence(file, "Idle", 0.f, 2.f, Nif::NiTimeController::ExtrapolationMode::Constant, {},
            { { 0.5f, "Hit" }, { 1.f, "Sound: Step" } });

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        const std::set<std::string> keys = textKeys(holder.mTextKeys);
        EXPECT_EQ(keys.count("0.500000 hit"), 1u);
        EXPECT_EQ(keys.count("1.000000 sound: step"), 1u);
    }

    TEST(NifOsgLoadKfControllerSequence, makesAControllerOfTheKeysOfEachNode)
    {
        Nif::NIFFile file(idleKf);
        addSequence(file, "Idle", 0.f, 2.f, Nif::NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01 Pelvis", { { 0.f, osg::Vec3f(0, 0, 0) }, { 2.f, osg::Vec3f(0, 0, 10) } } },
                { "Bip01 Head", { { 0.f, osg::Vec3f(1, 0, 0) }, { 1.f, osg::Vec3f(3, 0, 0) } } } });

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        ASSERT_EQ(holder.mKeyframeControllers.size(), 2u);
        const auto pelvis = holder.mKeyframeControllers.find("Bip01 Pelvis");
        const auto head = holder.mKeyframeControllers.find("Bip01 Head");
        ASSERT_NE(pelvis, holder.mKeyframeControllers.end());
        ASSERT_NE(head, holder.mKeyframeControllers.end());
        EXPECT_EQ(pelvis->second->getTranslation(1.f), osg::Vec3f(0, 0, 5));
        EXPECT_EQ(head->second->getTranslation(0.5f), osg::Vec3f(2, 0, 0));
    }

    TEST(NifOsgLoadKfControllerSequence, keepsTheFirstControllerOfANodeThatTheSequenceDrivesTwice)
    {
        Nif::NIFFile file(idleKf);
        addSequence(file, "Idle", 0.f, 1.f, Nif::NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01 Head", { { 0.f, osg::Vec3f(1, 0, 0) } } }, { "Bip01 Head", { { 0.f, osg::Vec3f(7, 0, 0) } } } });

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        ASSERT_EQ(holder.mKeyframeControllers.size(), 1u);
        EXPECT_EQ(holder.mKeyframeControllers.begin()->second->getTranslation(0.f), osg::Vec3f(1, 0, 0));
    }

    // The controllers are kept by the name of the node for the whole file, so a second sequence would play its group
    // with the tracks of the first.
    TEST(NifOsgLoadKfControllerSequence, takesOnlyTheFirstSequenceOfAFileWithSeveral)
    {
        Nif::NIFFile file(idleKf);
        addSequence(file, "Walk", 0.f, 1.f, Nif::NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01 Head", { { 0.f, osg::Vec3f(1, 0, 0) } } } });
        addSequence(file, "Run", 0.f, 1.f, Nif::NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01 Head", { { 0.f, osg::Vec3f(7, 0, 0) } } } });

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        EXPECT_THAT(holder.mTextKeys.getGroups(), ElementsAre("mtidle"));
        ASSERT_EQ(holder.mKeyframeControllers.size(), 1u);
        EXPECT_EQ(holder.mKeyframeControllers.begin()->second->getTranslation(0.f), osg::Vec3f(1, 0, 0));
    }

    // The engine cannot play a sequence forward and back, a pose held after one pass is further from it than a loop.
    TEST(NifOsgLoadKfControllerSequence, loopsASequenceThatReverses)
    {
        Nif::NIFFile file(idleKf);
        addSequence(file, "Idle", 0.f, 2.f, Nif::NiTimeController::ExtrapolationMode::Reverse, {});

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        EXPECT_THAT(textKeys(holder.mTextKeys),
            UnorderedElementsAre("0.000000 mtidle: start", "0.000000 mtidle: loop start", "2.000000 mtidle: stop",
                "2.000000 mtidle: loop stop"));
    }

    // A channel that an interpolator has no keys for holds one value, which is the pose of the bone
    TEST(NifOsgLoadKfControllerSequence, appliesTheValuesOfAnInterpolatorThatHasNoKeys)
    {
        Nif::NIFFile file(idleKf);
        const Nif::NiQuatTransform pose{ osg::Vec3f(1, 2, 3), osg::Quat(0.5, osg::Vec3f(0, 0, 1)), 2.f };
        addSequence(
            file, "Idle", 0.f, 1.f, Nif::NiTimeController::ExtrapolationMode::Constant, { { "Bip01 Head", {}, pose } });

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        const auto transform = transformAtStart(*holder.mKeyframeControllers.at("Bip01 Head"));
        ASSERT_TRUE(transform.mTranslation.has_value());
        ASSERT_TRUE(transform.mRotation.has_value());
        ASSERT_TRUE(transform.mScale.has_value());
        EXPECT_EQ(*transform.mTranslation, osg::Vec3f(1, 2, 3));
        EXPECT_EQ(*transform.mRotation, osg::Quat(0.5, osg::Vec3f(0, 0, 1)));
        EXPECT_EQ(*transform.mScale, 2.f);
    }

    // The files mark a channel that has nothing set with the smallest float
    TEST(NifOsgLoadKfControllerSequence, leavesAChannelAloneThatTheFileMarksAsNotSet)
    {
        Nif::NIFFile file(idleKf);
        const Nif::NiQuatTransform pose{ osg::Vec3f(notSet, notSet, notSet), osg::Quat(notSet, 0, 0, 0), notSet };
        addSequence(file, "Idle", 0.f, 1.f, Nif::NiTimeController::ExtrapolationMode::Constant,
            { { "Bip01 Head", {}, pose }, { "Bip01 Neck", { { 0.f, osg::Vec3f(4, 0, 0) } }, pose } });

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        const auto head = transformAtStart(*holder.mKeyframeControllers.at("Bip01 Head"));
        EXPECT_FALSE(head.mTranslation.has_value());
        EXPECT_FALSE(head.mRotation.has_value());
        EXPECT_FALSE(head.mScale.has_value());
        // the keys of the channel still count
        const auto neck = transformAtStart(*holder.mKeyframeControllers.at("Bip01 Neck"));
        ASSERT_TRUE(neck.mTranslation.has_value());
        EXPECT_EQ(*neck.mTranslation, osg::Vec3f(4, 0, 0));
        EXPECT_FALSE(neck.mRotation.has_value());
    }

    // Most of the bones of the animation files of the games are driven by the control points of a B-spline
    TEST(NifOsgLoadKfControllerSequence, makesAControllerOfTheCurvesOfABSplineInterpolator)
    {
        Nif::NIFFile file(idleKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 2.f, Nif::NiTimeController::ExtrapolationMode::Cycle, {});
        SplineTrack track;
        track.mNode = "Bip01 Head";
        track.mStop = 2.f;
        track.mCompact = false;
        track.mNumControlPoints = 2;
        track.mFloatPoints = { 0.f, 0.f, 0.f, 0.f, 0.f, 10.f };
        track.mTranslationHandle = 0;
        addSplineBlock(file, sequence, track);

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        ASSERT_EQ(holder.mKeyframeControllers.size(), 1u);
        const auto& head = *holder.mKeyframeControllers.at("Bip01 Head");
        EXPECT_EQ(head.getTranslation(1.f), osg::Vec3f(0, 0, 5));
        EXPECT_EQ(head.getTranslation(2.f), osg::Vec3f(0, 0, 10));
    }

    // The pose that an interpolator holds is that of the channels that have no control points
    TEST(NifOsgLoadKfControllerSequence, appliesTheHeldValuesToTheChannelsOfABSplineWithoutPoints)
    {
        Nif::NIFFile file(idleKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, Nif::NiTimeController::ExtrapolationMode::Constant, {});
        SplineTrack track;
        track.mNode = "Bip01 Head";
        track.mCompact = false;
        track.mNumControlPoints = 2;
        track.mFloatPoints = { 4.f, 5.f, 6.f, 7.f, 8.f, 9.f };
        track.mTranslationHandle = 0;
        track.mValue = { osg::Vec3f(1, 2, 3), osg::Quat(0.5, osg::Vec3f(0, 0, 1)), 2.f };
        addSplineBlock(file, sequence, track);

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        const auto transform = transformAtStart(*holder.mKeyframeControllers.at("Bip01 Head"));
        ASSERT_TRUE(transform.mTranslation.has_value());
        ASSERT_TRUE(transform.mRotation.has_value());
        ASSERT_TRUE(transform.mScale.has_value());
        EXPECT_EQ(*transform.mTranslation, osg::Vec3f(4, 5, 6));
        EXPECT_EQ(*transform.mRotation, osg::Quat(0.5, osg::Vec3f(0, 0, 1)));
        EXPECT_EQ(*transform.mScale, 2.f);
    }

    TEST(NifOsgLoadKfControllerSequence, leavesAChannelOfABSplineAloneThatTheFileMarksAsNotSet)
    {
        Nif::NIFFile file(idleKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, Nif::NiTimeController::ExtrapolationMode::Constant, {});
        SplineTrack track;
        track.mNode = "Bip01 Head";
        track.mCompact = false;
        track.mNumControlPoints = 2;
        track.mFloatPoints = { 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f };
        track.mRotationHandle = 0;
        track.mValue = { osg::Vec3f(notSet, notSet, notSet), osg::Quat(notSet, 0, 0, 0), notSet };
        addSplineBlock(file, sequence, track);

        SceneUtil::KeyframeHolder holder;
        NifOsg::Loader::loadKf(file, holder);

        const auto transform = transformAtStart(*holder.mKeyframeControllers.at("Bip01 Head"));
        EXPECT_FALSE(transform.mTranslation.has_value());
        EXPECT_TRUE(transform.mRotation.has_value());
        EXPECT_FALSE(transform.mScale.has_value());
    }
}
