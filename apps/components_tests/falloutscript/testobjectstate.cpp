#include "scriptbuilder.hpp"

#include <components/falloutscript/objectstate.hpp>
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
    using FalloutScript::FormId;

    constexpr FormId player = 0x14;
    constexpr FormId playerBase = 0x7;
    constexpr FormId guard = 0x00100001;
    constexpr FormId guardBase = 0x00100002;
    constexpr FormId crate = 0x00100003;
    constexpr FormId faction = 0x00100004;
    constexpr FormId cell = 0x00100005;
    constexpr FormId other = 0x00100006;
    constexpr FormId nothing = 0x00100007;

    constexpr std::uint16_t OpIgnoreCrime = 0x10AB;
    constexpr std::uint16_t OpGetIgnoreCrime = 0x10C0;
    constexpr std::uint16_t OpSetCellPublicFlag = 0x10BB;
    constexpr std::uint16_t OpSetGhost = 0x10EC;
    constexpr std::uint16_t OpGetIsGhost = 0x10ED;
    constexpr std::uint16_t OpSetQuestObject = 0x110D;
    constexpr std::uint16_t OpIsOwner = 0x1116;
    constexpr std::uint16_t OpSetOwnership = 0x1117;
    constexpr std::uint16_t OpSetCellOwnership = 0x1119;
    constexpr std::uint16_t OpSetPlayerTeammate = 0x11C5;
    constexpr std::uint16_t OpGetPlayerTeammate = 0x11C6;
    constexpr std::uint16_t OpHasBeenEaten = 0x107F;
    constexpr std::uint16_t OpSetCombatStyle = 0x10B1;
    constexpr std::uint16_t OpGetPCEnemyofFaction = 0x10C5;
    constexpr std::uint16_t OpSetPCEnemyofFaction = 0x10C6;
    constexpr std::uint16_t OpGetCauseofDeath = 0x118D;
    constexpr std::uint16_t OpGetDestructionStage = 0x11D7;
    constexpr FormId style = 0x00100008;

    class FakeRecords : public FalloutScript::ObjectStateRecords
    {
    public:
        std::set<FormId> mActors{ guard, ::player, other };
        std::map<FormId, FormId> mOwners;
        std::map<FormId, FormId> mCellOwners;
        std::set<FormId> mPublic;
        std::set<FormId> mGhostRecords;

        bool isActor(FormId reference) override { return mActors.count(reference) != 0; }
        FormId player() override { return ::player; }
        FormId playerBase() override { return ::playerBase; }
        FormId ownerForm(FormId form) override
        {
            // The references of characters stand for the characters they are of
            if (form == ::player)
                return ::playerBase;
            return form == guard ? guardBase : form;
        }
        FormId recordOwner(FormId reference) override
        {
            const auto found = mOwners.find(reference);
            return found == mOwners.end() ? 0 : found->second;
        }
        FormId recordCellOwner(FormId id) override
        {
            const auto found = mCellOwners.find(id);
            return found == mCellOwners.end() ? 0 : found->second;
        }
        bool recordCellPublic(FormId id) override { return mPublic.count(id) != 0; }
        bool recordGhost(FormId actor) override { return mGhostRecords.count(actor) != 0; }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeRecords mRecords;
        FalloutScript::ObjectState mState{ mRecords };

        Runner() { FalloutScript::addObjectStateCommands(mInterpreter, mState, mRecords); }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    TEST(FalloutScriptObjectStateTest, a_reference_is_owned_by_the_owner_its_record_gives_it)
    {
        Runner run;
        run.mRecords.mOwners[crate] = faction;
        EXPECT_EQ(run.call(OpIsOwner, crate, { faction }), 1.0);
        EXPECT_EQ(run.call(OpIsOwner, crate, { other }), 0.0);
        // Without an owner named, the player is asked about
        EXPECT_EQ(run.call(OpIsOwner, crate), 0.0);
        EXPECT_EQ(run.call(OpIsOwner, nothing, { faction }), 0.0);
    }

    TEST(FalloutScriptObjectStateTest, setting_the_ownership_without_an_owner_makes_the_player_the_owner)
    {
        Runner run;
        run.mRecords.mOwners[crate] = faction;
        run.call(OpSetOwnership, crate);
        EXPECT_EQ(run.call(OpIsOwner, crate), 1.0);
        EXPECT_EQ(run.call(OpIsOwner, crate, { faction }), 0.0);
        EXPECT_EQ(run.call(OpIsOwner, crate, { player }), 1.0);
        EXPECT_EQ(run.mState.owner(crate), playerBase);
    }

    TEST(FalloutScriptObjectStateTest, an_owner_is_told_by_the_form_it_is_of)
    {
        Runner run;
        run.call(OpSetOwnership, crate, { guard });
        EXPECT_EQ(run.mState.owner(crate), guardBase);
        EXPECT_EQ(run.call(OpIsOwner, crate, { guard }), 1.0);
        EXPECT_EQ(run.call(OpIsOwner, crate, { guardBase }), 1.0);
    }

    TEST(FalloutScriptObjectStateTest, only_the_owner_a_reference_has_itself_counts)
    {
        Runner run;
        // The place is someone's, the reference in it is no one's
        run.mRecords.mCellOwners[cell] = faction;
        EXPECT_EQ(run.call(OpIsOwner, crate, { faction }), 0.0);
        EXPECT_EQ(run.mState.cellOwner(cell), faction);
        // The command is about a reference and has none without one
        run.call(OpSetOwnership, 0, { faction });
        EXPECT_EQ(run.mState.owner(0), 0u);
    }

    TEST(FalloutScriptObjectStateTest, a_cell_has_the_owner_and_the_public_flag_that_scripts_gave_it)
    {
        Runner run;
        run.mRecords.mPublic.insert(cell);
        EXPECT_TRUE(run.mState.cellPublic(cell));
        run.call(OpSetCellPublicFlag, 0, { cell, 0 });
        EXPECT_FALSE(run.mState.cellPublic(cell));
        run.call(OpSetCellPublicFlag, 0, { cell, 1 });
        EXPECT_TRUE(run.mState.cellPublic(cell));
        run.call(OpSetCellOwnership, 0, { cell, faction });
        EXPECT_EQ(run.mState.cellOwner(cell), faction);
        run.call(OpSetCellOwnership, 0, { cell });
        EXPECT_EQ(run.mState.cellOwner(cell), playerBase);
    }

    TEST(FalloutScriptObjectStateTest, crime_is_ignored_by_a_character_until_the_flag_is_taken_away)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetIgnoreCrime, guard), 0.0);
        run.call(OpIgnoreCrime, guard, { 1 });
        EXPECT_EQ(run.call(OpGetIgnoreCrime, guard), 1.0);
        EXPECT_TRUE(run.mState.ignoresCrime(guard));
        run.call(OpIgnoreCrime, guard, { 0 });
        EXPECT_EQ(run.call(OpGetIgnoreCrime, guard), 0.0);
        // A thing that is no character has none
        run.call(OpIgnoreCrime, crate, { 1 });
        EXPECT_EQ(run.call(OpGetIgnoreCrime, crate), 0.0);
    }

    TEST(FalloutScriptObjectStateTest, a_ghost_is_a_character_that_scripts_made_one)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetIsGhost, guard), 0.0);
        run.call(OpSetGhost, guard, { 1 });
        EXPECT_EQ(run.call(OpGetIsGhost, guard), 1.0);
        EXPECT_TRUE(run.mState.ghost(guard));
        run.call(OpSetGhost, guard, { 0 });
        EXPECT_EQ(run.call(OpGetIsGhost, guard), 0.0);
        run.call(OpSetGhost, crate, { 1 });
        EXPECT_FALSE(run.mState.ghost(crate));
    }

    TEST(FalloutScriptObjectStateTest, a_creature_that_its_record_makes_a_ghost_stays_one_until_a_script_says_it_is_not)
    {
        Runner run;
        run.mRecords.mGhostRecords.insert(guard);
        EXPECT_EQ(run.call(OpGetIsGhost, guard), 1.0);
        EXPECT_EQ(run.call(OpGetIsGhost, other), 0.0);
        run.call(OpSetGhost, guard, { 0 });
        EXPECT_EQ(run.call(OpGetIsGhost, guard), 0.0);

        // What the script said is kept in a saved game, also when it is that the record is wrong
        FalloutScript::SaveWriter out;
        FalloutScript::writeObjectState(run.mState, out);
        Runner again;
        again.mRecords.mGhostRecords.insert(guard);
        EXPECT_EQ(again.call(OpGetIsGhost, guard), 1.0);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readObjectState(again.mState, in, FalloutScript::objectStateSectionVersion);
        EXPECT_EQ(again.call(OpGetIsGhost, guard), 0.0);
    }

    TEST(FalloutScriptObjectStateTest, the_quest_mark_is_on_the_form_and_not_on_a_reference)
    {
        Runner run;
        EXPECT_FALSE(run.mState.questObject(crate));
        run.call(OpSetQuestObject, 0, { crate, 1 });
        EXPECT_TRUE(run.mState.questObject(crate));
        EXPECT_FALSE(run.mState.questObject(guard));
        run.call(OpSetQuestObject, 0, { crate, 0 });
        EXPECT_FALSE(run.mState.questObject(crate));
    }

    TEST(FalloutScriptObjectStateTest, a_character_can_follow_the_player_as_a_teammate_and_the_player_cannot)
    {
        Runner run;
        run.call(OpSetPlayerTeammate, guard, { 1 });
        run.call(OpSetPlayerTeammate, other, { 1 });
        run.call(OpSetPlayerTeammate, player, { 1 });
        run.call(OpSetPlayerTeammate, crate, { 1 });
        EXPECT_EQ(run.call(OpGetPlayerTeammate, guard), 1.0);
        EXPECT_EQ(run.call(OpGetPlayerTeammate, player), 0.0);
        EXPECT_EQ(run.call(OpGetPlayerTeammate, crate), 0.0);
        EXPECT_THAT(run.mState.teammates(), UnorderedElementsAre(guard, other));
        run.call(OpSetPlayerTeammate, guard, { 0 });
        EXPECT_EQ(run.call(OpGetPlayerTeammate, guard), 0.0);
        EXPECT_THAT(run.mState.teammates(), ElementsAre(other));
    }

    TEST(FalloutScriptObjectStateTest, a_saved_game_brings_back_what_scripts_marked)
    {
        Runner run;
        run.mRecords.mPublic.insert(cell);
        run.call(OpSetOwnership, crate, { faction });
        run.call(OpSetCellOwnership, 0, { cell, other });
        run.call(OpSetCellPublicFlag, 0, { cell, 0 });
        run.call(OpIgnoreCrime, guard, { 1 });
        run.call(OpSetGhost, other, { 1 });
        run.call(OpSetQuestObject, 0, { crate, 1 });
        run.call(OpSetPlayerTeammate, guard, { 1 });
        run.call(OpSetPCEnemyofFaction, 0, { faction, 1 });
        run.call(OpSetCombatStyle, guard, { style });
        FalloutScript::SaveWriter out;
        FalloutScript::writeObjectState(run.mState, out);

        Runner again;
        again.call(OpSetGhost, guard, { 1 });
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readObjectState(again.mState, in, FalloutScript::objectStateSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(again.mState.owner(crate), faction);
        EXPECT_EQ(again.mState.cellOwner(cell), other);
        EXPECT_FALSE(again.mState.cellPublic(cell));
        EXPECT_TRUE(again.mState.ignoresCrime(guard));
        EXPECT_TRUE(again.mState.ghost(other));
        EXPECT_FALSE(again.mState.ghost(guard));
        EXPECT_TRUE(again.mState.questObject(crate));
        EXPECT_THAT(again.mState.teammates(), ElementsAre(guard));
        EXPECT_TRUE(again.mState.enemyOfFaction(faction));
        EXPECT_EQ(again.mState.combatStyle(guard), style);

        again.mState.clear();
        EXPECT_FALSE(again.mState.enemyOfFaction(faction));
        EXPECT_EQ(again.mState.combatStyle(guard), 0u);
        EXPECT_EQ(again.mState.owner(crate), 0u);
        EXPECT_TRUE(again.mState.teammates().empty());
    }

    TEST(FalloutScriptObjectStateTest, a_faction_is_made_an_enemy_of_the_player_and_a_friend_again)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetPCEnemyofFaction, 0, { faction }), 0.0);
        run.call(OpSetPCEnemyofFaction, 0, { faction, 1 });
        EXPECT_EQ(run.call(OpGetPCEnemyofFaction, 0, { faction }), 1.0);
        EXPECT_EQ(run.call(OpGetPCEnemyofFaction, 0, { other }), 0.0);
        run.call(OpSetPCEnemyofFaction, 0, { faction, 0 });
        EXPECT_EQ(run.call(OpGetPCEnemyofFaction, 0, { faction }), 0.0);
    }

    TEST(FalloutScriptObjectStateTest, a_character_gets_a_combat_style_from_a_script_and_a_thing_does_not)
    {
        Runner run;
        run.call(OpSetCombatStyle, guard, { style });
        EXPECT_EQ(run.mState.combatStyle(guard), style);
        run.call(OpSetCombatStyle, crate, { style });
        EXPECT_EQ(run.mState.combatStyle(crate), 0u);
        run.call(OpSetCombatStyle, guard);
        EXPECT_EQ(run.mState.combatStyle(guard), 0u);
    }

    TEST(FalloutScriptObjectStateTest, the_death_stage_and_feeding_questions_give_the_answers_of_no_data)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetCauseofDeath, guard), -1.0);
        EXPECT_EQ(run.call(OpGetDestructionStage, crate), -1.0);
        EXPECT_EQ(run.call(OpHasBeenEaten, guard), 0.0);
    }
}
