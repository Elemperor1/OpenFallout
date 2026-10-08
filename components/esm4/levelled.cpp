#include "levelled.hpp"

#include <algorithm>
#include <cmath>

namespace ESM4
{
    int levelFromConfig(const ACBS_FO3& config, int playerLevel)
    {
        constexpr std::uint32_t levelIsMultiplier = 0x80; // PCLevelMult, of NPC_ and CREA alike
        int level = config.levelOrMult;
        if ((config.flags & levelIsMultiplier) != 0)
        {
            // The number is a multiplier in thousandths
            level = static_cast<int>(std::lround(playerLevel * (config.levelOrMult / 1000.0)));
            if (config.calcMinlevel > 0)
                level = std::max<int>(level, config.calcMinlevel);
            if (config.calcMaxlevel > 0)
                level = std::min<int>(level, config.calcMaxlevel);
        }
        return std::max(level, 1);
    }

    LevelledOutcome chooseLevelledEntries(const std::vector<LVLO>& entries, const LevelledRules& rules, int level,
        LevelledRandom& random, std::vector<LevelledChoice>& chosen)
    {
        if (rules.mChanceNone > 0 && static_cast<int>(random.below(100)) < rules.mChanceNone)
            return LevelledOutcome::EmptyByChance;

        std::vector<const LVLO*> eligible;
        for (const LVLO& entry : entries)
            if (entry.item != 0 && (rules.mUseAll || entry.level <= level))
                eligible.push_back(&entry);
        if (eligible.empty())
            return LevelledOutcome::EmptyByLevel;

        if (!rules.mUseAll && !rules.mAllLevels)
        {
            const std::int16_t highest
                = (*std::max_element(eligible.begin(), eligible.end(), [](const LVLO* a, const LVLO* b) {
                      return a->level < b->level;
                  }))->level;
            eligible.erase(std::remove_if(eligible.begin(), eligible.end(),
                               [highest](const LVLO* entry) { return entry->level != highest; }),
                eligible.end());
        }

        if (rules.mUseAll)
        {
            for (const LVLO* entry : eligible)
                chosen.push_back({ ESM::FormId::fromUint32(entry->item), std::max<int>(entry->count, 1) });
        }
        else
        {
            const LVLO* entry = eligible[random.below(eligible.size())];
            chosen.push_back({ ESM::FormId::fromUint32(entry->item), std::max<int>(entry->count, 1) });
        }
        return LevelledOutcome::Chosen;
    }
}
