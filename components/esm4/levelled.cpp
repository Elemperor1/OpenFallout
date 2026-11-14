#include "levelled.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>

namespace ESM4
{
    int levelFromConfig(const ACBS_FO3& config, int playerLevel)
    {
        constexpr std::uint32_t levelIsMultiplier = 0x80; // PCLevelMult, of NPC_ and CREA alike
        int level = config.levelOrMult;
        if ((config.flags & levelIsMultiplier) != 0)
        {
            // The number is a multiplier in thousandths, which the executable keeps as a float; the product with the
            // level (a whole number, so the double holds it exactly) is cut at the point
            const float multiplier = static_cast<float>(config.levelOrMult / 1000.0);
            level = static_cast<int>(static_cast<double>(playerLevel) * static_cast<double>(multiplier));
            if (config.calcMinlevel > 0 && level < config.calcMinlevel)
                level = config.calcMinlevel;
            else if (config.calcMaxlevel > 0 && level > config.calcMaxlevel)
                level = config.calcMaxlevel;
        }
        return std::max(level, 1);
    }

    int levelledChanceFromGlobal(float value)
    {
        // A value that does not fit a whole number becomes the lowest one the processor has (low byte 0)
        if (!(value > -2147483648.f && value < 2147483648.f))
            return 0;
        return static_cast<int>(value) & 0xFF;
    }

    namespace
    {
        std::uint16_t levelOf(const LVLO& entry)
        {
            return static_cast<std::uint16_t>(entry.level);
        }

        LevelledChoice choiceOf(const LVLO& entry)
        {
            return { ESM::FormId::fromUint32(entry.item), std::max<int>(entry.count, 1) };
        }
    }

    LevelledOutcome chooseLevelledEntries(const std::vector<LVLO>& entries, const LevelledRules& rules, int level,
        LevelledRandom& random, std::vector<LevelledChoice>& chosen)
    {
        // A level below 1 counts as 1 (0x487F70)
        level = std::max(level, 1);

        if (rules.mChanceNone > 0 && static_cast<int>(random.below(100)) < rules.mChanceNone)
            return LevelledOutcome::EmptyByChance;

        if (rules.mUseAll)
        {
            // Every entry that names an item, whatever its level
            bool any = false;
            for (const LVLO& entry : entries)
                if (entry.item != 0)
                {
                    chosen.push_back(choiceOf(entry));
                    any = true;
                }
            return any ? LevelledOutcome::Chosen : LevelledOutcome::EmptyByLevel;
        }

        // A list that calculates from all levels, or that a placement gave a level modifier, chooses from the entries
        // that are at the level or below, the settings for the difference allowing; its level does not go above that
        // of its highest entry. Any other list chooses from the entries of the highest level that is not above the
        // level (0x487D00).
        const bool fromAllLevels = (rules.mAllLevels && rules.mLevelMod == 0) || rules.mLevelMod > 0;
        int threshold = level;
        if (fromAllLevels)
        {
            int highest = 0;
            for (const LVLO& entry : entries)
                highest = std::max<int>(highest, levelOf(entry));
            level = std::min(level, highest);
            threshold = rules.mLevelDifference != 0 ? level - rules.mLevelDifference : -1;
        }

        // The executable walks the list in order and stops at the first entry above the level, so the entries to
        // choose from are those from `first` on, `count` of them. An entry of level 0 never starts the group.
        constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
        std::size_t first = none;
        std::size_t count = 1;
        int best = 0;
        bool reached = false;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            const int entryLevel = levelOf(entries[i]);
            if (entryLevel > level)
                break;
            if (entryLevel > best && (best == 0 || !reached))
            {
                if (entryLevel >= threshold)
                    reached = true;
                count = 1;
                first = i;
                best = entryLevel;
            }
            else
                ++count;
        }
        if (first == none)
            return LevelledOutcome::EmptyByLevel;

        const std::size_t picked = first + random.below(count);
        if (picked >= entries.size() || entries[picked].item == 0)
            return LevelledOutcome::EmptyByLevel;
        chosen.push_back(choiceOf(entries[picked]));
        return LevelledOutcome::Chosen;
    }

    void insertLevelledEntry(std::vector<LVLO>& entries, const LVLO& entry)
    {
        const auto above = std::find_if(entries.begin(), entries.end(),
            [&entry](const LVLO& existing) { return levelOf(existing) > levelOf(entry); });
        entries.insert(above, entry);
    }

    namespace
    {
        std::atomic<bool> sHasAdditions{ false };
        std::mutex sAdditionsMutex;
        LevelledAdditions sAdditions;
    }

    void setLevelledAdditions(LevelledAdditions additions)
    {
        const std::lock_guard lock(sAdditionsMutex);
        sHasAdditions = static_cast<bool>(additions);
        sAdditions = std::move(additions);
    }

    const std::vector<LVLO>& levelledEntriesOf(
        ESM::FormId list, const std::vector<LVLO>& entries, std::vector<LVLO>& storage)
    {
        if (!sHasAdditions.load(std::memory_order_relaxed))
            return entries;
        LevelledAdditions additions;
        {
            const std::lock_guard lock(sAdditionsMutex);
            additions = sAdditions;
        }
        if (additions && additions(list, entries, storage))
            return storage;
        return entries;
    }

    namespace
    {
        std::atomic<bool> sHasSpawnRolls{ false };
        std::mutex sSpawnRollMutex;
        SpawnRollProvider sSpawnRolls;
    }

    void setSpawnRollProvider(SpawnRollProvider provider)
    {
        const std::lock_guard lock(sSpawnRollMutex);
        sHasSpawnRolls = static_cast<bool>(provider);
        sSpawnRolls = std::move(provider);
    }

    SpawnRoll spawnRollOf(ESM::FormId reference, int fallbackLevel)
    {
        if (sHasSpawnRolls.load(std::memory_order_relaxed))
        {
            SpawnRollProvider provider;
            {
                const std::lock_guard lock(sSpawnRollMutex);
                provider = sSpawnRolls;
            }
            if (provider)
                return provider(reference);
        }
        return SpawnRoll{ fallbackLevel, 0 };
    }
}
