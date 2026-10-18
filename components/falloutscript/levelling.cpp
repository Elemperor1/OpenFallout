#include "levelling.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include <components/misc/strings/lower.hpp>

namespace FalloutScript
{
    namespace
    {
        constexpr std::array<std::string_view, 5> bandNames{ "VeryEasy", "Easy", "Average", "Hard", "VeryHard" };
        constexpr std::array<std::string_view, 5> difficultyNames{ "VE", "E", "N", "H", "VH" };

        /// What a skill can be raised to by points
        constexpr double skillLimit = 100;

        void take(const LevellingRules::Lookup& lookup, std::string_view name, double& setting)
        {
            if (const std::optional<double> value = lookup(name))
                setting = *value;
        }

        void takeBands(const LevellingRules::Lookup& lookup, std::string_view levelPrefix,
            std::string_view rewardPrefix, std::array<ExperienceBand, 5>& bands)
        {
            for (std::size_t i = 0; i < bands.size(); ++i)
            {
                if (const auto level = lookup(std::string(levelPrefix) + std::string(bandNames[i])))
                    bands[i].mLevel = static_cast<int>(std::lround(*level));
                if (const auto reward = lookup(std::string(rewardPrefix) + std::string(bandNames[i])))
                    bands[i].mReward = static_cast<int>(std::lround(*reward));
            }
        }

        int clampToInt(std::int64_t value)
        {
            return static_cast<int>(
                std::clamp<std::int64_t>(value, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
        }

        std::int64_t uncappedExperience(const LevellingRules& rules, std::int64_t level)
        {
            if (level <= 1)
                return 0;
            const double steps = static_cast<double>(level - 1) * rules.mExperienceBase
                + rules.mExperienceBump * static_cast<double>(level - 1) * static_cast<double>(level - 2) / 2;
            return static_cast<std::int64_t>(std::llround(steps));
        }
    }

    LevellingRules LevellingRules::fromGameSettings(Game game, const Lookup& lookup)
    {
        LevellingRules rules;
        rules.mGame = game;
        // The executable builds these when the files do not: the highest level is 20, a perk every second level in
        // New Vegas (Fallout 3 has no such setting and offers one on every level)
        rules.mMaxLevel = 20;
        rules.mLevelsPerPerk = game == Game::Fallout3 ? 1 : 2;
        if (!lookup)
            return rules;
        take(lookup, "iXPBase", rules.mExperienceBase);
        take(lookup, "iXPBumpBase", rules.mExperienceBump);
        if (const auto maxLevel = lookup("iMaxCharacterLevel"))
            rules.mMaxLevel = std::max(static_cast<int>(std::lround(*maxLevel)), 0);
        take(lookup, "iLevelUpSkillPointsBase", rules.mSkillPointsBase);
        take(lookup, "iLevelUpSkillPointsInterval", rules.mSkillPointsInterval);
        if (game != Game::Fallout3)
            if (const auto perLevels = lookup("iLevelsPerPerk"))
                rules.mLevelsPerPerk = std::max(static_cast<int>(std::lround(*perLevels)), 1);
        takeBands(lookup, "iXPLevelKillCreature", "iXPRewardKillOpponent", rules.mKillCreature);
        takeBands(lookup, "iXPLevelKillNPC", "iXPRewardKillNPC", rules.mKillCharacter);
        take(lookup, "iXPDeathRewardHealthThreshold", rules.mDeathHealthThreshold);
        for (std::size_t i = 0; i < difficultyNames.size(); ++i)
            take(lookup, "fDiffMultXP" + std::string(difficultyNames[i]), rules.mDifficultyMult[i]);
        return rules;
    }

    int Levelling::experienceForLevel(const LevellingRules& rules, int level)
    {
        if (rules.mMaxLevel > 0)
            level = std::min(level, rules.mMaxLevel);
        return clampToInt(uncappedExperience(rules, level));
    }

    int Levelling::levelForExperience(const LevellingRules& rules, int experience)
    {
        // The largest level whose experience the amount reaches: the experience grows with the level, so search
        std::int64_t low = 1;
        std::int64_t high = rules.mMaxLevel > 0 ? rules.mMaxLevel : 100000;
        while (low < high)
        {
            const std::int64_t middle = (low + high + 1) / 2;
            if (uncappedExperience(rules, middle) <= experience)
                low = middle;
            else
                high = middle - 1;
        }
        return static_cast<int>(low);
    }

    int Levelling::maxLevelForFiles(const LevellingRules& rules, const std::vector<std::string>& files)
    {
        const auto loaded = [&files](std::string_view prefix) {
            return std::any_of(files.begin(), files.end(), [prefix](const std::string& file) {
                return Misc::StringUtils::lowerCase(file).compare(0, prefix.size(), prefix) == 0;
            });
        };
        if (rules.mGame == Game::Fallout3)
            return loaded("brokensteel") ? std::max(rules.mMaxLevel, 30) : rules.mMaxLevel;
        int level = 30;
        for (const std::string_view addOn : { "deadmoney", "honesthearts", "oldworldblues", "lonesomeroad" })
            if (loaded(addOn))
                level += 5;
        return std::clamp(level, 30, 50);
    }

    int Levelling::skillPoints(const LevellingRules& rules, double intelligence, int level)
    {
        const double clamped = std::min(intelligence, 10.0);
        if (rules.mGame == Game::Fallout3)
        {
            const double steps = std::min(intelligence - 1, 10.0);
            return std::max(
                static_cast<int>(std::floor(rules.mSkillPointsBase + rules.mSkillPointsInterval * steps)), 0);
        }
        // New Vegas does not use those settings: half the intelligence and ten, one more when the level reached is
        // even and the intelligence odd (the level argument is a guess)
        int points = static_cast<int>(std::floor(0.5 * clamped + 10));
        if (level % 2 == 0 && static_cast<int>(std::floor(clamped)) % 2 == 1)
            ++points;
        return points;
    }

    int Levelling::skillRoom(double value)
    {
        return static_cast<int>(std::floor(std::max(skillLimit - value, 0.0)));
    }

    bool Levelling::givesPerk(const LevellingRules& rules, int level)
    {
        return level >= 2 && level % std::max(rules.mLevelsPerPerk, 1) == 0;
    }

    int Levelling::killExperience(const LevellingRules& rules, bool creature, int victimLevel, int difficulty)
    {
        const std::array<ExperienceBand, 5>& bands = creature ? rules.mKillCreature : rules.mKillCharacter;
        // The first row whose level reaches the level of the victim; a victim above all of them gets the last row
        const ExperienceBand* row = &bands.back();
        for (const ExperienceBand& band : bands)
            if (band.mLevel >= victimLevel)
            {
                row = &band;
                break;
            }
        const double multiplier = rules.mDifficultyMult[static_cast<std::size_t>(std::clamp(difficulty, 0, 4))];
        return std::max(static_cast<int>(std::lround(row->mReward * multiplier)), 0);
    }
}
