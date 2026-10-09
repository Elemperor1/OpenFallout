#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using FalloutScript::Game;

    TEST(FalloutScriptVanillaCommandsTest, hasTheScriptCommandsOfEachExecutable)
    {
        EXPECT_EQ(FalloutScript::vanillaCommands(Game::NewVegas).size(), 615u);
        EXPECT_EQ(FalloutScript::vanillaCommands(Game::Fallout3).size(), 545u);
    }

    TEST(FalloutScriptVanillaCommandsTest, hasTheCommandsOfQuestsInBothGames)
    {
        for (const Game game : { Game::Fallout3, Game::NewVegas })
        {
            const FalloutScript::CommandTable table = FalloutScript::vanillaCommands(game);
            for (const char* name : { "GetStage", "GetStageDone", "SetStage", "GetQuestRunning", "StartQuest",
                     "StopQuest", "GetQuestCompleted", "CompleteQuest", "SetObjectiveDisplayed",
                     "SetObjectiveCompleted", "GetObjectiveDisplayed", "GetObjectiveCompleted" })
                EXPECT_NE(table.find(name), nullptr) << name;

            const FalloutScript::CommandInfo* setStage = table.find(std::uint16_t(0x1039));
            ASSERT_NE(setStage, nullptr);
            EXPECT_EQ(setStage->mName, "SetStage");
            EXPECT_FALSE(setStage->mNeedsReference);
            ASSERT_EQ(setStage->mParameters.size(), 2u);
            EXPECT_EQ(setStage->mParameters[0].mType, 14u); // quest
            EXPECT_EQ(setStage->mParameters[1].mType, 23u); // quest stage
        }
    }

    TEST(FalloutScriptVanillaCommandsTest, knowsWhichCommandsNeedAReference)
    {
        const FalloutScript::CommandTable table = FalloutScript::vanillaCommands(Game::NewVegas);
        ASSERT_NE(table.find("AddItem"), nullptr);
        EXPECT_TRUE(table.find("AddItem")->mNeedsReference);
        ASSERT_NE(table.find("GetGlobalValue"), nullptr);
        EXPECT_FALSE(table.find("GetGlobalValue")->mNeedsReference);
    }

    TEST(FalloutScriptVanillaCommandsTest, theGamesDifferInTheCommandsAndParametersOfAFewCommands)
    {
        const FalloutScript::CommandTable newVegas = FalloutScript::vanillaCommands(Game::NewVegas);
        const FalloutScript::CommandTable fallout3 = FalloutScript::vanillaCommands(Game::Fallout3);

        // A command that only New Vegas has
        ASSERT_NE(newVegas.find(std::uint16_t(0x127C)), nullptr);
        EXPECT_EQ(fallout3.find(std::uint16_t(0x127C)), nullptr);

        // A command whose later parameters were added in New Vegas
        ASSERT_NE(newVegas.find("HasPerk"), nullptr);
        ASSERT_NE(fallout3.find("HasPerk"), nullptr);
        EXPECT_EQ(newVegas.find("HasPerk")->mParameters.size(), 2u);
        EXPECT_TRUE(newVegas.find("HasPerk")->mParameters[1].mOptional);
        EXPECT_EQ(fallout3.find("HasPerk")->mParameters.size(), 1u);
        EXPECT_EQ(newVegas.find("HasPerk")->mOpcode, fallout3.find("HasPerk")->mOpcode);
    }

    TEST(FalloutScriptVanillaCommandsTest, hasTheCommandsThatTheFirstTableReadingMissed)
    {
        // Scripts of both games call these, and the entries stand between ones that are not commands
        for (const Game game : { Game::Fallout3, Game::NewVegas })
        {
            const FalloutScript::CommandTable table = FalloutScript::vanillaCommands(game);
            ASSERT_NE(table.find(std::uint16_t(0x116B)), nullptr);
            EXPECT_EQ(table.find(std::uint16_t(0x116B))->mName, "GetLinkedRef");
            ASSERT_NE(table.find(std::uint16_t(0x1177)), nullptr);
            EXPECT_EQ(table.find(std::uint16_t(0x1177))->mName, "RewardXP");
            ASSERT_NE(table.find("sbm"), nullptr);
            EXPECT_EQ(table.find("sbm")->mOpcode, 0x1173);
        }

        // AddPerk has one more optional parameter in New Vegas
        EXPECT_EQ(FalloutScript::vanillaCommands(Game::NewVegas).find("AddPerk")->mParameters.size(), 2u);
        EXPECT_EQ(FalloutScript::vanillaCommands(Game::Fallout3).find("AddPerk")->mParameters.size(), 1u);
        // PlaceAtReticle is only in New Vegas
        EXPECT_NE(FalloutScript::vanillaCommands(Game::NewVegas).find(std::uint16_t(0x1263)), nullptr);
        EXPECT_EQ(FalloutScript::vanillaCommands(Game::Fallout3).find(std::uint16_t(0x1263)), nullptr);
    }

    TEST(FalloutScriptVanillaCommandsTest, findsCommandsByShortName)
    {
        const FalloutScript::CommandTable table = FalloutScript::vanillaCommands(Game::NewVegas);
        ASSERT_NE(table.find("GetAV"), nullptr);
        EXPECT_EQ(table.find("GetAV")->mName, "GetActorValue");
        EXPECT_EQ(table.find("getav")->mOpcode, 0x100E);
    }
}
