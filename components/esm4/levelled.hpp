#ifndef OPENFALLOUT_COMPONENTS_ESM4_LEVELLED_H
#define OPENFALLOUT_COMPONENTS_ESM4_LEVELLED_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <components/esm/formid.hpp>

#include "actor.hpp" // ACBS_FO3
#include "inventory.hpp" // LVLO

namespace ESM4
{
    // How a levelled list of Fallout 3 and New Vegas (LVLI, LVLN, LVLC) chooses what it gives. A list that lies deeper
    // in other lists than this is a cycle or a mistake.
    constexpr std::size_t maxLevelledDepth = 8;

    // The level that a character or creature of Fallout 3 or New Vegas has (ActorBaseData::GetLevel of the New Vegas
    // executable, 0x47DED0): the number in its ACBS, or, when the flag for it is set, the level of the player times
    // that number in thousandths with the fraction cut off. The minimum then wins over the maximum: a result below a
    // minimum that is above 0 becomes the minimum, and only otherwise a result above a maximum that is above 0 becomes
    // the maximum. The callers that make a character (0x567E10) count nothing below 1.
    int levelFromConfig(const ACBS_FO3& config, int playerLevel);

    // The chance in a hundred that a levelled list gives nothing when the chance comes from a global variable (LVLG):
    // the value with the fraction cut off, as one byte (the executable keeps the chance in a byte, 0x487D00)
    int levelledChanceFromGlobal(float value);

    // What the executable's settings for levelled lists are when no file gives them: iLevItemLevelDifferenceMax,
    // iLevCharLevelDifferenceMax and iLevCreaLevelDifferenceMax are 0 (no file of either game has the settings)
    constexpr int defaultLevelDifference = 0;

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
        /// The level modifier that a script gave a placement (PlaceLeveledActorAtMe): 0 none, then easy, medium, hard,
        /// boss. A list with a modifier above 0 chooses as one that calculates from all levels does.
        int mLevelMod = 0;
        /// The setting for how far below the level an entry may lie, when the list chooses from all levels
        int mLevelDifference = defaultLevelDifference;
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

    // Adds an entry to a list the way the executable does it for the commands that scripts add entries with (0x487A60):
    // the entry goes behind the last entry whose level is not above its own, in front of the first that is above it
    // (the walk stops at the first entry above, so a list that is not sorted gets it there too).
    void insertLevelledEntry(std::vector<LVLO>& entries, const LVLO& entry);

    // What scripts added to the lists (the commands AddItemToLeveledList, AddNPCToLeveledList and
    // AddCreatureToLeveledList): the entries of the list in the file with the additions put in. False when the list has
    // none, and the entries of the file stand. It is called from any thread.
    using LevelledAdditions = std::function<bool(ESM::FormId list, const std::vector<LVLO>& entries, std::vector<LVLO>& merged)>;
    void setLevelledAdditions(LevelledAdditions additions);

    // The level that the levelled lists of a placed character or creature are rolled for, and the level modifier of its
    // placement (0 when it has none). The host that knows the player and the zones gives it for each reference and keeps
    // it with the reference; without one the callers' default stands.
    struct SpawnRoll
    {
        int mLevel = 1;
        int mLevelMod = 0;
    };
    using SpawnRollProvider = std::function<SpawnRoll(ESM::FormId reference)>;
    // Setting or clearing a provider waits for the calls that are running one already (so that the host that it captures
    // can go away after clearing it; the wait ends after a few seconds at most). The same holds for the additions above.
    void setSpawnRollProvider(SpawnRollProvider provider);
    // The roll of the reference: the provider's, or the fallback level with no modifier. It is called from any thread.
    SpawnRoll spawnRollOf(ESM::FormId reference, int fallbackLevel);

    // The entries to choose from for the list: those of the record, or the merged ones when scripts added some.
    // `storage` keeps the merged entries.
    const std::vector<LVLO>& levelledEntriesOf(
        ESM::FormId list, const std::vector<LVLO>& entries, std::vector<LVLO>& storage);

    // The record that a form id names, or that a levelled list of records (LVLN of NPC_, LVLC of CREA) in its place
    // gives for the level: the list chooses an entry, and an entry that is a list again chooses in turn. If the entry
    // chosen gives no record (the list gives nothing, or what it names does not exist), the next of the entries chosen
    // is tried. `find` takes a form id and gives a pointer to the record or null; `findList` the same for the lists.
    // `emptyLists` counts the lists that gave no record.
    template <class Record, class Find, class FindList>
    const Record* resolveLevelledRecord(const Find& find, const FindList& findList, ESM::FormId id, int level,
        LevelledRandom& random, std::size_t* emptyLists = nullptr, std::size_t depth = 0, int levelMod = 0)
    {
        if (const Record* record = find(id))
            return record;
        const auto* list = findList(id);
        if (list == nullptr || depth >= maxLevelledDepth)
            return nullptr;

        LevelledRules rules;
        rules.mChanceNone = list->chanceNone();
        rules.mAllLevels = list->calcAllLvlLessThanPlayer();
        rules.mLevelMod = levelMod;
        std::vector<LVLO> storage;
        std::vector<LevelledChoice> chosen;
        chooseLevelledEntries(levelledEntriesOf(id, list->mLvlObject, storage), rules, level, random, chosen);
        for (const LevelledChoice& entry : chosen)
            if (const Record* record = resolveLevelledRecord<Record>(
                    find, findList, entry.mItem, level, random, emptyLists, depth + 1, levelMod))
                return record;
        if (emptyLists != nullptr)
            ++*emptyLists;
        return nullptr;
    }
}

#endif
