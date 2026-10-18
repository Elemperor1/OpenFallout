#include "scriptbuilder.hpp"

#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/playerprogress.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>
#include <map>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    using FalloutScript::PlayerProgress;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId player = 0x00000014;
    constexpr FormId companion = 0x01000100;
    constexpr FormId toughness = 0x01000200;
    constexpr FormId otherPerk = 0x01000201;
    constexpr FormId note = 0x01000300;
    constexpr FormId otherNote = 0x01000301;
    constexpr FormId scriptOfPlayer = 0x01002000;

    constexpr std::uint32_t integerVariable = 1;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpRewardXP = 0x1177;
    constexpr std::uint16_t OpRewardKarma = 0x1204;
    constexpr std::uint16_t OpSetLevel = 0x1133;
    constexpr std::uint16_t OpResetXP = 0x126D;
    constexpr std::uint16_t OpAddPerk = 0x1176;
    constexpr std::uint16_t OpRemovePerk = 0x11A8;
    constexpr std::uint16_t OpHasPerk = 0x11C1;
    constexpr std::uint16_t OpAddNote = 0x117C;
    constexpr std::uint16_t OpRemoveNote = 0x117D;
    constexpr std::uint16_t OpGetHasNote = 0x117E;

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
        PlayerProgress mProgress;

        Runner() { FalloutScript::addPlayerProgressCommands(mInterpreter, mProgress); }

        /// Runs the script in the OnActivate block of the object `owner`
        void run(FormId owner, const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfPlayer];
            record.mId = ESM::FormId::fromUint32(scriptOfPlayer);
            record.mScript = builder.definition();
            mObjects.add(owner, scriptOfPlayer);
            mObjects.trigger(owner, BlockType::OnActivate);
        }

        double variable(FormId owner, std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(owner);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    TEST(FalloutScriptPlayerProgressTest, experience_adds_up_and_stays_above_zero)
    {
        PlayerProgress progress;

        progress.rewardExperience(150);
        progress.rewardExperience(50);
        EXPECT_EQ(progress.experience(), 200);
        progress.rewardExperience(-500);
        EXPECT_EQ(progress.experience(), 0);
    }

    TEST(FalloutScriptPlayerProgressTest, experience_stops_at_the_largest_number)
    {
        PlayerProgress progress;

        progress.rewardExperience(std::numeric_limits<int>::max());
        progress.rewardExperience(std::numeric_limits<int>::max());

        EXPECT_EQ(progress.experience(), std::numeric_limits<int>::max());
    }

    TEST(FalloutScriptPlayerProgressTest, the_level_follows_the_experience_in_steps_that_grow_by_150)
    {
        EXPECT_EQ(PlayerProgress::levelFor(-5), 1);
        EXPECT_EQ(PlayerProgress::levelFor(0), 1);
        EXPECT_EQ(PlayerProgress::levelFor(199), 1);
        EXPECT_EQ(PlayerProgress::levelFor(200), 2);
        EXPECT_EQ(PlayerProgress::levelFor(549), 2);
        EXPECT_EQ(PlayerProgress::levelFor(550), 3);
        EXPECT_EQ(PlayerProgress::levelFor(1049), 3);
        EXPECT_EQ(PlayerProgress::levelFor(1050), 4);
        EXPECT_EQ(PlayerProgress::levelFor(1699), 4);
        EXPECT_EQ(PlayerProgress::levelFor(1700), 5);
        EXPECT_EQ(PlayerProgress::levelFor(29449), 19);
        EXPECT_EQ(PlayerProgress::levelFor(29450), 20);
        // The largest number of experience is a level, not an overflow
        const long long highest = PlayerProgress::levelFor(std::numeric_limits<int>::max());
        EXPECT_GT(highest, 4000);
        EXPECT_LT(highest, 7000);
        EXPECT_LE((highest - 1) * (75 * highest + 50), std::numeric_limits<int>::max());
        EXPECT_GT(highest * (75 * (highest + 1) + 50), std::numeric_limits<int>::max());
    }

    TEST(FalloutScriptPlayerProgressTest, the_level_of_the_player_is_told_from_its_experience)
    {
        PlayerProgress progress;
        EXPECT_EQ(progress.level(), 1);

        progress.rewardExperience(650);
        EXPECT_EQ(progress.level(), 3);
        progress.rewardExperience(-200);
        EXPECT_EQ(progress.level(), 2);
    }

    TEST(FalloutScriptPlayerProgressTest, karma_moves_both_ways_within_its_limits)
    {
        PlayerProgress progress;

        progress.rewardKarma(-250);
        EXPECT_EQ(progress.karma(), -250);
        progress.rewardKarma(300);
        EXPECT_EQ(progress.karma(), 50);
        progress.rewardKarma(5000);
        EXPECT_EQ(progress.karma(), PlayerProgress::karmaLimit);
        progress.rewardKarma(std::numeric_limits<int>::min());
        EXPECT_EQ(progress.karma(), -PlayerProgress::karmaLimit);
    }

    TEST(FalloutScriptPlayerProgressTest, a_perk_gains_a_rank_each_time_it_is_added_and_goes_at_once)
    {
        PlayerProgress progress;

        EXPECT_EQ(progress.perkRank(player, toughness), 0);
        progress.addPerk(player, toughness);
        EXPECT_EQ(progress.perkRank(player, toughness), 1);
        progress.addPerk(player, toughness);
        EXPECT_EQ(progress.perkRank(player, toughness), 2);
        // Another character and another perk are not changed
        EXPECT_EQ(progress.perkRank(companion, toughness), 0);
        EXPECT_EQ(progress.perkRank(player, otherPerk), 0);
        progress.removePerk(player, toughness);
        EXPECT_EQ(progress.perkRank(player, toughness), 0);
    }

    TEST(FalloutScriptPlayerProgressTest, no_perk_is_added_to_nobody_or_for_nothing)
    {
        PlayerProgress progress;

        progress.addPerk(0, toughness);
        progress.addPerk(player, 0);

        EXPECT_EQ(progress.perkRank(0, toughness), 0);
        EXPECT_EQ(progress.perkRank(player, 0), 0);
    }

    TEST(FalloutScriptPlayerProgressTest, notes_are_added_and_removed)
    {
        PlayerProgress progress;

        EXPECT_FALSE(progress.hasNote(note));
        progress.addNote(note);
        progress.addNote(note);
        EXPECT_TRUE(progress.hasNote(note));
        EXPECT_FALSE(progress.hasNote(otherNote));
        progress.removeNote(note);
        EXPECT_FALSE(progress.hasNote(note));
        progress.addNote(0);
        EXPECT_FALSE(progress.hasNote(0));
    }

    TEST(FalloutScriptPlayerProgressTest, clearing_forgets_everything)
    {
        PlayerProgress progress;
        progress.rewardExperience(100);
        progress.rewardKarma(100);
        progress.addPerk(player, toughness);
        progress.addNote(note);

        progress.clear();

        EXPECT_EQ(progress.experience(), 0);
        EXPECT_EQ(progress.karma(), 0);
        EXPECT_EQ(progress.perkRank(player, toughness), 0);
        EXPECT_FALSE(progress.hasNote(note));
    }

    TEST(FalloutScriptPlayerProgressCommandsTest, a_script_rewards_experience_and_karma)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpRewardXP, arguments(1).add(intArg(120)))
            + command(OpRewardXP, arguments(1).add(intArg(30))) + command(OpRewardKarma, arguments(1).add(intArg(-45)))
            + end());

        setup.run(player, builder);

        EXPECT_EQ(setup.mProgress.experience(), 150);
        EXPECT_EQ(setup.mProgress.karma(), -45);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPlayerProgressCommandsTest, a_script_sets_the_level_of_the_player_and_takes_its_experience)
    {
        Runner levelled;
        ScriptBuilder levelling;
        levelling.code(
            scriptName() + begin(BlockType::OnActivate) + command(OpSetLevel, arguments(1).add(intArg(5))) + end());
        levelled.run(player, levelling);
        EXPECT_EQ(levelled.mProgress.level(), 5);
        EXPECT_EQ(levelled.mProgress.experience(), 1700);

        Runner reset;
        ScriptBuilder resetting;
        resetting.code(scriptName() + begin(BlockType::OnActivate) + command(OpRewardXP, arguments(1).add(intArg(300)))
            + command(OpResetXP, arguments(0)) + end());
        reset.run(player, resetting);
        EXPECT_EQ(reset.mProgress.level(), 1);
        EXPECT_EQ(reset.mProgress.experience(), 0);
        EXPECT_THAT(reset.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPlayerProgressCommandsTest, the_level_of_someone_else_leaves_the_player_alone)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t target = builder.form(companion);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpRewardXP, arguments(1).add(intArg(300)))
            + command(OpSetLevel, arguments(1).add(intArg(9)), target) + command(OpResetXP, arguments(0), target)
            + end());

        setup.run(player, builder);

        EXPECT_EQ(setup.mProgress.experience(), 300);
        EXPECT_EQ(setup.mProgress.level(), 2);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPlayerProgressCommandsTest, a_script_gives_and_takes_perks_of_its_object)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t perk = builder.form(toughness);
        const std::uint16_t other = builder.form(otherPerk);
        const Bytes hasPerk = callToken(OpHasPerk, arguments(1).add(refArg(perk)));
        builder.code(scriptName() + begin(BlockType::OnActivate) + setTo(var('s', 1), hasPerk)
            + command(OpAddPerk, arguments(1).add(refArg(perk))) + setTo(var('s', 2), hasPerk)
            + command(OpAddPerk, arguments(1).add(refArg(perk)))
            // The second argument asks about the companions of the player, not about a rank
            + setTo(var('s', 3), callToken(OpHasPerk, arguments(2).add(refArg(perk)).add(intArg(1))))
            + setTo(var('s', 4), callToken(OpHasPerk, arguments(2).add(refArg(perk)).add(intArg(0))))
            + setTo(var('s', 5), callToken(OpHasPerk, arguments(1).add(refArg(other))))
            + command(OpRemovePerk, arguments(1).add(refArg(perk))) + setTo(var('s', 6), hasPerk) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "once")
            .variable(3, integerVariable, "forTeammates")
            .variable(4, integerVariable, "notForTeammates")
            .variable(5, integerVariable, "other")
            .variable(6, integerVariable, "removed");

        setup.run(player, builder);

        EXPECT_EQ(setup.variable(player, 1), 0);
        EXPECT_EQ(setup.variable(player, 2), 1);
        EXPECT_EQ(setup.variable(player, 3), 1);
        EXPECT_EQ(setup.variable(player, 4), 1);
        EXPECT_EQ(setup.variable(player, 5), 0);
        EXPECT_EQ(setup.variable(player, 6), 0);
        // The two ranks went with the perk
        EXPECT_EQ(setup.mProgress.perkRank(player, toughness), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPlayerProgressCommandsTest, a_perk_goes_to_the_character_the_command_is_called_on)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t perk = builder.form(toughness);
        const std::uint16_t target = builder.form(companion);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpAddPerk, arguments(1).add(refArg(perk)), target)
            + setTo(var('s', 1), callToken(OpHasPerk, arguments(1).add(refArg(perk)), target))
            + setTo(var('s', 2), callToken(OpHasPerk, arguments(1).add(refArg(perk)))) + end());
        builder.variable(1, integerVariable, "companion").variable(2, integerVariable, "self");

        setup.run(player, builder);

        EXPECT_EQ(setup.variable(player, 1), 1);
        EXPECT_EQ(setup.variable(player, 2), 0);
        EXPECT_EQ(setup.mProgress.perkRank(companion, toughness), 1);
        EXPECT_EQ(setup.mProgress.perkRank(player, toughness), 0);
    }

    TEST(FalloutScriptPlayerProgressCommandsTest, a_script_adds_and_removes_notes)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(note);
        const std::uint16_t second = builder.form(otherNote);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetHasNote, arguments(1).add(refArg(first))))
            + command(OpAddNote, arguments(1).add(refArg(first))) + command(OpAddNote, arguments(1).add(refArg(second)))
            + setTo(var('s', 2), callToken(OpGetHasNote, arguments(1).add(refArg(first))))
            + command(OpRemoveNote, arguments(1).add(refArg(first)))
            + setTo(var('s', 3), callToken(OpGetHasNote, arguments(1).add(refArg(first))))
            + setTo(var('s', 4), callToken(OpGetHasNote, arguments(1).add(refArg(second)))) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "added")
            .variable(3, integerVariable, "removed")
            .variable(4, integerVariable, "kept");

        setup.run(player, builder);

        EXPECT_EQ(setup.variable(player, 1), 0);
        EXPECT_EQ(setup.variable(player, 2), 1);
        EXPECT_EQ(setup.variable(player, 3), 0);
        EXPECT_EQ(setup.variable(player, 4), 1);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }
}
