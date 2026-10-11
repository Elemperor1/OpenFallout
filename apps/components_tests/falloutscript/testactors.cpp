#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::ActorState;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId guard = 0x01000100;
    constexpr FormId crate = 0x01000101;
    constexpr FormId player = 0x01000014;
    constexpr FormId scriptOfGuards = 0x01002000;
    constexpr FormId caps = 0x0100F001;
    constexpr FormId stimpak = 0x0100F002;
    constexpr FormId ncr = 0x01004000;
    constexpr FormId legion = 0x01004001;

    constexpr std::uint32_t integerVariable = 1;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpAddItem = 0x1002;
    constexpr std::uint16_t OpGetDead = 0x102E;
    constexpr std::uint16_t OpGetItemCount = 0x102F;
    constexpr std::uint16_t OpGetInFaction = 0x1047;
    constexpr std::uint16_t OpGetFactionRank = 0x1049;
    constexpr std::uint16_t OpRemoveItem = 0x1052;
    constexpr std::uint16_t OpSetFactionRank = 0x1089;
    constexpr std::uint16_t OpKillActor = 0x108B;
    constexpr std::uint16_t OpResurrectActor = 0x108C;
    constexpr std::uint16_t OpAddToFaction = 0x117F;
    constexpr std::uint16_t OpRemoveFromFaction = 0x1180;

    class FakeRecords : public FalloutScript::ActorRecords
    {
    public:
        std::map<FormId, std::vector<std::pair<FormId, int>>> mItems;
        std::map<FormId, std::vector<std::pair<FormId, int>>> mFactions;
        std::set<FormId> mUnknown;
        int mItemQuestions = 0;

        bool initialItems(FormId holder, std::vector<std::pair<FormId, int>>& items) override
        {
            ++mItemQuestions;
            if (mUnknown.count(holder) != 0)
                return false;
            items = mItems[holder];
            return true;
        }

        bool initialFactions(FormId actor, std::vector<std::pair<FormId, int>>& factions) override
        {
            if (mUnknown.count(actor) != 0)
                return false;
            factions = mFactions[actor];
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
        FakeRecords mRecords;
        ActorState mState{ mRecords };

        Runner() { FalloutScript::addActorCommands(mInterpreter, mState); }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGuards];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards);
            record.mScript = builder.definition();
            mObjects.add(guard, scriptOfGuards);
            mObjects.trigger(guard, BlockType::OnActivate);
        }

        double variable(std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(guard);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    Bytes itemArgs(std::uint16_t item, std::int32_t count)
    {
        return arguments(2).add(refArg(item)).add(intArg(count));
    }

    TEST(FalloutScriptActorStateTest, a_holder_has_what_its_record_holds_until_a_script_changes_it)
    {
        FakeRecords records;
        records.mItems[guard] = { { caps, 40 }, { stimpak, 2 } };
        ActorState state(records);

        EXPECT_EQ(state.itemCount(guard, caps), 40);
        state.addItem(guard, caps, 10);
        // The record is not read again
        records.mItems[guard] = { { caps, 1 } };
        EXPECT_EQ(state.itemCount(guard, caps), 50);
        EXPECT_EQ(state.itemCount(guard, stimpak), 2);
        EXPECT_EQ(state.itemCount(crate, caps), 0);
    }

    TEST(FalloutScriptActorStateTest, removing_more_than_is_held_takes_what_there_is)
    {
        FakeRecords records;
        ActorState state(records);
        state.addItem(player, caps, 5);

        EXPECT_EQ(state.removeItem(player, caps, 3), 3);
        EXPECT_EQ(state.removeItem(player, caps, 10), 2);
        EXPECT_EQ(state.itemCount(player, caps), 0);
        EXPECT_EQ(state.removeItem(player, caps, 1), 0);
    }

    TEST(FalloutScriptActorStateTest, a_count_that_is_not_positive_changes_nothing)
    {
        FakeRecords records;
        ActorState state(records);
        state.addItem(player, caps, 5);

        state.addItem(player, caps, 0);
        state.addItem(player, caps, -3);
        EXPECT_EQ(state.removeItem(player, caps, -2), 0);
        EXPECT_EQ(state.itemCount(player, caps), 5);
    }

    TEST(FalloutScriptActorStateTest, a_count_does_not_overflow)
    {
        FakeRecords records;
        ActorState state(records);
        state.addItem(player, caps, 2147483647);
        state.addItem(player, caps, 100);

        EXPECT_EQ(state.itemCount(player, caps), 2147483647);
    }

    TEST(FalloutScriptActorStateTest, a_holder_the_records_can_not_tell_is_asked_again)
    {
        FakeRecords records;
        records.mUnknown.insert(guard);
        records.mItems[guard] = { { caps, 40 } };
        ActorState state(records);

        EXPECT_EQ(state.itemCount(guard, caps), 0);
        state.addItem(guard, stimpak, 1);

        records.mUnknown.clear();
        EXPECT_EQ(state.itemCount(guard, caps), 40);
        EXPECT_EQ(state.itemCount(guard, stimpak), 1);
        const int questions = records.mItemQuestions;
        EXPECT_EQ(state.itemCount(guard, caps), 40);
        EXPECT_EQ(records.mItemQuestions, questions);
    }

    TEST(FalloutScriptActorStateTest, an_actor_is_dead_until_it_is_brought_back)
    {
        FakeRecords records;
        ActorState state(records);

        state.setDead(guard, true);
        EXPECT_TRUE(state.isDead(guard));
        EXPECT_FALSE(state.isDead(crate));
        state.setDead(guard, false);
        EXPECT_FALSE(state.isDead(guard));
        state.setDead(0, true);
        EXPECT_FALSE(state.isDead(0));
    }

    TEST(FalloutScriptActorStateTest, factions_come_from_the_record_and_scripts_change_them)
    {
        FakeRecords records;
        records.mFactions[guard] = { { ncr, 2 } };
        ActorState state(records);

        EXPECT_EQ(state.factionRank(guard, ncr), 2);
        EXPECT_EQ(state.factionRank(guard, legion), -1);
        state.setFactionRank(guard, legion, 0);
        state.setFactionRank(guard, ncr, 4);
        EXPECT_EQ(state.factionRank(guard, legion), 0);
        EXPECT_EQ(state.factionRank(guard, ncr), 4);
        state.removeFromFaction(guard, ncr);
        EXPECT_EQ(state.factionRank(guard, ncr), -1);
        state.setFactionRank(guard, legion, -1);
        EXPECT_EQ(state.factionRank(guard, legion), -1);
    }

    TEST(FalloutScriptActorCommandsTest, a_script_adds_and_removes_items_and_counts_them)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t item = builder.form(caps);
        const std::uint16_t other = builder.form(player);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpAddItem, itemArgs(item, 10))
            + command(OpRemoveItem, itemArgs(item, 3)) + command(OpAddItem, itemArgs(item, 5), other)
            + setTo(var('s', 1), callToken(OpGetItemCount, arguments(1).add(refArg(item))))
            + setTo(var('s', 2), callToken(OpGetItemCount, arguments(1).add(refArg(item)), other)) + end());
        builder.variable(1, integerVariable, "own").variable(2, integerVariable, "players");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 7);
        EXPECT_EQ(setup.variable(2), 5);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorCommandsTest, a_script_kills_and_brings_back_an_actor)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetDead, arguments(0))) + command(OpKillActor, arguments(0))
            + setTo(var('s', 2), callToken(OpGetDead, arguments(0))) + command(OpResurrectActor, arguments(0))
            + setTo(var('s', 3), callToken(OpGetDead, arguments(0))) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "killed")
            .variable(3, integerVariable, "back");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorCommandsTest, a_script_works_with_the_factions_of_an_actor)
    {
        Runner setup;
        setup.mRecords.mFactions[guard] = { { ncr, 3 } };
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(ncr);
        const std::uint16_t second = builder.form(legion);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetFactionRank, arguments(1).add(refArg(first))))
            + command(OpAddToFaction, itemArgs(second, 1))
            + setTo(var('s', 2), callToken(OpGetInFaction, arguments(1).add(refArg(second))))
            + command(OpSetFactionRank, itemArgs(second, 4))
            + setTo(var('s', 3), callToken(OpGetFactionRank, arguments(1).add(refArg(second))))
            + command(OpRemoveFromFaction, arguments(1).add(refArg(first)))
            + setTo(var('s', 4), callToken(OpGetInFaction, arguments(1).add(refArg(first)))) + end());
        builder.variable(1, integerVariable, "ranked")
            .variable(2, integerVariable, "joined")
            .variable(3, integerVariable, "promoted")
            .variable(4, integerVariable, "left");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 3);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), 4);
        EXPECT_EQ(setup.variable(4), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorCommandsTest, a_rank_below_nothing_is_the_lowest_rank)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t faction = builder.form(ncr);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpAddToFaction, itemArgs(faction, -2))
            + setTo(var('s', 1), callToken(OpGetFactionRank, arguments(1).add(refArg(faction)))) + end());
        builder.variable(1, integerVariable, "rank");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
    }
}
