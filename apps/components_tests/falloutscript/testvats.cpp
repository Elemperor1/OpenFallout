#include <components/falloutscript/vats.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <string>

namespace
{
    using namespace testing;
    namespace Vats = FalloutScript::Vats;

    Vats::Weapon rifle()
    {
        Vats::Weapon weapon;
        weapon.mAnimationType = 5;
        weapon.mSpread = 1.0;
        weapon.mRange = 5000;
        return weapon;
    }

    Vats::Weapon fists()
    {
        return Vats::Weapon();
    }

    TEST(FalloutScriptVatsTest, theSettingsOfTheFilesChangeTheRules)
    {
        const std::map<std::string, double> settings{ { "fVATSMaxChance", 90 }, { "fActionPointsAttackRifle", 31 },
            { "fActionPointsRestoreRate", 6 } };
        const Vats::Rules rules = Vats::rules([&settings](std::string_view name) -> std::optional<double> {
            const auto found = settings.find(std::string(name));
            if (found == settings.end())
                return std::nullopt;
            return found->second;
        });
        EXPECT_DOUBLE_EQ(rules.mMaxChance, 90);
        EXPECT_DOUBLE_EQ(rules.mRifle, 31);
        EXPECT_DOUBLE_EQ(rules.mRestoreRate, 6);
        // What the files do not have stays as it was
        EXPECT_DOUBLE_EQ(rules.mPistol, 20);
        EXPECT_DOUBLE_EQ(Vats::rules(nullptr).mMeleeMaxDistance, 300);
    }

    TEST(FalloutScriptVatsTest, anAttackCostsByTheKindOfWeapon)
    {
        const Vats::Rules rules;
        Vats::Weapon weapon;
        const auto cost = [&](std::uint32_t type) {
            weapon.mAnimationType = type;
            return Vats::attackCost(rules, weapon);
        };
        EXPECT_DOUBLE_EQ(cost(0), 22);
        EXPECT_DOUBLE_EQ(cost(1), 15);
        EXPECT_DOUBLE_EQ(cost(2), 30);
        EXPECT_DOUBLE_EQ(cost(3), 20);
        EXPECT_DOUBLE_EQ(cost(4), 20);
        EXPECT_DOUBLE_EQ(cost(5), 25);
        EXPECT_DOUBLE_EQ(cost(7), 25);
        EXPECT_DOUBLE_EQ(cost(8), 20);
        EXPECT_DOUBLE_EQ(cost(9), 40);
        EXPECT_DOUBLE_EQ(cost(10), 35);
        EXPECT_DOUBLE_EQ(cost(12), 10);
    }

    TEST(FalloutScriptVatsTest, theWeaponCanNameTheCostItself)
    {
        const Vats::Rules rules;
        Vats::Weapon weapon = rifle();
        weapon.mActionPoints = 12;
        EXPECT_DOUBLE_EQ(Vats::attackCost(rules, weapon), 12);
        weapon.mActionPoints = 0;
        EXPECT_DOUBLE_EQ(Vats::attackCost(rules, weapon), 0);
    }

    Vats::Shot shotAt(double distance, double partToHit = 60)
    {
        Vats::Shot shot;
        shot.distance = distance;
        shot.partToHit = partToHit;
        return shot;
    }

    Vats::Weapon grenade()
    {
        Vats::Weapon weapon;
        weapon.mAnimationType = 10;
        weapon.mRange = 1000;
        return weapon;
    }

    TEST(FalloutScriptVatsTest, aBlowOfTheHandsIsSureWithinItsReachAndNeverBeyond)
    {
        const Vats::Rules rules;
        // The part and the skill do not count: the most the files allow
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, fists(), shotAt(250, 10)), 95.0);
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, fists(), shotAt(300, 80)), 95.0);
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, fists(), shotAt(301, 80)), 0.0);
    }

    TEST(FalloutScriptVatsTest, aShotHasTheShareOfTheSpreadThatFallsOnTheTargetTimesTheChanceOfThePart)
    {
        const Vats::Rules rules;
        // 1 degree at 1500 units is a circle of 26 units; the target (128 wide) is hit by 1.037 of its area, and a
        // torso is 60 percent of that
        EXPECT_NEAR(Vats::hitChance(rules, rifle(), shotAt(1500, 60)), 62.245, 1e-2);
        EXPECT_NEAR(Vats::hitChance(rules, rifle(), shotAt(1500, 30)), 31.123, 1e-2);
    }

    TEST(FalloutScriptVatsTest, aShotChancesFallWithTheDistance)
    {
        const Vats::Rules rules;
        const double near = Vats::hitChance(rules, rifle(), shotAt(1000));
        const double far = Vats::hitChance(rules, rifle(), shotAt(6000));
        EXPECT_GT(near, far);
        EXPECT_GT(far, 0.0);
    }

    TEST(FalloutScriptVatsTest, theRangeOfAWeaponOfFixedRangeIsALimitAndOtherRangesAreNot)
    {
        const Vats::Rules rules;
        Vats::Weapon weapon = rifle();
        EXPECT_GT(Vats::hitChance(rules, weapon, shotAt(5001)), 0.0);
        weapon.mRangeFixed = true;
        EXPECT_GT(Vats::hitChance(rules, weapon, shotAt(5000)), 0.0);
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, weapon, shotAt(5001)), 0.0);
    }

    TEST(FalloutScriptVatsTest, theReachOfAWeaponIsTheDistanceWhereItsChanceEnds)
    {
        const Vats::Rules rules;
        EXPECT_DOUBLE_EQ(Vats::reach(rules, fists()), rules.mMeleeMaxDistance);
        EXPECT_DOUBLE_EQ(Vats::reach(rules, grenade()), grenade().mRange * rules.mThrownRangeMult);
        // A shot of a weapon that does not fix its range can be aimed at a target beyond it
        Vats::Weapon weapon = rifle();
        EXPECT_TRUE(std::isinf(Vats::reach(rules, weapon)));
        EXPECT_GT(Vats::hitChance(rules, weapon, shotAt(weapon.mRange + 1)), 0.0);
        weapon.mRangeFixed = true;
        EXPECT_DOUBLE_EQ(Vats::reach(rules, weapon), weapon.mRange);
        // At the reach there is a chance, beyond it none, for every kind of weapon
        for (const Vats::Weapon& kind : { fists(), grenade(), weapon })
        {
            const double reach = Vats::reach(rules, kind);
            EXPECT_GT(Vats::hitChance(rules, kind, shotAt(reach - 1)), 0.0);
            EXPECT_DOUBLE_EQ(Vats::hitChance(rules, kind, shotAt(reach + 1)), 0.0);
        }
    }

    TEST(FalloutScriptVatsTest, aWiderSpreadAndASmallerTargetMakeAHitLessLikely)
    {
        const Vats::Rules rules;
        Vats::Shot shot = shotAt(2000);
        const double base = Vats::hitChance(rules, rifle(), shot);
        shot.extraSpread = 1.5;
        EXPECT_LT(Vats::hitChance(rules, rifle(), shot), base);
        shot.extraSpread = 0;
        shot.bound = 64;
        EXPECT_LT(Vats::hitChance(rules, rifle(), shot), base);
        // A target with no size counts as one of 128
        shot.bound = 0;
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, rifle(), shot), Vats::hitChance(rules, rifle(), shotAt(2000)));
    }

    TEST(FalloutScriptVatsTest, theSkillOfAShooterActsThroughTheSpreadOnly)
    {
        const Vats::Rules rules;
        Vats::Shot low = shotAt(2000);
        low.skill = 10;
        Vats::Shot high = shotAt(2000);
        high.skill = 90;
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, rifle(), low), Vats::hitChance(rules, rifle(), high));
        const Vats::Weapon gun = rifle();
        EXPECT_GT(Vats::spreadDegrees(rules, Vats::spreadFactor(rules, gun, 1, 0, 0, 10)),
            Vats::spreadDegrees(rules, Vats::spreadFactor(rules, gun, 1, 0, 0, 90)));
    }

    TEST(FalloutScriptVatsTest, noChanceIsAbovetheMostTheFilesAllow)
    {
        const Vats::Rules rules;
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, rifle(), shotAt(10, 100)), 95.0);
        Vats::Rules lower;
        lower.mMaxChance = 80;
        EXPECT_DOUBLE_EQ(Vats::hitChance(lower, rifle(), shotAt(10, 100)), 80.0);
    }

    TEST(FalloutScriptVatsTest, aTargetThatHidesIsHarderToHit)
    {
        const Vats::Rules rules;
        Vats::Shot shot = shotAt(1500);
        const double seen = Vats::hitChance(rules, rifle(), shot);
        shot.hidden = true;
        EXPECT_NEAR(Vats::hitChance(rules, rifle(), shot), seen * 0.5, 1e-9);
        // The blows of the hands are still sure but are halved too, and the most counts after it
        shot.distance = 200;
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, fists(), shot), 50.0);
    }

    TEST(FalloutScriptVatsTest, fallout3CountsTheUncertaintyAndTheFactorOfDistance)
    {
        Vats::Rules rules;
        rules.mNewVegas = false;
        // (1 + 0.2) degrees at 1500 units, with the area of the spread as the radius to the power of 2: 51.87
        EXPECT_NEAR(Vats::hitChance(rules, rifle(), shotAt(1500, 60)), 51.869, 1e-2);
        rules.mDistanceFactor = 1.5;
        EXPECT_NE(Vats::hitChance(rules, rifle(), shotAt(1500, 60)), 51.869);
    }

    TEST(FalloutScriptVatsTest, theChanceOfAThrownWeaponFallsWithTheRangeAndRisesWithTheSkill)
    {
        const Vats::Rules rules;
        Vats::Shot shot = shotAt(500);
        shot.skill = 50;
        // Near, the most the files allow
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, grenade(), shot), 95.0);
        // 1 - (1500 - 128) / (2000 - 128) of the range, and 0.4 x the skill (0.2)
        shot.distance = 1500;
        EXPECT_NEAR(Vats::hitChance(rules, grenade(), shot), 46.709, 1e-2);
        shot.distance = 1800;
        shot.skill = 0;
        const double low = Vats::hitChance(rules, grenade(), shot);
        shot.skill = 100;
        EXPECT_GT(Vats::hitChance(rules, grenade(), shot), low);
        shot.distance = 2001;
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, grenade(), shot), 0.0);
    }

    TEST(FalloutScriptVatsTest, theSpreadOfTheAimGrowsWithCrippledArmsAndWhatTheWeaponWantsMoreOf)
    {
        const Vats::Rules rules;
        const Vats::Weapon gun = rifle();
        const double steady = Vats::spreadFactor(rules, gun, 0, 0, 0, 0);
        EXPECT_DOUBLE_EQ(steady, rules.mMinSpread);
        const double oneArm = Vats::spreadFactor(rules, gun, 1, 0, 0, 0);
        const double twoArms = Vats::spreadFactor(rules, gun, 2, 0, 0, 0);
        EXPECT_DOUBLE_EQ(oneArm, 0.4);
        EXPECT_DOUBLE_EQ(twoArms, 0.6);
        // A weapon of one hand has smaller penalties
        Vats::Weapon pistol;
        pistol.mAnimationType = 3;
        EXPECT_DOUBLE_EQ(Vats::spreadFactor(rules, pistol, 1, 0, 0, 0), 0.2);
        EXPECT_DOUBLE_EQ(Vats::spreadFactor(rules, pistol, 2, 0, 0, 0), 0.4);
        // 3 points of strength lacking at 0.1 each, and a skill of 100 halves the sum
        EXPECT_NEAR(Vats::spreadFactor(rules, gun, 0, 3, 0, 100), 0.15, 1e-9);
        // More than 10 points are not counted
        EXPECT_DOUBLE_EQ(Vats::spreadFactor(rules, gun, 0, 50, 0, 0), 1.0);
        EXPECT_DOUBLE_EQ(Vats::spreadDegrees(rules, 0.2), 1.0);
    }

    TEST(FalloutScriptVatsTest, actionPointsComeBackAtTheRateOfTheFiles)
    {
        Vats::Rules rules;
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 80, 0.5), 2.0);
        rules.mRestoreShare = 0.06;
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 100, 2.0), 12.0);
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 100, -1.0), 0.0);
    }

    TEST(FalloutScriptVatsTest, thePerksOfNewVegasChangeTheShareOfActionPointsThatComesBack)
    {
        const auto twice = [](double share) { return share * 2.0; };
        Vats::Rules rules;
        // Fallout 3 gives back points a second and has no perk for it
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 80, 0.5, twice), 2.0);
        rules.mRestoreShare = 0.06;
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 100, 2.0, twice), 24.0);
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 50, 1.0, twice), 6.0);
        // Perks that take it all away leave nothing, and never a loss
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 100, 2.0, [](double share) { return share - 1.0; }), 0.0);
    }

    TEST(FalloutScriptVatsTest, thePerksRaiseTheChanceOfAHitAndNeverLowerIt)
    {
        const auto quarterMore = [](double chance) { return chance * 1.25; };
        EXPECT_DOUBLE_EQ(Vats::adjustedChance(60.0, quarterMore), 75.0);
        // The executables cap the chance before the perks and not after: 95 with a quarter more is 118.75
        EXPECT_DOUBLE_EQ(Vats::adjustedChance(95.0, quarterMore), 118.75);
        // A perk that lowers the chance (Fast Shot) is dropped, the larger of the two is kept
        EXPECT_DOUBLE_EQ(Vats::adjustedChance(40.0, [](double chance) { return chance * 0.8; }), 40.0);
        EXPECT_DOUBLE_EQ(Vats::adjustedChance(40.0, [](double chance) { return chance - 1.0; }), 40.0);
        // No chance (the target is out of reach) stays none, whatever the perks add
        EXPECT_DOUBLE_EQ(Vats::adjustedChance(0.0, [](double chance) { return chance + 0.5; }), 0.0);
        EXPECT_DOUBLE_EQ(Vats::adjustedChance(40.0, nullptr), 40.0);
    }
}
