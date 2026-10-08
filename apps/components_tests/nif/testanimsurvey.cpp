#include "sequence.hpp"

#include <components/nif/animsurvey.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace
{
    using namespace testing;
    using namespace Nif;
    using namespace Nif::Testing;

    constexpr VFS::Path::NormalizedView kfName("meshes/characters/_male/mtidle.kf");

    TEST(NifAnimSurvey, countsWhatASequenceHolds)
    {
        NIFFile file(kfName);
        addSequence(file, "Idle", 0.f, 2.f, NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01", { { 0.f, osg::Vec3f(0, 0, 0) }, { 2.f, osg::Vec3f(0, 30, 0) } } },
                { "Bip01 Head", { { 0.f, osg::Vec3f(1, 0, 0) } } } },
            { { 0.f, "Start" }, { 2.f, "End" }, { 1.f, "Sound: Step" } }, "Bip01");

        AnimSurvey survey;
        survey.addFile(file, "meshes/characters/_male/mtidle.kf");

        EXPECT_EQ(survey.count("Files", "kf file"), 1u);
        EXPECT_EQ(survey.count("Sequences in a kf file", "01"), 1u);
        EXPECT_EQ(survey.count("Cycle type", "cycle"), 1u);
        EXPECT_EQ(survey.count("Duration", animSurveyTimeAnswer(2.f)), 1u);
        EXPECT_EQ(survey.count("Accumulation root", "bip01"), 1u);
        EXPECT_EQ(survey.count("Controlled blocks in a sequence", animSurveyCountAnswer(2)), 1u);
        EXPECT_EQ(survey.count("Interpolator of a block", "NiTransformInterpolator"), 2u);
        EXPECT_EQ(survey.count("Driven node", "bip01"), 1u);
        EXPECT_EQ(survey.count("Driven node", "bip01 head"), 1u);
        EXPECT_EQ(survey.count("Text keys in a sequence", animSurveyCountAnswer(3)), 1u);
        EXPECT_EQ(survey.count("Text key", "sound: step"), 1u);
        EXPECT_EQ(survey.count("Text key group (before the colon)", "sound"), 1u);
        EXPECT_EQ(survey.count("Text key group (before the colon)", "(no colon)"), 2u);
        EXPECT_EQ(survey.count("Translation key type", "linear"), 2u);
        EXPECT_EQ(survey.count("Rotation key type", "no keys"), 2u);
    }

    TEST(NifAnimSurvey, measuresHowFarTheAccumulationRootTravels)
    {
        NIFFile file(kfName);
        addSequence(file, "Walk", 0.f, 2.f, NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01", { { 0.f, osg::Vec3f(0, 0, 0) }, { 2.f, osg::Vec3f(0, 160, 0) } } } }, {}, "Bip01");

        AnimSurvey survey;
        survey.addFile(file, "meshes/characters/_male/mtforward.kf");

        EXPECT_EQ(survey.count("Travel of the accumulation root in a sequence", "5: 120 to 250"), 1u);
        EXPECT_EQ(survey.count("Axis of that travel", "y"), 1u);
        // 160 units in 2 seconds
        EXPECT_EQ(survey.count("Speed of that travel", "3: 50 to 100"), 1u);
    }

    TEST(NifAnimSurvey, findsNodesThatTheSkeletonOfTheFolderLacks)
    {
        AnimSurvey survey;

        // A skeleton file: more than twenty nodes, one of them Bip01
        NIFFile skeleton(VFS::Path::NormalizedView("meshes/characters/_male/skeleton.nif"));
        for (int i = 0; i < 25; ++i)
        {
            auto& node = addRecord<NiNode>(skeleton, "NiNode", RC_NiNode);
            init(node);
            node.mName = i == 0 ? "Bip01" : "Bone" + std::to_string(i);
        }
        survey.addFile(skeleton, "meshes/characters/_male/skeleton.nif");

        NIFFile good(kfName);
        addSequence(good, "A", 0.f, 1.f, NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01", { { 0.f, osg::Vec3f() } } }, { "Bone3", { { 0.f, osg::Vec3f() } } } });
        survey.addFile(good, "meshes/characters/_male/a.kf");

        NIFFile bad(kfName);
        addSequence(bad, "B", 0.f, 1.f, NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01", { { 0.f, osg::Vec3f() } } }, { "Weapon", { { 0.f, osg::Vec3f() } } } });
        survey.addFile(bad, "meshes/characters/_male/idleanims/b.kf");

        NIFFile lost(kfName);
        addSequence(lost, "C", 0.f, 1.f, NiTimeController::ExtrapolationMode::Cycle,
            { { "Bip01", { { 0.f, osg::Vec3f() } } } });
        survey.addFile(lost, "meshes/creatures/rat/c.kf");

        std::ostringstream out;
        survey.print(out);

        EXPECT_EQ(survey.count("Skeleton files", "named skeleton"), 1u);
        EXPECT_EQ(
            survey.count("Kf files by nodes that the skeleton of their folder lacks", animSurveyCountAnswer(0)), 1u);
        EXPECT_EQ(
            survey.count("Kf files by nodes that the skeleton of their folder lacks", animSurveyCountAnswer(1)), 1u);
        EXPECT_EQ(survey.count("Kf files by nodes that the skeleton of their folder lacks",
                      "no skeleton in the folder or above"),
            1u);
        EXPECT_EQ(survey.count("Driven node that the skeleton of the folder lacks", "weapon"), 1u);
        EXPECT_NE(out.str().find("Driven node that the skeleton of the folder lacks"), std::string::npos);
    }

    TEST(NifAnimSurveyHelpers, foldersAreLowerCaseWithASlash)
    {
        EXPECT_EQ(animSurveyFolder("Meshes\\Characters\\_Male\\mtidle.kf"), "meshes/characters/_male/");
        EXPECT_EQ(animSurveyFolder("mtidle.kf"), "");
    }

    TEST(NifAnimSurveyHelpers, limitsTheAnswersToTheCommonestOnes)
    {
        AnimSurvey survey(1, 2);
        for (int i = 0; i < 3; ++i)
            survey.add("Q", "common", "a");
        for (int i = 0; i < 2; ++i)
            survey.add("Q", "less", "b");
        survey.add("Q", "rare", "c");
        survey.add("Q", "rare too", "d");

        std::ostringstream out;
        survey.print(out);
        EXPECT_NE(out.str().find("common"), std::string::npos);
        EXPECT_NE(out.str().find("less"), std::string::npos);
        EXPECT_EQ(out.str().find("rare too"), std::string::npos);
        EXPECT_NE(out.str().find("2 rarer answers not shown"), std::string::npos);
    }
}
