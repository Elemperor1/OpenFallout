#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/hardcore.hpp>
#include <components/falloutscript/magic.hpp>

#include <gtest/gtest.h>

#include <map>
#include <vector>

namespace
{
    namespace ActorValue = FalloutScript::ActorValue;
    using FalloutScript::ActorValues;
    using FalloutScript::FormId;
    using FalloutScript::HardcoreNeeds;
    using FalloutScript::Magic;
    using FalloutScript::SpellInfo;

    constexpr FormId player = 0x00000014;
    constexpr FormId thirst200 = 0x0015FA1C;
    constexpr FormId thirst400 = 0x0015FA1D;
    constexpr FormId thirst1000 = 0x0015FA20;
    constexpr FormId hunger200 = 0x0015FA21;
    constexpr FormId sleep200 = 0x0015FA26;

    constexpr int dehydration = HardcoreNeeds::firstActorValue + HardcoreNeeds::Dehydration;
    constexpr int hunger = HardcoreNeeds::firstActorValue + HardcoreNeeds::Hunger;
    constexpr int sleepiness = HardcoreNeeds::firstActorValue + HardcoreNeeds::Sleep;

    class FakeSpells : public FalloutScript::MagicRecords
    {
    public:
        std::map<FormId, SpellInfo> mSpells;

        FakeSpells()
        {
            // The stages are diseases that lower values, and the last one an ability that takes all the health
            add(thirst200, 1, { { ActorValue::endurance, 1 } });
            add(thirst400, 1, { { ActorValue::endurance, 2 }, { ActorValue::perception, 1 } });
            add(thirst1000, 4, { { ActorValue::health, 10000 } });
            add(hunger200, 1, { { ActorValue::strength, 1 } });
            add(sleep200, 1, { { ActorValue::agility, 1 } });
        }

        void add(FormId spell, int type, std::vector<std::pair<int, double>> lowers)
        {
            SpellInfo info;
            info.mType = type;
            for (const auto& [value, amount] : lowers)
                info.mEffects.push_back({ spell + 0x1000, 0, value, amount, 0, true });
            mSpells[spell] = info;
        }

        bool spell(FormId spell, SpellInfo& info) override
        {
            const auto found = mSpells.find(spell);
            if (found == mSpells.end())
                return false;
            info = found->second;
            return true;
        }
        bool isActor(FormId reference) override { return reference == player; }
    };

    struct Fixture
    {
        ActorValues mValues;
        FakeSpells mRecords;
        Magic mMagic{ mRecords };
        std::vector<FormId> mTold;
        // The stages are given out of order: they are put in order of the threshold
        HardcoreNeeds mNeeds{ [](int need) {
                                 switch (need)
                                 {
                                     case HardcoreNeeds::Dehydration:
                                         return std::vector<HardcoreNeeds::Stage>{ { 1000, thirst1000 },
                                             { 200, thirst200 }, { 400, thirst400 } };
                                     case HardcoreNeeds::Hunger:
                                         return std::vector<HardcoreNeeds::Stage>{ { 200, hunger200 } };
                                     default:
                                         return std::vector<HardcoreNeeds::Stage>{ { 200, sleep200 } };
                                 }
                             },
            mValues, mMagic };

        Fixture()
        {
            mMagic.setModifier(
                [this](FormId actor, int value, double amount) { mValues.modify(actor, value, amount); });
            mNeeds.setStageListener([this](FormId actor) { mTold.push_back(actor); });
        }

        double level(int value) { return mValues.current(player, value); }
        void raise(int value, double amount) { mValues.addBase(player, value, amount, 0.0); }
    };

    TEST(FalloutScriptHardcoreTest, the_needs_go_up_by_whole_intervals_of_the_scaled_rates_and_the_rest_is_dropped)
    {
        Fixture setup;
        // The first call only says where counting starts from
        setup.mNeeds.update(player, 1000, 30, false);
        EXPECT_EQ(setup.level(dehydration), 0);

        // At the scale of 30 the intervals are 5, 12.5 and 25 game minutes
        setup.mNeeds.update(player, 1004.9, 30, false);
        EXPECT_EQ(setup.level(dehydration), 0);
        setup.mNeeds.update(player, 1060, 30, false);
        // Counting from the first minute, a whole hour gives 12, 4 and 2 points
        EXPECT_EQ(setup.level(dehydration), 12);
        EXPECT_EQ(setup.level(hunger), 4);
        EXPECT_EQ(setup.level(sleepiness), 2);

        // The time that was left over was dropped: two minutes later nothing more has come
        setup.mNeeds.update(player, 1062, 30, false);
        EXPECT_EQ(setup.level(dehydration), 12);
        // A larger scale makes the intervals longer
        setup.mNeeds.update(player, 1060 + 9, 60, false);
        EXPECT_EQ(setup.level(dehydration), 12);
        setup.mNeeds.update(player, 1060 + 10, 60, false);
        EXPECT_EQ(setup.level(dehydration), 13);
    }

    TEST(FalloutScriptHardcoreTest, the_rates_of_the_settings_set_the_intervals)
    {
        Fixture setup;
        setup.mNeeds.setRates(20, 25, 50);
        setup.mNeeds.update(player, 1000, 30, false);
        setup.mNeeds.update(player, 1060, 30, false);
        EXPECT_EQ(setup.level(dehydration), 6);
        EXPECT_EQ(setup.level(hunger), 4);
        EXPECT_EQ(setup.level(sleepiness), 2);
    }

    TEST(FalloutScriptHardcoreTest, a_first_call_or_a_time_that_went_backwards_only_sets_where_counting_starts)
    {
        Fixture setup;
        setup.mNeeds.update(player, 5000, 30, false);
        setup.mNeeds.update(player, 5060, 30, false);
        EXPECT_EQ(setup.level(dehydration), 12);

        // A game that was loaded is at an earlier time: nothing is added for it, and counting goes on from there
        setup.mNeeds.update(player, 100, 30, false);
        EXPECT_EQ(setup.level(dehydration), 12);
        setup.mNeeds.update(player, 105, 30, false);
        EXPECT_EQ(setup.level(dehydration), 13);

        setup.mNeeds.clear();
        setup.mNeeds.update(player, 900, 30, false);
        EXPECT_EQ(setup.level(dehydration), 13);

        // Nothing happens for no one or for a clock that does not run
        setup.mNeeds.update(0, 960, 30, false);
        setup.mNeeds.update(player, 960, 0, false);
        EXPECT_EQ(setup.level(dehydration), 13);
    }

    TEST(FalloutScriptHardcoreTest, sleeping_adds_no_sleep_deprivation_but_the_time_is_counted)
    {
        Fixture setup;
        setup.mNeeds.update(player, 1000, 30, false);
        setup.mNeeds.update(player, 1060, 30, true);
        EXPECT_EQ(setup.level(sleepiness), 0);
        // The other needs go up all the same
        EXPECT_EQ(setup.level(dehydration), 12);
        EXPECT_EQ(setup.level(hunger), 4);

        // The hour that was slept is behind: it does not come back when the player is up
        setup.mNeeds.update(player, 1070, 30, false);
        EXPECT_EQ(setup.level(sleepiness), 0);
        setup.mNeeds.update(player, 1085, 30, false);
        EXPECT_EQ(setup.level(sleepiness), 1);
    }

    TEST(FalloutScriptHardcoreTest, a_stage_puts_its_spell_on_the_player_and_takes_the_one_before_it_off)
    {
        Fixture setup;
        setup.mNeeds.update(player, 1000, 30, false);
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst200));

        setup.raise(dehydration, 200);
        setup.mNeeds.update(player, 1001, 30, false);
        EXPECT_TRUE(setup.mMagic.hasSpell(player, thirst200));
        EXPECT_EQ(setup.level(ActorValue::endurance), ActorValues::startingValue(ActorValue::endurance) - 1);

        setup.raise(dehydration, 250);
        setup.mNeeds.update(player, 1002, 30, false);
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst200));
        EXPECT_TRUE(setup.mMagic.hasSpell(player, thirst400));
        EXPECT_EQ(setup.level(ActorValue::endurance), ActorValues::startingValue(ActorValue::endurance) - 2);
        EXPECT_EQ(setup.level(ActorValue::perception), ActorValues::startingValue(ActorValue::perception) - 1);

        // Drinking takes the level down, and the stage goes with it
        setup.raise(dehydration, -400);
        setup.mNeeds.update(player, 1003, 30, false);
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst400));
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst200));
        EXPECT_EQ(setup.level(ActorValue::endurance), ActorValues::startingValue(ActorValue::endurance));
        EXPECT_EQ(setup.level(ActorValue::perception), ActorValues::startingValue(ActorValue::perception));
        // Each change of stage was told
        EXPECT_EQ(setup.mTold.size(), 3u);
    }

    TEST(FalloutScriptHardcoreTest, the_stages_of_a_need_start_at_their_thresholds_and_the_needs_stand_apart)
    {
        Fixture setup;
        setup.mNeeds.update(player, 1000, 30, false);
        setup.raise(dehydration, 199);
        setup.raise(hunger, 200);
        setup.raise(sleepiness, 199);
        setup.mNeeds.update(player, 1001, 30, false);
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst200));
        EXPECT_TRUE(setup.mMagic.hasSpell(player, hunger200));
        EXPECT_FALSE(setup.mMagic.hasSpell(player, sleep200));

        // One more point is the threshold
        setup.raise(dehydration, 1);
        setup.mNeeds.update(player, 1002, 30, false);
        EXPECT_TRUE(setup.mMagic.hasSpell(player, thirst200));
    }

    TEST(FalloutScriptHardcoreTest, the_last_stage_is_told_so_that_the_health_it_took_can_be_acted_on)
    {
        Fixture setup;
        setup.mNeeds.update(player, 1000, 30, false);
        setup.raise(dehydration, 1000);
        setup.mNeeds.update(player, 1001, 30, false);
        EXPECT_TRUE(setup.mMagic.hasSpell(player, thirst1000));
        EXPECT_LE(setup.level(ActorValue::health), 0);
        ASSERT_EQ(setup.mTold.size(), 1u);
        EXPECT_EQ(setup.mTold.front(), player);

        // The level is the same at the next call: nothing is chosen again
        setup.mNeeds.update(player, 1002, 30, false);
        EXPECT_EQ(setup.mTold.size(), 1u);
    }

    TEST(FalloutScriptHardcoreTest, a_stage_spell_that_a_loaded_game_has_is_taken_off_when_the_level_is_elsewhere)
    {
        Fixture setup;
        // The spell is on the player, as a saved game brings it, and the needs have not seen it
        setup.mMagic.addSpell(player, thirst200);
        setup.raise(dehydration, 450);
        setup.mNeeds.update(player, 1000, 30, false);
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst200));
        EXPECT_TRUE(setup.mMagic.hasSpell(player, thirst400));
        EXPECT_EQ(setup.level(ActorValue::endurance), ActorValues::startingValue(ActorValue::endurance) - 2);
    }

    TEST(FalloutScriptHardcoreTest, an_hour_of_sleep_lowers_the_sleep_deprivation_by_the_amount_or_what_there_is)
    {
        Fixture setup;
        setup.raise(sleepiness, 250);
        setup.mNeeds.update(player, 1000, 30, false);
        EXPECT_TRUE(setup.mMagic.hasSpell(player, sleep200));

        setup.mNeeds.sleepHour(player, 60);
        EXPECT_EQ(setup.level(sleepiness), 190);
        // Below the first threshold the stage is gone with it
        EXPECT_FALSE(setup.mMagic.hasSpell(player, sleep200));
        setup.mNeeds.sleepHour(player, 60);
        setup.mNeeds.sleepHour(player, 60);
        setup.mNeeds.sleepHour(player, 60);
        EXPECT_EQ(setup.level(sleepiness), 10);
        setup.mNeeds.sleepHour(player, 60);
        EXPECT_EQ(setup.level(sleepiness), 0);
        setup.mNeeds.sleepHour(player, 60);
        EXPECT_EQ(setup.level(sleepiness), 0);
        setup.mNeeds.sleepHour(0, 60);
    }

    TEST(FalloutScriptHardcoreTest, the_end_of_the_mode_takes_the_stages_off_and_starts_counting_over)
    {
        Fixture setup;
        setup.raise(dehydration, 450);
        setup.raise(hunger, 250);
        setup.mNeeds.update(player, 1000, 30, false);
        ASSERT_TRUE(setup.mMagic.hasSpell(player, thirst400));
        ASSERT_TRUE(setup.mMagic.hasSpell(player, hunger200));

        setup.mNeeds.end(player);
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst400));
        EXPECT_FALSE(setup.mMagic.hasSpell(player, hunger200));
        EXPECT_EQ(setup.level(ActorValue::strength), ActorValues::startingValue(ActorValue::strength));
        EXPECT_EQ(setup.level(ActorValue::endurance), ActorValues::startingValue(ActorValue::endurance));
        // The levels stay as they are, and the stage comes back with the mode
        EXPECT_EQ(setup.level(dehydration), 450);
        setup.mNeeds.update(player, 1100, 30, false);
        EXPECT_TRUE(setup.mMagic.hasSpell(player, thirst400));
    }

    TEST(FalloutScriptHardcoreTest, there_are_no_stages_to_put_on_when_the_records_have_none)
    {
        ActorValues values;
        FakeSpells records;
        Magic magic(records);
        HardcoreNeeds needs([](int) { return std::vector<HardcoreNeeds::Stage>(); }, values, magic);
        needs.update(player, 1000, 30, false);
        needs.update(player, 1060, 30, false);
        EXPECT_EQ(values.current(player, dehydration), 12);
        needs.end(player);
        needs.sleepHour(player, 60);
        EXPECT_TRUE(magic.spells(player).empty());
    }
}
