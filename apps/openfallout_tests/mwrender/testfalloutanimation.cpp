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
            "meshes/characters/_male/locomotion/a/b/mtforward.kf", "meshes/characters/_male/locomotion/mtbackward.kf" };
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(mFolder, files, false);
        EXPECT_EQ(locomotion.mWalk, "");
        EXPECT_EQ(locomotion.mRun, "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, takesTheUsualFilesBeforeTheOtherNamesOfACreature)
    {
        const std::string folder = "meshes/creatures/dog/";
        const std::vector<std::string> files = { "meshes/creatures/dog/locomotion/forward.kf",
            "meshes/creatures/dog/locomotion/h2hforward.kf", "meshes/creatures/dog/locomotion/mtforward.kf",
            "meshes/creatures/dog/locomotion/fastforward.kf", "meshes/creatures/dog/locomotion/mtfastforward.kf" };
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(folder, files, false);
        EXPECT_EQ(locomotion.mWalk, "meshes/creatures/dog/locomotion/mtforward.kf");
        EXPECT_EQ(locomotion.mRun, "meshes/creatures/dog/locomotion/mtfastforward.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, knowsTheNamesCreaturesHaveForTheirMovements)
    {
        const std::string folder = "meshes/creatures/dog/";
        // the dog
        FalloutLocomotion locomotion = chooseFalloutLocomotion(folder,
            { "meshes/creatures/dog/locomotion/h2hforward.kf", "meshes/creatures/dog/locomotion/h2hfastforward.kf",
                "meshes/creatures/dog/locomotion/mtbackward.kf" },
            false);
        EXPECT_EQ(locomotion.mWalk, "meshes/creatures/dog/locomotion/h2hforward.kf");
        EXPECT_EQ(locomotion.mRun, "meshes/creatures/dog/locomotion/h2hfastforward.kf");

        // a creature of the downloadable content
        locomotion = chooseFalloutLocomotion(folder,
            { "meshes/creatures/dog/locomotion/forwardwalk.kf", "meshes/creatures/dog/locomotion/fastforward.kf",
                "meshes/creatures/dog/locomotion/forward.kf", "meshes/creatures/dog/locomotion/backward.kf" },
            false);
        EXPECT_EQ(locomotion.mWalk, "meshes/creatures/dog/locomotion/forwardwalk.kf");
        EXPECT_EQ(locomotion.mRun, "meshes/creatures/dog/locomotion/fastforward.kf");

        // the misspelling in the files of the deathclaw
        locomotion = chooseFalloutLocomotion(folder,
            { "meshes/creatures/dog/locomotion/mtfoward.kf", "meshes/creatures/dog/locomotion/mtfastforward.kf" },
            false);
        EXPECT_EQ(locomotion.mWalk, "meshes/creatures/dog/locomotion/mtfoward.kf");
        EXPECT_EQ(locomotion.mRun, "meshes/creatures/dog/locomotion/mtfastforward.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, takesAnyFileThatGoesForwardWhenNoKnownNameIsThere)
    {
        const std::string folder = "meshes/creatures/x/";
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(folder,
            { "meshes/creatures/x/locomotion/mtbackward.kf", "meshes/creatures/x/locomotion/1hmfastforward.kf",
                "meshes/creatures/x/locomotion/1hmforward.kf", "meshes/creatures/x/locomotion/mtturnleft.kf" },
            false);
        EXPECT_EQ(locomotion.mWalk, "meshes/creatures/x/locomotion/1hmforward.kf");
        EXPECT_EQ(locomotion.mRun, "meshes/creatures/x/locomotion/1hmfastforward.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutLocomotion, doesNotWalkWithARunOrRunWithAWalk)
    {
        const std::string folder = "meshes/creatures/x/";
        const FalloutLocomotion locomotion
            = chooseFalloutLocomotion(folder, { "meshes/creatures/x/locomotion/runforward.kf" }, false);
        EXPECT_EQ(locomotion.mWalk, "");
        EXPECT_EQ(locomotion.mRun, "");
        EXPECT_EQ(chooseFalloutLocomotion(folder, { "meshes/creatures/x/locomotion/xforward.kf" }, false).mRun, "");
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

    TEST(OpenFalloutRenderChooseFalloutPose, isTheLoopOfThePoseInTheIdleAnimationsFolder)
    {
        const std::string folder = "meshes/characters/_male/";
        const std::vector<std::string> files = { folder + "idleanims/dynamicidle_sit.kf",
            folder + "idleanims/dynamicidle_sleep.kf", folder + "idleanims/dynamicidle_sitstool.kf",
            folder + "idleanims/chair_forwardenter.kf", folder + "locomotion/mtforward.kf" };
        // A chair on any of its sides, and the other seats
        for (const int kind : { 10, 11, 12, 13, 14, 16, 17, 20 })
            EXPECT_EQ(chooseFalloutPose(folder, kind, files), folder + "idleanims/dynamicidle_sit.kf") << kind;
        EXPECT_EQ(chooseFalloutPose(folder, 15, files), folder + "idleanims/dynamicidle_sitstool.kf");
        // A bed
        EXPECT_EQ(chooseFalloutPose(folder, 1, files), folder + "idleanims/dynamicidle_sleep.kf");
        EXPECT_EQ(chooseFalloutPose(folder, 2, files), folder + "idleanims/dynamicidle_sleep.kf");
        // The files that this folder does not have, and the kinds without a pose
        EXPECT_EQ(chooseFalloutPose(folder, 3, files), "");
        EXPECT_EQ(chooseFalloutPose(folder, 19, files), "");
        EXPECT_EQ(chooseFalloutPose(folder, 22, files), "");
        EXPECT_EQ(chooseFalloutPose(folder, 0, files), "");
        EXPECT_EQ(chooseFalloutPose("", 14, files), "");
        // The files of another folder are not used
        EXPECT_EQ(chooseFalloutPose("meshes/creatures/robobrain/", 14, files), "");
    }

    struct OpenFalloutRenderChooseFalloutWeaponAnimation : Test
    {
        const std::string mFolder = "meshes/characters/_male/";
        std::vector<std::string> files(std::initializer_list<std::string> names) const
        {
            std::vector<std::string> result;
            for (const std::string& name : names)
                result.push_back(mFolder + name + ".kf");
            return result;
        }
    };

    TEST(OpenFalloutRenderFalloutWeaponPrefix, isTheLettersOfTheAnimationType)
    {
        EXPECT_EQ(falloutWeaponPrefix(0), "h2h");
        EXPECT_EQ(falloutWeaponPrefix(3), "1hp");
        EXPECT_EQ(falloutWeaponPrefix(4), "2hr");
        EXPECT_EQ(falloutWeaponPrefix(10), "1lm");
        EXPECT_EQ(falloutWeaponPrefix(-1), "h2h");
        EXPECT_EQ(falloutWeaponPrefix(11), "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutWeaponAnimation, aGunAttacksWithAttack3AndFallsBackToTheNextOfTheNumbered)
    {
        EXPECT_EQ(chooseFalloutWeaponAnimation(
                      mFolder, 3, FalloutWeaponMove::Attack, files({ "1hpattack3", "1hpattackleft", "2hrattack3" })),
            "meshes/characters/_male/1hpattack3.kf");
        EXPECT_EQ(
            chooseFalloutWeaponAnimation(mFolder, 3, FalloutWeaponMove::Attack, files({ "1hpattack5", "1hpattack4" })),
            "meshes/characters/_male/1hpattack4.kf");
        EXPECT_EQ(chooseFalloutWeaponAnimation(mFolder, 4, FalloutWeaponMove::Attack, files({ "1hpattack3" })), "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutWeaponAnimation, aMeleeBlowAlternatesBetweenLeftAndRight)
    {
        const auto known = files({ "h2hattackleft_a", "h2hattackright_a", "h2hattackleft_b", "h2hattackpower" });
        const auto blow = [&](unsigned counter) {
            return chooseFalloutWeaponAnimation(mFolder, 0, FalloutWeaponMove::Attack, known, counter);
        };
        EXPECT_EQ(blow(0), "meshes/characters/_male/h2hattackleft_a.kf");
        EXPECT_EQ(blow(1), "meshes/characters/_male/h2hattackright_a.kf");
        EXPECT_EQ(blow(2), "meshes/characters/_male/h2hattackleft_b.kf");
        EXPECT_EQ(blow(3), "meshes/characters/_male/h2hattackleft_a.kf");
        // A character with no weapon uses the hands
        EXPECT_EQ(chooseFalloutWeaponAnimation(mFolder, -1, FalloutWeaponMove::Attack, known, 1),
            "meshes/characters/_male/h2hattackright_a.kf");
        // A weapon with only the plain files
        EXPECT_EQ(chooseFalloutWeaponAnimation(
                      mFolder, 1, FalloutWeaponMove::Attack, files({ "1hmattackright", "1hmattackleft" }), 0),
            "meshes/characters/_male/1hmattackleft.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutWeaponAnimation, theOtherMovesAreNamedForTheWeaponAndTheMove)
    {
        const auto known = files({ "1hpequip", "1hpunequip", "1hpaim", "h2hrecoil", "1hpholster" });
        const auto move = [&](int type, FalloutWeaponMove which) {
            return chooseFalloutWeaponAnimation(mFolder, type, which, known);
        };
        EXPECT_EQ(move(3, FalloutWeaponMove::Equip), "meshes/characters/_male/1hpequip.kf");
        EXPECT_EQ(move(3, FalloutWeaponMove::Unequip), "meshes/characters/_male/1hpunequip.kf");
        EXPECT_EQ(move(3, FalloutWeaponMove::Aim), "meshes/characters/_male/1hpaim.kf");
        EXPECT_EQ(move(3, FalloutWeaponMove::Recoil), "");
        EXPECT_EQ(move(0, FalloutWeaponMove::Recoil), "meshes/characters/_male/h2hrecoil.kf");
        EXPECT_EQ(move(4, FalloutWeaponMove::Equip), "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutWeaponAnimation, throwingAndMinesHaveTheirOwnFiles)
    {
        const auto known = files({ "1gtattackthrow", "1gtattackthrow2", "1mdplacemine" });
        EXPECT_EQ(chooseFalloutWeaponAnimation(mFolder, 8, FalloutWeaponMove::Attack, known),
            "meshes/characters/_male/1gtattackthrow.kf");
        EXPECT_EQ(chooseFalloutWeaponAnimation(mFolder, 9, FalloutWeaponMove::Attack, known),
            "meshes/characters/_male/1mdplacemine.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutWeaponAnimation, filesOfOtherFoldersAndNothingAreNoAnimation)
    {
        EXPECT_EQ(chooseFalloutWeaponAnimation(
                      mFolder, 3, FalloutWeaponMove::Attack, { "meshes/characters/_male/idleanims/1hpattack3.kf" }),
            "");
        EXPECT_EQ(chooseFalloutWeaponAnimation(mFolder, 3, FalloutWeaponMove::Attack, {}), "");
        EXPECT_EQ(chooseFalloutWeaponAnimation("", 3, FalloutWeaponMove::Attack, files({ "1hpattack3" })), "");
        EXPECT_EQ(chooseFalloutWeaponAnimation(mFolder, 40, FalloutWeaponMove::Attack, files({ "1hpattack3" })), "");
    }
}
