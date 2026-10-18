#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LEVELLING_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LEVELLING_H

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "vanillacommands.hpp"

namespace FalloutScript
{
    /// One row of the tables that pay for a kill: a victim of at most that level is worth that much
    struct ExperienceBand
    {
        int mLevel = 0;
        int mReward = 0;
    };

    /// How the player levels up in the games, read from the executables and the settings of the files (see the notes
    /// of the M6 mac round 3): the experience a level needs, the highest level, the skill points and perks a level
    /// gives, and what a kill is worth. A setting the files do not have keeps the value of the executable (or of
    /// the files of the game when the executable has none).
    struct LevellingRules
    {
        Game mGame = Game::NewVegas;
        /// iXPBase and iXPBumpBase: the experience of level n is (n - 1) * base + bump * (n - 1) * (n - 2) / 2
        double mExperienceBase = 200;
        double mExperienceBump = 150;
        /// iMaxCharacterLevel; 0 for no highest level
        int mMaxLevel = 0;
        /// Fallout 3: iLevelUpSkillPointsBase and iLevelUpSkillPointsInterval (10 + intelligence in its files)
        double mSkillPointsBase = 11;
        double mSkillPointsInterval = 1;
        /// iLevelsPerPerk: New Vegas offers a perk every second level, Fallout 3 on every level
        int mLevelsPerPerk = 2;
        /// iXPLevelKillCreature... with iXPRewardKillOpponent..., and iXPLevelKillNPC... with iXPRewardKillNPC...:
        /// very easy to very hard
        std::array<ExperienceBand, 5> mKillCreature{ { { 1, 1 }, { 3, 5 }, { 6, 10 }, { 9, 25 }, { 12, 50 } } };
        std::array<ExperienceBand, 5> mKillCharacter{ { { 0, 0 }, { 1, 10 }, { 7, 20 }, { 10, 30 }, { 13, 50 } } };
        /// iXPDeathRewardHealthThreshold: the share of the health of the victim in percent that the player has to
        /// have taken for the kill to count
        double mDeathHealthThreshold = 40;
        /// fDiffMultXPVE to fDiffMultXPVH: what the difficulty makes of the experience of a kill
        std::array<double, 5> mDifficultyMult{ 0.5, 0.75, 1, 1.5, 2 };

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The rules of the game with the settings that the lookup (a GMST by its editor id) has
        static LevellingRules fromGameSettings(Game game, const Lookup& lookup);
    };

    namespace Levelling
    {
        /// The experience that a level starts at (0 for level 1)
        int experienceForLevel(const LevellingRules& rules, int level);
        /// The level that an amount of experience is, up to the highest level
        int levelForExperience(const LevellingRules& rules, int experience);
        /// The highest level with the files that are loaded. The executable of New Vegas sets it when the data are
        /// loaded: 30, and 5 more for every one of its add-ons (DeadMoney, HonestHearts, OldWorldBlues, LonesomeRoad,
        /// found by the start of the name of the file), within 30 and 50, whatever the files say. In Fallout 3 the
        /// setting stands (20 in its files), and BrokenSteel makes it at least 30.
        int maxLevelForFiles(const LevellingRules& rules, const std::vector<std::string>& files);
        /// The skill points that reaching the level gives to a player with that intelligence
        int skillPoints(const LevellingRules& rules, double intelligence, int level);
        /// Whether reaching the level gives a perk to choose
        bool givesPerk(const LevellingRules& rules, int level);
        /// What killing a creature or a character of that level is worth for a game at that difficulty (0 very easy
        /// to 4 very hard)
        int killExperience(const LevellingRules& rules, bool creature, int victimLevel, int difficulty = 2);
    }
}

#endif
