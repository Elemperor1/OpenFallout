#include "scriptbuilder.hpp"

#include <components/falloutscript/dismember.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::BodyPart;
    using FalloutScript::Cut;
    using FalloutScript::DismemberEntry;
    using FalloutScript::DismemberHit;
    using FalloutScript::DismemberSettings;
    using FalloutScript::FormId;
    namespace Rule = FalloutScript::DismemberRule;
    namespace Flag = FalloutScript::DismemberLimb;

    constexpr FormId raider = 0x01000500;
    constexpr FormId robot = 0x01000501;
    constexpr FormId crate = 0x01000502;

    constexpr std::uint16_t OpGetCauseofDeath = 0x118D;
    constexpr std::uint16_t OpIsLimbGone = 0x118E;
    constexpr std::uint16_t OpSetCriticalStage = 0x1212;
    constexpr std::uint16_t OpIsInCriticalStage = 0x1213;

    constexpr int head = 1;
    constexpr int leftArm = 3;
    constexpr int leftHand = 4;
    constexpr int rightArm = 5;
    constexpr int leftLeg = 7;
    constexpr int rightLeg = 10;

    constexpr int headCondition = 25;
    constexpr int torsoCondition = 26;
    constexpr int leftArmCondition = 27;
    constexpr int rightArmCondition = 28;
    constexpr int leftLegCondition = 29;
    constexpr int rightLegCondition = 30;

    BodyPart part(int type, int condition, int flags, int chance = 0)
    {
        BodyPart result;
        result.mType = type;
        result.mCondition = condition;
        result.mFlags = flags;
        result.mExplodeChance = chance;
        return result;
    }

    /// The body of a person: every limb can be torn off and can burst by the chance of 50 percent (the torso 10)
    std::vector<BodyPart> body()
    {
        const int both = Flag::severable | Flag::explodable;
        return { part(0, torsoCondition, Flag::explodable, 10), part(head, headCondition, both, 50),
            part(leftArm, leftArmCondition, both, 50), part(rightArm, rightArmCondition, both, 50),
            part(leftLeg, leftLegCondition, both, 50), part(rightLeg, rightLegCondition, both, 50) };
    }

    /// The conditions of a body with nothing hurt, and of one with the given limbs crippled
    std::function<double(int)> conditions(std::set<int> crippled = {})
    {
        return [crippled](int value) { return crippled.count(value) != 0 ? 0.0 : 100.0; };
    }

    /// The random numbers come out of the list one after the other (and then 99)
    std::function<int()> numbers(std::vector<int> values)
    {
        auto next = std::make_shared<std::size_t>(0);
        return [values = std::move(values), next]() {
            return *next < values.size() ? values[(*next)++] : 99;
        };
    }

    const std::function<bool(int)> nothingGone = [](int) { return false; };

    class FakeRecords final : public FalloutScript::DismemberRecords
    {
    public:
        std::set<FormId> mActors;
        std::vector<std::pair<FormId, int>> mStages;

        bool dismemberIsActor(FormId reference) override { return mActors.count(reference) != 0; }
        void criticalStageSet(FormId actor, int stage) override { mStages.emplace_back(actor, stage); }
    };

    TEST(FalloutScriptDismemberTest, the_settings_come_from_the_game_settings_and_default_to_the_values_of_the_games)
    {
        const DismemberSettings fallback = DismemberSettings::fromGameSettings(
            [](std::string_view) -> std::optional<double> { return std::nullopt; });
        EXPECT_EQ(fallback.mExplodePartChance, 75);
        EXPECT_EQ(fallback.mDismemberPartChance, 50);
        EXPECT_EQ(fallback.mMessTargetedExplodeBonus, 0);
        EXPECT_EQ(fallback.mMessCrippledExplodeBonus, 25);
        EXPECT_EQ(fallback.mMessIntactExplodeBonus, 25);
        EXPECT_EQ(fallback.mMessIntactDismemberChance, 15);
        EXPECT_EQ(fallback.mMessTorsoExplodeChance, 10);
        EXPECT_EQ(fallback.mPerFrame, 2);
        EXPECT_FALSE(fallback.mGoreOff);

        const std::map<std::string, double> values{ { "iCombatExplodePartChance", 40 },
            { "iCombatDismemberPartChance", 20 }, { "iMessTargetedLimbExplodeBonus", 5 },
            { "iMessCrippledLimbExplodeBonus", 6 }, { "iMessIntactLimbExplodeBonus", 7 },
            { "iMessIntactLimbDismemberChance", 8 }, { "iMessTorsoExplodeChance", 9 } };
        const DismemberSettings set = DismemberSettings::fromGameSettings([&values](std::string_view name) {
            const auto found = values.find(std::string(name));
            return found == values.end() ? std::nullopt : std::optional<double>(found->second);
        });
        EXPECT_EQ(set.mExplodePartChance, 40);
        EXPECT_EQ(set.mDismemberPartChance, 20);
        EXPECT_EQ(set.mMessTargetedExplodeBonus, 5);
        EXPECT_EQ(set.mMessCrippledExplodeBonus, 6);
        EXPECT_EQ(set.mMessIntactExplodeBonus, 7);
        EXPECT_EQ(set.mMessIntactDismemberChance, 8);
        EXPECT_EQ(set.mMessTorsoExplodeChance, 9);
    }

    TEST(FalloutScriptDismemberTest, the_roll_bursts_by_a_share_of_the_part_chance_and_else_tears_by_the_setting)
    {
        const DismemberSettings settings;
        // 75 percent of 60 is 45
        EXPECT_TRUE(Rule::roll(settings, 44, 60, 0, 0, false, 0).mExplode);
        EXPECT_FALSE(Rule::roll(settings, 44, 60, 0, 0, false, 0).mDismember);
        // 45 and above does not burst; below 50 it tears
        const Rule::Roll tears = Rule::roll(settings, 45, 60, 0, 0, false, 0);
        EXPECT_FALSE(tears.mExplode);
        EXPECT_TRUE(tears.mDismember);
        EXPECT_TRUE(Rule::roll(settings, 49, 60, 0, 0, false, 0).mDismember);
        const Rule::Roll none = Rule::roll(settings, 50, 60, 0, 0, false, 0);
        EXPECT_FALSE(none.mExplode);
        EXPECT_FALSE(none.mDismember);
    }

    TEST(FalloutScriptDismemberTest, the_chance_of_a_burst_is_cut_to_a_whole_number)
    {
        const DismemberSettings settings;
        // 75 percent of 15 is 11.25: 11 bursts, 12 does not
        EXPECT_TRUE(Rule::roll(settings, 10, 15, 0, 0, false, 0).mExplode);
        EXPECT_FALSE(Rule::roll(settings, 11, 15, 0, 0, false, 0).mExplode);
        // the bonus counts before the share: (75 + 25) percent of 15 is 15
        EXPECT_TRUE(Rule::roll(settings, 14, 15, 0, 25, false, 0).mExplode);
        EXPECT_FALSE(Rule::roll(settings, 15, 15, 0, 25, false, 0).mExplode);
    }

    TEST(FalloutScriptDismemberTest, an_absolute_chance_is_the_part_chance_and_the_bonus_added)
    {
        const DismemberSettings settings;
        EXPECT_TRUE(Rule::roll(settings, 59, 40, 0, 20, true, 0).mExplode);
        EXPECT_FALSE(Rule::roll(settings, 60, 40, 0, 20, true, 0).mExplode);
        // A part that cannot burst (chance 0) still tears by the setting
        EXPECT_TRUE(Rule::roll(settings, 10, 0, 0, 0, true, 0).mDismember);
        EXPECT_FALSE(Rule::roll(settings, 10, 0, 0, 0, true, 0).mExplode);
    }

    TEST(FalloutScriptDismemberTest, the_bonus_of_tearing_raises_the_limit_of_the_tear)
    {
        const DismemberSettings settings;
        EXPECT_FALSE(Rule::roll(settings, 60, 0, 0, 0, false, 0).mDismember);
        EXPECT_TRUE(Rule::roll(settings, 60, 0, 20, 0, false, 0).mDismember);
        EXPECT_TRUE(Rule::roll(settings, 99, 0, 100, 0, false, 0).mDismember);
    }

    TEST(FalloutScriptDismemberTest, the_setting_of_the_weapon_changes_what_came_out)
    {
        const DismemberSettings settings;
        // a burst (random 0 against a certain chance) torn instead
        const Rule::Roll tornInstead = Rule::roll(settings, 0, 100, 0, 0, true, 1);
        EXPECT_FALSE(tornInstead.mExplode);
        EXPECT_TRUE(tornInstead.mDismember);
        // a tear (random 49 against a chance of 0) burst instead
        const Rule::Roll burstInstead = Rule::roll(settings, 49, 0, 0, 0, false, 2);
        EXPECT_TRUE(burstInstead.mExplode);
        EXPECT_FALSE(burstInstead.mDismember);
        // neither
        const Rule::Roll neither = Rule::roll(settings, 0, 100, 0, 0, true, 3);
        EXPECT_FALSE(neither.mExplode);
        EXPECT_FALSE(neither.mDismember);
        // nothing came out: the setting adds nothing
        const Rule::Roll nothing = Rule::roll(settings, 99, 0, 0, 0, false, 2);
        EXPECT_FALSE(nothing.mExplode);
        EXPECT_FALSE(nothing.mDismember);
        // an unknown setting is as no setting
        EXPECT_TRUE(Rule::roll(settings, 0, 100, 0, 0, true, 0).mExplode);
        EXPECT_TRUE(Rule::roll(settings, 0, 100, 0, 0, true, 9).mExplode);
    }

    TEST(FalloutScriptDismemberTest, without_Bloody_Mess_only_a_crippled_part_that_was_hit_is_tried)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mLimb = leftArm;
        const auto crippledArm = conditions({ leftArmCondition });
        Rule::Plan plan = Rule::plan(settings, body(), hit, crippledArm, numbers({}));
        ASSERT_EQ(plan.mEntries.size(), 1u);
        EXPECT_EQ(plan.mEntries[0].mLimb, leftArm);
        EXPECT_EQ(plan.mEntries[0].mCondition, leftArmCondition);
        EXPECT_EQ(plan.mEntries[0].mDismemberBonus, 0);
        EXPECT_EQ(plan.mEntries[0].mExplodeBonus, 0);
        EXPECT_FALSE(plan.mTorsoExplodes);

        // a part that still has some condition is left alone, and so is a blow at no part
        EXPECT_TRUE(Rule::plan(settings, body(), hit, conditions(), numbers({})).mEntries.empty());
        hit.mLimb = -1;
        EXPECT_TRUE(Rule::plan(settings, body(), hit, crippledArm, numbers({})).mEntries.empty());
        // the part that is not in the body
        hit.mLimb = 12;
        EXPECT_TRUE(Rule::plan(settings, body(), hit, crippledArm, numbers({})).mEntries.empty());
    }

    TEST(FalloutScriptDismemberTest, without_Bloody_Mess_the_torso_is_tried_only_when_its_burst_is_certain)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mLimb = 0;
        const auto crippledTorso = conditions({ torsoCondition });
        EXPECT_TRUE(Rule::plan(settings, body(), hit, crippledTorso, numbers({})).mEntries.empty());
        std::vector<BodyPart> certain = body();
        certain[0].mFlags = Flag::explodable | Flag::absoluteChance;
        certain[0].mExplodeChance = 100;
        const Rule::Plan plan = Rule::plan(settings, certain, hit, crippledTorso, numbers({}));
        ASSERT_EQ(plan.mEntries.size(), 1u);
        EXPECT_EQ(plan.mEntries[0].mLimb, 0);
        // and a certain burst of a torso that is whole is not tried
        EXPECT_TRUE(Rule::plan(settings, certain, hit, conditions(), numbers({})).mEntries.empty());
    }

    TEST(FalloutScriptDismemberTest, without_Bloody_Mess_an_explosion_tries_the_parts_it_reached)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mFromExplosion = true;
        hit.mLimb = head;
        hit.mExplosionLimbs = { leftLeg, rightLeg, 12 };
        const Rule::Plan plan = Rule::plan(settings, body(), hit, conditions(), numbers({}));
        ASSERT_EQ(plan.mEntries.size(), 2u);
        EXPECT_EQ(plan.mEntries[0].mLimb, leftLeg);
        EXPECT_EQ(plan.mEntries[1].mLimb, rightLeg);
        EXPECT_EQ(plan.mEntries[1].mCondition, rightLegCondition);
    }

    TEST(FalloutScriptDismemberTest, with_Bloody_Mess_the_limb_aimed_at_comes_first_with_the_aimed_bonus)
    {
        DismemberSettings settings;
        settings.mMessTargetedExplodeBonus = 7;
        DismemberHit hit;
        hit.mBloodyMess = true;
        hit.mLimb = head;
        // nothing else rolls in: every other limb is whole and the roll of each is 99
        const Rule::Plan plan = Rule::plan(settings, body(), hit, conditions(), numbers({}));
        ASSERT_EQ(plan.mEntries.size(), 1u);
        EXPECT_EQ(plan.mEntries[0].mLimb, head);
        EXPECT_EQ(plan.mEntries[0].mCondition, headCondition);
        EXPECT_EQ(plan.mEntries[0].mDismemberBonus, 100);
        EXPECT_EQ(plan.mEntries[0].mExplodeBonus, 7);
    }

    TEST(FalloutScriptDismemberTest, with_Bloody_Mess_a_crippled_limb_goes_and_a_whole_one_goes_by_the_roll)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mBloodyMess = true;
        hit.mLimb = head;
        // the conditions 25 to 31 are tried in order: the head is the one that was hit, 26 is the torso and is passed
        // over, then the left arm (crippled), the right arm (whole: the roll 15 passes, it is at most 15), the left leg
        // (whole: 16 does not) and the right leg (whole: 0 passes)
        const Rule::Plan plan
            = Rule::plan(settings, body(), hit, conditions({ leftArmCondition }), numbers({ 15, 16, 0 }));
        ASSERT_EQ(plan.mEntries.size(), 4u);
        EXPECT_EQ(plan.mEntries[0].mLimb, head);
        EXPECT_EQ(plan.mEntries[1].mLimb, leftArm);
        EXPECT_EQ(plan.mEntries[1].mExplodeBonus, 25);
        EXPECT_EQ(plan.mEntries[1].mDismemberBonus, 100);
        EXPECT_EQ(plan.mEntries[2].mLimb, rightArm);
        EXPECT_EQ(plan.mEntries[2].mExplodeBonus, 25);
        EXPECT_EQ(plan.mEntries[3].mLimb, rightLeg);
    }

    TEST(FalloutScriptDismemberTest, with_Bloody_Mess_a_blow_that_bursts_the_torso_brings_every_limb_with_it)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mBloodyMess = true;
        hit.mLimb = 0;
        // The torso bursts by the roll (below 10), and the limbs then go whatever the roll gives
        const Rule::Plan plan = Rule::plan(settings, body(), hit, conditions(), numbers({ 9 }));
        EXPECT_TRUE(plan.mTorsoExplodes);
        ASSERT_EQ(plan.mEntries.size(), 6u);
        EXPECT_EQ(plan.mEntries[0].mLimb, head);
        EXPECT_EQ(plan.mEntries.back().mLimb, 0);
        EXPECT_EQ(plan.mEntries.back().mCondition, torsoCondition);
        EXPECT_EQ(plan.mEntries.back().mExplodeBonus, 100);
        // The torso that does not burst leaves the whole limbs to their rolls (99: none)
        const Rule::Plan whole = Rule::plan(settings, body(), hit, conditions(), numbers({ 10 }));
        EXPECT_FALSE(whole.mTorsoExplodes);
        EXPECT_TRUE(whole.mEntries.empty());
    }

    TEST(FalloutScriptDismemberTest, with_Bloody_Mess_a_blow_that_bursts_what_it_hits_makes_every_limb_that_can_burst_certain)
    {
        DismemberSettings settings;
        settings.mMessTargetedExplodeBonus = 0;
        DismemberHit hit;
        hit.mBloodyMess = true;
        hit.mExplodes = true;
        hit.mLimb = rightLeg;
        const Rule::Plan plan = Rule::plan(settings, body(), hit, conditions({ leftLegCondition }), numbers({}));
        // the limb that was hit, then the head, both arms and the left leg (crippled): all with the certain bonus
        ASSERT_EQ(plan.mEntries.size(), 5u);
        EXPECT_EQ(plan.mEntries[0].mLimb, rightLeg);
        for (const DismemberEntry& entry : plan.mEntries)
        {
            EXPECT_EQ(entry.mExplodeBonus, 100);
            EXPECT_EQ(entry.mDismemberBonus, 100);
        }
        std::set<int> limbs;
        for (const DismemberEntry& entry : plan.mEntries)
            limbs.insert(entry.mLimb);
        EXPECT_THAT(limbs, ElementsAre(head, leftArm, rightArm, leftLeg, rightLeg));
    }

    TEST(FalloutScriptDismemberTest, there_are_at_most_seven_parts_in_a_plan)
    {
        std::vector<BodyPart> many;
        for (int i = 0; i < 12; ++i)
            many.push_back(part(i, 25 + (i % 7), Flag::severable));
        DismemberHit hit;
        hit.mBloodyMess = true;
        hit.mFromExplosion = false;
        const Rule::Plan plan = Rule::plan(DismemberSettings(), many, hit, conditions({ 25, 27, 28, 29, 30, 31 }), numbers({}));
        EXPECT_LE(plan.mEntries.size(), 7u);
        hit.mBloodyMess = false;
        hit.mFromExplosion = true;
        for (int i = 0; i < 12; ++i)
            hit.mExplosionLimbs.push_back(i);
        EXPECT_EQ(Rule::plan(DismemberSettings(), many, hit, conditions(), numbers({})).mEntries.size(), 7u);
    }

    TEST(FalloutScriptDismemberTest, a_part_that_is_no_limb_or_is_gone_is_not_decided_for)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mLimb = leftArm;
        hit.mDismembers = true;
        EXPECT_FALSE(Rule::decide(settings, body(), hit, { leftArm, -1, 0, 0 }, false, nothingGone, numbers({})));
        EXPECT_FALSE(Rule::decide(settings, body(), hit, { leftArm, 40, 0, 0 }, false, nothingGone, numbers({})));
        EXPECT_FALSE(Rule::decide(settings, body(), hit, { leftArm, leftArmCondition, 0, 0 }, false,
            [](int limb) { return limb == leftArm; }, numbers({})));
        const std::optional<Cut> cut
            = Rule::decide(settings, body(), hit, { leftArm, leftArmCondition, 0, 0 }, false, nothingGone, numbers({}));
        ASSERT_TRUE(cut);
        EXPECT_EQ(cut->mLimb, leftArm);
        EXPECT_FALSE(cut->mExploded);
    }

    TEST(FalloutScriptDismemberTest, a_blow_that_tears_or_bursts_says_so_and_needs_no_roll)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mLimb = head;
        hit.mExplodes = true;
        const std::optional<Cut> burst
            = Rule::decide(settings, body(), hit, { head, headCondition, 0, 0 }, false, nothingGone, numbers({}));
        ASSERT_TRUE(burst);
        EXPECT_TRUE(burst->mExploded);
        // the torso bursts by the plan: the torso part that cannot be torn off bursts
        hit.mExplodes = false;
        const std::optional<Cut> torso
            = Rule::decide(settings, body(), hit, { 0, torsoCondition, 100, 100 }, true, nothingGone, numbers({}));
        ASSERT_TRUE(torso);
        EXPECT_EQ(torso->mLimb, 0);
        EXPECT_TRUE(torso->mExploded);
    }

    TEST(FalloutScriptDismemberTest, a_part_that_is_certain_to_burst_bursts)
    {
        std::vector<BodyPart> parts = body();
        parts[1].mFlags = Flag::explodable | Flag::absoluteChance;
        parts[1].mExplodeChance = 100;
        DismemberHit hit;
        hit.mLimb = head;
        const std::optional<Cut> cut
            = Rule::decide(DismemberSettings(), parts, hit, { head, headCondition, 0, 0 }, false, nothingGone, numbers({}));
        ASSERT_TRUE(cut);
        EXPECT_TRUE(cut->mExploded);
    }

    TEST(FalloutScriptDismemberTest, the_roll_decides_for_the_rest_and_nothing_comes_when_it_misses)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mLimb = leftArm;
        // 75 percent of 50 is 37: a roll of 36 bursts, 37 to 49 tears, 50 does nothing
        const DismemberEntry entry{ leftArm, leftArmCondition, 0, 0 };
        const std::optional<Cut> burst = Rule::decide(settings, body(), hit, entry, false, nothingGone, numbers({ 36 }));
        ASSERT_TRUE(burst);
        EXPECT_TRUE(burst->mExploded);
        const std::optional<Cut> tear = Rule::decide(settings, body(), hit, entry, false, nothingGone, numbers({ 37 }));
        ASSERT_TRUE(tear);
        EXPECT_FALSE(tear->mExploded);
        EXPECT_FALSE(Rule::decide(settings, body(), hit, entry, false, nothingGone, numbers({ 50 })));
    }

    TEST(FalloutScriptDismemberTest, the_setting_of_the_weapon_is_for_the_part_that_was_hit_and_not_for_the_others)
    {
        const DismemberSettings settings;
        DismemberHit hit;
        hit.mLimb = leftArm;
        hit.mOnHit = 3;
        // the weapon allows neither for the arm that was hit
        EXPECT_FALSE(Rule::decide(settings, body(), hit, { leftArm, leftArmCondition, 0, 0 }, false, nothingGone,
            numbers({ 0 })));
        // and has nothing to say about another limb
        EXPECT_TRUE(Rule::decide(settings, body(), hit, { rightArm, rightArmCondition, 100, 0 }, false, nothingGone,
            numbers({ 0 })));
    }

    TEST(FalloutScriptDismemberTest, the_part_that_goes_is_the_first_of_the_body_that_has_the_condition)
    {
        std::vector<BodyPart> parts = body();
        // a hand that has the condition of the arm: the arm is first in the order of the types
        parts.push_back(part(leftHand, leftArmCondition, Flag::severable));
        DismemberHit hit;
        hit.mLimb = leftHand;
        hit.mDismembers = true;
        const std::optional<Cut> cut = Rule::decide(
            DismemberSettings(), parts, hit, { leftHand, leftArmCondition, 0, 0 }, false, nothingGone, numbers({}));
        ASSERT_TRUE(cut);
        EXPECT_EQ(cut->mLimb, leftArm);
    }

    TEST(FalloutScriptDismemberTest, a_burst_of_a_part_that_cannot_burst_is_a_tear_and_a_part_that_can_neither_stays)
    {
        std::vector<BodyPart> parts = body();
        parts[1].mFlags = Flag::severable;
        parts[2].mFlags = 0;
        DismemberHit hit;
        hit.mExplodes = true;
        hit.mLimb = head;
        const std::optional<Cut> cut
            = Rule::decide(DismemberSettings(), parts, hit, { head, headCondition, 0, 0 }, false, nothingGone, numbers({}));
        ASSERT_TRUE(cut);
        EXPECT_EQ(cut->mLimb, head);
        EXPECT_FALSE(cut->mExploded);
        hit.mLimb = leftArm;
        EXPECT_FALSE(Rule::decide(
            DismemberSettings(), parts, hit, { leftArm, leftArmCondition, 0, 0 }, false, nothingGone, numbers({})));
    }

    TEST(FalloutScriptDismemberTest, a_blow_goes_through_the_plan_and_what_goes_first_is_gone_for_the_rest)
    {
        DismemberSettings settings;
        DismemberHit hit;
        hit.mBloodyMess = true;
        hit.mLimb = head;
        hit.mDismembers = true;
        // the plan: the head, then the crippled left arm and the right leg that the roll lets (the rolls of the plan: 99
        // for the right arm, 99 for the left leg and 0 for the right leg)
        const std::vector<Cut> cuts = Rule::cuts(
            settings, body(), hit, conditions({ leftArmCondition }), nothingGone, numbers({ 99, 99, 0 }));
        ASSERT_EQ(cuts.size(), 3u);
        EXPECT_EQ(cuts[0].mLimb, head);
        EXPECT_EQ(cuts[1].mLimb, leftArm);
        EXPECT_EQ(cuts[2].mLimb, rightLeg);
        // a limb that went in an earlier blow is not torn off again
        const std::vector<Cut> again = Rule::cuts(settings, body(), hit, conditions({ leftArmCondition }),
            [](int limb) { return limb == head; }, numbers({ 99, 99, 0 }));
        ASSERT_EQ(again.size(), 2u);
        EXPECT_EQ(again[0].mLimb, leftArm);
    }

    TEST(FalloutScriptDismemberTest, the_cause_of_a_death_is_what_dealt_the_blow)
    {
        EXPECT_EQ(Rule::causeOf(true, true, true), FalloutScript::DeathCause::explosion);
        EXPECT_EQ(Rule::causeOf(false, true, true), FalloutScript::DeathCause::projectile);
        EXPECT_EQ(Rule::causeOf(false, false, true), FalloutScript::DeathCause::weapon);
        EXPECT_EQ(Rule::causeOf(false, false, false), FalloutScript::DeathCause::other);
        EXPECT_EQ(FalloutScript::DeathCause::explosion, 0);
        EXPECT_EQ(FalloutScript::DeathCause::projectile, 1);
        EXPECT_EQ(FalloutScript::DeathCause::weapon, 2);
        EXPECT_EQ(FalloutScript::DeathCause::other, 3);
    }

    TEST(FalloutScriptDismemberTest, the_state_keeps_the_limbs_that_are_gone_and_what_burst)
    {
        FalloutScript::Dismemberment state;
        EXPECT_FALSE(state.isGone(raider, leftArm));
        EXPECT_EQ(state.mask(raider), 0);
        state.markGone(raider, leftArm, false);
        state.markGone(raider, head, true);
        EXPECT_TRUE(state.isGone(raider, leftArm));
        EXPECT_TRUE(state.isGone(raider, head));
        EXPECT_FALSE(state.isGone(raider, rightArm));
        EXPECT_FALSE(state.isGone(robot, leftArm));
        EXPECT_EQ(state.mask(raider), (1 << leftArm) | (1 << head));
        EXPECT_EQ(state.exploded(raider), 1 << head);
        // a limb that is not one of the 15 is nothing
        state.markGone(raider, 15, false);
        state.markGone(raider, -1, false);
        state.markGone(0, leftArm, false);
        EXPECT_FALSE(state.isGone(raider, 15));
        EXPECT_FALSE(state.isGone(raider, -1));
        EXPECT_EQ(state.mask(raider), (1 << leftArm) | (1 << head));
        EXPECT_THAT(state.actors(), ElementsAre(raider));
        state.forget(raider);
        EXPECT_EQ(state.mask(raider), 0);
        state.markGone(raider, head, false);
        state.restore(raider);
        EXPECT_EQ(state.mask(raider), 0);
        state.markGone(raider, head, false);
        state.clear();
        EXPECT_TRUE(state.actors().empty());
    }

    TEST(FalloutScriptDismemberTest, the_state_keeps_what_killed_who_and_when)
    {
        FalloutScript::Dismemberment state;
        EXPECT_EQ(state.cause(raider), FalloutScript::DeathCause::none);
        EXPECT_EQ(state.killer(raider), 0u);
        state.recordDeath(raider, FalloutScript::DeathCause::projectile, robot);
        EXPECT_EQ(state.cause(raider), 1);
        EXPECT_EQ(state.killer(raider), robot);
        state.recordDeath(0, 1, 0);
        EXPECT_EQ(state.deathHour(raider), 0.0);
        state.setDeathHour(raider, 1500);
        EXPECT_EQ(state.deathHour(raider), 1500.0);
        state.setDeathHour(raider, 0);
        EXPECT_EQ(state.deathHour(raider), 0.0);
        state.setDeathHour(0, 5);
        state.setDeathHour(robot, 0);
        EXPECT_THAT(state.actors(), ElementsAre(raider));
    }

    TEST(FalloutScriptDismemberTest, only_so_many_characters_lose_limbs_in_a_frame)
    {
        FalloutScript::Dismemberment state;
        EXPECT_TRUE(state.takeSlot(2));
        EXPECT_TRUE(state.takeSlot(2));
        EXPECT_FALSE(state.takeSlot(2));
        state.newFrame();
        EXPECT_TRUE(state.takeSlot(2));
        state.newFrame();
        EXPECT_FALSE(state.takeSlot(0));
    }

    TEST(FalloutScriptDismemberTest, the_state_survives_a_save)
    {
        FalloutScript::Dismemberment state;
        state.markGone(raider, leftArm, false);
        state.markGone(raider, head, true);
        state.recordDeath(raider, FalloutScript::DeathCause::explosion, robot);
        state.setDeathHour(raider, 1234);
        state.setCriticalStage(raider, FalloutScript::CriticalStage::disintegrateEnd);
        state.markGone(robot, rightLeg, false);
        FalloutScript::SaveWriter out;
        FalloutScript::writeDismemberment(state, out);

        FalloutScript::Dismemberment loaded;
        loaded.markGone(crate, head, false);
        FalloutScript::SaveReader in(out.bytes());
        FalloutScript::readDismemberment(loaded, in, FalloutScript::dismemberSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_TRUE(loaded.isGone(raider, leftArm));
        EXPECT_TRUE(loaded.isGone(raider, head));
        EXPECT_EQ(loaded.exploded(raider), 1 << head);
        EXPECT_EQ(loaded.cause(raider), 0);
        EXPECT_EQ(loaded.killer(raider), robot);
        EXPECT_EQ(loaded.deathHour(raider), 1234.0);
        EXPECT_EQ(loaded.criticalStage(raider), 4);
        EXPECT_TRUE(loaded.isGone(robot, rightLeg));
        EXPECT_FALSE(loaded.isGone(crate, head));

        FalloutScript::SaveWriter again;
        FalloutScript::writeDismemberment(loaded, again);
        EXPECT_EQ(again.bytes(), out.bytes());

        const std::string cut = out.bytes().substr(0, out.bytes().size() - 5);
        FalloutScript::SaveReader shortIn(cut);
        FalloutScript::Dismemberment broken;
        EXPECT_THROW(FalloutScript::readDismemberment(broken, shortIn, 1), FalloutScript::SaveError);
    }

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeRecords mRecords;
        FalloutScript::Dismemberment mState;

        Runner()
        {
            mRecords.mActors = { raider, robot };
            FalloutScript::addDismemberCommands(mInterpreter, mState, mRecords);
        }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    TEST(FalloutScriptDismemberTest, IsLimbGone_tells_the_limbs_of_a_character_and_nothing_for_a_thing)
    {
        Runner run;
        EXPECT_EQ(run.call(OpIsLimbGone, raider, { double(leftArm) }), 0.0);
        run.mState.markGone(raider, leftArm, false);
        EXPECT_EQ(run.call(OpIsLimbGone, raider, { double(leftArm) }), 1.0);
        EXPECT_EQ(run.call(OpIsLimbGone, raider, { double(rightArm) }), 0.0);
        EXPECT_EQ(run.call(OpIsLimbGone, raider, { 99 }), 0.0);
        EXPECT_EQ(run.call(OpIsLimbGone, raider, { -3 }), 0.0);
        run.mState.markGone(crate, leftArm, false);
        EXPECT_EQ(run.call(OpIsLimbGone, crate, { double(leftArm) }), 0.0);
        EXPECT_EQ(run.call(OpIsLimbGone, 0, { double(leftArm) }), 0.0);
    }

    TEST(FalloutScriptDismemberTest, GetCauseofDeath_is_minus_one_until_a_death_is_kept)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetCauseofDeath, raider), -1.0);
        run.mState.recordDeath(raider, FalloutScript::DeathCause::weapon, robot);
        EXPECT_EQ(run.call(OpGetCauseofDeath, raider), 2.0);
        EXPECT_EQ(run.call(OpGetCauseofDeath, 0), -1.0);
    }

    TEST(FalloutScriptDismemberTest, the_critical_stage_is_kept_for_characters_in_the_range_of_the_enum)
    {
        Runner run;
        EXPECT_EQ(run.call(OpIsInCriticalStage, raider, { 0 }), 1.0);
        EXPECT_EQ(run.call(OpIsInCriticalStage, raider, { 3 }), 0.0);
        run.call(OpSetCriticalStage, raider, { 3 });
        EXPECT_EQ(run.call(OpIsInCriticalStage, raider, { 3 }), 1.0);
        EXPECT_EQ(run.call(OpIsInCriticalStage, raider, { 0 }), 0.0);
        EXPECT_THAT(run.mRecords.mStages, ElementsAre(Pair(raider, 3)));
        // outside the enum nothing changes, and a thing has no stage
        run.call(OpSetCriticalStage, raider, { 5 });
        run.call(OpSetCriticalStage, raider, { -1 });
        EXPECT_EQ(run.call(OpIsInCriticalStage, raider, { 3 }), 1.0);
        run.call(OpSetCriticalStage, crate, { 1 });
        EXPECT_EQ(run.call(OpIsInCriticalStage, crate, { 1 }), 0.0);
        EXPECT_EQ(run.call(OpIsInCriticalStage, crate, { 0 }), 0.0);
        run.call(OpSetCriticalStage, raider, { 0 });
        EXPECT_EQ(run.call(OpIsInCriticalStage, raider, { 0 }), 1.0);
        EXPECT_EQ(run.mState.criticalStage(raider), 0);
        run.call(OpSetCriticalStage, 0, { 1 });
        EXPECT_EQ(run.call(OpIsInCriticalStage, 0, { 0 }), 0.0);
    }
}
