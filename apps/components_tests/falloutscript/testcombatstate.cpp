#include <components/falloutscript/combatstate.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using FalloutScript::CombatState;
    using FalloutScript::FormId;

    constexpr FormId guard = 0x01000100;
    constexpr FormId raider = 0x01000200;
    constexpr FormId ghoul = 0x01000300;
    constexpr FormId vault = 0x01001000;
    constexpr FormId gang = 0x01001001;

    TEST(FalloutScriptCombatStateTest, an_actor_fights_one_target_at_a_time)
    {
        CombatState state;
        EXPECT_FALSE(state.inCombat(guard));
        EXPECT_EQ(state.target(guard), 0u);

        state.start(guard, raider);
        EXPECT_TRUE(state.inCombat(guard));
        EXPECT_EQ(state.target(guard), raider);
        EXPECT_TRUE(state.isFought(raider));
        EXPECT_FALSE(state.isFought(guard));

        state.start(guard, ghoul);
        EXPECT_EQ(state.target(guard), ghoul);
        EXPECT_FALSE(state.isFought(raider));

        state.stop(guard);
        EXPECT_FALSE(state.inCombat(guard));
        EXPECT_FALSE(state.isFought(ghoul));
    }

    TEST(FalloutScriptCombatStateTest, no_one_fights_itself_or_nothing)
    {
        CombatState state;
        state.start(guard, guard);
        state.start(guard, 0);
        state.start(0, raider);
        EXPECT_FALSE(state.inCombat(guard));
        EXPECT_FALSE(state.isFought(raider));
        EXPECT_THAT(state.takeChanges(), IsEmpty());
    }

    TEST(FalloutScriptCombatStateTest, those_who_fight_a_target_are_told_in_the_order_of_their_forms)
    {
        CombatState state;
        state.start(raider, guard);
        state.start(ghoul, guard);
        state.start(guard, raider);
        EXPECT_THAT(state.attackersOf(guard), ElementsAre(raider, ghoul));
        EXPECT_THAT(state.attackersOf(raider), ElementsAre(guard));
        EXPECT_THAT(state.attackersOf(ghoul), IsEmpty());
    }

    TEST(FalloutScriptCombatStateTest, the_beginning_and_the_end_of_a_fight_are_told_once)
    {
        CombatState state;
        state.start(guard, raider);
        state.start(guard, raider); // no change
        state.start(guard, ghoul);
        state.stop(guard);
        state.stop(guard); // nothing to stop

        const std::vector<CombatState::Change> changes = state.takeChanges();
        ASSERT_EQ(changes.size(), 3u);
        EXPECT_EQ(changes[0].mActor, guard);
        EXPECT_EQ(changes[0].mTarget, raider);
        EXPECT_EQ(changes[0].mWas, 0u);
        EXPECT_EQ(changes[1].mTarget, ghoul);
        EXPECT_EQ(changes[1].mWas, raider);
        EXPECT_EQ(changes[2].mTarget, 0u);
        EXPECT_EQ(changes[2].mWas, ghoul);
        EXPECT_THAT(state.takeChanges(), IsEmpty());
    }

    TEST(FalloutScriptCombatStateTest, alert_unconscious_and_restrained_are_kept_for_each_actor)
    {
        CombatState state;
        state.setAlert(guard, true);
        state.setUnconscious(raider, true);
        state.setRestrained(ghoul, true);
        EXPECT_TRUE(state.alert(guard));
        EXPECT_FALSE(state.alert(raider));
        EXPECT_TRUE(state.unconscious(raider));
        EXPECT_FALSE(state.unconscious(guard));
        EXPECT_TRUE(state.restrained(ghoul));

        state.setAlert(guard, false);
        state.setUnconscious(raider, false);
        state.setRestrained(ghoul, false);
        EXPECT_FALSE(state.alert(guard));
        EXPECT_FALSE(state.unconscious(raider));
        EXPECT_FALSE(state.restrained(ghoul));
    }

    TEST(FalloutScriptCombatStateTest, a_script_can_make_an_actor_essential_or_not)
    {
        CombatState state;
        EXPECT_FALSE(state.essential(guard).has_value());
        state.setEssential(guard, true);
        ASSERT_TRUE(state.essential(guard).has_value());
        EXPECT_TRUE(*state.essential(guard));
        state.setEssential(guard, false);
        ASSERT_TRUE(state.essential(guard).has_value());
        EXPECT_FALSE(*state.essential(guard));
    }

    TEST(FalloutScriptCombatStateTest, how_a_faction_stands_to_another_is_its_own_say_in_each_direction)
    {
        CombatState state;
        EXPECT_FALSE(state.relation(vault, gang).has_value());
        state.setRelation(vault, gang, CombatState::Relation::Enemy);
        EXPECT_EQ(state.relation(vault, gang), CombatState::Relation::Enemy);
        EXPECT_FALSE(state.relation(gang, vault).has_value());

        state.setRelation(gang, vault, CombatState::Relation::Friend);
        EXPECT_EQ(state.relation(gang, vault), CombatState::Relation::Friend);
        EXPECT_EQ(state.relation(vault, gang), CombatState::Relation::Enemy);

        state.setRelation(vault, 0, CombatState::Relation::Enemy);
        EXPECT_FALSE(state.relation(vault, 0).has_value());
    }

    TEST(FalloutScriptCombatStateTest, withdrawing_an_actor_ends_its_fight_and_those_against_it)
    {
        CombatState state;
        state.start(guard, raider);
        state.start(ghoul, guard);
        state.start(raider, guard);
        state.takeChanges();

        state.withdraw(guard);

        EXPECT_FALSE(state.inCombat(guard));
        EXPECT_FALSE(state.inCombat(ghoul));
        EXPECT_FALSE(state.inCombat(raider));
        EXPECT_FALSE(state.isFought(guard));
        // Each of the three fights that ended is told once (OnCombatEnd)
        EXPECT_THAT(state.takeChanges(), SizeIs(3));
        state.withdraw(guard);
        EXPECT_THAT(state.takeChanges(), IsEmpty());
    }

    TEST(FalloutScriptCombatStateTest, an_unconscious_actor_gets_up_by_itself_when_the_time_has_passed)
    {
        CombatState state;
        state.setUnconscious(guard, true, 15.f);
        state.setUnconscious(raider, true);

        state.advance(10.f);
        EXPECT_TRUE(state.unconscious(guard));
        EXPECT_THAT(state.takeWoken(), IsEmpty());
        state.advance(6.f);
        EXPECT_FALSE(state.unconscious(guard));
        EXPECT_THAT(state.takeWoken(), ElementsAre(guard));
        // Told once, and an actor with no time stays where it is
        EXPECT_THAT(state.takeWoken(), IsEmpty());
        state.advance(100.f);
        EXPECT_TRUE(state.unconscious(raider));
        EXPECT_THAT(state.takeWoken(), IsEmpty());
    }

    TEST(FalloutScriptCombatStateTest, waking_an_actor_early_cancels_the_time_it_had)
    {
        CombatState state;
        state.setUnconscious(guard, true, 15.f);
        state.setUnconscious(guard, false);
        // Put out again by a script, with no time: the old time does not get it up
        state.setUnconscious(guard, true);
        state.advance(20.f);

        EXPECT_TRUE(state.unconscious(guard));
        EXPECT_THAT(state.takeWoken(), IsEmpty());

        // Put out again without a time while it has one: the time it has stays
        state.setUnconscious(raider, true, 5.f);
        state.setUnconscious(raider, true);
        state.advance(6.f);
        EXPECT_THAT(state.takeWoken(), ElementsAre(raider));
    }

    TEST(FalloutScriptCombatStateTest, a_hit_actor_counts_as_attacked_for_a_short_time)
    {
        CombatState state;
        CombatState::Hit hit;
        hit.mAttacker = raider;
        hit.mDamage = 12;
        hit.mCritical = true;
        hit.mLimb = 3;
        state.noteHit(guard, hit);

        EXPECT_TRUE(state.attacked(guard));
        EXPECT_FALSE(state.attacked(raider));
        state.advance(1.f);
        EXPECT_TRUE(state.attacked(guard));
        state.advance(1.5f);
        EXPECT_FALSE(state.attacked(guard));

        // The blow is the last one it took, whenever that was
        CombatState::Hit last;
        ASSERT_TRUE(state.lastHit(guard, last));
        EXPECT_EQ(last.mAttacker, raider);
        EXPECT_DOUBLE_EQ(last.mDamage, 12);
        EXPECT_TRUE(last.mCritical);
        EXPECT_EQ(last.mLimb, 3);
        EXPECT_FALSE(state.lastHit(raider, last));
    }

    TEST(FalloutScriptCombatStateTest, the_blows_are_told_once_in_the_order_they_were_struck)
    {
        CombatState state;
        CombatState::Hit first;
        first.mAttacker = raider;
        first.mDamage = 4;
        CombatState::Hit second;
        second.mAttacker = ghoul;
        second.mDamage = 9;
        state.noteHit(guard, first);
        state.noteHit(raider, second);

        const std::vector<CombatState::HitEvent> hits = state.takeHits();
        ASSERT_EQ(hits.size(), 2u);
        EXPECT_EQ(hits[0].mVictim, guard);
        EXPECT_EQ(hits[0].mHit.mAttacker, raider);
        EXPECT_EQ(hits[1].mVictim, raider);
        EXPECT_DOUBLE_EQ(hits[1].mHit.mDamage, 9);
        EXPECT_THAT(state.takeHits(), IsEmpty());
    }

    TEST(FalloutScriptCombatStateTest, clearing_forgets_everything)
    {
        CombatState state;
        state.start(guard, raider);
        state.setAlert(guard, true);
        state.setUnconscious(guard, true);
        state.setRestrained(guard, true);
        state.setEssential(guard, true);
        state.setRelation(vault, gang, CombatState::Relation::Enemy);
        state.noteHit(guard, {});

        state.clear();

        EXPECT_FALSE(state.inCombat(guard));
        EXPECT_FALSE(state.alert(guard));
        EXPECT_FALSE(state.unconscious(guard));
        EXPECT_FALSE(state.restrained(guard));
        EXPECT_FALSE(state.essential(guard).has_value());
        EXPECT_FALSE(state.relation(vault, gang).has_value());
        EXPECT_FALSE(state.attacked(guard));
        CombatState::Hit hit;
        EXPECT_FALSE(state.lastHit(guard, hit));
        EXPECT_THAT(state.takeChanges(), IsEmpty());
        EXPECT_THAT(state.takeHits(), IsEmpty());
    }
}
