#include <components/esm4/levelled.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <map>
#include <set>
#include <thread>
#include <vector>

namespace
{
    using namespace testing;

    ESM::FormId id(std::uint32_t value)
    {
        return ESM::FormId::fromUint32(value);
    }

    ESM4::LVLO entry(int level, std::uint32_t item, int count = 1)
    {
        return { static_cast<std::int16_t>(level), 0, item, static_cast<std::int16_t>(count), 0 };
    }

    TEST(ESM4LevelledTest, choosesTheEntriesOfTheHighestLevelAtOrBelowTheLevel)
    {
        const std::vector<ESM4::LVLO> entries{ entry(1, 0x10), entry(5, 0x11), entry(9, 0x12) };
        ESM4::LevelledRandom random(1);
        std::vector<ESM4::LevelledChoice> chosen;

        EXPECT_EQ(ESM4::chooseLevelledEntries(entries, {}, 6, random, chosen), ESM4::LevelledOutcome::Chosen);
        ASSERT_EQ(chosen.size(), 1u);
        EXPECT_EQ(chosen[0].mItem, id(0x11));
    }

    TEST(ESM4LevelledTest, tellsWhyAListGaveNothing)
    {
        const std::vector<ESM4::LVLO> entries{ entry(5, 0x10) };
        ESM4::LevelledRandom random(1);
        std::vector<ESM4::LevelledChoice> chosen;

        EXPECT_EQ(ESM4::chooseLevelledEntries(entries, {}, 2, random, chosen), ESM4::LevelledOutcome::EmptyByLevel);
        ESM4::LevelledRules always;
        always.mChanceNone = 100;
        EXPECT_EQ(
            ESM4::chooseLevelledEntries(entries, always, 9, random, chosen), ESM4::LevelledOutcome::EmptyByChance);
        EXPECT_TRUE(chosen.empty());
    }

    TEST(ESM4LevelledTest, anEntryHasAtLeastOneOfItsItem)
    {
        const std::vector<ESM4::LVLO> entries{ entry(1, 0x10, 0), entry(1, 0x11, 3), entry(1, 0x12, -2) };
        ESM4::LevelledRules all;
        all.mUseAll = true;
        ESM4::LevelledRandom random(1);
        std::vector<ESM4::LevelledChoice> chosen;

        ESM4::chooseLevelledEntries(entries, all, 5, random, chosen);

        ASSERT_EQ(chosen.size(), 3u);
        EXPECT_EQ(chosen[0].mCount, 1);
        EXPECT_EQ(chosen[1].mCount, 3);
        EXPECT_EQ(chosen[2].mCount, 1);
    }

    TEST(ESM4LevelledTest, leavesOutEntriesWithoutAnItem)
    {
        const std::vector<ESM4::LVLO> entries{ entry(1, 0) };
        ESM4::LevelledRandom random(1);
        std::vector<ESM4::LevelledChoice> chosen;

        EXPECT_EQ(ESM4::chooseLevelledEntries(entries, {}, 5, random, chosen), ESM4::LevelledOutcome::EmptyByLevel);
    }

    struct Thing
    {
        int mValue;
    };

    struct List
    {
        std::vector<ESM4::LVLO> mLvlObject;
        std::int8_t mChance = 0;
        std::int8_t chanceNone() const { return mChance; }
        bool calcAllLvlLessThanPlayer() const { return false; }
    };

    TEST(ESM4LevelledTest, countsTheListsThatGaveNoRecord)
    {
        std::map<ESM::FormId, Thing> things{ { id(0x10), { 7 } } };
        std::map<ESM::FormId, List> lists;
        lists[id(0x20)].mLvlObject = { entry(1, 0x21) };
        lists[id(0x21)].mLvlObject = { entry(1, 0x99) }; // names nothing
        lists[id(0x22)].mLvlObject = { entry(1, 0x10) };
        const auto find = [&](ESM::FormId formId) -> const Thing* {
            const auto found = things.find(formId);
            return found == things.end() ? nullptr : &found->second;
        };
        const auto findList = [&](ESM::FormId formId) -> const List* {
            const auto found = lists.find(formId);
            return found == lists.end() ? nullptr : &found->second;
        };
        ESM4::LevelledRandom random(1);

        std::size_t empty = 0;
        EXPECT_EQ(ESM4::resolveLevelledRecord<Thing>(find, findList, id(0x20), 5, random, &empty), nullptr);
        EXPECT_EQ(empty, 2u); // the list inside the list, and the one around it

        empty = 0;
        const Thing* thing = ESM4::resolveLevelledRecord<Thing>(find, findList, id(0x22), 5, random, &empty);
        ASSERT_NE(thing, nullptr);
        EXPECT_EQ(thing->mValue, 7);
        EXPECT_EQ(empty, 0u);

        // a record that is there needs no list
        EXPECT_EQ(ESM4::resolveLevelledRecord<Thing>(find, findList, id(0x10), 5, random)->mValue, 7);
    }

    TEST(ESM4LevelledTest, readsTheLevelOfAnActorFromItsConfig)
    {
        ESM4::ACBS_FO3 config{};
        config.levelOrMult = 7;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 7);

        config.flags = 0x80;
        config.levelOrMult = 1500;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 7); // 7.5 is cut at the point (0x47DED0)
        config.calcMinlevel = 9;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 9);
        config.calcMinlevel = 0;
        config.calcMaxlevel = 4;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 4);
    }

    TEST(ESM4LevelledTest, theMinimumOfAnActorWinsOverItsMaximum)
    {
        ESM4::ACBS_FO3 config{};
        config.flags = 0x80;
        config.levelOrMult = 2000;
        config.calcMinlevel = 30;
        config.calcMaxlevel = 20;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 30); // below the minimum: the maximum is not looked at
        EXPECT_EQ(ESM4::levelFromConfig(config, 20), 20); // 40 is above the minimum, so the maximum cuts it
    }

    TEST(ESM4LevelledTest, aMultiplierCutsTheFractionOfTheProduct)
    {
        ESM4::ACBS_FO3 config{};
        config.flags = 0x80;
        config.levelOrMult = 500;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 2); // 2.5
        EXPECT_EQ(ESM4::levelFromConfig(config, 1), 1); // 0.5, and nothing is below level 1
        config.levelOrMult = 1250;
        EXPECT_EQ(ESM4::levelFromConfig(config, 3), 3); // 3.75
    }

    TEST(ESM4LevelledTest, theChanceOfNothingFromAGlobalIsItsLowByteWithTheFractionCut)
    {
        EXPECT_EQ(ESM4::levelledChanceFromGlobal(25.9f), 25);
        EXPECT_EQ(ESM4::levelledChanceFromGlobal(0.f), 0);
        EXPECT_EQ(ESM4::levelledChanceFromGlobal(100.f), 100);
        EXPECT_EQ(ESM4::levelledChanceFromGlobal(256.f), 0);
        EXPECT_EQ(ESM4::levelledChanceFromGlobal(300.f), 44);
        EXPECT_EQ(ESM4::levelledChanceFromGlobal(-4.f), 252);
        EXPECT_EQ(ESM4::levelledChanceFromGlobal(1e20f), 0); // does not fit a whole number
    }

    std::set<std::uint32_t> pickedIn(const std::vector<ESM4::LVLO>& entries, const ESM4::LevelledRules& rules, int level)
    {
        std::set<std::uint32_t> picked;
        ESM4::LevelledRandom random(7);
        for (int i = 0; i < 200; ++i)
        {
            std::vector<ESM4::LevelledChoice> chosen;
            if (ESM4::chooseLevelledEntries(entries, rules, level, random, chosen) == ESM4::LevelledOutcome::Chosen)
                for (const ESM4::LevelledChoice& entry : chosen)
                    picked.insert(entry.mItem.toUint32());
        }
        return picked;
    }

    TEST(ESM4LevelledTest, aListThatCalculatesFromAllLevelsMayGiveAnyEntryAtOrBelowTheLevel)
    {
        const std::vector<ESM4::LVLO> entries{ entry(1, 0x10), entry(5, 0x11), entry(5, 0x12), entry(9, 0x13) };
        EXPECT_THAT(pickedIn(entries, {}, 6), ElementsAre(0x11u, 0x12u));
        ESM4::LevelledRules all;
        all.mAllLevels = true;
        EXPECT_THAT(pickedIn(entries, all, 6), ElementsAre(0x10u, 0x11u, 0x12u));
        EXPECT_THAT(pickedIn(entries, all, 99), ElementsAre(0x10u, 0x11u, 0x12u, 0x13u));
    }

    TEST(ESM4LevelledTest, aLevelModifierMakesAListChooseFromAllLevels)
    {
        const std::vector<ESM4::LVLO> entries{ entry(1, 0x10), entry(5, 0x11), entry(9, 0x13) };
        ESM4::LevelledRules modified;
        modified.mLevelMod = 2;
        EXPECT_THAT(pickedIn(entries, modified, 6), ElementsAre(0x10u, 0x11u));
    }

    TEST(ESM4LevelledTest, theSettingForTheLevelDifferenceKeepsTheLowestEntriesOut)
    {
        // The level does not go above the highest entry (8), so with a difference of 3 the entries below level 5 are
        // left out: the first entry that is not, and every one after it, count
        const std::vector<ESM4::LVLO> entries{ entry(1, 0x10), entry(5, 0x11), entry(6, 0x12), entry(8, 0x13) };
        ESM4::LevelledRules all;
        all.mAllLevels = true;
        all.mLevelDifference = 3;
        EXPECT_THAT(pickedIn(entries, all, 9), ElementsAre(0x11u, 0x12u, 0x13u));
        all.mLevelDifference = 0;
        EXPECT_THAT(pickedIn(entries, all, 9), ElementsAre(0x10u, 0x11u, 0x12u, 0x13u));
    }

    TEST(ESM4LevelledTest, anEntryOfLevelZeroIsNeverTheStartOfTheChoice)
    {
        const std::vector<ESM4::LVLO> entries{ entry(0, 0x10) };
        ESM4::LevelledRandom random(1);
        std::vector<ESM4::LevelledChoice> chosen;
        EXPECT_EQ(ESM4::chooseLevelledEntries(entries, {}, 5, random, chosen), ESM4::LevelledOutcome::EmptyByLevel);
    }

    TEST(ESM4LevelledTest, aListThatUsesAllGivesEveryEntryWhateverItsLevel)
    {
        const std::vector<ESM4::LVLO> entries{ entry(1, 0x10), entry(50, 0x11) };
        ESM4::LevelledRules all;
        all.mUseAll = true;
        ESM4::LevelledRandom random(1);
        std::vector<ESM4::LevelledChoice> chosen;
        EXPECT_EQ(ESM4::chooseLevelledEntries(entries, all, 2, random, chosen), ESM4::LevelledOutcome::Chosen);
        EXPECT_EQ(chosen.size(), 2u);
    }

    TEST(ESM4LevelledTest, anAddedEntryGoesBehindTheEntriesAtItsLevelAndInFrontOfThoseAboveIt)
    {
        std::vector<ESM4::LVLO> entries{ entry(1, 0x10), entry(5, 0x11), entry(5, 0x12), entry(9, 0x13) };
        ESM4::insertLevelledEntry(entries, entry(5, 0x20));
        ASSERT_EQ(entries.size(), 5u);
        EXPECT_EQ(entries[3].item, 0x20u);
        EXPECT_EQ(entries[4].item, 0x13u);
        ESM4::insertLevelledEntry(entries, entry(1, 0x21));
        EXPECT_EQ(entries[1].item, 0x21u);
        ESM4::insertLevelledEntry(entries, entry(40, 0x22));
        EXPECT_EQ(entries.back().item, 0x22u);

        std::vector<ESM4::LVLO> empty;
        ESM4::insertLevelledEntry(empty, entry(3, 0x30));
        EXPECT_EQ(empty.size(), 1u);

        // the walk stops at the first entry above the new one, even when the list is not sorted
        std::vector<ESM4::LVLO> unsorted{ entry(9, 0x40), entry(2, 0x41) };
        ESM4::insertLevelledEntry(unsorted, entry(4, 0x42));
        EXPECT_EQ(unsorted[0].item, 0x42u);
    }

    TEST(ESM4LevelledTest, theEntriesThatScriptsAddedTakePartInTheChoiceOfALaterRoll)
    {
        std::map<ESM::FormId, List> lists;
        lists[id(0x20)].mLvlObject = { entry(1, 0x21) };
        std::map<ESM::FormId, Thing> things{ { id(0x21), { 1 } }, { id(0x22), { 2 } } };
        const auto find = [&](ESM::FormId formId) -> const Thing* {
            const auto found = things.find(formId);
            return found == things.end() ? nullptr : &found->second;
        };
        const auto findList = [&](ESM::FormId formId) -> const List* {
            const auto found = lists.find(formId);
            return found == lists.end() ? nullptr : &found->second;
        };

        ESM4::LevelledRandom before(1);
        EXPECT_EQ(ESM4::resolveLevelledRecord<Thing>(find, findList, id(0x20), 9, before)->mValue, 1);

        ESM4::setLevelledAdditions([](ESM::FormId list, const std::vector<ESM4::LVLO>& entries,
                                       std::vector<ESM4::LVLO>& merged) {
            if (list != id(0x20))
                return false;
            merged = entries;
            ESM4::insertLevelledEntry(merged, entry(5, 0x22));
            return true;
        });
        ESM4::LevelledRandom after(1);
        // the added entry is at level 5, which is the highest at or below 9, so it is the one
        EXPECT_EQ(ESM4::resolveLevelledRecord<Thing>(find, findList, id(0x20), 9, after)->mValue, 2);
        // below its level the entry of the record is chosen as before
        ESM4::LevelledRandom low(1);
        EXPECT_EQ(ESM4::resolveLevelledRecord<Thing>(find, findList, id(0x20), 3, low)->mValue, 1);
        ESM4::setLevelledAdditions({});
        ESM4::LevelledRandom gone(1);
        EXPECT_EQ(ESM4::resolveLevelledRecord<Thing>(find, findList, id(0x20), 9, gone)->mValue, 1);
    }

    TEST(ESM4LevelledTest, clearingTheAdditionsHookWaitsForTheCallThatRunsIt)
    {
        std::promise<void> entered;
        std::promise<void> release;
        std::shared_future<void> released = release.get_future().share();
        ESM4::setLevelledAdditions([&entered, released](ESM::FormId, const std::vector<ESM4::LVLO>& entries,
                                       std::vector<ESM4::LVLO>& merged) {
            entered.set_value();
            released.wait();
            merged = entries;
            return true;
        });
        std::atomic<bool> cleared{ false };
        std::vector<ESM4::LVLO> storage;
        const std::vector<ESM4::LVLO> entries{ entry(1, 0x21) };
        std::thread caller([&] { ESM4::levelledEntriesOf(id(0x20), entries, storage); });
        entered.get_future().wait();
        std::thread clearer([&cleared] {
            ESM4::setLevelledAdditions({});
            cleared = true;
        });
        // the call has not returned, so what the hook captures is still in use and the clear has not finished
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_FALSE(cleared.load());
        release.set_value();
        caller.join();
        clearer.join();
        EXPECT_TRUE(cleared.load());
        // with no hook the entries of the file stand
        std::vector<ESM4::LVLO> other;
        EXPECT_EQ(&ESM4::levelledEntriesOf(id(0x20), entries, other), &entries);
    }

    TEST(ESM4LevelledTest, clearingTheSpawnRollProviderWaitsForTheCallThatRunsIt)
    {
        std::promise<void> entered;
        std::promise<void> release;
        std::shared_future<void> released = release.get_future().share();
        ESM4::setSpawnRollProvider([&entered, released](ESM::FormId) {
            entered.set_value();
            released.wait();
            return ESM4::SpawnRoll{ 7, 1 };
        });
        std::atomic<bool> cleared{ false };
        ESM4::SpawnRoll roll;
        std::thread caller([&roll] { roll = ESM4::spawnRollOf(id(0x30), 1); });
        entered.get_future().wait();
        std::thread clearer([&cleared] {
            ESM4::setSpawnRollProvider({});
            cleared = true;
        });
        // the call has not returned, so what the provider captures is still in use and the clear has not finished
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_FALSE(cleared.load());
        release.set_value();
        caller.join();
        clearer.join();
        EXPECT_TRUE(cleared.load());
        EXPECT_EQ(roll.mLevel, 7);
        // with no provider the fallback stands
        EXPECT_EQ(ESM4::spawnRollOf(id(0x30), 4).mLevel, 4);
    }
}
