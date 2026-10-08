#include "wornarmor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ESM4
{
    namespace
    {
        // A list of lists that goes deeper than this, or a chain of templates that is longer, is a cycle or a mistake
        constexpr std::size_t maxListDepth = 8;
        constexpr std::size_t maxTemplateDepth = 16;
        // A list that calculates for each item in its count is rolled once per item, and a list can hold lists that do
        // the same. The counts of the real records are a few at most; this many rolls in all, for one character, stop
        // a corrupt count of billions (or lists nested in lists with counts of thousands) from holding the game up,
        // and a list with dozens of entries has given every one of them long before.
        constexpr std::uint32_t maxRollsPerCharacter = 16384;

        // splitmix64, which gives the same numbers on every platform
        class Random
        {
        public:
            explicit Random(std::uint64_t seed)
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
            bool mUseAll = false; // every entry that may be chosen is
        };

        enum class Outcome
        {
            Chosen,
            EmptyByChance,
            EmptyByLevel,
        };

        // The entries of a levelled list that a character of the level gets. The entries of a list are all those at or
        // below the level; unless the list calculates from all levels, only the highest of them. Of those one is
        // chosen, or all of them if the list uses all. The list may also give nothing, by a chance in a hundred.
        struct Choice
        {
            ESM::FormId mItem;
            int mCount;
        };

        Outcome chooseEntries(const std::vector<LVLO>& entries, const LevelledRules& rules, int level, Random& random,
            std::vector<Choice>& chosen)
        {
            if (rules.mChanceNone > 0 && static_cast<int>(random.below(100)) < rules.mChanceNone)
                return Outcome::EmptyByChance;

            std::vector<const LVLO*> eligible;
            for (const LVLO& entry : entries)
                if (entry.level <= level && entry.item != 0)
                    eligible.push_back(&entry);
            if (eligible.empty())
                return Outcome::EmptyByLevel;

            if (!rules.mAllLevels)
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
            return Outcome::Chosen;
        }

        const Npc* resolveNpc(const WornArmorSource& source, ESM::FormId id, int level, Random& random,
            WornArmorTrace* trace, std::size_t depth)
        {
            if (const Npc* npc = source.findNpc(id))
                return npc;
            const LevelledNpc* list = source.findLevelledNpc(id);
            if (list == nullptr || depth >= maxListDepth)
                return nullptr;

            LevelledRules rules;
            rules.mChanceNone = list->chanceNone();
            rules.mAllLevels = list->calcAllLvlLessThanPlayer();
            std::vector<Choice> chosen;
            chooseEntries(list->mLvlObject, rules, level, random, chosen);
            for (const Choice& entry : chosen)
                if (const Npc* npc = resolveNpc(source, entry.mItem, level, random, trace, depth + 1))
                    return npc;
            if (trace != nullptr)
                ++trace->mTemplateListsEmpty;
            return nullptr;
        }

        class ArmorCollector
        {
        public:
            ArmorCollector(const WornArmorSource& source, int level, Random& random, WornArmorTrace* trace)
                : mSource(source)
                , mLevel(level)
                , mRandom(random)
                , mTrace(trace)
            {
            }

            void addEntry(ESM::FormId id, std::uint32_t count)
            {
                if (mTrace != nullptr)
                    ++mTrace->mItems;
                if (mSource.findArmor(id) != nullptr)
                {
                    if (mTrace != nullptr)
                        ++mTrace->mArmorListed;
                    add(id);
                }
                else if (mSource.findLevelledItem(id) != nullptr)
                {
                    if (mTrace != nullptr)
                        ++mTrace->mListsEntered;
                    expand(id, 0, count);
                }
                else if (mTrace != nullptr)
                    ++mTrace->mOtherItems;
            }

            std::vector<const Armor*> take() { return std::move(mArmor); }

        private:
            void add(ESM::FormId id)
            {
                const Armor* armor = mSource.findArmor(id);
                if (armor != nullptr && std::find(mArmor.begin(), mArmor.end(), armor) == mArmor.end())
                    mArmor.push_back(armor);
            }

            // Adds the list `count` times: a list that calculates for each item in its count gives a new roll for every
            // item, any other gives the same entries each time, which adds no armour that is not there already
            void expand(ESM::FormId listId, std::size_t depth, std::uint32_t count)
            {
                const LevelledItem* list = mSource.findLevelledItem(listId);
                if (list == nullptr)
                    return;
                if (depth >= maxListDepth)
                {
                    if (mTrace != nullptr)
                        ++mTrace->mListsTooDeep;
                    return;
                }

                const std::uint32_t rolls = list->calcEachItemInCount() ? std::max<std::uint32_t>(count, 1) : 1;
                for (std::uint32_t roll = 0; roll < rolls && mRollsLeft > 0; ++roll)
                    rollOnce(*list, depth);
            }

            void rollOnce(const LevelledItem& list, std::size_t depth)
            {
                if (mRollsLeft == 0)
                    return;
                --mRollsLeft;
                LevelledRules rules;
                rules.mChanceNone = list.chanceNone();
                rules.mAllLevels = list.calcAllLvlLessThanPlayer();
                rules.mUseAll = list.useAll();
                std::vector<Choice> chosen;
                switch (chooseEntries(list.mLvlObject, rules, mLevel, mRandom, chosen))
                {
                    case Outcome::EmptyByChance:
                        if (mTrace != nullptr)
                            ++mTrace->mListsEmptyByChance;
                        return;
                    case Outcome::EmptyByLevel:
                        if (mTrace != nullptr)
                            ++mTrace->mListsEmptyByLevel;
                        return;
                    case Outcome::Chosen:
                        break;
                }
                if (rules.mUseAll && mTrace != nullptr)
                    ++mTrace->mListsUsingAll;

                for (const Choice& entry : chosen)
                {
                    if (mSource.findArmor(entry.mItem) != nullptr)
                    {
                        if (mTrace != nullptr)
                            ++mTrace->mArmorFromLists;
                        add(entry.mItem);
                    }
                    else if (mSource.findLevelledItem(entry.mItem) != nullptr)
                        expand(entry.mItem, depth + 1, static_cast<std::uint32_t>(entry.mCount));
                }
            }

            const WornArmorSource& mSource;
            int mLevel;
            Random& mRandom;
            WornArmorTrace* mTrace;
            std::uint32_t mRollsLeft = maxRollsPerCharacter;
            std::vector<const Armor*> mArmor;
        };
    }

    int actorLevel(const Npc& npc, int playerLevel)
    {
        if (!npc.mIsFONV)
            return playerLevel;

        const ACBS_FO3& config = npc.mBaseConfig.fo3;
        int level = config.levelOrMult;
        if ((config.flags & Npc::FO3_PCLevelMult) != 0)
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

    std::vector<const Npc*> templateChain(
        const WornArmorSource& source, const Npc& npc, int playerLevel, std::uint32_t seed, WornArmorTrace* trace)
    {
        Random random(seed ^ 0x5851F42Dull);
        std::vector<const Npc*> chain{ &npc };
        while (chain.size() < maxTemplateDepth)
        {
            const Npc& current = *chain.back();
            if (current.mBaseTemplate.isZeroOrUnset())
                break;
            const Npc* next
                = resolveNpc(source, current.mBaseTemplate, actorLevel(current, playerLevel), random, trace, 0);
            if (next == nullptr || std::find(chain.begin(), chain.end(), next) != chain.end())
                break;
            chain.push_back(next);
        }
        return chain;
    }

    const Npc* templateOwner(const std::vector<const Npc*>& chain, std::uint16_t flag)
    {
        for (const Npc* npc : chain)
            if (!npc->takesFromTemplate(flag))
                return npc;
        return nullptr;
    }

    std::vector<const Armor*> wornArmor(
        const WornArmorSource& source, const Npc& character, int playerLevel, std::uint32_t seed, WornArmorTrace* trace)
    {
        if (trace != nullptr)
            ++trace->mCharacters;

        const std::vector<const Npc*> chain = templateChain(source, character, playerLevel, seed, trace);
        const Npc* inventoryOwner = templateOwner(chain, Npc::Template_UseInventory);
        if (inventoryOwner == nullptr)
        {
            if (trace != nullptr)
                ++trace->mNoInventory;
            return {};
        }
        if (trace != nullptr && inventoryOwner != &character)
            ++trace->mInventoryFromTemplate;

        // The level of a character is the one of the record that has its stats
        const Npc* statsOwner = templateOwner(chain, Npc::Template_UseStats);
        const int level = actorLevel(statsOwner != nullptr ? *statsOwner : character, playerLevel);

        Random random(seed);
        ArmorCollector collector(source, level, random, trace);
        for (const InventoryItem& item : inventoryOwner->mInventory)
            collector.addEntry(ESM::FormId::fromUint32(item.item), item.count);
        return collector.take();
    }

    WornPieces wornPieces(const std::vector<const Armor*>& armor, bool isFemale)
    {
        WornPieces worn;
        for (const Armor* piece : armor)
        {
            if (piece == nullptr)
                continue;
            const std::uint32_t slots = piece->mArmorFlags & bipedSlotMask;
            const ESM::Path& model = isFemale && !piece->mModelFemale.empty() ? piece->mModelFemale : piece->mModelMale;
            if (slots == 0)
                ++worn.mWithoutSlots;
            else if (model.empty())
                ++worn.mWithoutModel;
            else if ((slots & worn.mCovered) != 0)
                ++worn.mOverlapping;
            else
            {
                worn.mCovered |= slots;
                worn.mPieces.push_back({ piece, &model, slots });
            }
        }
        return worn;
    }
}
