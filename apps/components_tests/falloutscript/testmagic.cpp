#include "scriptbuilder.hpp"

#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/magic.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <set>
#include <tuple>

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
    constexpr FormId twinSpell = 0x00100009;
    constexpr FormId twinFirstEffect = 0x00200009;
    constexpr FormId twinSecondEffect = 0x0020000A;
    constexpr FormId twinFirstScript = 0x00300004;
    constexpr FormId twinSecondScript = 0x00300005;
    constexpr FormId twinAbility = 0x0010000A;
    constexpr FormId twinAbilityFirstEffect = 0x0020000B;
    constexpr FormId twinAbilitySecondEffect = 0x0020000C;
    constexpr FormId twinAbilityFirstScript = 0x00300006;
    constexpr FormId twinAbilitySecondScript = 0x00300007;
    constexpr FormId powerSpell = 0x0010000B;
    constexpr FormId poisonSpell = 0x0010000C;
    constexpr FormId addictionSpell = 0x0010000D;
    constexpr FormId pairSpell = 0x0010000E;
    constexpr FormId gatedScriptedAbility = 0x0010000F;
    constexpr FormId doubleGatedAbility = 0x00100010;
    constexpr FormId enchantment = 0x00110001;
    constexpr FormId aidItem = 0x00110002;
    constexpr FormId ingredient = 0x00110003;
    constexpr FormId powerEffect = 0x0020000D;
    constexpr FormId poisonEffect = 0x0020000E;
    constexpr FormId addictionEffect = 0x0020000F;
    constexpr FormId pairFirstEffect = 0x00200010;
    constexpr FormId pairSecondEffect = 0x00200011;
    constexpr FormId gatedScriptEffect = 0x00200012;
    constexpr FormId itemEffect = 0x00200013;
    constexpr FormId gatedScript = 0x00300008;

    constexpr std::uint16_t OpAddSpell = 0x101C;
    constexpr std::uint16_t OpRemoveSpell = 0x101D;
    constexpr std::uint16_t OpCast = 0x101E;
    constexpr std::uint16_t OpHasMagicEffect = 0x10D6;
    constexpr std::uint16_t OpIsSpellTarget = 0x10DF;
    constexpr std::uint16_t OpCastImmediateOnSelf = 0x11D9;
    constexpr std::uint16_t OpSelectPlayerSpell = 0x10B3;
    constexpr std::uint16_t OpDispel = 0x1142;
    constexpr std::uint16_t OpDispelAllSpells = 0x1148;
    constexpr std::uint16_t OpEvaluateSpellConditions = 0x119D;
    constexpr std::uint16_t OpGetSpellUsageNum = 0x122B;

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
            // A spell and an ability with two script effects each
            SpellInfo twin;
            twin.mType = 0;
            twin.mEffects.push_back({ twinFirstEffect, 1, -1, 0, 100, false });
            twin.mEffects.push_back({ twinSecondEffect, 1, -1, 0, 100, false });
            mSpells[twinSpell] = twin;
            SpellInfo twinKept;
            twinKept.mType = 4;
            twinKept.mEffects.push_back({ twinAbilityFirstEffect, 1, -1, 0, 0, false });
            twinKept.mEffects.push_back({ twinAbilitySecondEffect, 1, -1, 0, 0, false });
            mSpells[twinAbility] = twinKept;
            // Spells of the other kinds the dispel of everything tells apart: a power, a poison and an addiction
            SpellInfo power;
            power.mType = 2;
            power.mEffects.push_back({ powerEffect, 0, -1, 7, 20, false });
            mSpells[powerSpell] = power;
            SpellInfo poison;
            poison.mType = 5;
            poison.mEffects.push_back({ poisonEffect, 0, -1, 4, 20, true });
            mSpells[poisonSpell] = poison;
            SpellInfo addiction;
            addiction.mType = 10;
            addiction.mEffects.push_back({ addictionEffect, 0, -1, 1, 20, true });
            mSpells[addictionSpell] = addiction;
            // A spell with two effects that last
            SpellInfo pair;
            pair.mType = 0;
            pair.mEffects.push_back({ pairFirstEffect, 0, -1, 3, 10, false });
            pair.mEffects.push_back({ pairSecondEffect, 0, -1, 4, 10, false });
            mSpells[pairSpell] = pair;
            // An ability with a script effect that is on the character only while a condition holds
            SpellInfo gatedScripted;
            gatedScripted.mType = 4;
            gatedScripted.mEffects.push_back({ gatedScriptEffect, 1, -1, 0, 0, false });
            gatedScripted.mEffects.back().mConditions.emplace_back();
            mSpells[gatedScriptedAbility] = gatedScripted;
            // An ability with two effects that are the same magic effect, each with a condition of its own
            SpellInfo doubled;
            doubled.mType = 4;
            for (const float comparison : { 1.0f, 2.0f })
            {
                doubled.mEffects.push_back({ gatedBoost, 0, FalloutScript::ActorValue::strength, 2, 0, false });
                doubled.mEffects.back().mConditions.emplace_back();
                doubled.mEffects.back().mConditions.back().comparison = comparison;
            }
            mSpells[doubleGatedAbility] = doubled;
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
            return effect == restEffect             ? restScript
                : effect == flashEffect             ? flashScript
                : effect == scriptedAbilityEffect   ? scriptedAbilityScript
                : effect == twinFirstEffect         ? twinFirstScript
                : effect == twinSecondEffect        ? twinSecondScript
                : effect == twinAbilityFirstEffect  ? twinAbilityFirstScript
                : effect == twinAbilitySecondEffect ? twinAbilitySecondScript
                : effect == gatedScriptEffect       ? gatedScript
                                                    : 0;
        }
        int itemKind(FormId item) override
        {
            return item == enchantment ? 6 : item == aidItem ? 7 : item == ingredient ? 8 : -1;
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
        /// The seconds that the stages cover (0 for all but an update), in the same order
        std::vector<double> mSeconds;
        /// What the scripts do when a stage has been noted (a script that casts or takes away a spell)
        std::function<void(FormId, Magic::ScriptStage)> mOnStage;
        /// The effects that were ended before their time: the character, the spell or item and the magic effect
        std::vector<std::tuple<FormId, FormId, FormId>> mEnded;

        Runner()
        {
            mMagic.setEnder(
                [this](FormId actor, FormId source, FormId effect) { mEnded.emplace_back(actor, source, effect); });
            mMagic.setScriptRunner(
                [this](FormId actor, FormId script, Magic::ScriptStage stage, Magic::EffectId id, double seconds) {
                    EXPECT_EQ(actor, courier);
                    mStages.emplace_back(script, stage);
                    mEffectIds.push_back(id);
                    mSeconds.push_back(seconds);
                    if (mOnStage)
                        mOnStage(script, stage);
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

        // The step in which it runs out runs its update for the seconds it was still running (40 of the 50), and then
        // finishes it
        run.mStages.clear();
        run.mSeconds.clear();
        run.mMagic.update(50);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Update), Pair(restScript, Stage::Finish)));
        EXPECT_THAT(run.mSeconds, ElementsAre(40.0, 0.0));
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
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Update), Pair(restScript, Stage::Finish)));
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
        // The first one ends and the second goes on, each by its own number (the first runs its last update, for the 3
        // seconds it had left, and then finishes)
        EXPECT_THAT(run.mStages,
            ElementsAre(
                Pair(restScript, Stage::Update), Pair(restScript, Stage::Update), Pair(restScript, Stage::Finish)));
        EXPECT_THAT(run.mEffectIds, ElementsAre(second, first, first));
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
        EXPECT_THAT(run.mStages,
            ElementsAre(
                Pair(restScript, Stage::Start), Pair(restScript, Stage::Update), Pair(restScript, Stage::Finish)));
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
        EXPECT_THAT(other.mStages, ElementsAre(Pair(restScript, Stage::Update), Pair(restScript, Stage::Finish)));
    }

    TEST(FalloutScriptMagicTest, a_step_of_a_rest_runs_an_effect_for_the_seconds_it_was_still_running)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        run.mStages.clear();
        run.mSeconds.clear();
        // An effect of 100 seconds has 80 left after a step of 20, and a step of 120 seconds (an hour of rest at the
        // time scale of 30) goes past its end
        run.mMagic.update(20);
        run.mMagic.update(120);
        EXPECT_THAT(run.mStages,
            ElementsAre(
                Pair(restScript, Stage::Update), Pair(restScript, Stage::Update), Pair(restScript, Stage::Finish)));
        // The first step covers all of its 20 seconds, the second only the 80 that the effect had left, and the end
        // covers none
        EXPECT_THAT(run.mSeconds, ElementsAre(20.0, 80.0, 0.0));
    }

    TEST(FalloutScriptMagicTest, an_effect_that_an_earlier_start_took_away_with_its_spell_does_not_begin)
    {
        Runner run;
        bool removed = false;
        run.mOnStage = [&](FormId script, Magic::ScriptStage stage) {
            if (script == twinAbilityFirstScript && stage == Magic::ScriptStage::Start && !removed)
            {
                removed = true;
                run.call(OpRemoveSpell, courier, { twinAbility });
            }
        };
        run.call(OpAddSpell, courier, { twinAbility });
        // The first begins and ends with the spell; the second ends without having begun, and is not begun after it
        EXPECT_THAT(run.mStages,
            ElementsAre(Pair(twinAbilityFirstScript, Magic::ScriptStage::Start),
                Pair(twinAbilityFirstScript, Magic::ScriptStage::Finish),
                Pair(twinAbilitySecondScript, Magic::ScriptStage::Finish)));
    }

    TEST(FalloutScriptMagicTest, an_effect_that_an_earlier_start_renewed_does_not_begin_as_the_old_one)
    {
        Runner run;
        bool cast = false;
        run.mOnStage = [&](FormId script, Magic::ScriptStage stage) {
            if (script == twinFirstScript && stage == Magic::ScriptStage::Start && !cast)
            {
                cast = true;
                run.call(OpCastImmediateOnSelf, courier, { twinSpell });
            }
        };
        run.call(OpCastImmediateOnSelf, courier, { twinSpell });
        // The cast that the first start made ends the two effects of the first cast and starts its own two; the second
        // effect of the first cast does not start after its end
        EXPECT_THAT(run.mStages,
            ElementsAre(Pair(twinFirstScript, Magic::ScriptStage::Start),
                Pair(twinFirstScript, Magic::ScriptStage::Finish), Pair(twinSecondScript, Magic::ScriptStage::Finish),
                Pair(twinFirstScript, Magic::ScriptStage::Start), Pair(twinSecondScript, Magic::ScriptStage::Start)));
        const std::set<Magic::EffectId> started = { run.mEffectIds[3], run.mEffectIds[4] };
        EXPECT_EQ(started.size(), 2u);
        EXPECT_EQ(started.count(run.mEffectIds[0]), 0u);
    }

    TEST(FalloutScriptMagicTest, dispel_ends_the_effects_of_one_spell_and_leaves_those_of_the_others)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { blast });
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        run.mStages.clear();

        EXPECT_EQ(run.call(OpDispel, courier, { blast }), 1.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 0.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { blast }), 0.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { restEffect }), 1.0);
        EXPECT_THAT(run.mEnded, ElementsAre(std::make_tuple(courier, blast, fire)));
        // The script of the other spell went on
        EXPECT_TRUE(run.mStages.empty());
    }

    TEST(FalloutScriptMagicTest, the_end_of_every_dispelled_entry_is_done_before_a_script_finishes)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { twinSpell });
        run.mStages.clear();
        // What whoever kept the effects was told when each script finished: a script that finishes can cast again, and
        // the end of an entry that came after it would take its cast away
        std::vector<std::size_t> endedWhenFinished;
        run.mOnStage = [&](FormId, Stage stage) {
            if (stage == Stage::Finish)
                endedWhenFinished.push_back(run.mEnded.size());
        };
        EXPECT_EQ(run.mMagic.dispel(courier, twinSpell), 2);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(twinFirstScript, Stage::Finish), Pair(twinSecondScript, Stage::Finish)));
        EXPECT_THAT(endedWhenFinished, ElementsAre(2u, 2u));
    }

    TEST(FalloutScriptMagicTest, a_dispelled_script_effect_finishes_once_and_is_not_run_again)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { restSpell });
        run.mStages.clear();

        run.call(OpDispel, courier, { restSpell });
        EXPECT_THAT(run.mStages, ElementsAre(Pair(restScript, Stage::Finish)));
        run.mStages.clear();
        run.mMagic.update(200);
        run.call(OpDispel, courier, { restSpell });
        EXPECT_TRUE(run.mStages.empty());
        EXPECT_EQ(run.mEnded.size(), 1u);
    }

    TEST(FalloutScriptMagicTest, dispelling_an_ability_changes_the_value_back_and_the_spell_stays_on_the_list)
    {
        Runner run;
        run.call(OpAddSpell, courier, { ability });
        run.call(OpDispel, courier, { ability });
        EXPECT_THAT(run.mChanges,
            ElementsAre(
                Pair(FalloutScript::ActorValue::strength, 2.0), Pair(FalloutScript::ActorValue::strength, -2.0)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { strengthBoost }), 0.0);
        EXPECT_TRUE(run.mMagic.hasSpell(courier, ability));

        // Taking the spell away takes nothing back a second time
        run.call(OpRemoveSpell, courier, { ability });
        EXPECT_EQ(run.mChanges.size(), 2u);
    }

    TEST(FalloutScriptMagicTest, dispel_by_magic_effect_ends_only_that_effect_of_the_spell)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { pairSpell });
        EXPECT_EQ(run.mMagic.dispel(courier, pairSpell, pairFirstEffect), 1);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { pairFirstEffect }), 0.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { pairSecondEffect }), 1.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { pairSpell }), 1.0);
        EXPECT_EQ(run.mMagic.dispel(courier, pairSpell), 1);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { pairSpell }), 0.0);
        EXPECT_THAT(run.mEnded,
            ElementsAre(std::make_tuple(courier, pairSpell, pairFirstEffect),
                std::make_tuple(courier, pairSpell, pairSecondEffect)));
    }

    TEST(FalloutScriptMagicTest, dispel_ends_nothing_for_a_form_without_effects_or_one_that_is_no_character)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { blast });
        EXPECT_EQ(run.mMagic.dispel(courier, restSpell), 0);
        EXPECT_EQ(run.mMagic.dispel(courier, 0), 0);
        EXPECT_EQ(run.mMagic.dispel(0, blast), 0);
        EXPECT_EQ(run.mMagic.dispelAll(0), 0);
        // The command asks first whether the reference is a character
        EXPECT_EQ(run.call(OpDispel, crate, { blast }), 1.0);
        EXPECT_EQ(run.call(OpDispelAllSpells, crate), 1.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 1.0);
        EXPECT_TRUE(run.mEnded.empty());
    }

    TEST(FalloutScriptMagicTest, dispelling_everything_ends_spells_powers_and_items_and_keeps_the_rest)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { blast });
        run.call(OpCastImmediateOnSelf, courier, { powerSpell });
        run.call(OpCastImmediateOnSelf, courier, { poisonSpell });
        run.call(OpCastImmediateOnSelf, courier, { addictionSpell });
        run.call(OpAddSpell, courier, { disease });
        run.call(OpAddSpell, courier, { ability });
        run.mMagic.noteEffect(courier, itemEffect, aidItem, 30);
        run.mMagic.noteEffect(courier, itemEffect, ingredient, 30);
        run.mMagic.noteEffect(courier, itemEffect, enchantment, 30);

        EXPECT_EQ(run.call(OpDispelAllSpells, courier), 1.0);
        // A spell, a power, an aid item and an ingredient are ended
        EXPECT_THAT(run.mEnded,
            UnorderedElementsAre(std::make_tuple(courier, blast, fire), std::make_tuple(courier, powerSpell, powerEffect),
                std::make_tuple(courier, aidItem, itemEffect), std::make_tuple(courier, ingredient, itemEffect)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 0.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { powerEffect }), 0.0);
        // A poison, an addiction, a disease, an ability and an enchantment are not
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { poisonEffect }), 1.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { addictionEffect }), 1.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { sickness }), 1.0);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { strengthBoost }), 1.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { enchantment }), 1.0);
        EXPECT_EQ(run.mMagic.dispelAll(courier), 0);
    }

    TEST(FalloutScriptMagicTest, the_usage_of_a_spell_is_the_sum_of_the_magnitudes_of_its_running_effects)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetSpellUsageNum, courier, { pairSpell }), 0.0);
        run.call(OpCastImmediateOnSelf, courier, { pairSpell });
        run.call(OpCastImmediateOnSelf, courier, { blast });
        EXPECT_EQ(run.call(OpGetSpellUsageNum, courier, { pairSpell }), 7.0);
        EXPECT_EQ(run.call(OpGetSpellUsageNum, courier, { blast }), 5.0);
        // An item that was used has the magnitude it started with
        run.mMagic.noteEffect(courier, itemEffect, aidItem, 30, 12);
        EXPECT_EQ(run.mMagic.usage(courier, aidItem), 12.0);
        EXPECT_EQ(run.call(OpGetSpellUsageNum, crate, { blast }), 0.0);

        run.call(OpDispel, courier, { pairSpell });
        EXPECT_EQ(run.call(OpGetSpellUsageNum, courier, { pairSpell }), 0.0);
        run.mMagic.update(11);
        EXPECT_EQ(run.call(OpGetSpellUsageNum, courier, { blast }), 0.0);
    }

    TEST(FalloutScriptMagicTest, the_conditions_of_an_effect_an_ability_keeps_are_asked_again_each_interval)
    {
        Runner run;
        bool holds = false;
        int asked = 0;
        run.mMagic.setConditions([&](const std::vector<ESM4::TargetCondition>&, const FalloutScript::ConditionContext&) {
            ++asked;
            return holds;
        });
        run.call(OpAddSpell, courier, { gatedAbility });
        EXPECT_EQ(asked, 1);
        EXPECT_TRUE(run.mChanges.empty());
        EXPECT_EQ(run.call(OpGetSpellUsageNum, courier, { gatedAbility }), 0.0);

        // Not before the interval (a second) has passed
        run.mMagic.update(0.5);
        EXPECT_EQ(asked, 1);
        run.mMagic.update(0.6);
        EXPECT_EQ(asked, 2);
        EXPECT_TRUE(run.mChanges.empty());

        // It begins when the condition holds, and ends when it stops holding
        holds = true;
        run.mMagic.update(1.2);
        EXPECT_THAT(run.mChanges, ElementsAre(Pair(FalloutScript::ActorValue::strength, 2.0)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedBoost }), 1.0);
        EXPECT_EQ(run.call(OpIsSpellTarget, courier, { gatedAbility }), 1.0);
        run.mMagic.update(1.2);
        EXPECT_EQ(run.mChanges.size(), 1u);

        holds = false;
        run.mMagic.update(1.2);
        EXPECT_THAT(run.mChanges,
            ElementsAre(
                Pair(FalloutScript::ActorValue::strength, 2.0), Pair(FalloutScript::ActorValue::strength, -2.0)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedBoost }), 0.0);
        EXPECT_TRUE(run.mMagic.hasSpell(courier, gatedAbility));

        // The interval is the setting's, and a number that is no interval is a second
        run.mMagic.setConditionInterval(5);
        asked = 0;
        run.mMagic.update(4);
        EXPECT_EQ(asked, 0);
        run.mMagic.update(1);
        EXPECT_EQ(asked, 1);
        run.mMagic.setConditionInterval(0);
        run.mMagic.update(0.2);
        EXPECT_EQ(asked, 1);
        run.mMagic.update(0.2);
        EXPECT_EQ(asked, 2);
    }

    TEST(FalloutScriptMagicTest, evaluating_the_spell_conditions_asks_them_at_once_and_runs_the_script_of_an_effect_again)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        bool holds = false;
        run.mMagic.setConditions([&](const std::vector<ESM4::TargetCondition>&, const FalloutScript::ConditionContext&) {
            return holds;
        });
        run.call(OpAddSpell, courier, { gatedScriptedAbility });
        EXPECT_TRUE(run.mStages.empty());

        holds = true;
        run.call(OpEvaluateSpellConditions, courier);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(gatedScript, Stage::Start)));
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { gatedScriptEffect }), 1.0);
        run.mStages.clear();
        run.call(OpEvaluateSpellConditions, courier);
        EXPECT_TRUE(run.mStages.empty());

        holds = false;
        run.call(OpEvaluateSpellConditions, courier);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(gatedScript, Stage::Finish)));
        run.mStages.clear();

        // An effect that is not on the character has nothing to finish when its spell is taken away
        run.call(OpRemoveSpell, courier, { gatedScriptedAbility });
        EXPECT_TRUE(run.mStages.empty());
        run.call(OpEvaluateSpellConditions, crate);
    }

    TEST(FalloutScriptMagicTest, the_script_of_an_effect_whose_conditions_do_not_hold_is_not_updated)
    {
        using Stage = Magic::ScriptStage;
        Runner run;
        bool holds = false;
        run.mMagic.setConditions([&](const std::vector<ESM4::TargetCondition>&, const FalloutScript::ConditionContext&) {
            return holds;
        });
        // The condition fails when the ability is added: no start, and no update before one
        run.call(OpAddSpell, courier, { gatedScriptedAbility });
        run.mMagic.update(0.4);
        EXPECT_TRUE(run.mStages.empty());

        // It starts when the condition holds and is updated while it does
        holds = true;
        run.mMagic.update(1.2);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(gatedScript, Stage::Start)));
        run.mStages.clear();
        run.mMagic.update(0.2);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(gatedScript, Stage::Update)));
        run.mStages.clear();

        // It finishes when the condition stops holding and is not updated after its finish
        holds = false;
        run.mMagic.update(1.2);
        EXPECT_THAT(run.mStages, ElementsAre(Pair(gatedScript, Stage::Update), Pair(gatedScript, Stage::Finish)));
        run.mStages.clear();
        run.mMagic.update(0.2);
        run.mMagic.update(0.2);
        EXPECT_TRUE(run.mStages.empty());
    }

    TEST(FalloutScriptMagicTest, a_dispelled_effect_that_was_not_on_the_character_changes_and_finishes_nothing)
    {
        Runner run;
        run.mMagic.setConditions(
            [](const std::vector<ESM4::TargetCondition>&, const FalloutScript::ConditionContext&) { return false; });
        run.call(OpAddSpell, courier, { gatedAbility });
        run.call(OpAddSpell, courier, { gatedScriptedAbility });
        EXPECT_EQ(run.mMagic.dispel(courier, gatedAbility), 1);
        EXPECT_EQ(run.mMagic.dispel(courier, gatedScriptedAbility), 1);
        EXPECT_TRUE(run.mChanges.empty());
        EXPECT_TRUE(run.mStages.empty());
    }

    TEST(FalloutScriptMagicTest, the_chosen_spell_is_kept_and_forgotten_with_a_new_game)
    {
        Runner run;
        EXPECT_EQ(run.mMagic.selectedSpell(), 0u);
        EXPECT_EQ(run.call(OpSelectPlayerSpell, courier, { powerSpell }), 1.0);
        EXPECT_EQ(run.mMagic.selectedSpell(), powerSpell);
        run.mMagic.clear();
        EXPECT_EQ(run.mMagic.selectedSpell(), 0u);
    }

    TEST(FalloutScriptMagicTest, a_saved_game_keeps_the_magnitudes_the_conditions_and_the_chosen_spell)
    {
        Runner run;
        run.call(OpCastImmediateOnSelf, courier, { pairSpell });
        run.mMagic.setConditions(
            [](const std::vector<ESM4::TargetCondition>&, const FalloutScript::ConditionContext&) { return false; });
        run.call(OpAddSpell, courier, { gatedAbility });
        run.mMagic.selectSpell(powerSpell);
        FalloutScript::SaveWriter out;
        FalloutScript::writeMagic(run.mMagic, out);

        Runner other;
        bool holds = false;
        other.mMagic.setConditions(
            [&](const std::vector<ESM4::TargetCondition>&, const FalloutScript::ConditionContext&) { return holds; });
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMagic(other.mMagic, in, FalloutScript::magicSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(other.mMagic.selectedSpell(), powerSpell);
        EXPECT_EQ(other.call(OpGetSpellUsageNum, courier, { pairSpell }), 7.0);
        // The effect that was not on the character is still not
        EXPECT_EQ(other.call(OpHasMagicEffect, courier, { gatedBoost }), 0.0);

        // Its condition is asked of the records again, and it begins when it holds
        holds = true;
        other.call(OpEvaluateSpellConditions, courier);
        EXPECT_EQ(other.call(OpHasMagicEffect, courier, { gatedBoost }), 1.0);
        EXPECT_THAT(other.mChanges, ElementsAre(Pair(FalloutScript::ActorValue::strength, 2.0)));
    }

    TEST(FalloutScriptMagicTest, a_saved_game_gives_each_of_two_effects_of_the_same_kind_the_condition_of_its_own)
    {
        // The first effect holds when its condition's number is 2, and only the second does
        const auto holdsFor2 = [](const std::vector<ESM4::TargetCondition>& conditions,
                                   const FalloutScript::ConditionContext&) {
            return !conditions.empty() && conditions.front().comparison == 2.0f;
        };
        Runner run;
        run.mMagic.setConditions(holdsFor2);
        run.call(OpAddSpell, courier, { doubleGatedAbility });
        EXPECT_THAT(run.mChanges, ElementsAre(Pair(FalloutScript::ActorValue::strength, 2.0)));
        FalloutScript::SaveWriter out;
        FalloutScript::writeMagic(run.mMagic, out);

        Runner other;
        other.mMagic.setConditions(holdsFor2);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMagic(other.mMagic, in, FalloutScript::magicSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(other.call(OpHasMagicEffect, courier, { gatedBoost }), 1.0);
        // The conditions are asked again: the effect that was on stays on, with nothing taken away or given twice
        other.call(OpEvaluateSpellConditions, courier);
        EXPECT_TRUE(other.mChanges.empty());
        EXPECT_EQ(other.call(OpGetSpellUsageNum, courier, { doubleGatedAbility }), 2.0);
    }

    TEST(FalloutScriptMagicTest, a_game_saved_before_the_magnitude_was_kept_loads_with_the_effects_on_the_character)
    {
        // The section as version 2 wrote it: no spells and one running effect with a number, and no chosen spell
        FalloutScript::SaveWriter out;
        out.count(0);
        out.count(1);
        out.form(courier);
        out.form(fire);
        out.form(blast);
        out.f64(8);
        out.i32(-1);
        out.f64(0);
        out.u64(7);
        const std::string bytes = out.take();

        Runner run;
        run.mMagic.selectSpell(powerSpell);
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMagic(run.mMagic, in, 2);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(run.mMagic.selectedSpell(), 0u);
        EXPECT_EQ(run.call(OpHasMagicEffect, courier, { fire }), 1.0);
        EXPECT_EQ(run.mMagic.usage(courier, blast), 0.0);
    }

    TEST(FalloutScriptMagicTest, a_dispel_needs_neither_a_runner_nor_a_modifier_nor_an_ender)
    {
        FakeRecords records;
        Magic magic(records);
        magic.castOnSelf(courier, blast);
        magic.castOnSelf(courier, restSpell);
        magic.addSpell(courier, ability);
        EXPECT_EQ(magic.dispelAll(courier), 2);
        EXPECT_EQ(magic.dispel(courier, ability), 1);
        EXPECT_FALSE(magic.hasEffect(courier, strengthBoost));
    }

    TEST(FalloutScriptMagicTest, a_spell_cast_with_a_scale_has_the_magnitude_of_each_effect_multiplied)
    {
        Runner run;
        std::vector<double> told;
        run.mMagic.setCaster([&](FormId, FormId, const std::vector<FalloutScript::SpellEffect>& effects) {
            for (const FalloutScript::SpellEffect& effect : effects)
                told.push_back(effect.mMagnitude);
        });
        run.mMagic.castOnSelf(courier, pairSpell, 0.5);
        EXPECT_THAT(told, ElementsAre(1.5, 2.0));
        EXPECT_EQ(run.mMagic.usage(courier, pairSpell), 3.5);

        // Without a scale the effects are as the record says
        told.clear();
        run.mMagic.castOnSelf(courier, blast);
        EXPECT_THAT(told, ElementsAre(5.0));
        EXPECT_EQ(run.mMagic.usage(courier, blast), 5.0);
    }
}
