#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/playerprogress.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::ActorState;
    using FalloutScript::ActorValues;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    using FalloutScript::PlayerProgress;
    namespace ActorValue = FalloutScript::ActorValue;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId guard = 0x01000100;
    constexpr FormId thePlayer = 0x00000014;
    constexpr FormId scriptOfGuards = 0x01002000;

    // The numbers of some of the actor values in the games
    constexpr int variable01 = 62;
    constexpr int perceptionCondition = 25;
    constexpr int barter = 32;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpGetActorValue = 0x100E;
    constexpr std::uint16_t OpSetActorValue = 0x100F;
    constexpr std::uint16_t OpModActorValue = 0x1010;
    constexpr std::uint16_t OpGetDead = 0x102E;
    constexpr std::uint16_t OpGetLevel = 0x1050;
    constexpr std::uint16_t OpKillActor = 0x108B;
    constexpr std::uint16_t OpResurrectActor = 0x108C;
    constexpr std::uint16_t OpGetBaseActorValue = 0x1115;
    constexpr std::uint16_t OpForceActorValue = 0x110E;
    constexpr std::uint16_t OpGetHealthPercentage = 0x11AF;
    constexpr std::uint16_t OpGetPermanentActorValue = 0x11EF;
    constexpr std::uint16_t OpGetWeaponHealthPerc = 0x11F4;
    constexpr std::uint16_t OpDamageActorValue = 0x1181;
    constexpr std::uint16_t OpRestoreActorValue = 0x1182;
    constexpr std::uint16_t OpRewardXP = 0x1177;

    class NoRecords : public FalloutScript::ActorRecords
    {
    public:
        bool initialItems(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
        bool initialFactions(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
    };

    // Records that give the guard level 7 and everyone next to it a level above the player's
    class LevelRecords : public FalloutScript::ActorValueRecords
    {
    public:
        bool initialValues(FormId, std::vector<std::pair<int, double>>&) override { return true; }
        bool levelOf(FormId actor, int playerLevel, int& level) override
        {
            if (actor == guard)
                level = 7;
            else if (actor == guard + 1)
                level = playerLevel + 1;
            else
                return false;
            return true;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        NoRecords mRecords;
        ActorState mActors{ mRecords };
        LevelRecords mLevels;
        ActorValues mValues{ &mLevels };
        PlayerProgress mProgress;

        Runner()
        {
            FalloutScript::addActorCommands(mInterpreter, mActors);
            FalloutScript::addPlayerProgressCommands(mInterpreter, mProgress);
            FalloutScript::addActorValueCommands(mInterpreter, mValues, mActors, mProgress);
        }

        /// Runs the script in the OnActivate block of the object `owner`
        void run(FormId owner, const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGuards];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards);
            record.mScript = builder.definition();
            mObjects.add(owner, scriptOfGuards);
            mObjects.trigger(owner, BlockType::OnActivate);
        }

        double variable(FormId owner, std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(owner);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    // The first argument of the commands is the number of the actor value, written bare in two bytes
    Bytes valueOnly(int value)
    {
        return arguments(1).u16(static_cast<std::uint16_t>(value));
    }

    Bytes valueAndInteger(int value, std::int32_t amount)
    {
        return arguments(2).u16(static_cast<std::uint16_t>(value)).add(intArg(amount));
    }

    Bytes valueAndFloat(int value, double amount)
    {
        return arguments(2).u16(static_cast<std::uint16_t>(value)).add(floatArg(amount));
    }

    TEST(FalloutScriptActorValuesTest, an_actor_value_nobody_set_starts_at_what_a_fresh_character_has)
    {
        ActorValues values;

        EXPECT_EQ(values.current(guard, ActorValue::strength), 5);
        EXPECT_EQ(values.current(guard, ActorValue::luck), 5);
        EXPECT_EQ(values.current(guard, ActorValue::health), 100);
        EXPECT_EQ(values.current(guard, ActorValue::fatigue), 100);
        EXPECT_EQ(values.current(guard, perceptionCondition), 100);
        EXPECT_EQ(values.current(guard, barter), 15);
        EXPECT_EQ(values.current(guard, variable01), 0);
        EXPECT_EQ(values.current(guard, ActorValue::karma), 0);
    }

    // Records that give the guard stronger arms and a stronger heart, and count how often they are asked
    class GuardRecords : public FalloutScript::ActorValueRecords
    {
    public:
        bool initialValues(FormId actor, std::vector<std::pair<int, double>>& values) override
        {
            ++mAsked;
            if (!mKnown || actor != guard)
                return mKnown;
            values = { { ActorValue::strength, 8 }, { ActorValue::health, 250 }, { barter, 60 }, { 4000, 1 } };
            return true;
        }

        bool mKnown = true;
        int mAsked = 0;
    };

    TEST(FalloutScriptActorValuesTest, an_actor_value_nobody_set_starts_at_what_the_record_says)
    {
        GuardRecords records;
        ActorValues values(&records);

        EXPECT_EQ(values.current(guard, ActorValue::strength), 8);
        EXPECT_EQ(values.base(guard, ActorValue::health), 250);
        EXPECT_EQ(values.permanent(guard, barter), 60);
        // what the record does not say is a fresh character's
        EXPECT_EQ(values.current(guard, ActorValue::luck), 5);
        EXPECT_EQ(values.current(guard, variable01), 0);
        // a character with no values in its record, and a number that is not an actor value
        EXPECT_EQ(values.current(guard + 1, ActorValue::strength), 5);
        EXPECT_EQ(values.current(guard, 4000), 0);
    }

    TEST(FalloutScriptActorValuesTest, the_record_is_asked_once_and_what_scripts_did_goes_on_top_of_it)
    {
        GuardRecords records;
        ActorValues values(&records);

        values.damage(guard, ActorValue::health, 50);
        values.modify(guard, ActorValue::strength, 2);
        EXPECT_EQ(values.current(guard, ActorValue::health), 200);
        EXPECT_EQ(values.current(guard, ActorValue::strength), 10);
        values.setBase(guard, ActorValue::strength, 3);
        EXPECT_EQ(values.current(guard, ActorValue::strength), 5);
        EXPECT_EQ(values.base(guard, barter), 60);
        EXPECT_EQ(records.mAsked, 1);

        // a new game asks again
        values.clear();
        EXPECT_EQ(values.current(guard, ActorValue::strength), 8);
        EXPECT_EQ(records.mAsked, 2);
    }

    TEST(FalloutScriptActorValuesTest, a_reference_the_host_can_not_tell_is_asked_again)
    {
        GuardRecords records;
        records.mKnown = false;
        ActorValues values(&records);

        EXPECT_EQ(values.current(guard, ActorValue::strength), 5);
        EXPECT_EQ(values.current(guard, ActorValue::strength), 5);
        EXPECT_EQ(records.mAsked, 2);

        records.mKnown = true;
        EXPECT_EQ(values.current(guard, ActorValue::strength), 8);
        EXPECT_EQ(values.current(guard, ActorValue::health), 250);
        EXPECT_EQ(records.mAsked, 3);
    }

    TEST(FalloutScriptActorValuesTest, the_value_is_the_base_and_the_modifier_less_the_damage)
    {
        ActorValues values;

        values.setBase(guard, variable01, 10);
        values.modify(guard, variable01, 5);
        values.damage(guard, variable01, 4);

        EXPECT_EQ(values.base(guard, variable01), 10);
        EXPECT_EQ(values.permanent(guard, variable01), 15);
        EXPECT_EQ(values.current(guard, variable01), 11);
        // Another character and another actor value are not changed
        EXPECT_EQ(values.current(thePlayer, variable01), 0);
        EXPECT_EQ(values.current(guard, variable01 + 1), 0);
    }

    TEST(FalloutScriptActorValuesTest, restoring_takes_damage_back_but_does_not_add_to_the_value)
    {
        ActorValues values;

        values.damage(guard, ActorValue::health, 30);
        values.restore(guard, ActorValue::health, 10);
        EXPECT_EQ(values.current(guard, ActorValue::health), 80);
        values.restore(guard, ActorValue::health, 500);
        EXPECT_EQ(values.current(guard, ActorValue::health), 100);
        // Nothing was damaged, so there is nothing to take back
        values.restore(guard, ActorValue::fatigue, 20);
        EXPECT_EQ(values.current(guard, ActorValue::fatigue), 100);
    }

    TEST(FalloutScriptActorValuesTest, forcing_a_value_makes_it_the_current_one_whatever_changed_it)
    {
        ActorValues values;
        values.setBase(guard, variable01, 10);
        values.modify(guard, variable01, 5);
        values.damage(guard, variable01, 4);

        values.setCurrent(guard, variable01, 50);

        EXPECT_EQ(values.current(guard, variable01), 50);
        // The base and the damage stay, the permanent modifier moved
        EXPECT_EQ(values.base(guard, variable01), 10);
        EXPECT_EQ(values.permanent(guard, variable01), 54);
        // Forcing a value nobody set moves the modifier from the starting value
        values.setCurrent(guard, ActorValue::health, 30);
        EXPECT_EQ(values.base(guard, ActorValue::health), 100);
        EXPECT_EQ(values.current(guard, ActorValue::health), 30);
    }

    TEST(FalloutScriptActorValuesTest, damage_is_never_less_than_none)
    {
        ActorValues values;
        values.damage(guard, ActorValue::health, 10);

        values.damage(guard, ActorValue::health, -20);
        EXPECT_EQ(values.current(guard, ActorValue::health), 100);
        values.damage(guard, ActorValue::health, 5);
        EXPECT_EQ(values.current(guard, ActorValue::health), 95);
        values.restore(guard, ActorValue::health, 50);
        EXPECT_EQ(values.current(guard, ActorValue::health), 100);
    }

    TEST(FalloutScriptActorValuesTest, healing_takes_all_the_damage_back)
    {
        ActorValues values;
        values.damage(guard, ActorValue::health, 250);

        EXPECT_EQ(values.current(guard, ActorValue::health), -150);
        values.heal(guard, ActorValue::health);
        EXPECT_EQ(values.current(guard, ActorValue::health), 100);
    }

    TEST(FalloutScriptActorValuesTest, healing_brings_back_a_value_that_a_script_took_away_altogether)
    {
        ActorValues values;
        values.setCurrent(guard, ActorValue::health, 0);
        ASSERT_EQ(values.current(guard, ActorValue::health), 0);

        values.heal(guard, ActorValue::health);

        EXPECT_EQ(values.current(guard, ActorValue::health), 100);
        // a modifier that leaves something of the value stays
        values.modify(guard, ActorValue::health, 20);
        values.damage(guard, ActorValue::health, 50);
        values.heal(guard, ActorValue::health);
        EXPECT_EQ(values.current(guard, ActorValue::health), 120);
    }

    TEST(FalloutScriptActorValuesTest, nobody_and_numbers_that_are_no_actor_values_change_nothing)
    {
        ActorValues values;

        values.setBase(0, variable01, 10);
        values.setBase(guard, -1, 10);
        values.setBase(guard, ActorValue::count, 10);
        values.modify(guard, 5000, 10);

        EXPECT_EQ(values.current(0, variable01), 0);
        EXPECT_FALSE(ActorValues::isValid(-1));
        EXPECT_FALSE(ActorValues::isValid(ActorValue::count));
        EXPECT_TRUE(ActorValues::isValid(0));
    }

    TEST(FalloutScriptActorValuesTest, amounts_that_are_not_numbers_stay_numbers)
    {
        ActorValues values;

        values.setBase(guard, variable01, std::numeric_limits<double>::quiet_NaN());
        EXPECT_EQ(values.current(guard, variable01), 0);
        values.modify(guard, variable01, std::numeric_limits<double>::infinity());
        values.modify(guard, variable01, std::numeric_limits<double>::infinity());
        values.damage(guard, variable01, -std::numeric_limits<double>::infinity());
        EXPECT_TRUE(std::isfinite(values.current(guard, variable01)));
    }

    TEST(FalloutScriptActorValueCommandsTest, a_script_sets_changes_hurts_and_heals_an_actor_value)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetActorValue, valueOnly(variable01)))
            + command(OpSetActorValue, valueAndInteger(variable01, 7))
            + command(OpModActorValue, valueAndInteger(variable01, 3))
            + setTo(var('f', 2), callToken(OpGetActorValue, valueOnly(variable01)))
            + setTo(var('f', 3), callToken(OpGetBaseActorValue, valueOnly(variable01)))
            + command(OpDamageActorValue, valueAndFloat(variable01, 4))
            + setTo(var('f', 4), callToken(OpGetActorValue, valueOnly(variable01)))
            + setTo(var('f', 5), callToken(OpGetPermanentActorValue, valueOnly(variable01)))
            + command(OpRestoreActorValue, valueAndFloat(variable01, 100))
            + setTo(var('f', 6), callToken(OpGetActorValue, valueOnly(variable01))) + end());
        builder.variable(1, 0, "before")
            .variable(2, 0, "set and changed")
            .variable(3, 0, "base")
            .variable(4, 0, "hurt")
            .variable(5, 0, "permanent")
            .variable(6, 0, "healed");

        setup.run(guard, builder);

        EXPECT_EQ(setup.variable(guard, 1), 0);
        EXPECT_EQ(setup.variable(guard, 2), 10);
        EXPECT_EQ(setup.variable(guard, 3), 7);
        EXPECT_EQ(setup.variable(guard, 4), 6);
        EXPECT_EQ(setup.variable(guard, 5), 10);
        EXPECT_EQ(setup.variable(guard, 6), 10);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorValueCommandsTest, forcing_a_value_makes_it_the_current_one)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpModActorValue, valueAndInteger(barter, 10))
            + command(OpDamageActorValue, valueAndFloat(barter, 5))
            + command(OpForceActorValue, valueAndInteger(barter, 60))
            + setTo(var('f', 1), callToken(OpGetActorValue, valueOnly(barter))) + end());
        builder.variable(1, 0, "forced");

        setup.run(guard, builder);

        EXPECT_EQ(setup.variable(guard, 1), 60);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorValueCommandsTest, a_number_that_is_no_actor_value_answers_nothing_and_changes_nothing)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpSetActorValue, valueAndInteger(5000, 9))
            + setTo(var('f', 1), callToken(OpGetActorValue, valueOnly(5000))) + end());
        builder.variable(1, 0, "answer");

        setup.run(guard, builder);

        EXPECT_EQ(setup.variable(guard, 1), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorValueCommandsTest, damage_takes_a_share_of_the_health_and_enough_of_it_kills)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetHealthPercentage, arguments(0)))
            + command(OpDamageActorValue, valueAndFloat(ActorValue::health, 25))
            + setTo(var('f', 2), callToken(OpGetHealthPercentage, arguments(0)))
            + setTo(var('s', 3), callToken(OpGetDead, arguments(0)))
            + command(OpDamageActorValue, valueAndFloat(ActorValue::health, 75))
            + setTo(var('f', 4), callToken(OpGetHealthPercentage, arguments(0)))
            + setTo(var('s', 5), callToken(OpGetDead, arguments(0)))
            + setTo(var('f', 6), callToken(OpGetActorValue, valueOnly(ActorValue::health)))
            + command(OpResurrectActor, arguments(0))
            + setTo(var('f', 7), callToken(OpGetHealthPercentage, arguments(0)))
            + setTo(var('s', 8), callToken(OpGetDead, arguments(0))) + end());
        builder.variable(1, 0, "all")
            .variable(2, 0, "three quarters")
            .variable(3, 1, "alive")
            .variable(4, 0, "none")
            .variable(5, 1, "dead")
            .variable(6, 0, "health of the dead")
            .variable(7, 0, "back")
            .variable(8, 1, "alive again");

        setup.run(guard, builder);

        EXPECT_FLOAT_EQ(setup.variable(guard, 1), 1.0f);
        EXPECT_FLOAT_EQ(setup.variable(guard, 2), 0.75f);
        EXPECT_EQ(setup.variable(guard, 3), 0);
        EXPECT_EQ(setup.variable(guard, 4), 0);
        EXPECT_EQ(setup.variable(guard, 5), 1);
        EXPECT_EQ(setup.variable(guard, 6), 0);
        EXPECT_FLOAT_EQ(setup.variable(guard, 7), 1.0f);
        EXPECT_EQ(setup.variable(guard, 8), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorValueCommandsTest, a_character_killed_by_a_script_has_no_health_left)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpKillActor, arguments(0))
            + setTo(var('f', 1), callToken(OpGetActorValue, valueOnly(ActorValue::health)))
            + setTo(var('f', 2), callToken(OpGetHealthPercentage, arguments(0))) + end());
        builder.variable(1, 0, "health").variable(2, 0, "share");

        setup.run(guard, builder);

        EXPECT_EQ(setup.variable(guard, 1), 0);
        EXPECT_EQ(setup.variable(guard, 2), 0);
    }

    TEST(FalloutScriptActorValueCommandsTest, the_karma_and_experience_of_the_player_are_what_the_story_rewards)
    {
        Runner setup;
        setup.mProgress.rewardKarma(-40);
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetActorValue, valueOnly(ActorValue::karma)))
            + command(OpSetActorValue, valueAndInteger(ActorValue::karma, 100))
            + command(OpModActorValue, valueAndInteger(ActorValue::karma, -30))
            + setTo(var('f', 2), callToken(OpGetActorValue, valueOnly(ActorValue::karma)))
            + command(OpRewardXP, arguments(1).add(intArg(250)))
            + setTo(var('f', 3), callToken(OpGetActorValue, valueOnly(ActorValue::experience)))
            + command(OpForceActorValue, valueAndInteger(ActorValue::experience, 100))
            + setTo(var('f', 4), callToken(OpGetActorValue, valueOnly(ActorValue::experience))) + end());
        builder.variable(1, 0, "before").variable(2, 0, "karma").variable(3, 0, "xp").variable(4, 0, "forced xp");

        setup.run(thePlayer, builder);

        EXPECT_EQ(setup.variable(thePlayer, 1), -40);
        EXPECT_EQ(setup.variable(thePlayer, 2), 70);
        EXPECT_EQ(setup.variable(thePlayer, 3), 250);
        EXPECT_EQ(setup.variable(thePlayer, 4), 100);
        EXPECT_EQ(setup.mProgress.karma(), 70);
        EXPECT_EQ(setup.mProgress.experience(), 100);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorValueCommandsTest, the_karma_of_anyone_else_is_their_own)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSetActorValue, valueAndInteger(ActorValue::karma, 30))
            + setTo(var('f', 1), callToken(OpGetActorValue, valueOnly(ActorValue::karma))) + end());
        builder.variable(1, 0, "karma");

        setup.run(guard, builder);

        EXPECT_EQ(setup.variable(guard, 1), 30);
        EXPECT_EQ(setup.mProgress.karma(), 0);
    }

    TEST(FalloutScriptActorValueCommandsTest, a_weapon_that_nothing_wore_out_is_as_good_as_new)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetWeaponHealthPerc, arguments(0))) + end());
        builder.variable(1, 0, "weapon");

        setup.run(thePlayer, builder);

        EXPECT_EQ(setup.variable(thePlayer, 1), 100);
    }

    TEST(FalloutScriptActorValueCommandsTest,
        the_level_of_the_player_follows_its_experience_and_anyone_elses_is_the_records)
    {
        Runner setup;
        setup.mProgress.rewardExperience(250);
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetLevel, arguments(0))) + end());
        builder.variable(1, 0, "level");

        setup.run(thePlayer, builder);
        EXPECT_EQ(setup.variable(thePlayer, 1), 2);
        setup.run(guard, builder);
        EXPECT_EQ(setup.variable(guard, 1), 7);
        // a share of the level of the player is the records' to work out
        setup.run(guard + 1, builder);
        EXPECT_EQ(setup.variable(guard + 1, 1), 3);
        // a reference the records do not know as a character has no level
        setup.run(guard + 2, builder);
        EXPECT_EQ(setup.variable(guard + 2, 1), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorValueCommandsTest, without_records_no_one_but_the_player_has_a_level)
    {
        ActorValues values;

        EXPECT_FALSE(values.levelOf(guard, 4));
        EXPECT_FALSE(values.levelOf(0, 4));
    }
}
