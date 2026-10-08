#include "../nif/sequence.hpp"

#include <components/nifosg/nifloader.hpp>
#include <components/sceneutil/keyframe.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <osg/Vec3f>

#include <set>
#include <string>

namespace
{
    using namespace testing;
    using namespace Nif::Testing;

    constexpr VFS::Path::NormalizedView idleKf("meshes/characters/_male/mtidle.kf");

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
}
