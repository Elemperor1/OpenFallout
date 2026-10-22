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
    constexpr FormId notASpell = 0x00100099;
    constexpr FormId strengthBoost = 0x00200001;
    constexpr FormId sickness = 0x00200002;
    constexpr FormId fire = 0x00200003;

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

        Runner()
        {
            mMagic.setModifier(
                [this](FormId, int actorValue, double amount) { mChanges.emplace_back(actorValue, amount); });
            mMagic.setCaster([this](FormId, FormId spell) { mCast.push_back(spell); });
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
}
