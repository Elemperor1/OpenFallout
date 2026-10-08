#ifndef OPENFALLOUT_COMPONENTS_ESM4_LEVELLED_H
#define OPENFALLOUT_COMPONENTS_ESM4_LEVELLED_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <components/esm/formid.hpp>

#include "actor.hpp" // ACBS_FO3
#include "inventory.hpp" // LVLO

namespace ESM4
{
    // How a levelled list of Fallout 3 and New Vegas (LVLI, LVLN, LVLC) chooses what it gives. A list that lies deeper
    // in other lists than this is a cycle or a mistake.
    constexpr std::size_t maxLevelledDepth = 8;

    // The level that a character or creature of Fallout 3 or New Vegas has: the number in its ACBS, or the level of the
    // player times that number (in thousandths) when the flag for it is set, between the limits that ACBS names.
    int levelFromConfig(const ACBS_FO3& config, int playerLevel);

    // splitmix64, which gives the same numbers on every platform, so that a seed always makes the same choices
    class LevelledRandom
    {
    public:
        explicit LevelledRandom(std::uint64_t seed)
            : mState(seed)
        {
        }

        std::uint64_t next()
        {
            std::uint64_t z = (mState += 0x9E3779B97F4A7C15ull);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }

        // A number from 0 to count - 1
        std::size_t below(std::size_t count) { return static_cast<std::size_t>(next() % count); }

    private:
        std::uint64_t mState;
    };

    struct LevelledRules
    {
        int mChanceNone = 0;
        bool mAllLevels = false; // an entry at any level up to the level of the character may be chosen
        bool mUseAll = false; // every entry is chosen, whatever its level
    };

    enum class LevelledOutcome
    {
        Chosen,
        EmptyByChance,
        EmptyByLevel,
    };

    struct LevelledChoice
    {
        ESM::FormId mItem;
        int mCount = 1; // at least 1
    };

    // The entries of a levelled list that something of the level gets. The entries of a list are all those at or below
    // the level; unless the list calculates from all levels, only the highest of them. Of those one is chosen. A list
    // that uses all gives every entry it has instead, whatever the level of the entry (the GECK says "all the items on
    // the list are added" and that it supersedes the other two flags). The list may also give nothing, by a chance in
    // a hundred.
    LevelledOutcome chooseLevelledEntries(const std::vector<LVLO>& entries, const LevelledRules& rules, int level,
        LevelledRandom& random, std::vector<LevelledChoice>& chosen);

    // The record that a form id names, or that a levelled list of records (LVLN of NPC_, LVLC of CREA) in its place
    // gives for the level: the list chooses an entry, and an entry that is a list again chooses in turn. If the entry
    // chosen gives no record (the list gives nothing, or what it names does not exist), the next of the entries chosen
    // is tried. `find` takes a form id and gives a pointer to the record or null; `findList` the same for the lists.
    // `emptyLists` counts the lists that gave no record.
    template <class Record, class Find, class FindList>
    const Record* resolveLevelledRecord(const Find& find, const FindList& findList, ESM::FormId id, int level,
        LevelledRandom& random, std::size_t* emptyLists = nullptr, std::size_t depth = 0)
    {
        if (const Record* record = find(id))
            return record;
        const auto* list = findList(id);
        if (list == nullptr || depth >= maxLevelledDepth)
            return nullptr;

        LevelledRules rules;
        rules.mChanceNone = list->chanceNone();
        rules.mAllLevels = list->calcAllLvlLessThanPlayer();
        std::vector<LevelledChoice> chosen;
        chooseLevelledEntries(list->mLvlObject, rules, level, random, chosen);
        for (const LevelledChoice& entry : chosen)
            if (const Record* record
                = resolveLevelledRecord<Record>(find, findList, entry.mItem, level, random, emptyLists, depth + 1))
                return record;
        if (emptyLists != nullptr)
            ++*emptyLists;
        return nullptr;
    }
}

#endif
