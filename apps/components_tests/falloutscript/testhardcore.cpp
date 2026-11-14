#include "scriptbuilder.hpp"

#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/hardcore.hpp>
#include <components/falloutscript/magic.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <vector>

namespace
{
    namespace ActorValue = FalloutScript::ActorValue;
    using FalloutScript::ActorValues;
    using FalloutScript::FormId;
    using FalloutScript::HardcoreMode;
    using FalloutScript::HardcoreNeeds;
    using FalloutScript::NeedsDisplay;
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

    TEST(FalloutScriptHardcoreTest, a_change_of_stage_tells_the_player_and_a_change_inside_a_stage_does_not)
    {
        Fixture setup;
        std::vector<HardcoreNeeds::Notice> notices;
        setup.mNeeds.setNoticeListener([&notices](const HardcoreNeeds::Notice& notice) { notices.push_back(notice); });
        // The first level the needs see is not a change
        setup.raise(dehydration, 150);
        setup.mNeeds.update(player, 1000, 30, false);
        EXPECT_TRUE(notices.empty());
        // Into the first stage
        setup.raise(dehydration, 60);
        setup.mNeeds.update(player, 1001, 30, false);
        ASSERT_EQ(notices.size(), 1u);
        EXPECT_EQ(notices[0].mNeed, HardcoreNeeds::Dehydration);
        EXPECT_TRUE(notices[0].mIncreased);
        EXPECT_EQ(notices[0].mStage, thirst200);
        // Within it, nothing
        setup.raise(dehydration, 50);
        setup.mNeeds.update(player, 1002, 30, false);
        EXPECT_EQ(notices.size(), 1u);
        // On to the next, and back out of every stage
        setup.raise(dehydration, 150);
        setup.mNeeds.update(player, 1003, 30, false);
        ASSERT_EQ(notices.size(), 2u);
        EXPECT_EQ(notices[1].mStage, thirst400);
        setup.raise(dehydration, -500);
        setup.mNeeds.update(player, 1004, 30, false);
        ASSERT_EQ(notices.size(), 3u);
        EXPECT_FALSE(notices[2].mIncreased);
        EXPECT_EQ(notices[2].mStage, 0u);
    }

    TEST(FalloutScriptHardcoreTest, the_needs_are_told_apart_in_the_notices)
    {
        Fixture setup;
        std::vector<HardcoreNeeds::Notice> notices;
        setup.mNeeds.setNoticeListener([&notices](const HardcoreNeeds::Notice& notice) { notices.push_back(notice); });
        setup.mNeeds.update(player, 1000, 30, false);
        setup.raise(hunger, 300);
        setup.raise(sleepiness, 210);
        setup.mNeeds.refresh(player);
        ASSERT_EQ(notices.size(), 2u);
        EXPECT_EQ(notices[0].mNeed, HardcoreNeeds::Hunger);
        EXPECT_EQ(notices[0].mStage, hunger200);
        EXPECT_EQ(notices[1].mNeed, HardcoreNeeds::Sleep);
        EXPECT_EQ(notices[1].mStage, sleep200);
        // Nothing to refresh for nobody
        setup.mNeeds.refresh(0);
        EXPECT_EQ(notices.size(), 2u);
    }

    TEST(FalloutScriptHardcoreTest, a_notice_is_the_word_for_the_level_and_then_the_stage_or_the_end_of_it)
    {
        using Notice = HardcoreNeeds::Notice;
        auto lines = HardcoreNeeds::noticeLines(Notice{ HardcoreNeeds::Dehydration, true, thirst200 });
        ASSERT_EQ(lines.size(), 2u);
        EXPECT_EQ(lines[0].mSetting, "sDehydrationIncrease");
        EXPECT_FALSE(lines[0].mStageName);
        EXPECT_EQ(lines[0].mFace, 3);
        EXPECT_EQ(lines[1].mSetting, "sRadiationSick");
        EXPECT_TRUE(lines[1].mStageName);
        EXPECT_EQ(lines[1].mFace, 3);

        lines = HardcoreNeeds::noticeLines(Notice{ HardcoreNeeds::Hunger, false, 0 });
        ASSERT_EQ(lines.size(), 2u);
        EXPECT_EQ(lines[0].mSetting, "sHungerDecrease");
        EXPECT_EQ(lines[0].mFace, 1);
        EXPECT_EQ(lines[1].mSetting, "sHungerNotSick");
        EXPECT_FALSE(lines[1].mStageName);
        EXPECT_EQ(lines[1].mFace, 1);

        // Going down into a lower stage is still a stage
        lines = HardcoreNeeds::noticeLines(Notice{ HardcoreNeeds::Sleep, false, sleep200 });
        EXPECT_EQ(lines[0].mSetting, "sSleepDeprevationDecrease");
        EXPECT_EQ(lines[1].mSetting, "sRadiationSick");
        EXPECT_STREQ(HardcoreNeeds::settingName(HardcoreNeeds::Sleep), "SleepDeprevation");
    }

    TEST(FalloutScriptHardcoreTest, turning_the_mode_off_takes_every_level_to_nothing_and_tells_the_stages_that_ended)
    {
        Fixture setup;
        std::vector<HardcoreNeeds::Notice> notices;
        setup.mNeeds.setNoticeListener([&notices](const HardcoreNeeds::Notice& notice) { notices.push_back(notice); });
        setup.raise(dehydration, 450);
        setup.raise(hunger, 250);
        setup.raise(sleepiness, 50);
        setup.mNeeds.update(player, 1000, 30, false);
        ASSERT_TRUE(setup.mMagic.hasSpell(player, thirst400));
        notices.clear();

        setup.mNeeds.reset(player);
        EXPECT_EQ(setup.level(dehydration), 0);
        EXPECT_EQ(setup.level(hunger), 0);
        EXPECT_EQ(setup.level(sleepiness), 0);
        EXPECT_FALSE(setup.mMagic.hasSpell(player, thirst400));
        EXPECT_FALSE(setup.mMagic.hasSpell(player, hunger200));
        // The two needs that were in a stage say so, the one that was not stays quiet
        ASSERT_EQ(notices.size(), 2u);
        EXPECT_FALSE(notices[0].mIncreased);
        EXPECT_EQ(notices[0].mStage, 0u);
        // And counting starts over
        setup.mNeeds.update(player, 2000, 30, false);
        EXPECT_EQ(setup.level(dehydration), 0);
        EXPECT_EQ(setup.level(ActorValue::endurance), ActorValues::startingValue(ActorValue::endurance));
        setup.mNeeds.reset(0);
    }

    TEST(FalloutScriptHardcoreTest, the_mode_is_on_for_exactly_one_and_is_always_hardcore_until_it_is_turned_off)
    {
        HardcoreMode mode;
        // A new game: not hardcore, and nothing has turned it off
        EXPECT_FALSE(mode.on());
        EXPECT_TRUE(mode.always());
        bool zero = true;
        EXPECT_TRUE(mode.command(1, zero));
        EXPECT_FALSE(zero);
        EXPECT_TRUE(mode.on());
        EXPECT_TRUE(mode.always());
        // Off again: the needs are zeroed and the tracking is gone
        EXPECT_TRUE(mode.command(0, zero));
        EXPECT_TRUE(zero);
        EXPECT_FALSE(mode.on());
        EXPECT_FALSE(mode.always());
        // The tracking does not come back with the mode
        mode.command(1, zero);
        EXPECT_TRUE(mode.on());
        EXPECT_FALSE(mode.always());
        mode.reset();
        EXPECT_FALSE(mode.on());
        EXPECT_TRUE(mode.always());
    }

    TEST(FalloutScriptHardcoreTest, a_value_that_is_not_zero_or_one_is_refused_by_the_command_and_kept_by_the_setter)
    {
        HardcoreMode mode;
        bool zero = true;
        EXPECT_FALSE(mode.command(2, zero));
        EXPECT_FALSE(zero);
        EXPECT_FALSE(mode.command(-1, zero));
        EXPECT_FALSE(mode.on());
        EXPECT_TRUE(mode.always());
        // The setter the other code of the game calls keeps what it is given, and only 1 is hardcore
        EXPECT_FALSE(mode.set(2));
        EXPECT_EQ(mode.value(), 2);
        EXPECT_FALSE(mode.on());
        EXPECT_FALSE(mode.set(1));
        EXPECT_TRUE(mode.on());
    }

    TEST(FalloutScriptHardcoreTest, a_load_does_not_zero_the_needs_or_turn_the_tracking_off)
    {
        HardcoreMode mode;
        mode.set(1);
        EXPECT_FALSE(mode.set(0, true));
        EXPECT_TRUE(mode.always());
        EXPECT_FALSE(mode.on());
        // Off by anything but a load turns the tracking off and zeroes the needs
        EXPECT_TRUE(mode.set(0, false));
        EXPECT_FALSE(mode.always());
    }

    TEST(FalloutScriptHardcoreTest, disabling_the_tracking_makes_always_hardcore_answer_nothing)
    {
        HardcoreMode mode;
        mode.set(1);
        mode.disableTracking();
        EXPECT_TRUE(mode.on());
        EXPECT_FALSE(mode.always());
    }

    TEST(FalloutScriptHardcoreTest, the_mode_comes_back_with_a_saved_game)
    {
        HardcoreMode mode;
        mode.set(1);
        FalloutScript::SaveWriter out;
        mode.write(out);
        const std::string bytes = out.take();
        HardcoreMode loaded;
        FalloutScript::SaveReader in(bytes);
        loaded.read(in, HardcoreMode::savedSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_TRUE(loaded.on());
        EXPECT_TRUE(loaded.always());

        mode.disableTracking();
        FalloutScript::SaveWriter second;
        mode.write(second);
        const std::string again = second.take();
        FalloutScript::SaveReader reader(again);
        loaded.read(reader, HardcoreMode::savedSectionVersion);
        EXPECT_TRUE(loaded.on());
        EXPECT_FALSE(loaded.always());

        // A save of a game that was not hardcore, and one that was cut short
        HardcoreMode off;
        FalloutScript::SaveWriter plain;
        off.write(plain);
        const std::string plainBytes = plain.take();
        FalloutScript::SaveReader plainReader(plainBytes);
        loaded.read(plainReader, HardcoreMode::savedSectionVersion);
        EXPECT_FALSE(loaded.on());
        const std::string cut = plainBytes.substr(0, 2);
        FalloutScript::SaveReader cutReader(cut);
        EXPECT_THROW(loaded.read(cutReader, HardcoreMode::savedSectionVersion), FalloutScript::SaveError);
    }

    struct CommandRunner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        FalloutScriptTest::TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        HardcoreMode mMode;
        int mZeroed = 0;

        CommandRunner()
        {
            FalloutScript::addHardcoreCommands(mInterpreter, mMode, [this] { ++mZeroed; });
        }

        double call(std::uint16_t opcode, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, 0, false, std::move(arguments));
        }
    };

    constexpr std::uint16_t OpIsHardcore = 0x124A;
    constexpr std::uint16_t OpSetHardcore = 0x124B;
    constexpr std::uint16_t OpDisableHardcoreTracking = 0x126A;
    constexpr std::uint16_t OpIsAlwaysHardcore = 0x126B;

    TEST(FalloutScriptHardcoreTest, the_commands_set_and_ask_the_mode)
    {
        CommandRunner run;
        EXPECT_EQ(run.call(OpIsHardcore), 0.0);
        EXPECT_EQ(run.call(OpIsAlwaysHardcore), 1.0);
        EXPECT_EQ(run.call(OpSetHardcore, { 1 }), 1.0);
        EXPECT_EQ(run.call(OpIsHardcore), 1.0);
        EXPECT_EQ(run.mZeroed, 0);
        EXPECT_EQ(run.call(OpIsAlwaysHardcore), 1.0);
        EXPECT_EQ(run.call(OpSetHardcore, { 0 }), 1.0);
        EXPECT_EQ(run.call(OpIsHardcore), 0.0);
        EXPECT_EQ(run.call(OpIsAlwaysHardcore), 0.0);
        EXPECT_EQ(run.mZeroed, 1);
    }

    TEST(FalloutScriptHardcoreTest, a_value_the_command_does_not_take_changes_nothing_and_says_so)
    {
        CommandRunner run;
        run.call(OpSetHardcore, { 1 });
        EXPECT_EQ(run.call(OpSetHardcore, { 7 }), 0.0);
        EXPECT_EQ(run.call(OpIsHardcore), 1.0);
        EXPECT_EQ(run.mZeroed, 0);
        ASSERT_EQ(run.mHost.mLog.size(), 1u);
        EXPECT_THAT(run.mHost.mLog[0], testing::HasSubstr("SetHardcore"));
    }

    TEST(FalloutScriptHardcoreTest, the_command_with_no_argument_is_on_and_the_tracking_command_turns_it_off)
    {
        CommandRunner run;
        run.call(OpSetHardcore);
        EXPECT_EQ(run.call(OpIsHardcore), 1.0);
        run.call(OpDisableHardcoreTracking);
        EXPECT_EQ(run.call(OpIsAlwaysHardcore), 0.0);
        EXPECT_EQ(run.call(OpIsHardcore), 1.0);
    }

    TEST(FalloutScriptHardcoreTest, the_display_gives_a_need_a_tile_by_the_level_it_has_reached)
    {
        NeedsDisplay display;
        EXPECT_EQ(display.tile(0).mAlpha, 0);
        EXPECT_EQ(display.tile(199).mAlpha, 0);
        EXPECT_EQ(display.tile(200).mAlpha, 0x7C);
        EXPECT_EQ(display.tile(200).mColor, 1);
        EXPECT_EQ(display.tile(399).mAlpha, 0x7C);
        EXPECT_EQ(display.tile(400).mAlpha, 255);
        EXPECT_EQ(display.tile(400).mColor, 1);
        EXPECT_EQ(display.tile(599).mColor, 1);
        EXPECT_EQ(display.tile(600).mAlpha, 255);
        EXPECT_EQ(display.tile(600).mColor, 2);
        EXPECT_EQ(display.tile(799).mAlpha, 255);
        EXPECT_EQ(display.tile(799).mColor, 2);
        // From the flash threshold the alpha is the flashing one
        EXPECT_EQ(display.tile(800).mColor, 2);
        EXPECT_EQ(display.tile(1000).mAlpha, static_cast<int>(display.flash()));
    }

    TEST(FalloutScriptHardcoreTest, the_thresholds_of_the_settings_move_the_tiles)
    {
        NeedsDisplay display;
        display.setThresholds({ 10, 20, 30, 40 });
        EXPECT_EQ(display.tile(9).mAlpha, 0);
        EXPECT_EQ(display.tile(10).mAlpha, 0x7C);
        EXPECT_EQ(display.tile(30).mColor, 2);
        EXPECT_EQ(display.thresholds().mFlash, 40);
    }

    TEST(FalloutScriptHardcoreTest, the_flash_runs_between_128_and_255_at_half_a_unit_each_millisecond)
    {
        NeedsDisplay display;
        // The first call sets the time, and the flash starts at its top, going down
        display.advance(1000);
        EXPECT_DOUBLE_EQ(display.flash(), 255.0);
        display.advance(1100);
        EXPECT_DOUBLE_EQ(display.flash(), 205.0);
        // To the bottom, where it turns
        display.advance(1400);
        EXPECT_DOUBLE_EQ(display.flash(), 128.0);
        display.advance(1500);
        EXPECT_DOUBLE_EQ(display.flash(), 178.0);
        // Up to the top and back: a long step is held between the two
        display.advance(1900);
        EXPECT_DOUBLE_EQ(display.flash(), 255.0);
        display.advance(1950);
        EXPECT_DOUBLE_EQ(display.flash(), 230.0);
    }

    TEST(FalloutScriptHardcoreTest, the_tile_of_the_limbs_goes_by_the_number_of_crippled_parts)
    {
        const NeedsDisplay display;
        EXPECT_EQ(display.limbLevel(0), 0.0);
        EXPECT_EQ(display.limbLevel(1), 201.0);
        EXPECT_EQ(display.limbLevel(2), 401.0);
        EXPECT_EQ(display.limbLevel(3), 601.0);
        EXPECT_EQ(display.limbLevel(4), 801.0);
        EXPECT_EQ(display.limbLevel(6), 801.0);
        // One part is the half alpha, two the full one, three red, four and more flashing red
        EXPECT_EQ(display.tile(display.limbLevel(1)).mAlpha, 0x7C);
        EXPECT_EQ(display.tile(display.limbLevel(2)).mAlpha, 255);
        EXPECT_EQ(display.tile(display.limbLevel(2)).mColor, 1);
        EXPECT_EQ(display.tile(display.limbLevel(3)).mColor, 2);
        EXPECT_EQ(display.tile(display.limbLevel(4)).mColor, 2);
        EXPECT_EQ(display.tile(display.limbLevel(0)).mAlpha, 0);
    }

    TEST(FalloutScriptHardcoreTest, ammunition_weighs_only_in_the_hardcore_mode)
    {
        EXPECT_EQ(FalloutScript::carriedAmmoWeight(true, 0.5), 0.5);
        EXPECT_EQ(FalloutScript::carriedAmmoWeight(false, 0.5), 0.0);
        EXPECT_EQ(FalloutScript::carriedAmmoWeight(true, 0.0), 0.0);
    }

    TEST(FalloutScriptHardcoreTest, a_companion_can_die_only_in_the_hardcore_mode)
    {
        EXPECT_TRUE(FalloutScript::companionIsEssential(true, false));
        EXPECT_FALSE(FalloutScript::companionIsEssential(true, true));
        // The others are not made essential by the mode
        EXPECT_FALSE(FalloutScript::companionIsEssential(false, false));
        EXPECT_FALSE(FalloutScript::companionIsEssential(false, true));
    }
}
