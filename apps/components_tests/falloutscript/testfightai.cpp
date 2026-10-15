#include <components/falloutscript/fightai.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace FalloutScript::FightAI;

    TEST(FalloutScriptFightAiTest, an_unaggressive_character_never_attacks_on_sight)
    {
        for (const Standing standing : { Standing::Neutral, Standing::Enemy, Standing::Ally, Standing::Friend })
            EXPECT_FALSE(attacksOnSight(Aggression::unaggressive, standing));
    }

    TEST(FalloutScriptFightAiTest, the_more_aggressive_attacks_more_of_those_it_notices)
    {
        EXPECT_TRUE(attacksOnSight(Aggression::aggressive, Standing::Enemy));
        EXPECT_FALSE(attacksOnSight(Aggression::aggressive, Standing::Neutral));
        EXPECT_FALSE(attacksOnSight(Aggression::aggressive, Standing::Ally));

        EXPECT_TRUE(attacksOnSight(Aggression::veryAggressive, Standing::Enemy));
        EXPECT_TRUE(attacksOnSight(Aggression::veryAggressive, Standing::Neutral));
        EXPECT_FALSE(attacksOnSight(Aggression::veryAggressive, Standing::Ally));
        EXPECT_FALSE(attacksOnSight(Aggression::veryAggressive, Standing::Friend));

        for (const Standing standing : { Standing::Neutral, Standing::Enemy, Standing::Ally, Standing::Friend })
            EXPECT_TRUE(attacksOnSight(Aggression::frenzied, standing));
        // a value no game has
        EXPECT_FALSE(attacksOnSight(9, Standing::Enemy));
    }

    TEST(FalloutScriptFightAiTest, a_coward_runs_at_once_and_a_foolhardy_character_never)
    {
        EXPECT_TRUE(runsAway(Confidence::cowardly, 1.0));
        EXPECT_FALSE(runsAway(Confidence::foolhardy, 0.01));
        EXPECT_FALSE(runsAway(Confidence::average, 0.5));
        EXPECT_TRUE(runsAway(Confidence::average, 0.2));
        EXPECT_TRUE(runsAway(Confidence::cautious, 0.4));
        EXPECT_FALSE(runsAway(Confidence::brave, 0.2));
        EXPECT_TRUE(runsAway(Confidence::brave, 0.05));
    }

    TEST(FalloutScriptFightAiTest, a_character_attacks_from_a_distance_that_fits_the_weapon)
    {
        EXPECT_DOUBLE_EQ(engageDistance(true, 130), 91);
        EXPECT_DOUBLE_EQ(engageDistance(true, 10), 50);
        // a gun: half the range, but not from nearer than a short weapon reaches, nor from beyond what it can aim at
        EXPECT_DOUBLE_EQ(engageDistance(false, 1000), 500);
        EXPECT_DOUBLE_EQ(engageDistance(false, 200), 200);
        EXPECT_DOUBLE_EQ(engageDistance(false, 10000), 1500);
    }

    TEST(FalloutScriptFightAiTest, the_move_follows_the_distance_to_the_target)
    {
        EXPECT_EQ(decide(500, 300, true, false), Move::Approach);
        EXPECT_EQ(decide(250, 300, true, false), Move::Hold);
        // a target it does not see is gone for, whatever the distance
        EXPECT_EQ(decide(100, 300, false, false), Move::Approach);
        // a character that runs goes on until it is far enough
        EXPECT_EQ(decide(100, 300, true, true), Move::Retreat);
        EXPECT_EQ(decide(fleeDistance + 1, 300, true, true), Move::Hold);
    }

    TEST(FalloutScriptFightAiTest, the_weapon_to_hold_is_the_one_that_does_most_that_can_be_used)
    {
        std::vector<WeaponCandidate> weapons;
        EXPECT_EQ(bestWeapon(weapons), 0u);

        weapons.push_back({ 0x10, 8, true, -1, 1 }); // a knife
        weapons.push_back({ 0x11, 20, false, 0, 1 }); // a rifle without rounds
        EXPECT_EQ(bestWeapon(weapons), 0x10u);

        weapons[1].mRounds = 12;
        EXPECT_EQ(bestWeapon(weapons), 0x11u);

        // a shot uses more rounds than the character has
        weapons[1].mAmmoUse = 20;
        EXPECT_EQ(bestWeapon(weapons), 0x10u);

        // a weapon that does nothing is no weapon; the hands are used then
        weapons = { { 0x12, 0, true, -1, 1 } };
        EXPECT_EQ(bestWeapon(weapons), 0u);
    }

    TEST(FalloutScriptFightAiTest, the_chance_to_hit_follows_the_skill_between_limits)
    {
        EXPECT_DOUBLE_EQ(hitChance(0), 0.3);
        EXPECT_DOUBLE_EQ(hitChance(60), 0.6);
        EXPECT_DOUBLE_EQ(hitChance(200), 0.95);
    }
}
