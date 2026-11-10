#include "scriptbuilder.hpp"

#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/magic.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::Magic;
    using FalloutScript::SpellInfo;

    constexpr FormId courier = 0x00000014;
    constexpr FormId crate = 0x00020001;
    constexpr FormId ability = 0x00100001;
    constexpr FormId disease = 0x00100002;
    constexpr FormId blast = 0x00100003;
    constexpr FormId gatedAbility = 0x00100004;
    constexpr FormId gatedBlast = 0x00100005;
    constexpr FormId notASpell = 0x00100099;
    constexpr FormId strengthBoost = 0x00200001;
    constexpr FormId sickness = 0x00200002;
    constexpr FormId fire = 0x00200003;
    constexpr FormId gatedBoost = 0x00200004;
    constexpr FormId gatedFire = 0x00200005;
    constexpr FormId restSpell = 0x00100006;
    constexpr FormId restEffect = 0x00200006;
    constexpr FormId restScript = 0x00300001;
    constexpr FormId flashSpell = 0x00100007;
    constexpr FormId flashEffect = 0x00200007;
    constexpr FormId flashScript = 0x00300002;
    constexpr FormId scriptedAbility = 0x00100008;
    constexpr FormId scriptedAbilityEffect = 0x00200008;
    constexpr FormId scriptedAbilityScript = 0x00300003;

    constexpr std::uint16_t OpAddSpell = 0x101C;
    constexpr std::uint16_t OpRemoveSpell = 0x101D;
    constexpr std::uint16_t OpCast = 0x101E;
    constexpr std::uint16_t OpHasMagicEffect = 0x10D6;
    constexpr std::uint16_t OpIsSpellTarget = 0x10DF;
    constexpr std::uint16_t OpCastImmediateOnSelf = 0x11D9;

    class FakeRecords : public FalloutScript::MagicRecords
    {
    public:
        std::map<FormId, SpellInfo> mSpells;

        FakeRecords()
        {
            SpellInfo boost;
            boost.mType = 4;
            boost.mEffects.push_back({ strengthBoost, 0, FalloutScript::ActorValue::strength, 2, 0, false });
            mSpells[ability] = boost;
            SpellInfo ill;
            ill.mType = 1;
            ill.mEffects.push_back({ sickness, 0, FalloutScript::ActorValue::endurance, 3, 0, true });
            mSpells[disease] = ill;
            SpellInfo flame;
            flame.mType = 0;
            flame.mEffects.push_back({ fire, 0, FalloutScript::ActorValue::health, 5, 10, true });
            mSpells[blast] = flame;
            // Effects that are on the character only while a condition holds
            SpellInfo gatedBoostSpell;
            gatedBoostSpell.mType = 4;
            gatedBoostSpell.mEffects.push_back({ gatedBoost, 0, FalloutScript::ActorValue::strength, 2, 0, false });
            gatedBoostSpell.mEffects.back().mConditions.emplace_back();
            mSpells[gatedAbility] = gatedBoostSpell;
            SpellInfo gatedFlame;
            gatedFlame.mType = 0;
            gatedFlame.mEffects.push_back({ gatedFire, 0, FalloutScript::ActorValue::health, 5, 10, true });
            gatedFlame.mEffects.back().mConditions.emplace_back();
            mSpells[gatedBlast] = gatedFlame;
            // Effects of the script archetype (1): one that lasts, one that happens at once, one an ability keeps
            SpellInfo rest;
            rest.mType = 0;
            rest.mEffects.push_back({ restEffect, 1, -1, 0, 100, false });
            mSpells[restSpell] = rest;
            SpellInfo flash;
            flash.mType = 0;
            flash.mEffects.push_back({ flashEffect, 1, -1, 0, 0, false });
            mSpells[flashSpell] = flash;
            SpellInfo kept;
            kept.mType = 4;
            kept.mEffects.push_back({ scriptedAbilityEffect, 1, -1, 0, 0, false });
            mSpells[scriptedAbility] = kept;
        }

        bool spell(FormId spell, SpellInfo& info) override
        {
            const auto found = mSpells.find(spell);
            if (found == mSpells.end())
                return false;
            info = found->second;
            return true;
        }
        bool isActor(FormId reference) override { return reference == courier; }
        FormId effectScript(FormId effect) override
        {
            return effect == restEffect           ? restScript
                : effect == flashEffect           ? flashScript
                : effect == scriptedAbilityEffect ? scriptedAbilityScript
                                                  : 0;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeRecords mRecords;
        Magic mMagic{ mRecords };
        std::vector<std::pair<int, double>> mChanges;
        std::vector<FormId> mCast;
        std::vector<std::size_t> mCastEffects;
        /// The scripts of effects that ran: the script and its stage, in order
        std::vector<std::pair<FormId, Magic::ScriptStage>> mStages;
        /// The effect numbers of the stages, in the same order
        std::vector<Magic::EffectId> mEffectIds;

        Runner()
        {
            mMagic.setScriptRunner([this](FormId actor, FormId script, Magic::ScriptStage stage, Magic::EffectId id) {
                EXPECT_EQ(actor, courier);
                mStages.emplace_back(script, stage);
                mEffectIds.push_back(id);
            });
            mMagic.setModifier(
                [this](FormId, int actorValue, double amount) { mChanges.emplace_back(actorValue, amount); });
            mMagic.setCaster([this](FormId, FormId spell, const std::vector<FalloutScript::SpellEffect>& effects) {
                mCast.push_back(spell);
                mCastEffects.push_back(effects.size());
            });
            FalloutScript::addMagicCommands(mInterpreter, mMagic, mRecords);
        }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    TEST(FalloutScriptMagicTest, an_ability_changes_the_value_until_it_is_taken_away)
    {
        Runner run;
        run.call(OpAddSpell, courier, { ability });
        EXPECT_THAT(run.mChanges, ElementsAre(Pair(FalloutScript::ActorValue::strength, 2.0)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { strengthBoost }), 1.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { ability }), 1.0);
        EXPECT_TRUE(run.mMagic.hasSpell(courier, ability));

        run.mMagic.update(1000);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { strengthBoost }), 1.0);

        run.call(OpRemoveSpell, courier, { ability });
        EXPECT_THAT(run.mChanges,
            ElementsAre(
                Pair(FalloutScript::ActorValue::strength, 2.0), Pair(FalloutScript::ActorValue::strength, -2.0)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { strengthBoost }), 0.0);
        EXPECT_FALSE(run.mMagic.hasSpell(courier, ability));
    }

    TEST(FalloutScriptMagicTest, a_disease_hurts_the_value_and_a_spell_of_another_kind_only_waits)
    {
        Runner run;
        run.call(OpAddSpell, courier, { disease });
        EXPECT_THAT(run.mChanges, ElementsAre(Pair(FalloutScript::ActorValue::endurance, -3.0)));

        run.mChanges.clear();
        run.call(OpAddSpell, courier, { blast });
        EXPECT_TRUE(run.mChanges.empty());
        EXPECT_TRUE(run.mMagic.hasSpell(courier, blast));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 0.0);
    }

    TEST(FalloutScriptMagicTest, a_spell_is_given_once_and_only_to_characters_and_only_when_it_is_one)
    {
        Runner run;
        run.call(OpAddSpell, courier, { ability });
        run.call(OpAddSpell, courier, { ability });
        EXPECT_EQ(run.mChanges.size(), 1u);
        run.call(OpAddSpell, courier, { notASpell });
        EXPECT_EQ(run.mMagic.spells(courier), std::vector<FormId>{ ability });
        run.call(OpAddSpell, crate, { disease });
        EXPECT_TRUE(run.mMagic.spells(crate).empty());
        EXPECT_EQ(run.call(OpRemoveSpell, courier, { disease }), 0.0);
        EXPECT_EQ(run.mChanges.size(), 1u);
    }

    TEST(FalloutScriptMagicTest, a_spell_cast_on_oneself_runs_for_its_time)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { blast });
        EXPECT_THAT(run.mCast, ElementsAre(blast));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 1.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { blast }), 1.0);

        run.mMagic.update(6);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 1.0);
        run.mMagic.update(6);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 0.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { blast }), 0.0);
    }

    TEST(FalloutScriptMagicTest, a_cast_goes_on_when_the_spell_that_was_learned_is_taken_away)
    {
        Runner run;
        run.call(OpAddSpell, courier, { blast });
        run.call(OpCastImmediateOnSelf, courier, { blast });
        run.call(OpRemoveSpell, courier, { blast });
        EXPECT_FALSE(run.mMagic.hasSpell(courier, blast));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 1.0);
        run.mMagic.update(12);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 0.0);
    }

    TEST(FalloutScriptMagicTest, an_effect_is_on_the_character_only_while_its_conditions_hold)
    {
        Runner run;
        // Nothing to ask the conditions: they do not hold
        run.call(OpCastImmediateOnSelf, courier, { gatedBlast });
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedFire }), 0.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { gatedBlast }), 0.0);

        bool holds = false;
        std::vector<FormId> asked;
        run.mMagic.setConditions(
            [&](const std::vector<ESM4::TargetCondition>&, const FalloutScript::ConditionContext& context) {
                asked.push_back(context.mSubject);
                return holds;
            });
        run.call(OpAddSpell, courier, { gatedAbility });
        EXPECT_TRUE(run.mChanges.empty());
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedBoost }), 0.0);
        EXPECT_THAT(asked, ElementsAre(courier));

        run.call(OpCastImmediateOnSelf, courier, { gatedBlast });
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedFire }), 0.0);

        holds = true;
        run.call(OpCastImmediateOnSelf, courier, { gatedBlast });
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedFire }), 1.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { gatedBlast }), 1.0);

        run.call(OpRemoveSpell, courier, { gatedAbility });
        run.call(OpAddSpell, courier, { gatedAbility });
        EXPECT_THAT(run.mChanges, ElementsAre(Pair(FalloutScript::ActorValue::strength, 2.0)));
    }

    TEST(FalloutScriptMagicTest, a_condition_that_changes_is_asked_once_and_the_caster_gets_what_was_kept)
    {
        Runner run;
        int asked = 0;
        // Holds the first time it is asked only, like a chance
        run.mMagic.setConditions([&](const std::vector<ESM4::TargetCondition>&,
                                     const FalloutScript::ConditionContext&) { return ++asked == 1; });
        run.call(OpCastImmediateOnSelf, courier, { gatedBlast });
        EXPECT_EQ(asked, 1);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedFire }), 1.0);
        EXPECT_THAT(run.mCastEffects, ElementsAre(1u));

        // The second cast fails its condition: nothing runs and the caster is given nothing
        run.call(OpCastImmediateOnSelf, courier, { gatedBlast });
        EXPECT_EQ(asked, 2);
        EXPECT_THAT(run.mCastEffects, ElementsAre(1u, 0u));
    }

    TEST(FalloutScriptMagicTest, an_ability_is_no_spell_to_cast_and_cast_does_nothing)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { ability });
        EXPECT_TRUE(run.mCast.empty());
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { strengthBoost }), 0.0);

        EXPECT_EQ(run.call(OpCast, courier, { blast, crate }), 1.0);
        EXPECT_TRUE(run.mCast.empty());
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 0.0);
    }

    TEST(FalloutScriptMagicTest, the_effect_of_an_item_is_found_for_the_time_it_lasts)
    {
        Runner run;
        run.mMagic.noteEffect(courier, fire, crate, 3);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 1.0);
        run.mMagic.update(4);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 0.0);
    }

    TEST(FalloutScriptMagicTest, a_saved_game_brings_the_spells_and_effects_back)
    {
        Runner run;
        run.call(OpAddSpell, courier, { ability });
        run.call(OpCastImmediateOnSelf, courier, { blast });
        FalloutScript::SaveWriter out;
        FalloutScript::writeMagic(run.mMagic, out);

        Runner other;
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMagic(other.mMagic, in, FalloutScript::magicSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_TRUE(other.mMagic.hasSpell(courier, ability));
        EXPECT_EQ(other.call(OpHasMagicEffect, courier, { strengthBoost }), 1.0);
        EXPECT_EQ(other.call(OpHasMagicEffect, courier, { fire }), 1.0);

        // Taking the spell away takes back the change that was made before the save
        other.call(OpRemoveSpell, courier, { ability });
        EXPECT_THAT(other.mChanges, ElementsAre(Pair(FalloutScript::ActorValue::strength, -2.0)));

        other.mMagic.clear();
        EXPECT_FALSE(other.mMagic.hasSpell(courier, ability));
        EXPECT_EQ(other.call(OpHasMagicEffect, courier, { fire }), 0.0);
    }

    TEST(FalloutScriptMagicTest, the_script_of_an_effect_starts_runs_each_frame_and_finishes_with_it)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Start)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { restEffect }), 1.0);

        run.mStages.clear();
        run.mMagic.update(30);
        run.mMagic.update(30);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Update), Pair(restScript, Stage::Update)));

        // The frame in which it runs out finishes it: there is no update for it
        run.mStages.clear();
        run.mMagic.update(50);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Finish)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { restEffect }), 0.0);

        run.mStages.clear();
        run.mMagic.update(50);
        EXPECT_TRUE(run.mStages.empty());
    }

    TEST(FalloutScriptMagicTest, a_spell_cast_again_renews_its_script_effect_and_the_old_one_does_not_end_the_new)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        run.mMagic.update(60);
        run.mStages.clear();

        // The earlier effect ends before the new one begins
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Finish), Pair(restScript, Stage::Start)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { restEffect }), 1.0);

        // The time the earlier effect had left (40) passes without it ending anything: the new one has its whole time
        run.mStages.clear();
        run.mMagic.update(50);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Update)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { restEffect }), 1.0);

        run.mStages.clear();
        run.mMagic.update(50);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Finish)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { restEffect }), 0.0);
    }

    TEST(FalloutScriptMagicTest, the_effects_of_one_script_on_one_character_are_told_apart_by_their_numbers)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        // An item used twice: two effects of one script, one running out before the other
        run.mMagic.noteEffect(courier, restEffect, crate, 3);
        run.mMagic.noteEffect(courier, restEffect, crate, 10);
        ASSERT_EQ(run.mEffectIds.size(), 2u);
        const Magic::EffectId first = run.mEffectIds[0];
        const Magic::EffectId second = run.mEffectIds[1];
        EXPECT_NE(first, second);

        run.mStages.clear();
        run.mEffectIds.clear();
        run.mMagic.update(4);
        // The first one ends and the second goes on, each by its own number
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Update), Pair(restScript, Stage::Finish)));
        EXPECT_THAT(run.mEffectIds, ElementsAre(second, first));
    }

    TEST(FalloutScriptMagicTest, a_renewed_script_effect_ends_the_old_number_and_starts_a_new_one)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        const Magic::EffectId first = run.mEffectIds.at(0);
        run.mEffectIds.clear();
        run.mStages.clear();
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        ASSERT_EQ(run.mEffectIds.size(), 2u);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Finish), Pair(restScript, Stage::Start)));
        EXPECT_EQ(run.mEffectIds[0], first);
        EXPECT_NE(run.mEffectIds[1], first);
    }

    TEST(FalloutScriptMagicTest, a_loaded_game_keeps_the_numbers_of_the_effects_that_run_and_numbers_new_ones_above)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        const Magic::EffectId saved = run.mEffectIds.at(0);
        FalloutScript::SaveWriter out;
        FalloutScript::writeMagic(run.mMagic, out);

        Runner other;
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMagic(other.mMagic, in, FalloutScript::magicSectionVersion);
        other.mStages.clear();
        other.mEffectIds.clear();
        other.mMagic.update(1);
        EXPECT_THAT(other.mStages, ElementsAre(Pair(restScript, Stage::Update)));
        // The saved variables of the script are held by this number
        EXPECT_THAT(other.mEffectIds, ElementsAre(saved));

        // What runs next does not take the number of one that runs
        other.mEffectIds.clear();
        other.mMagic.noteEffect(courier, restEffect, crate, 1);
        ASSERT_EQ(other.mEffectIds.size(), 1u);
        EXPECT_GT(other.mEffectIds[0], saved);
    }

    TEST(FalloutScriptMagicTest, a_game_saved_before_the_effects_were_numbered_numbers_them_when_it_is_loaded)
    {
        using Stage = Magic::ScriptStage;
        // The section as version 1 wrote it: no spells and one running effect, without a number
        FalloutScript::SaveWriter out;
        out.count(0);
        out.count(1);
        out.form(courier);
        out.form(restEffect);
        out.form(restSpell);
        out.f64(40);
        out.i32(-1);
        out.f64(0);
        const std::string bytes = out.take();

        Runner run;
        run.mMagic.noteEffect(courier, flashEffect, crate, 1);
        const Magic::EffectId before = run.mEffectIds.at(0);
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMagic(run.mMagic, in, 1);
        EXPECT_TRUE(in.atEnd());
        run.mStages.clear();
        run.mEffectIds.clear();
        run.mMagic.update(1);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Update)));
        ASSERT_EQ(run.mEffectIds.size(), 1u);
        EXPECT_GT(run.mEffectIds[0], before);
    }

    TEST(FalloutScriptMagicTest, an_effect_without_a_script_runs_none_and_the_scripts_wait_for_a_runner)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { blast });
        run.mMagic.update(3);
        run.mMagic.update(30);
        EXPECT_TRUE(run.mStages.empty());

        // Nothing runs, and nothing breaks, without a host that runs scripts
        Runner quiet;
        quiet.mMagic.setScriptRunner(nullptr);
        quiet.call(OpCastImmediateOnSelf, courier, { restSpell });
        quiet.mMagic.update(500);
        EXPECT_TRUE(quiet.mStages.empty());
        EXPECT_EQ(quiet.call(OpHasMagicEffect, courier, { restEffect }), 0.0);
    }

    TEST(FalloutScriptMagicTest, an_effect_that_happens_at_once_runs_its_script_from_start_to_finish)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { flashSpell });
        EXPECT_THAT(run.mStages, ElementsAre(Pair(flashScript, Stage::Start), Pair(flashScript, Stage::Finish)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { flashEffect }), 0.0);
    }

    TEST(FalloutScriptMagicTest, an_ability_with_a_script_finishes_it_when_it_is_taken_away)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpAddSpell, courier, { scriptedAbility });
        EXPECT_THAT(run.mStages, ElementsAre(Pair(scriptedAbilityScript, Stage::Start)));

        // It has no time to run out: it goes on each frame until the spell goes
        run.mStages.clear();
        run.mMagic.update(1000);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(scriptedAbilityScript, Stage::Update)));

        run.mStages.clear();
        run.call(OpRemoveSpell, courier, { scriptedAbility });
        EXPECT_THAT(run.mStages, ElementsAre(Pair(scriptedAbilityScript, Stage::Finish)));
    }

    TEST(FalloutScriptMagicTest, the_script_of_the_effect_of_an_item_runs_for_the_time_it_lasts)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.mMagic.noteEffect(courier, restEffect, crate, 3);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Start)));
        run.mMagic.update(4);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Start), Pair(restScript, Stage::Finish)));
    }

    TEST(FalloutScriptMagicTest, an_effect_that_was_running_when_the_game_was_saved_finishes_its_script_after_a_load)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        run.mMagic.update(40);
        FalloutScript::SaveWriter out;
        FalloutScript::writeMagic(run.mMagic, out);

        Runner other;
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMagic(other.mMagic, in, FalloutScript::magicSectionVersion);
        // The start is not run again: the effect was begun before the save
        EXPECT_TRUE(other.mStages.empty());
        other.mMagic.update(30);
        EXPECT_THAT(other.mStages, ElementsAre(Pair(restScript, Stage::Update)));
        other.mStages.clear();
        other.mMagic.update(40);
        EXPECT_THAT(other.mStages, ElementsAre(Pair(restScript, Stage::Finish)));
    }
}
