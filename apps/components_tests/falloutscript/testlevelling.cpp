#include <components/falloutscript/levelling.hpp>
#include <components/falloutscript/playerprogress.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::Game;
    using FalloutScript::LevellingRules;
    using FalloutScript::PlayerProgress;
    using FalloutScript::Levelling::experienceForLevel;
    using FalloutScript::Levelling::givesPerk;
    using FalloutScript::Levelling::killExperience;
    using FalloutScript::Levelling::levelForExperience;
    using FalloutScript::Levelling::maxLevelForFiles;
    using FalloutScript::Levelling::skillPoints;
    using FalloutScript::Levelling::skillRoom;

    LevellingRules rulesOf(Game game, const std::map<std::string, double>& settings = {})
    {
        return LevellingRules::fromGameSettings(game, [&settings](std::string_view name) -> std::optional<double> {
            const auto found = settings.find(std::string(name));
            if (found == settings.end())
                return std::nullopt;
            return found->second;
        });
    }

    TEST(FalloutScriptLevellingTest, newVegasRaisesTheHighestLevelForEveryAddOnThatIsLoaded)
    {
        // Whatever the files say: 30 and 5 for each of the four add-ons, by the start of the name of the file
        const LevellingRules rules = rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 20 } });
        EXPECT_EQ(maxLevelForFiles(rules, { "FalloutNV.esm" }), 30);
        EXPECT_EQ(maxLevelForFiles(rules, { "FalloutNV.esm", "DeadMoney.esm" }), 35);
        EXPECT_EQ(
            maxLevelForFiles(rules, { "FalloutNV.esm", "deadmoney.esm", "HonestHearts.esm", "Gun Runners.esm" }), 40);
        EXPECT_EQ(
            maxLevelForFiles(rules,
                { "FalloutNV.esm", "DeadMoney.esm", "HonestHearts.esm", "OldWorldBlues.esm", "LonesomeRoad.esm" }),
            50);
        // An add-on counts once
        EXPECT_EQ(maxLevelForFiles(rules, { "FalloutNV.esm", "DeadMoney.esm", "DeadMoney.esm" }), 35);
    }

    TEST(FalloutScriptLevellingTest, fallout3KeepsItsSettingAndBrokenSteelMakesItAtLeastThirty)
    {
        const LevellingRules rules = rulesOf(Game::Fallout3, { { "iMaxCharacterLevel", 20 } });
        EXPECT_EQ(maxLevelForFiles(rules, { "Fallout3.esm", "Anchorage.esm" }), 20);
        EXPECT_EQ(maxLevelForFiles(rules, { "Fallout3.esm", "BrokenSteel.esm" }), 30);
        const LevellingRules higher = rulesOf(Game::Fallout3, { { "iMaxCharacterLevel", 40 } });
        EXPECT_EQ(maxLevelForFiles(higher, { "Fallout3.esm", "BrokenSteel.esm" }), 40);
    }

    TEST(FalloutScriptLevellingTest, theProgressKnowsWhenItIsAtTheHighestLevel)
    {
        PlayerProgress progress;
        progress.setRules(rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } }));
        EXPECT_FALSE(progress.atHighestLevel());
        progress.setLevel(29);
        EXPECT_FALSE(progress.atHighestLevel());
        progress.setLevel(30);
        EXPECT_TRUE(progress.atHighestLevel());
        // Rules with no highest level never are
        progress.setRules(rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 0 } }));
        EXPECT_FALSE(progress.atHighestLevel());
    }

    TEST(FalloutScriptLevellingTest, needsTheExperienceThatTheExecutableAdds)
    {
        // iXPBase 200 of the executable and iXPBumpBase 150 of the files
        const LevellingRules rules = rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } });
        EXPECT_EQ(experienceForLevel(rules, 1), 0);
        EXPECT_EQ(experienceForLevel(rules, 2), 200);
        EXPECT_EQ(experienceForLevel(rules, 3), 550);
        EXPECT_EQ(experienceForLevel(rules, 10), 7200);
        EXPECT_EQ(experienceForLevel(rules, 20), 29450);
        EXPECT_EQ(experienceForLevel(rules, 30), 66700);
        // the highest level needs no more than itself
        EXPECT_EQ(experienceForLevel(rules, 31), 66700);
    }

    TEST(FalloutScriptLevellingTest, readsTheLevelFromTheExperienceUpToTheHighestLevel)
    {
        const LevellingRules rules = rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } });
        EXPECT_EQ(levelForExperience(rules, -3), 1);
        EXPECT_EQ(levelForExperience(rules, 199), 1);
        EXPECT_EQ(levelForExperience(rules, 200), 2);
        EXPECT_EQ(levelForExperience(rules, 549), 2);
        EXPECT_EQ(levelForExperience(rules, 550), 3);
        EXPECT_EQ(levelForExperience(rules, 66699), 29);
        EXPECT_EQ(levelForExperience(rules, 66700), 30);
        EXPECT_EQ(levelForExperience(rules, 1000000), 30);
    }

    TEST(FalloutScriptLevellingTest, highestLevelIsTwentyUntilTheFilesSayOtherwise)
    {
        EXPECT_EQ(rulesOf(Game::Fallout3).mMaxLevel, 20);
        EXPECT_EQ(rulesOf(Game::NewVegas).mMaxLevel, 20);
        EXPECT_EQ(rulesOf(Game::Fallout3, { { "iMaxCharacterLevel", 30 } }).mMaxLevel, 30);
    }

    TEST(FalloutScriptLevellingTest, givesTheSkillPointsOfFallout3FromTheSettingsOfItsFiles)
    {
        const LevellingRules rules
            = rulesOf(Game::Fallout3, { { "iLevelUpSkillPointsBase", 11 }, { "iLevelUpSkillPointsInterval", 1 } });
        EXPECT_EQ(skillPoints(rules, 1, 2), 11);
        EXPECT_EQ(skillPoints(rules, 5, 2), 15);
        EXPECT_EQ(skillPoints(rules, 10, 2), 20);
        // intelligence above 10 counts as 11 at most
        EXPECT_EQ(skillPoints(rules, 12, 2), 21);
    }

    TEST(FalloutScriptLevellingTest, theRoomOfASkillIsWhatIsLeftUnderOneHundredOfItsValue)
    {
        EXPECT_EQ(skillRoom(0), 100);
        EXPECT_EQ(skillRoom(95), 5);
        EXPECT_EQ(skillRoom(95.5), 4);
        // a bonus that takes the value to 100 or over leaves no room, a penalty makes room
        EXPECT_EQ(skillRoom(105), 0);
        EXPECT_EQ(skillRoom(100), 0);
        EXPECT_EQ(skillRoom(90), 10);
        EXPECT_EQ(skillRoom(-5), 105);
    }

    TEST(FalloutScriptLevellingTest, givesTheSkillPointsOfNewVegasFromItsOwnRule)
    {
        const LevellingRules rules = rulesOf(Game::NewVegas);
        EXPECT_EQ(skillPoints(rules, 5, 3), 12);
        EXPECT_EQ(skillPoints(rules, 5, 4), 13);
        EXPECT_EQ(skillPoints(rules, 6, 4), 13);
        EXPECT_EQ(skillPoints(rules, 10, 4), 15);
        EXPECT_EQ(skillPoints(rules, 15, 3), 15);
    }

    TEST(FalloutScriptLevellingTest, offersAPerkOnEveryLevelInFallout3AndEverySecondInNewVegas)
    {
        const LevellingRules fallout3 = rulesOf(Game::Fallout3);
        const LevellingRules newVegas = rulesOf(Game::NewVegas, { { "iLevelsPerPerk", 2 } });
        EXPECT_FALSE(givesPerk(fallout3, 1));
        EXPECT_TRUE(givesPerk(fallout3, 2));
        EXPECT_TRUE(givesPerk(fallout3, 3));
        EXPECT_FALSE(givesPerk(newVegas, 1));
        EXPECT_TRUE(givesPerk(newVegas, 2));
        EXPECT_FALSE(givesPerk(newVegas, 3));
        EXPECT_TRUE(givesPerk(newVegas, 4));
    }

    TEST(FalloutScriptLevellingTest, paysForAKillByTheRowOfTheLevelOfTheVictim)
    {
        const LevellingRules rules = rulesOf(Game::Fallout3);
        // a level 8 character is in the row of level 10: 30 points at the normal difficulty
        EXPECT_EQ(killExperience(rules, false, 8), 30);
        EXPECT_EQ(killExperience(rules, true, 8), 25);
        // a victim above every row gets the last one
        EXPECT_EQ(killExperience(rules, false, 40), 50);
        EXPECT_EQ(killExperience(rules, true, 40), 50);
        EXPECT_EQ(killExperience(rules, false, 0), 0);
    }

    TEST(FalloutScriptLevellingTest, scalesAKillByTheDifficulty)
    {
        const LevellingRules rules = rulesOf(Game::Fallout3, { { "fDiffMultXPH", 1.25 }, { "fDiffMultXPVH", 1.5 } });
        EXPECT_EQ(killExperience(rules, false, 8, 0), 15);
        EXPECT_EQ(killExperience(rules, false, 8, 2), 30);
        EXPECT_EQ(killExperience(rules, false, 8, 3), 38);
        EXPECT_EQ(killExperience(rules, false, 8, 4), 45);
        EXPECT_EQ(killExperience(rules, false, 8, 9), 45);
    }

    TEST(FalloutScriptLevellingTest, takesTheTablesOfKillsFromTheSettings)
    {
        const LevellingRules rules = rulesOf(Game::NewVegas,
            { { "iXPLevelKillNPCEasy", 4 }, { "iXPRewardKillNPCEasy", 99 }, { "iXPLevelKillNPCVeryEasy", 0 } });
        EXPECT_EQ(killExperience(rules, false, 2), 99);
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, stopsTheExperienceAtTheHighestLevel)
    {
        PlayerProgress progress;
        progress.setRules(rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } }));
        progress.rewardExperience(1000000);
        EXPECT_EQ(progress.experience(), 66700);
        EXPECT_EQ(progress.level(), 30);
        EXPECT_EQ(progress.experienceForNextLevel(), 66700);
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, handsOutThePointsOfEveryLevelReachedOnce)
    {
        PlayerProgress progress;
        progress.setRules(rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } }));
        EXPECT_FALSE(progress.awardNextLevel(5).has_value());

        progress.rewardExperience(560);
        EXPECT_EQ(progress.level(), 3);
        const auto second = progress.awardNextLevel(5);
        ASSERT_TRUE(second.has_value());
        EXPECT_EQ(second->mLevel, 2);
        EXPECT_EQ(second->mSkillPoints, 13);
        EXPECT_TRUE(second->mPerk);
        const auto third = progress.awardNextLevel(5);
        ASSERT_TRUE(third.has_value());
        EXPECT_EQ(third->mLevel, 3);
        EXPECT_EQ(third->mSkillPoints, 12);
        EXPECT_FALSE(third->mPerk);
        EXPECT_FALSE(progress.awardNextLevel(5).has_value());
        EXPECT_EQ(progress.skillPoints(), 25);
        EXPECT_EQ(progress.perkPoints(), 1);
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, choosesEachPerkAtTheLevelThatGaveItsPoint)
    {
        PlayerProgress progress;
        progress.setRules(rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } }));
        EXPECT_EQ(progress.perkLevel(), 1);

        // one award of three levels: a perk at level 2 and one at level 4, the level of the player being 5
        progress.rewardExperience(1700);
        ASSERT_EQ(progress.level(), 5);
        while (progress.awardNextLevel(5))
        {
        }
        EXPECT_EQ(progress.perkPoints(), 2);
        EXPECT_EQ(progress.perkLevel(), 2);
        EXPECT_TRUE(progress.spendPerkPoint());
        EXPECT_EQ(progress.perkLevel(), 4);
        EXPECT_TRUE(progress.spendPerkPoint());
        // with no point left of a level, the level of the player
        EXPECT_EQ(progress.perkLevel(), 5);
        EXPECT_FALSE(progress.spendPerkPoint());
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, aPerkPointFromAScriptIsChosenAtTheLevelOfThePlayer)
    {
        PlayerProgress progress;
        progress.setRules(rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } }));
        progress.rewardExperience(1700);
        progress.addPerkPoints(1);
        EXPECT_EQ(progress.perkLevel(), 5);
        // a script that takes points takes the latest levels first
        while (progress.awardNextLevel(5))
        {
        }
        EXPECT_EQ(progress.perkPoints(), 3);
        EXPECT_EQ(progress.perkLevel(), 2);
        progress.addPerkPoints(-2);
        EXPECT_EQ(progress.perkPoints(), 1);
        EXPECT_EQ(progress.perkLevel(), 2);
        progress.addPerkPoints(-1);
        EXPECT_EQ(progress.perkLevel(), 5);
        progress.addPerkPoints(1);
        progress.clear();
        EXPECT_EQ(progress.perkLevel(), 1);
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, spendsPointsOnlyThatItHas)
    {
        PlayerProgress progress;
        progress.addSkillPoints(5);
        progress.addPerkPoints(1);
        EXPECT_FALSE(progress.spendSkillPoints(6));
        EXPECT_TRUE(progress.spendSkillPoints(5));
        EXPECT_EQ(progress.skillPoints(), 0);
        EXPECT_TRUE(progress.spendPerkPoint());
        EXPECT_FALSE(progress.spendPerkPoint());
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, setsTheLevelAtOnceWithoutGivingPoints)
    {
        PlayerProgress progress;
        progress.setRules(rulesOf(Game::NewVegas, { { "iMaxCharacterLevel", 30 } }));
        progress.setLevel(6);
        EXPECT_EQ(progress.level(), 6);
        EXPECT_EQ(progress.experience(), 2500);
        EXPECT_FALSE(progress.awardNextLevel(5).has_value());
        progress.resetExperience();
        EXPECT_EQ(progress.level(), 1);
        progress.setLevel(99);
        EXPECT_EQ(progress.level(), 30);
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, countsTheStatsOfThePipBoy)
    {
        PlayerProgress progress;
        EXPECT_EQ(progress.miscStat(6), 0);
        progress.modifyMiscStat(6, 2);
        progress.modifyMiscStat(6, 1);
        progress.modifyMiscStat(7, -4);
        EXPECT_EQ(progress.miscStat(6), 3);
        EXPECT_EQ(progress.miscStat(7), 0);
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, listsThePerksOfACharacter)
    {
        PlayerProgress progress;
        progress.addPerk(0x14, 0x0100A001);
        progress.addPerk(0x14, 0x0100A001);
        progress.addPerk(0x14, 0x0100A002);
        progress.addPerk(0x15, 0x0100A003);
        const auto perks = progress.perksOf(0x14);
        ASSERT_EQ(perks.size(), 2u);
        const std::pair<FalloutScript::FormId, int> first(0x0100A001, 2);
        const std::pair<FalloutScript::FormId, int> second(0x0100A002, 1);
        EXPECT_EQ(perks[0], first);
        EXPECT_EQ(perks[1], second);
        EXPECT_TRUE(progress.perksOf(0x16).empty());
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, listsThePerksInTheOrderTheyWereGained)
    {
        PlayerProgress progress;
        progress.addPerk(0x14, 0x0100A002);
        progress.addPerk(0x14, 0x0100A001);
        std::vector<FalloutScript::FormId> order;
        for (const auto& [perk, rank] : progress.perksOf(0x14))
            order.push_back(perk);
        EXPECT_EQ(order, (std::vector<FalloutScript::FormId>{ 0x0100A002, 0x0100A001 }));
        // A perk that gains a rank is the last gained
        progress.addPerk(0x14, 0x0100A002);
        order.clear();
        for (const auto& [perk, rank] : progress.perksOf(0x14))
            order.push_back(perk);
        EXPECT_EQ(order, (std::vector<FalloutScript::FormId>{ 0x0100A001, 0x0100A002 }));
    }

    TEST(FalloutScriptPlayerProgressLevellingTest, listsTheCharactersThatHavePerks)
    {
        PlayerProgress progress;
        EXPECT_TRUE(progress.perkOwners().empty());
        progress.addPerk(0x15, 0x0100A003);
        progress.addPerk(0x14, 0x0100A001);
        progress.addPerk(0x14, 0x0100A002);
        const std::vector<FalloutScript::FormId> owners = progress.perkOwners();
        ASSERT_EQ(owners.size(), 2u);
        EXPECT_EQ(owners[0], 0x14u);
        EXPECT_EQ(owners[1], 0x15u);
        progress.removePerk(0x14, 0x0100A001);
        progress.removePerk(0x14, 0x0100A002);
        EXPECT_EQ(progress.perkOwners().size(), 1u);
    }
}
