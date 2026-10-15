#include <components/falloutscript/vats.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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

    TEST(FalloutScriptVatsTest, aBlowOfTheHandsReachesOnlyAShortWay)
    {
        const Vats::Rules rules;
        EXPECT_GT(Vats::hitChance(rules, fists(), 80, 250, 100, false), 0.0);
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, fists(), 80, 301, 100, false), 0.0);
    }

    TEST(FalloutScriptVatsTest, aShotChancesFallWithTheDistance)
    {
        const Vats::Rules rules;
        const double near = Vats::hitChance(rules, rifle(), 60, 300, 75, false);
        const double far = Vats::hitChance(rules, rifle(), 60, 3000, 75, false);
        EXPECT_GT(near, far);
        EXPECT_GT(far, 0.0);
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, rifle(), 60, 5001, 75, false), 0.0);
    }

    TEST(FalloutScriptVatsTest, aBetterSkillAndABiggerPartMakeAHitLikelier)
    {
        const Vats::Rules rules;
        EXPECT_GT(
            Vats::hitChance(rules, rifle(), 60, 1500, 90, false), Vats::hitChance(rules, rifle(), 60, 1500, 20, false));
        EXPECT_GT(
            Vats::hitChance(rules, rifle(), 90, 1500, 50, false), Vats::hitChance(rules, rifle(), 30, 1500, 50, false));
    }

    TEST(FalloutScriptVatsTest, noChanceIsAbovetheMostTheFilesAllow)
    {
        const Vats::Rules rules;
        EXPECT_DOUBLE_EQ(Vats::hitChance(rules, rifle(), 100, 1, 100, false), 95.0);
        EXPECT_LE(Vats::hitChance(rules, fists(), 100, 10, 100, false), 95.0);
    }

    TEST(FalloutScriptVatsTest, aTargetThatDoesNotSeeTheAttackerIsHarderToHit)
    {
        const Vats::Rules rules;
        const double seen = Vats::hitChance(rules, rifle(), 50, 1000, 50, false);
        EXPECT_NEAR(Vats::hitChance(rules, rifle(), 50, 1000, 50, true), seen * 0.5, 1e-9);
    }

    TEST(FalloutScriptVatsTest, actionPointsComeBackAtTheRateOfTheFiles)
    {
        Vats::Rules rules;
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 80, 0.5), 2.0);
        rules.mRestoreShare = 0.06;
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 100, 2.0), 12.0);
        EXPECT_DOUBLE_EQ(Vats::restored(rules, 100, -1.0), 0.0);
    }
}
