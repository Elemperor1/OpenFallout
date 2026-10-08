#include <apps/openfallout/mwrender/falloutanimation.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace OFRender;

    TEST(OpenFalloutRenderFalloutAnimationFolder, isTheFolderOfTheSkeleton)
    {
        EXPECT_EQ(falloutAnimationFolder("meshes/characters/_male/skeleton.nif"), "meshes/characters/_male/");
        EXPECT_EQ(falloutAnimationFolder("Meshes\\Characters\\_Male\\Skeleton.NIF"), "meshes/characters/_male/");
        EXPECT_EQ(falloutAnimationFolder("skeleton.nif"), "");
    }

    struct OpenFalloutRenderChooseFalloutIdle : Test
    {
        const std::string mFolder = "meshes/characters/_male/";
    };

    TEST_F(OpenFalloutRenderChooseFalloutIdle, takesMtidleBeforeIdle)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/idle.kf", "meshes/characters/_male/mtidle.kf",
            "meshes/characters/_male/h2hidle.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "meshes/characters/_male/mtidle.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, takesIdleBeforeH2hidle)
    {
        const std::vector<std::string> files
            = { "meshes/characters/_male/h2hidle.kf", "meshes/characters/_male/idle.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "meshes/characters/_male/idle.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, takesTheFirstIdleByNameWhenNoneIsPreferred)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/2hridle.kf",
            "meshes/characters/_male/1hpidle.kf", "meshes/characters/_male/forward.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "meshes/characters/_male/1hpidle.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, ignoresFoldersBelowAndOtherFolders)
    {
        const std::vector<std::string> files
            = { "meshes/characters/_male/idleanims/mtidle.kf", "meshes/characters/_female/mtidle.kf",
                  "meshes/characters/_male/idle.nif", "meshes/characters/_male/forward.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, hasNothingWithNoFiles)
    {
        EXPECT_EQ(chooseFalloutIdle(mFolder, {}), "");
    }

    struct OpenFalloutRenderChooseFalloutLocomotion : Test
    {
        const std::string mFolder = "meshes/characters/_male/";
    };

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, takesTheWalkAndTheRunOfTheLocomotionFolder)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/locomotion/mtfastforward.kf",
            "meshes/characters/_male/locomotion/mtforward.kf", "meshes/characters/_male/locomotion/mtbackward.kf",
            "meshes/characters/_male/mtidle.kf" };
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(mFolder, files, false);
        EXPECT_EQ(locomotion.mWalk, "meshes/characters/_male/locomotion/mtforward.kf");
        EXPECT_EQ(locomotion.mRun, "meshes/characters/_male/locomotion/mtfastforward.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, prefersTheFolderOfTheSexOfTheCharacter)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/locomotion/mtforward.kf",
            "meshes/characters/_male/locomotion/female/mtforward.kf",
            "meshes/characters/_male/locomotion/male/mtforward.kf",
            "meshes/characters/_male/locomotion/male/mtfastforward.kf" };
        EXPECT_EQ(chooseFalloutLocomotion(mFolder, files, false).mWalk,
            "meshes/characters/_male/locomotion/male/mtforward.kf");
        EXPECT_EQ(chooseFalloutLocomotion(mFolder, files, true).mWalk,
            "meshes/characters/_male/locomotion/female/mtforward.kf");
        // a run is taken from where there is one, the sex of the character aside
        EXPECT_EQ(chooseFalloutLocomotion(mFolder, files, true).mRun,
            "meshes/characters/_male/locomotion/male/mtfastforward.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, takesTheFolderItselfBeforeAnotherSexFolder)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/locomotion/male/mtforward.kf",
            "meshes/characters/_male/locomotion/mtforward.kf" };
        EXPECT_EQ(
            chooseFalloutLocomotion(mFolder, files, true).mWalk, "meshes/characters/_male/locomotion/mtforward.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, neverTakesTheMovementsOfChildrenAndTheHurt)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/locomotion/child/mtforward.kf",
            "meshes/characters/_male/locomotion/hurt/mtforward.kf",
            "meshes/characters/_male/locomotion/hurt/mtfastforward.kf" };
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(mFolder, files, false);
        EXPECT_EQ(locomotion.mWalk, "");
        EXPECT_EQ(locomotion.mRun, "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, ignoresOtherFoldersAndFiles)
    {
        const std::vector<std::string> files = { "meshes/characters/_female/locomotion/mtforward.kf",
            "meshes/characters/_male/mtforward.kf", "meshes/characters/_male/locomotion/mtforward.nif",
            "meshes/characters/_male/locomotion/a/b/mtforward.kf", "meshes/characters/_male/locomotion/mtforwardx.kf" };
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(mFolder, files, false);
        EXPECT_EQ(locomotion.mWalk, "");
        EXPECT_EQ(locomotion.mRun, "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, hasNothingWithNoFiles)
    {
        EXPECT_EQ(chooseFalloutLocomotion(mFolder, {}, false).mWalk, "");
    }

    struct OpenFalloutRenderChooseFalloutGait : Test
    {
        // a walk of 130 units a second and a run of 330, so the two look alike at 230
        const FalloutGaits mBoth{ true, true, 130.f, 330.f };
    };

    TEST_F(OpenFalloutRenderChooseFalloutGait, standsStillBelowTheSpeedToStart)
    {
        EXPECT_EQ(chooseFalloutGait(0.f, FalloutGait::Idle, mBoth), FalloutGait::Idle);
        EXPECT_EQ(chooseFalloutGait(9.f, FalloutGait::Idle, mBoth), FalloutGait::Idle);
        EXPECT_EQ(chooseFalloutGait(11.f, FalloutGait::Idle, mBoth), FalloutGait::Walk);
    }

    TEST_F(OpenFalloutRenderChooseFalloutGait, keepsWalkingAtASpeedBelowTheOneToStart)
    {
        EXPECT_EQ(chooseFalloutGait(7.f, FalloutGait::Walk, mBoth), FalloutGait::Walk);
        EXPECT_EQ(chooseFalloutGait(4.f, FalloutGait::Walk, mBoth), FalloutGait::Idle);
    }

    TEST_F(OpenFalloutRenderChooseFalloutGait, runsAboveTheSpeedHalfWayBetweenTheAnimations)
    {
        EXPECT_EQ(chooseFalloutGait(130.f, FalloutGait::Walk, mBoth), FalloutGait::Walk);
        EXPECT_EQ(chooseFalloutGait(240.f, FalloutGait::Walk, mBoth), FalloutGait::Walk);
        EXPECT_EQ(chooseFalloutGait(260.f, FalloutGait::Walk, mBoth), FalloutGait::Run);
        EXPECT_EQ(chooseFalloutGait(330.f, FalloutGait::Idle, mBoth), FalloutGait::Run);
    }

    TEST_F(OpenFalloutRenderChooseFalloutGait, keepsRunningAtASpeedJustBelowTheOneToStart)
    {
        EXPECT_EQ(chooseFalloutGait(215.f, FalloutGait::Run, mBoth), FalloutGait::Run);
        EXPECT_EQ(chooseFalloutGait(200.f, FalloutGait::Run, mBoth), FalloutGait::Walk);
    }

    TEST_F(OpenFalloutRenderChooseFalloutGait, usesAFixedSpeedForARunWhenTheAnimationsDoNotSayHowFastTheyTravel)
    {
        const FalloutGaits unknown{ true, true, 0.f, 0.f };
        EXPECT_EQ(chooseFalloutGait(200.f, FalloutGait::Walk, unknown), FalloutGait::Walk);
        EXPECT_EQ(chooseFalloutGait(300.f, FalloutGait::Walk, unknown), FalloutGait::Run);
    }

    TEST_F(OpenFalloutRenderChooseFalloutGait, movesWithTheOneAnimationThatThereIs)
    {
        EXPECT_EQ(
            chooseFalloutGait(500.f, FalloutGait::Idle, FalloutGaits{ true, false, 130.f, 0.f }), FalloutGait::Walk);
        EXPECT_EQ(
            chooseFalloutGait(20.f, FalloutGait::Idle, FalloutGaits{ false, true, 0.f, 330.f }), FalloutGait::Run);
    }

    TEST_F(OpenFalloutRenderChooseFalloutGait, staysIdleWithoutAnimationsToMoveWith)
    {
        EXPECT_EQ(chooseFalloutGait(500.f, FalloutGait::Idle, FalloutGaits{}), FalloutGait::Idle);
    }

    TEST(OpenFalloutRenderFalloutAnimationSpeed, isTheRatioOfTheSpeedToTheVelocityOfTheAnimation)
    {
        EXPECT_FLOAT_EQ(falloutAnimationSpeed(130.f, 130.f), 1.f);
        EXPECT_FLOAT_EQ(falloutAnimationSpeed(65.f, 130.f), 0.5f);
        EXPECT_FLOAT_EQ(falloutAnimationSpeed(195.f, 130.f), 1.5f);
    }

    TEST(OpenFalloutRenderFalloutAnimationSpeed, staysWithinWhatLooksRight)
    {
        EXPECT_FLOAT_EQ(falloutAnimationSpeed(1.f, 130.f), 0.25f);
        EXPECT_FLOAT_EQ(falloutAnimationSpeed(5000.f, 130.f), 4.f);
    }

    TEST(OpenFalloutRenderFalloutAnimationSpeed, isOneForAnAnimationThatDoesNotTravel)
    {
        EXPECT_FLOAT_EQ(falloutAnimationSpeed(130.f, 0.f), 1.f);
        EXPECT_FLOAT_EQ(falloutAnimationSpeed(130.f, 0.5f), 1.f);
    }
}
