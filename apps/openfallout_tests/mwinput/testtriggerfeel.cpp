#include "apps/openfallout/mwinput/triggerfeel.hpp"

#include <gtest/gtest.h>

namespace OFInput::TriggerFeel
{
    namespace
    {
        // The animation types of the weapon records (ESM4::Weapon::GameData::AnimationType)
        constexpr std::uint32_t handToHand = 0;
        constexpr std::uint32_t melee2Hand = 2;
        constexpr std::uint32_t pistolBallistic = 3;
        constexpr std::uint32_t rifleBallistic = 5;
        constexpr std::uint32_t rifleAutomatic = 6;
        constexpr std::uint32_t handle2Hand = 8;
        constexpr std::uint32_t launcher2Hand = 9;
        constexpr std::uint32_t grenadeThrown = 10;

        Weapon weapon(std::uint32_t animationType, bool automatic = false)
        {
            Weapon result;
            result.mAnimationType = animationType;
            result.mAutomatic = automatic;
            return result;
        }

        TEST(OFInputTriggerFeelTest, theHandsAMeleeWeaponAndAGrenadeHaveNoResistance)
        {
            EXPECT_FALSE(forWeapon(weapon(handToHand)).mResistance);
            EXPECT_FALSE(forWeapon(weapon(melee2Hand)).mResistance);
            EXPECT_FALSE(forWeapon(weapon(grenadeThrown)).mResistance);
        }

        TEST(OFInputTriggerFeelTest, aSemiAutomaticGunHasATriggerThatGivesWayBeforeTheEndOfThePull)
        {
            const std::optional<Resistance> pistol = forWeapon(weapon(pistolBallistic)).mResistance;
            ASSERT_TRUE(pistol);
            EXPECT_GT(pistol->mStart, 0.f);
            EXPECT_LT(pistol->mEnd, 1.f);
            EXPECT_GT(pistol->mStrength, 0.f);
        }

        TEST(OFInputTriggerFeelTest, aRifleIsHeavierThanAPistolAndALauncherHeaviestOfAll)
        {
            const float pistol = forWeapon(weapon(pistolBallistic)).mResistance->mStrength;
            const float rifle = forWeapon(weapon(rifleBallistic)).mResistance->mStrength;
            const float launcher = forWeapon(weapon(launcher2Hand)).mResistance->mStrength;
            EXPECT_LT(pistol, rifle);
            EXPECT_LT(rifle, launcher);
            EXPECT_FLOAT_EQ(launcher, 1.f);
        }

        TEST(OFInputTriggerFeelTest, anAutomaticWeaponResistsLightlyToTheEndOfThePullSoItCanBeHeld)
        {
            for (const Weapon& automatic :
                { weapon(rifleAutomatic), weapon(pistolBallistic, true), weapon(handle2Hand) })
            {
                const std::optional<Resistance> resistance = forWeapon(automatic).mResistance;
                ASSERT_TRUE(resistance);
                EXPECT_FLOAT_EQ(resistance->mEnd, 1.f);
                EXPECT_LE(resistance->mStrength, 0.5f);
            }
        }

        TEST(OFInputTriggerFeelTest, aShotBuzzesWithTheRumbleOfTheRecord)
        {
            Weapon rifle = weapon(rifleBallistic);
            rifle.mRumbleLeft = 0.25f;
            rifle.mRumbleRight = 0.75f;
            rifle.mRumbleSeconds = 0.2f;
            const Pulse shot = forWeapon(rifle).mShot;
            EXPECT_FLOAT_EQ(shot.mStrength, 0.75f);
            EXPECT_FLOAT_EQ(shot.mSeconds, 0.2f);
        }

        TEST(OFInputTriggerFeelTest, aShotOfARecordWithoutRumbleStillBuzzesBriefly)
        {
            for (const std::uint32_t type : { handToHand, pistolBallistic, launcher2Hand })
            {
                const Pulse shot = forWeapon(weapon(type)).mShot;
                EXPECT_GT(shot.mStrength, 0.f) << type;
                EXPECT_GT(shot.mSeconds, 0.f) << type;
                EXPECT_LE(shot.mSeconds, 0.5f) << type;
            }
        }

        TEST(OFInputTriggerFeelTest, theRumbleOfARecordIsKeptWithinWhatAPadCanDo)
        {
            Weapon wild = weapon(pistolBallistic);
            wild.mRumbleRight = 4.f;
            wild.mRumbleSeconds = 30.f;
            const Pulse shot = forWeapon(wild).mShot;
            EXPECT_FLOAT_EQ(shot.mStrength, 1.f);
            EXPECT_FLOAT_EQ(shot.mSeconds, 0.5f);
        }

        constexpr Resistance pistol{ 0.3f, 0.6f, 0.6f };
        constexpr Resistance rifle{ 0.35f, 0.7f, 0.8f };

        Situation inPlay()
        {
            Situation situation;
            situation.mWeaponDrawn = true;
            return situation;
        }

        TEST(OFInputTriggerPlannerTest, aDrawnWeaponInPlaySetsItsResistanceOnce)
        {
            Planner planner;
            const Command first = planner.update(inPlay(), pistol);
            EXPECT_EQ(first.mKind, Command::Kind::Set);
            EXPECT_EQ(first.mResistance, pistol);
            EXPECT_EQ(planner.update(inPlay(), pistol).mKind, Command::Kind::None);
        }

        TEST(OFInputTriggerPlannerTest, anotherWeaponSetsItsOwnResistance)
        {
            Planner planner;
            planner.update(inPlay(), pistol);
            const Command changed = planner.update(inPlay(), rifle);
            EXPECT_EQ(changed.mKind, Command::Kind::Set);
            EXPECT_EQ(changed.mResistance, rifle);
        }

        TEST(OFInputTriggerPlannerTest, aMenuTheHolsterOrALostFocusClearsTheResistance)
        {
            Situation menu = inPlay();
            menu.mMenu = true;
            Situation holstered = inPlay();
            holstered.mWeaponDrawn = false;
            Situation unfocused = inPlay();
            unfocused.mFocused = false;
            Situation disabled = inPlay();
            disabled.mEnabled = false;
            for (const Situation& situation : { menu, holstered, unfocused, disabled })
            {
                Planner planner;
                planner.update(inPlay(), pistol);
                EXPECT_EQ(planner.update(situation, pistol).mKind, Command::Kind::Clear);
                EXPECT_EQ(planner.update(situation, pistol).mKind, Command::Kind::None);
                // and back in play the weapon resists again
                EXPECT_EQ(planner.update(inPlay(), pistol).mKind, Command::Kind::Set);
            }
        }

        TEST(OFInputTriggerPlannerTest, aWeaponWithoutResistanceClearsTheTrigger)
        {
            Planner planner;
            planner.update(inPlay(), pistol);
            EXPECT_EQ(planner.update(inPlay(), std::nullopt).mKind, Command::Kind::Clear);
        }

        TEST(OFInputTriggerPlannerTest, aNewPadIsClearedFirstWhenThereIsNothingToSet)
        {
            // A pad that another game (or this one before a crash) left resisting is cleared once
            Planner planner;
            Situation holstered = inPlay();
            holstered.mWeaponDrawn = false;
            EXPECT_EQ(planner.update(holstered, pistol).mKind, Command::Kind::Clear);
            EXPECT_EQ(planner.update(holstered, pistol).mKind, Command::Kind::None);
        }

        TEST(OFInputTriggerPlannerTest, aPadThatChangedIsToldAgain)
        {
            Planner planner;
            planner.update(inPlay(), pistol);
            planner.forget();
            EXPECT_EQ(planner.update(inPlay(), pistol).mKind, Command::Kind::Set);
        }

        TEST(OFInputTriggerPlannerTest, aShotBuzzesOnlyInPlayWithTheWeaponOut)
        {
            EXPECT_TRUE(Planner::pulseAllowed(inPlay()));
            Situation menu = inPlay();
            menu.mMenu = true;
            Situation holstered = inPlay();
            holstered.mWeaponDrawn = false;
            Situation unfocused = inPlay();
            unfocused.mFocused = false;
            Situation disabled = inPlay();
            disabled.mEnabled = false;
            for (const Situation& situation : { menu, holstered, unfocused, disabled })
                EXPECT_FALSE(Planner::pulseAllowed(situation));
        }

        // The bytes of SDL's DS5EffectsState_t: the enable bits at 0 (0x04 the right trigger, 0x08 the left), the
        // effect of the right trigger at 10 and of the left one at 21 (a mode and its parameters, as SDL's own
        // testgamecontroller sends them: 0x05 off, 0x01 resistance from a position with a force, 0x02 resistance in a
        // section with a force)
        TEST(OFInputDualSenseEffectTest, aSectionOfResistanceOnTheRightTrigger)
        {
            const auto report = dualSenseEffect(Trigger::Right, Resistance{ 0.2f, 0.6f, 1.f });
            EXPECT_EQ(report.size(), 47u);
            EXPECT_EQ(report[0], 0x04);
            EXPECT_EQ(report[10], 0x02);
            EXPECT_EQ(report[11], 51);
            EXPECT_EQ(report[12], 153);
            EXPECT_EQ(report[13], 255);
            EXPECT_EQ(report[21], 0); // the left trigger is not touched
        }

        TEST(OFInputDualSenseEffectTest, aResistanceToTheEndOfThePullOnTheLeftTrigger)
        {
            const auto report = dualSenseEffect(Trigger::Left, Resistance{ 0.f, 1.f, 0.5f });
            EXPECT_EQ(report[0], 0x08);
            EXPECT_EQ(report[21], 0x01);
            EXPECT_EQ(report[22], 0);
            EXPECT_EQ(report[23], 128);
            EXPECT_EQ(report[10], 0); // the right trigger is not touched
        }

        TEST(OFInputDualSenseEffectTest, noResistanceTurnsTheEffectOff)
        {
            for (const std::optional<Resistance>& none :
                { std::optional<Resistance>(), std::optional<Resistance>(Resistance{ 0.2f, 0.6f, 0.f }) })
            {
                const auto report = dualSenseEffect(Trigger::Right, none);
                EXPECT_EQ(report[0], 0x04);
                EXPECT_EQ(report[10], 0x05);
                EXPECT_EQ(report[11], 0);
            }
        }

        TEST(OFInputDualSenseEffectTest, onlyTheTriggerBitIsSetSoTheRumbleAndLightsAreLeftAlone)
        {
            const auto report = dualSenseEffect(Trigger::Right, Resistance{ 0.2f, 0.6f, 1.f });
            EXPECT_EQ(report[1], 0);
            for (std::size_t i = 2; i < 10; ++i)
                EXPECT_EQ(report[i], 0) << i;
            for (std::size_t i = 32; i < report.size(); ++i)
                EXPECT_EQ(report[i], 0) << i;
        }
    }
}
