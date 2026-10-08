#include <components/esm4/levelled.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <set>

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
        EXPECT_EQ(ESM4::chooseLevelledEntries(entries, always, 9, random, chosen), ESM4::LevelledOutcome::EmptyByChance);
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
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 8); // 7.5 rounds up
        config.calcMinlevel = 9;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 9);
        config.calcMinlevel = 0;
        config.calcMaxlevel = 4;
        EXPECT_EQ(ESM4::levelFromConfig(config, 5), 4);
    }
}
