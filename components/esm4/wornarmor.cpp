#include "wornarmor.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "levelled.hpp"

namespace ESM4
{
    namespace
    {
        // A chain of templates that is longer is a cycle or a mistake
        constexpr std::size_t maxTemplateDepth = 16;
        // A list that calculates for each item in its count is rolled once per item, and a list can hold lists that do
        // the same. The counts of the real records are a few at most; this many rolls in all, for one character, stop
        // a corrupt count of billions (or lists nested in lists with counts of thousands) from holding the game up,
        // and a list with dozens of entries has given every one of them long before.
        constexpr std::uint32_t maxRollsPerCharacter = 16384;

        class ArmorCollector
        {
        public:
            ArmorCollector(const WornArmorSource& source, int level, LevelledRandom& random, WornArmorTrace* trace)
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
            // item, any other gives the same entries each time, which adds no armour that is not there already. A list
            // that uses all is rolled once, as the flag supersedes the other two.
            void expand(ESM::FormId listId, std::size_t depth, std::uint32_t count)
            {
                const LevelledItem* list = mSource.findLevelledItem(listId);
                if (list == nullptr)
                    return;
                if (depth >= maxLevelledDepth)
                {
                    if (mTrace != nullptr)
                        ++mTrace->mListsTooDeep;
                    return;
                }

                const std::uint32_t rolls
                    = list->calcEachItemInCount() && !list->useAll() ? std::max<std::uint32_t>(count, 1) : 1;
                for (std::uint32_t roll = 0; roll < rolls && mRollsLeft > 0; ++roll)
                    rollOnce(*list, depth);
            }

            // The chance in a hundred that the list gives nothing: the value of the global variable that the list
            // names, if the file has it, and otherwise the chance that the list holds
            int chanceNone(const LevelledItem& list)
            {
                if (list.mGlobal.isZeroOrUnset())
                    return list.chanceNone();
                const GlobalVariable* global = mSource.findGlobal(list.mGlobal);
                if (global == nullptr)
                    return list.chanceNone();
                if (mTrace != nullptr)
                    ++mTrace->mListsWithGlobalChance;
                const float value = global->mValue;
                return value > 0.f ? static_cast<int>(std::lround(std::min(value, 100.f))) : 0;
            }

            void rollOnce(const LevelledItem& list, std::size_t depth)
            {
                if (mRollsLeft == 0)
                    return;
                --mRollsLeft;
                LevelledRules rules;
                rules.mChanceNone = chanceNone(list);
                rules.mAllLevels = list.calcAllLvlLessThanPlayer();
                rules.mUseAll = list.useAll();
                std::vector<LevelledChoice> chosen;
                switch (chooseLevelledEntries(list.mLvlObject, rules, mLevel, mRandom, chosen))
                {
                    case LevelledOutcome::EmptyByChance:
                        if (mTrace != nullptr)
                            ++mTrace->mListsEmptyByChance;
                        return;
                    case LevelledOutcome::EmptyByLevel:
                        if (mTrace != nullptr)
                            ++mTrace->mListsEmptyByLevel;
                        return;
                    case LevelledOutcome::Chosen:
                        break;
                }
                if (rules.mUseAll && mTrace != nullptr)
                    ++mTrace->mListsUsingAll;

                for (const LevelledChoice& entry : chosen)
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
            LevelledRandom& mRandom;
            WornArmorTrace* mTrace;
            std::uint32_t mRollsLeft = maxRollsPerCharacter;
            std::vector<const Armor*> mArmor;
        };
    }

    int actorLevel(const Npc& npc, int playerLevel)
    {
        if (!npc.mIsFONV)
            return playerLevel;
        return levelFromConfig(npc.mBaseConfig.fo3, playerLevel);
    }

    std::vector<const Npc*> templateChain(
        const WornArmorSource& source, const Npc& npc, int playerLevel, std::uint32_t seed, WornArmorTrace* trace)
    {
        LevelledRandom random(seed ^ 0x5851F42Dull);
        std::vector<const Npc*> chain{ &npc };
        while (chain.size() < maxTemplateDepth)
        {
            const Npc& current = *chain.back();
            if (current.mBaseTemplate.isZeroOrUnset())
                break;
            const Npc* next = resolveLevelledRecord<Npc>([&source](ESM::FormId id) { return source.findNpc(id); },
                [&source](ESM::FormId id) { return source.findLevelledNpc(id); }, current.mBaseTemplate,
                actorLevel(current, playerLevel), random, trace != nullptr ? &trace->mTemplateListsEmpty : nullptr);
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

        LevelledRandom random(seed);
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
