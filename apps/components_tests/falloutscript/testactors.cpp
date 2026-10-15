#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/playerprogress.hpp>
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
    constexpr std::uint16_t OpGetDeadCount = 0x1054;
    constexpr std::uint16_t OpGetHealthPercentage = 0x11AF;
    constexpr std::uint16_t OpGetItemCount = 0x102F;
    constexpr std::uint16_t OpGetInFaction = 0x1047;
    constexpr std::uint16_t OpGetFactionRank = 0x1049;
    constexpr std::uint16_t OpRemoveItem = 0x1052;
    constexpr std::uint16_t OpRemoveAllItems = 0x10AD;
    constexpr std::uint16_t OpDuplicateAllItems = 0x114B;
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
        std::map<FormId, FormId> mBases;
        int mItemQuestions = 0;

        bool baseFormOf(FormId reference, FormId& base) override
        {
            const auto found = mBases.find(reference);
            if (found == mBases.end())
                return false;
            base = found->second;
            return true;
        }

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
        FalloutScript::ActorValues mValues;
        FalloutScript::PlayerProgress mProgress;

        Runner()
        {
            FalloutScript::addActorCommands(mInterpreter, mState);
            // The commands about the health of a character
            FalloutScript::addActorValueCommands(mInterpreter, mValues, mState, mProgress);
        }

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

    TEST(FalloutScriptActorStateTest, a_removal_made_before_the_records_can_be_read_is_made_to_what_they_say)
    {
        FakeRecords records;
        records.mUnknown.insert(guard);
        records.mItems[guard] = { { caps, 40 }, { stimpak, 2 } };
        ActorState state(records);

        // Counted from nothing until the records can be read
        state.addItem(guard, ncr, 3);
        EXPECT_EQ(state.removeItem(guard, ncr, 2), 2);
        EXPECT_EQ(state.removeItem(guard, caps, 15), 0);
        EXPECT_EQ(state.itemCount(guard, ncr), 1);
        EXPECT_EQ(state.itemCount(guard, caps), 0);
        state.removeItem(guard, stimpak, 5);
        state.addItem(guard, stimpak, 1);

        records.mUnknown.clear();
        EXPECT_EQ(state.itemCount(guard, caps), 25);
        // Five taken from two leave none, and the one added after that is the only one
        EXPECT_EQ(state.itemCount(guard, stimpak), 1);
        EXPECT_EQ(state.itemCount(guard, ncr), 1);
    }

    TEST(FalloutScriptActorStateTest, changes_made_before_the_records_can_be_read_keep_their_order)
    {
        FakeRecords records;
        records.mUnknown.insert(guard);
        records.mUnknown.insert(crate);
        records.mItems[guard] = { { caps, 4 } };
        records.mItems[crate] = { { caps, 4 } };
        ActorState state(records);

        state.removeItem(guard, caps, 10);
        state.addItem(guard, caps, 5);
        state.addItem(crate, caps, 5);
        state.removeItem(crate, caps, 3);
        state.addItem(crate, caps, 10);

        records.mUnknown.clear();
        // The count does not go below 0 in the middle of the changes: 4 - 10 is 0, and 0 + 5 is 5
        EXPECT_EQ(state.itemCount(guard, caps), 5);
        // 4 + 5 - 3 + 10
        EXPECT_EQ(state.itemCount(crate, caps), 16);
    }

    TEST(FalloutScriptActorStateTest, changes_that_never_end_are_still_numbers)
    {
        FakeRecords records;
        records.mUnknown.insert(guard);
        records.mItems[guard] = { { caps, 7 } };
        ActorState state(records);

        for (int i = 0; i < 100; ++i)
            state.addItem(guard, caps, 2147483647);
        state.removeItem(guard, caps, 5);

        records.mUnknown.clear();
        EXPECT_EQ(state.itemCount(guard, caps), 2147483647 - 5);
    }

    TEST(FalloutScriptActorStateTest, a_faction_left_before_the_records_can_be_read_stays_left)
    {
        FakeRecords records;
        records.mUnknown.insert(guard);
        records.mFactions[guard] = { { ncr, 3 }, { legion, 1 } };
        ActorState state(records);

        state.removeFromFaction(guard, ncr);
        state.setFactionRank(guard, legion, -1);
        state.setFactionRank(guard, caps, 2);
        EXPECT_EQ(state.factionRank(guard, ncr), -1);
        EXPECT_EQ(state.factionRank(guard, caps), 2);

        records.mUnknown.clear();
        EXPECT_EQ(state.factionRank(guard, ncr), -1);
        EXPECT_EQ(state.factionRank(guard, legion), -1);
        EXPECT_EQ(state.factionRank(guard, caps), 2);
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

    TEST(FalloutScriptActorStateTest, the_dead_are_counted_by_the_form_they_were_made_from)
    {
        FakeRecords records;
        constexpr FormId raider = 0x0100A000;
        constexpr FormId other = 0x0100A001;
        records.mBases
            = { { 0x01000201, raider }, { 0x01000202, raider }, { 0x01000203, raider }, { 0x01000204, other } };
        ActorState state(records);

        EXPECT_EQ(state.deadCount(raider), 0);
        state.setDead(0x01000201, true);
        state.setDead(0x01000202, true);
        state.setDead(0x01000204, true);
        // a reference the records can not tell is not counted
        state.setDead(0x01000299, true);
        EXPECT_EQ(state.deadCount(raider), 2);
        EXPECT_EQ(state.deadCount(other), 1);
        state.setDead(0x01000201, false);
        EXPECT_EQ(state.deadCount(raider), 1);
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

    TEST(FalloutScriptActorStateTest, a_holder_lists_what_it_has_and_the_changes_are_told_once)
    {
        FakeRecords records;
        records.mItems[guard] = { { caps, 40 } };
        ActorState state(records);

        std::vector<std::pair<FormId, int>> items;
        ASSERT_TRUE(state.items(guard, items));
        EXPECT_THAT(items, ElementsAre(std::make_pair(caps, 40)));
        // Reading the record is no change
        EXPECT_THAT(state.takeChanges(), IsEmpty());

        state.addItem(guard, stimpak, 2);
        state.removeItem(guard, caps, 100);
        state.removeItem(guard, ncr, 1);

        const std::vector<ActorState::ItemChange> changes = state.takeChanges();
        ASSERT_EQ(changes.size(), 2u);
        EXPECT_EQ(changes[0].mItem, stimpak);
        EXPECT_EQ(changes[0].mCount, 2);
        EXPECT_EQ(changes[1].mItem, caps);
        // Only what the holder had can be taken away
        EXPECT_EQ(changes[1].mCount, -40);
        EXPECT_EQ(changes[1].mHolder, guard);
        EXPECT_THAT(state.takeChanges(), IsEmpty());

        ASSERT_TRUE(state.items(guard, items));
        EXPECT_THAT(items, ElementsAre(std::make_pair(stimpak, 2)));
    }

    TEST(FalloutScriptActorStateTest, a_holder_that_is_not_known_yet_has_no_list)
    {
        FakeRecords records;
        records.mUnknown.insert(crate);
        ActorState state(records);

        std::vector<std::pair<FormId, int>> items;
        EXPECT_FALSE(state.items(crate, items));
        state.addItem(crate, caps, 5);
        EXPECT_FALSE(state.items(crate, items));

        records.mUnknown.clear();
        ASSERT_TRUE(state.items(crate, items));
        EXPECT_THAT(items, ElementsAre(std::make_pair(caps, 5)));
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

    TEST(FalloutScriptActorCommandsTest, a_script_moves_and_copies_all_items_to_another_holder)
    {
        Runner setup;
        setup.mRecords.mItems[guard] = { { caps, 10 }, { stimpak, 2 } };
        ScriptBuilder builder;
        const std::uint16_t other = builder.form(player);
        const std::uint16_t chest = builder.form(crate);
        const std::uint16_t money = builder.form(caps);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpDuplicateAllItems, arguments(1).add(refArg(chest)))
            + setTo(var('s', 1), callToken(OpGetItemCount, arguments(1).add(refArg(money))))
            + setTo(var('s', 2), callToken(OpGetItemCount, arguments(1).add(refArg(money)), chest))
            + command(OpRemoveAllItems, arguments(1).add(refArg(other)))
            + setTo(var('s', 3), callToken(OpGetItemCount, arguments(1).add(refArg(money))))
            + setTo(var('s', 4), callToken(OpGetItemCount, arguments(1).add(refArg(money)), other)) + end());
        builder.variable(1, integerVariable, "kept")
            .variable(2, integerVariable, "copied")
            .variable(3, integerVariable, "left")
            .variable(4, integerVariable, "moved");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 10);
        EXPECT_EQ(setup.variable(2), 10);
        EXPECT_EQ(setup.variable(3), 0);
        EXPECT_EQ(setup.variable(4), 10);
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

    TEST(FalloutScriptActorCommandsTest, a_script_counts_the_dead_of_a_base_form)
    {
        Runner setup;
        constexpr FormId raider = 0x0100A000;
        setup.mRecords.mBases = { { guard, raider }, { crate, raider } };
        ScriptBuilder builder;
        const std::uint16_t base = builder.form(raider);
        const std::uint16_t other = builder.form(crate);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetDeadCount, arguments(1).add(refArg(base))))
            + command(OpKillActor, arguments(0)) + command(OpKillActor, arguments(0), other)
            + setTo(var('s', 2), callToken(OpGetDeadCount, arguments(1).add(refArg(base)))) + end());
        builder.variable(1, integerVariable, "before").variable(2, integerVariable, "after");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 2);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorCommandsTest, a_living_actor_has_all_its_health_and_a_dead_one_none)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetHealthPercentage, arguments(0))) + command(OpKillActor, arguments(0))
            + setTo(var('f', 2), callToken(OpGetHealthPercentage, arguments(0))) + end());
        builder.variable(1, 0, "alive").variable(2, 0, "dead");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
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
