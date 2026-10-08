#include <components/esm4/wornarmor.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace
{
    using namespace testing;

    constexpr std::uint32_t head = 0x1;
    constexpr std::uint32_t hair = 0x2;
    constexpr std::uint32_t upperBody = 0x4;
    constexpr std::uint32_t leftHand = 0x8;
    constexpr std::uint32_t hat = 0x400;

    constexpr int playerLevel = 5;

    ESM::FormId id(std::uint32_t value)
    {
        return ESM::FormId::fromUint32(value);
    }

    // The records of a few plugins, found by their form IDs
    class TestSource final : public ESM4::WornArmorSource
    {
    public:
        std::map<ESM::FormId, ESM4::Npc> mNpcs;
        std::map<ESM::FormId, ESM4::LevelledNpc> mLevelledNpcs;
        std::map<ESM::FormId, ESM4::Armor> mArmor;
        std::map<ESM::FormId, ESM4::LevelledItem> mLevelledItems;
        std::map<ESM::FormId, ESM4::GlobalVariable> mGlobals;

        const ESM4::Npc* findNpc(ESM::FormId formId) const override { return find(mNpcs, formId); }
        const ESM4::LevelledNpc* findLevelledNpc(ESM::FormId formId) const override
        {
            return find(mLevelledNpcs, formId);
        }
        const ESM4::Armor* findArmor(ESM::FormId formId) const override { return find(mArmor, formId); }
        const ESM4::LevelledItem* findLevelledItem(ESM::FormId formId) const override
        {
            return find(mLevelledItems, formId);
        }
        const ESM4::GlobalVariable* findGlobal(ESM::FormId formId) const override { return find(mGlobals, formId); }

        void addGlobal(std::uint32_t value, float number)
        {
            ESM4::GlobalVariable global{};
            global.mId = id(value);
            global.mValue = number;
            mGlobals[id(value)] = global;
        }

        // Makes the chance of nothing of the list the value of the global variable
        void useGlobalChance(std::uint32_t list, std::uint32_t global)
        {
            mLevelledItems.at(id(list)).mGlobal = id(global);
        }

        void addArmor(std::uint32_t value, std::uint32_t slots, const char* male = "male.nif", const char* female = "")
        {
            ESM4::Armor armor{};
            armor.mId = id(value);
            armor.mArmorFlags = slots;
            armor.mModelMale = male;
            armor.mModelFemale = female;
            mArmor[id(value)] = armor;
        }

        // A list with the entries (level, item); the flags are those of LVLF
        void addList(std::uint32_t value, std::initializer_list<std::pair<int, std::uint32_t>> entries,
            std::uint8_t flags = 0, std::int8_t chanceNone = 0)
        {
            ESM4::LevelledItem list{};
            list.mId = id(value);
            list.mChanceNone = chanceNone;
            list.mHasLvlItemFlags = true;
            list.mLvlItemFlags = flags;
            for (const auto& [level, item] : entries)
                list.mLvlObject.push_back({ static_cast<std::int16_t>(level), 0, item, 1, 0 });
            mLevelledItems[id(value)] = list;
        }

        ESM4::Npc& addNpc(std::uint32_t value, std::initializer_list<std::uint32_t> items,
            std::uint16_t templateFlags = 0, std::uint32_t templateId = 0, int level = 1)
        {
            ESM4::Npc npc{};
            npc.mId = id(value);
            npc.mIsFONV = true;
            npc.mBaseConfig.fo3.levelOrMult = static_cast<std::int16_t>(level);
            npc.mBaseConfig.fo3.templateFlags = templateFlags;
            if (templateId != 0)
                npc.mBaseTemplate = id(templateId);
            for (const std::uint32_t item : items)
                npc.mInventory.push_back({ item, 1 });
            return mNpcs[id(value)] = npc;
        }

    private:
        template <class Record>
        static const Record* find(const std::map<ESM::FormId, Record>& records, ESM::FormId formId)
        {
            const auto found = records.find(formId);
            return found == records.end() ? nullptr : &found->second;
        }
    };

    // A character that has the level of the player, so that the level handed to the code decides what a list gives
    void followsThePlayer(ESM4::Npc& npc)
    {
        npc.mBaseConfig.fo3.flags = ESM4::Npc::FO3_PCLevelMult;
        npc.mBaseConfig.fo3.levelOrMult = 1000;
    }

    std::set<std::uint32_t> ids(const std::vector<const ESM4::Armor*>& armor)
    {
        std::set<std::uint32_t> result;
        for (const ESM4::Armor* piece : armor)
            result.insert(piece->mId.mIndex);
        return result;
    }

    // What a character wears across many seeds, as the sets of form IDs that came out
    std::set<std::set<std::uint32_t>> wornAcrossSeeds(const TestSource& source, const ESM4::Npc& npc, int level)
    {
        std::set<std::set<std::uint32_t>> results;
        for (std::uint32_t seed = 1; seed <= 64; ++seed)
            results.insert(ids(ESM4::wornArmor(source, npc, level, seed)));
        return results;
    }

    TEST(ESM4WornArmorTest, wearsTheArmourItsInventoryLists)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        source.addNpc(0x2001, { 0x1001, 0x1002, 0x1001 });

        ESM4::WornArmorTrace trace;
        const auto armor = ESM4::wornArmor(source, source.mNpcs.at(id(0x2001)), playerLevel, 1, &trace);

        // in the order of the inventory, each piece once
        ASSERT_EQ(armor.size(), 2u);
        EXPECT_EQ(armor[0]->mId.mIndex, 0x1001u);
        EXPECT_EQ(armor[1]->mId.mIndex, 0x1002u);
        EXPECT_EQ(trace.mCharacters, 1u);
        EXPECT_EQ(trace.mItems, 3u);
        EXPECT_EQ(trace.mArmorListed, 3u);
        EXPECT_EQ(trace.mListsEntered, 0u);
    }

    TEST(ESM4WornArmorTest, countsAnythingThatIsNotArmourOrAListApart)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addNpc(0x2001, { 0x1001, 0x1020, 0x1099 });

        ESM4::WornArmorTrace trace;
        const auto armor = ESM4::wornArmor(source, source.mNpcs.at(id(0x2001)), playerLevel, 1, &trace);

        EXPECT_EQ(ids(armor), (std::set<std::uint32_t>{ 0x1001 }));
        EXPECT_EQ(trace.mOtherItems, 2u);
    }

    TEST(ESM4WornArmorTest, takesTheInventoryOfTheTemplateThatHasOne)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        // 0x2003 has the inventory; 0x2002 takes everything from it; 0x2001 takes the inventory from 0x2002
        source.addNpc(0x2003, { 0x1001 });
        source.addNpc(0x2002, { 0x1002 }, ESM4::Npc::Template_UseInventory, 0x2003);
        source.addNpc(0x2001, { 0x1002 }, ESM4::Npc::Template_UseInventory, 0x2002);

        ESM4::WornArmorTrace trace;
        const auto armor = ESM4::wornArmor(source, source.mNpcs.at(id(0x2001)), playerLevel, 1, &trace);

        EXPECT_EQ(ids(armor), (std::set<std::uint32_t>{ 0x1001 }));
        EXPECT_EQ(trace.mInventoryFromTemplate, 1u);
    }

    TEST(ESM4WornArmorTest, keepsItsOwnInventoryWhenItDoesNotTakeOneFromItsTemplate)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        source.addNpc(0x2002, { 0x1001 });
        // the template flags name the model and not the inventory
        source.addNpc(0x2001, { 0x1002 }, ESM4::Npc::Template_UseModel, 0x2002);

        const auto armor = ESM4::wornArmor(source, source.mNpcs.at(id(0x2001)), playerLevel, 1);

        EXPECT_EQ(ids(armor), (std::set<std::uint32_t>{ 0x1002 }));
    }

    TEST(ESM4WornArmorTest, wearsNothingWhenEveryRecordOfTheChainTakesTheInventory)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addNpc(0x2002, { 0x1001 }, ESM4::Npc::Template_UseInventory, 0x2003);
        source.addNpc(0x2001, {}, ESM4::Npc::Template_UseInventory, 0x2002);

        ESM4::WornArmorTrace trace;
        const auto armor = ESM4::wornArmor(source, source.mNpcs.at(id(0x2001)), playerLevel, 1, &trace);

        // 0x2003 is not a record, so the chain ends at 0x2002, which takes its inventory from the missing one
        EXPECT_TRUE(armor.empty());
        EXPECT_EQ(trace.mNoInventory, 1u);
    }

    TEST(ESM4WornArmorTest, endsAChainOfTemplatesThatIsACycle)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addNpc(0x2001, { 0x1001 }, ESM4::Npc::Template_UseInventory, 0x2002);
        source.addNpc(0x2002, {}, ESM4::Npc::Template_UseInventory, 0x2001);

        const std::vector<const ESM4::Npc*> chain
            = ESM4::templateChain(source, source.mNpcs.at(id(0x2001)), playerLevel, 1);

        ASSERT_EQ(chain.size(), 2u);
        EXPECT_EQ(chain[0]->mId.mIndex, 0x2001u);
        EXPECT_EQ(chain[1]->mId.mIndex, 0x2002u);
        EXPECT_TRUE(ESM4::wornArmor(source, source.mNpcs.at(id(0x2001)), playerLevel, 1).empty());
    }

    TEST(ESM4WornArmorTest, findsTheRecordThatHasAThingByItsFlag)
    {
        TestSource source;
        source.addNpc(0x2002, {});
        source.addNpc(0x2001, {}, ESM4::Npc::Template_UseTraits | ESM4::Npc::Template_UseStats, 0x2002);
        const std::vector<const ESM4::Npc*> chain
            = ESM4::templateChain(source, source.mNpcs.at(id(0x2001)), playerLevel, 1);

        EXPECT_EQ(ESM4::templateOwner(chain, ESM4::Npc::Template_UseTraits), chain[1]);
        EXPECT_EQ(ESM4::templateOwner(chain, ESM4::Npc::Template_UseInventory), chain[0]);
        EXPECT_EQ(ESM4::templateOwner({ chain[0] }, ESM4::Npc::Template_UseStats), nullptr);
    }

    TEST(ESM4WornArmorTest, aLevelledListGivesTheEntryOfTheHighestLevelAtOrBelowTheLevel)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, upperBody);
        source.addArmor(0x1003, upperBody);
        source.addList(0x1010, { { 1, 0x1001 }, { 3, 0x1002 }, { 8, 0x1003 } });
        ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });
        followsThePlayer(npc);

        // at level 5 the entries of level 1 and 3 may be chosen, and only the higher one is
        EXPECT_THAT(wornAcrossSeeds(source, npc, 5), ElementsAre(std::set<std::uint32_t>{ 0x1002 }));
        EXPECT_THAT(wornAcrossSeeds(source, npc, 1), ElementsAre(std::set<std::uint32_t>{ 0x1001 }));
        EXPECT_THAT(wornAcrossSeeds(source, npc, 30), ElementsAre(std::set<std::uint32_t>{ 0x1003 }));
    }

    TEST(ESM4WornArmorTest, aListThatCalculatesFromAllLevelsChoosesAmongTheEntriesAtOrBelowTheLevel)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, upperBody);
        source.addArmor(0x1003, upperBody);
        source.addList(0x1010, { { 1, 0x1001 }, { 3, 0x1002 }, { 8, 0x1003 } }, 0x01);
        ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });
        followsThePlayer(npc);

        // both entries come out across seeds and the one above the level never does
        EXPECT_THAT(wornAcrossSeeds(source, npc, 5),
            UnorderedElementsAre(std::set<std::uint32_t>{ 0x1001 }, std::set<std::uint32_t>{ 0x1002 }));
    }

    TEST(ESM4WornArmorTest, aListThatUsesAllGivesEveryEntryWhateverItsLevel)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        source.addArmor(0x1003, leftHand);
        // entries of level 1 and 3 are below the level of the character, the one of level 9 is above; without the flag
        // only the highest one at or below the level, the entry of level 3, would be given
        source.addList(0x1010, { { 1, 0x1001 }, { 3, 0x1002 }, { 9, 0x1003 } }, 0x04);
        const ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });

        ESM4::WornArmorTrace trace;
        const auto armor = ESM4::wornArmor(source, npc, playerLevel, 1, &trace);

        ASSERT_EQ(armor.size(), 3u);
        EXPECT_EQ(armor[0]->mId.mIndex, 0x1001u);
        EXPECT_EQ(armor[1]->mId.mIndex, 0x1002u);
        EXPECT_EQ(armor[2]->mId.mIndex, 0x1003u);
        EXPECT_EQ(trace.mListsUsingAll, 1u);
        EXPECT_EQ(trace.mArmorFromLists, 3u);
        EXPECT_EQ(trace.mListsEntered, 1u);
    }

    TEST(ESM4WornArmorTest, aListGivesNothingByChanceOrWhenNoEntryIsAtOrBelowTheLevel)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addList(0x1010, { { 1, 0x1001 } }, 0, 100);
        source.addList(0x1011, { { 10, 0x1001 } });
        source.addList(0x1012, { { 1, 0x1001 } }, 0, 0);
        const ESM4::Npc& always = source.addNpc(0x2001, { 0x1010 });
        const ESM4::Npc& tooHigh = source.addNpc(0x2002, { 0x1011 });
        const ESM4::Npc& never = source.addNpc(0x2003, { 0x1012 });

        ESM4::WornArmorTrace trace;
        EXPECT_TRUE(ESM4::wornArmor(source, always, playerLevel, 1, &trace).empty());
        EXPECT_TRUE(ESM4::wornArmor(source, tooHigh, playerLevel, 1, &trace).empty());
        EXPECT_FALSE(ESM4::wornArmor(source, never, playerLevel, 1, &trace).empty());
        EXPECT_EQ(trace.mListsEmptyByChance, 1u);
        EXPECT_EQ(trace.mListsEmptyByLevel, 1u);
    }

    TEST(ESM4WornArmorTest, aListThatNamesAGlobalTakesTheChanceOfNothingFromItsValue)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addGlobal(0x3001, 100.f);
        source.addGlobal(0x3002, 0.f);
        source.addGlobal(0x3003, 250.f);
        source.addGlobal(0x3004, -4.f);
        // The chance stored in the list is the opposite of what the global says, so that it shows which one counts
        source.addList(0x1010, { { 1, 0x1001 } }, 0, 0);
        source.addList(0x1011, { { 1, 0x1001 } }, 0, 100);
        source.addList(0x1012, { { 1, 0x1001 } }, 0, 0);
        source.addList(0x1013, { { 1, 0x1001 } }, 0, 100);
        source.useGlobalChance(0x1010, 0x3001);
        source.useGlobalChance(0x1011, 0x3002);
        source.useGlobalChance(0x1012, 0x3003);
        source.useGlobalChance(0x1013, 0x3004);
        const ESM4::Npc& alwaysNothing = source.addNpc(0x2001, { 0x1010 });
        const ESM4::Npc& alwaysSomething = source.addNpc(0x2002, { 0x1011 });
        const ESM4::Npc& aboveAHundred = source.addNpc(0x2003, { 0x1012 });
        const ESM4::Npc& belowZero = source.addNpc(0x2004, { 0x1013 });

        ESM4::WornArmorTrace trace;
        for (std::uint32_t seed = 1; seed <= 16; ++seed)
        {
            EXPECT_TRUE(ESM4::wornArmor(source, alwaysNothing, playerLevel, seed, &trace).empty());
            EXPECT_FALSE(ESM4::wornArmor(source, alwaysSomething, playerLevel, seed, &trace).empty());
            EXPECT_TRUE(ESM4::wornArmor(source, aboveAHundred, playerLevel, seed, &trace).empty());
            EXPECT_FALSE(ESM4::wornArmor(source, belowZero, playerLevel, seed, &trace).empty());
        }
        EXPECT_EQ(trace.mListsWithGlobalChance, 64u);
    }

    TEST(ESM4WornArmorTest, aListWhoseGlobalIsNotThereKeepsItsOwnChanceOfNothing)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addList(0x1010, { { 1, 0x1001 } }, 0, 100);
        source.addList(0x1011, { { 1, 0x1001 } }, 0, 0);
        source.useGlobalChance(0x1010, 0x3099);
        source.useGlobalChance(0x1011, 0x3099);
        const ESM4::Npc& always = source.addNpc(0x2001, { 0x1010 });
        const ESM4::Npc& never = source.addNpc(0x2002, { 0x1011 });

        ESM4::WornArmorTrace trace;
        EXPECT_TRUE(ESM4::wornArmor(source, always, playerLevel, 1, &trace).empty());
        EXPECT_FALSE(ESM4::wornArmor(source, never, playerLevel, 1, &trace).empty());
        EXPECT_EQ(trace.mListsWithGlobalChance, 0u);
    }

    TEST(ESM4WornArmorTest, aListThatGivesNothingNinetyPercentOfTheTimeGivesSomethingAboutAtenth)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addList(0x1010, { { 1, 0x1001 } }, 0, 90);
        const ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });

        int dressed = 0;
        for (std::uint32_t seed = 1; seed <= 1000; ++seed)
            if (!ESM4::wornArmor(source, npc, playerLevel, seed).empty())
                ++dressed;
        EXPECT_GT(dressed, 50);
        EXPECT_LT(dressed, 160);
    }

    TEST(ESM4WornArmorTest, aListThatCalculatesForEachItemInTheCountIsRolledOnceForEachItem)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        source.addList(0x1010, { { 1, 0x1001 }, { 1, 0x1002 } }, 0x02);
        source.addList(0x1011, { { 1, 0x1001 }, { 1, 0x1002 } }, 0);
        ESM4::Npc& each = source.addNpc(0x2001, { 0x1010 });
        each.mInventory.back().count = 16;
        ESM4::Npc& once = source.addNpc(0x2002, { 0x1011 });
        once.mInventory.back().count = 16;
        ESM4::Npc& single = source.addNpc(0x2003, { 0x1010 });

        // sixteen rolls of a list with two pieces give both of them, one roll gives one, whatever the count of a list
        // that does not calculate for each item
        EXPECT_THAT(wornAcrossSeeds(source, each, 5), UnorderedElementsAre(std::set<std::uint32_t>{ 0x1001, 0x1002 }));
        EXPECT_THAT(wornAcrossSeeds(source, once, 5),
            UnorderedElementsAre(std::set<std::uint32_t>{ 0x1001 }, std::set<std::uint32_t>{ 0x1002 }));
        EXPECT_THAT(wornAcrossSeeds(source, single, 5),
            UnorderedElementsAre(std::set<std::uint32_t>{ 0x1001 }, std::set<std::uint32_t>{ 0x1002 }));
    }

    TEST(ESM4WornArmorTest, aListThatUsesAllIsRolledOnceWhateverTheCount)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        // it also calculates for each item, which "use all" supersedes; half of the rolls give nothing
        source.addList(0x1010, { { 1, 0x1001 }, { 1, 0x1002 } }, 0x06, 50);
        ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });
        npc.mInventory.back().count = 16;

        // sixteen rolls would nearly always give both pieces, one roll gives both or nothing
        EXPECT_THAT(wornAcrossSeeds(source, npc, 5),
            UnorderedElementsAre(std::set<std::uint32_t>{}, std::set<std::uint32_t>{ 0x1001, 0x1002 }));
    }

    TEST(ESM4WornArmorTest, aListInAListIsRolledOnceForEachItemOfTheCountOfItsEntry)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        source.addList(0x1011, { { 1, 0x1001 }, { 1, 0x1002 } }, 0x02);
        source.addList(0x1010, { { 1, 0x1011 } });
        source.mLevelledItems.at(id(0x1010)).mLvlObject[0].count = 16;
        const ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });

        EXPECT_THAT(wornAcrossSeeds(source, npc, 5), UnorderedElementsAre(std::set<std::uint32_t>{ 0x1001, 0x1002 }));
    }

    TEST(ESM4WornArmorTest, rollsAListOnceForEachItemOfALargeCount)
    {
        // a hundred items of a list of a hundred pieces: with fewer rolls than that some piece would be missed
        TestSource source;
        std::vector<std::pair<int, std::uint32_t>> entries;
        for (std::uint32_t piece = 0; piece < 100; ++piece)
        {
            source.addArmor(0x1001 + piece, upperBody);
            entries.push_back({ 1, 0x1001 + piece });
        }
        ESM4::LevelledItem list{};
        list.mId = id(0x2000);
        list.mHasLvlItemFlags = true;
        list.mLvlItemFlags = 0x02;
        for (const auto& [level, item] : entries)
            list.mLvlObject.push_back({ static_cast<std::int16_t>(level), 0, item, 1, 0 });
        source.mLevelledItems[id(0x2000)] = list;
        ESM4::Npc& npc = source.addNpc(0x3001, { 0x2000 });
        npc.mInventory.back().count = 2000;

        // (a character wears the first piece that covers a slot, but the armour it was given is all of them)
        EXPECT_EQ(ESM4::wornArmor(source, npc, playerLevel, 1).size(), 100u);

        // a count of billions is cut short
        npc.mInventory.back().count = 4000000000u;
        EXPECT_EQ(ESM4::wornArmor(source, npc, playerLevel, 1).size(), 100u);
    }

    TEST(ESM4WornArmorTest, boundsTheRollsOfListsInListsThatCountForEachItem)
    {
        // Four lists, each with a count of 30000 for the list in it, all calculating for each item: the product of
        // the counts is not a number of rolls that can be made, and the character is dressed all the same
        TestSource source;
        source.addArmor(0x1001, upperBody);
        for (std::uint32_t level = 0; level < 4; ++level)
        {
            ESM4::LevelledItem list{};
            list.mId = id(0x2000 + level);
            list.mHasLvlItemFlags = true;
            list.mLvlItemFlags = 0x02;
            list.mLvlObject.push_back({ 1, 0, level == 3 ? 0x1001u : 0x2001u + level, 30000, 0 });
            source.mLevelledItems[id(0x2000 + level)] = list;
        }
        ESM4::Npc& npc = source.addNpc(0x3001, { 0x2000 });
        npc.mInventory.back().count = 4000000000u;

        EXPECT_EQ(ESM4::wornArmor(source, npc, playerLevel, 1).size(), 1u);
    }

    TEST(ESM4WornArmorTest, followsListsInLists)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        source.addList(0x1012, { { 1, 0x1001 } });
        source.addList(0x1011, { { 1, 0x1012 } });
        source.addList(0x1010, { { 1, 0x1011 }, { 1, 0x1002 } }, 0x04);
        const ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });

        ESM4::WornArmorTrace trace;
        const auto armor = ESM4::wornArmor(source, npc, playerLevel, 1, &trace);

        EXPECT_EQ(ids(armor), (std::set<std::uint32_t>{ 0x1001, 0x1002 }));
        EXPECT_EQ(trace.mArmorFromLists, 2u);
    }

    TEST(ESM4WornArmorTest, endsListsThatContainEachOther)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addList(0x1010, { { 1, 0x1011 }, { 1, 0x1001 } }, 0x04);
        source.addList(0x1011, { { 1, 0x1010 } });
        const ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });

        ESM4::WornArmorTrace trace;
        const auto armor = ESM4::wornArmor(source, npc, playerLevel, 1, &trace);

        EXPECT_EQ(ids(armor), (std::set<std::uint32_t>{ 0x1001 }));
        EXPECT_EQ(trace.mListsTooDeep, 1u);
    }

    TEST(ESM4WornArmorTest, dressesTheSameCharacterTheSameAndTwoCharactersAlike)
    {
        TestSource source;
        for (std::uint32_t armor = 0x1001; armor < 0x1009; ++armor)
            source.addArmor(armor, upperBody);
        source.addList(0x1010,
            { { 1, 0x1001 }, { 1, 0x1002 }, { 1, 0x1003 }, { 1, 0x1004 }, { 1, 0x1005 }, { 1, 0x1006 }, { 1, 0x1007 },
                { 1, 0x1008 } },
            0x01);
        const ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 });

        // the same seed, the same outfit; the seeds of a crowd between them have more than one
        EXPECT_EQ(ids(ESM4::wornArmor(source, npc, playerLevel, 7)), ids(ESM4::wornArmor(source, npc, playerLevel, 7)));
        EXPECT_GT(wornAcrossSeeds(source, npc, playerLevel).size(), 4u);
    }

    TEST(ESM4WornArmorTest, takesTheLevelOfTheRecordThatHasTheStats)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, upperBody);
        source.addList(0x1010, { { 1, 0x1001 }, { 20, 0x1002 } });
        // the template has level 25 and the character, who takes its stats, says level 1
        source.addNpc(0x2002, {}, 0, 0, 25);
        const ESM4::Npc& npc = source.addNpc(0x2001, { 0x1010 }, ESM4::Npc::Template_UseStats, 0x2002, 1);

        EXPECT_THAT(wornAcrossSeeds(source, npc, playerLevel), ElementsAre(std::set<std::uint32_t>{ 0x1002 }));
    }

    TEST(ESM4WornArmorTest, aLevelledListOfCharactersGivesOneCharacterByTheLevelOfThePlayer)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody);
        source.addArmor(0x1002, hat);
        source.addNpc(0x2011, { 0x1001 });
        source.addNpc(0x2012, { 0x1002 });
        ESM4::LevelledNpc list{};
        list.mId = id(0x2010);
        list.mLvlObject.push_back({ 1, 0, 0x2011, 1, 0 });
        list.mLvlObject.push_back({ 9, 0, 0x2012, 1, 0 });
        source.mLevelledNpcs[id(0x2010)] = list;
        // the stored level of a record that takes its stats from its template is 1 in nearly all the real ones, so
        // the list is no use if it is chosen from by that level; both characters get the same by the player's level
        const ESM4::Npc& first = source.addNpc(0x2001, {}, ESM4::Npc::Template_UseInventory, 0x2010, 1);
        const ESM4::Npc& second = source.addNpc(0x2002, {}, ESM4::Npc::Template_UseInventory, 0x2010, 30);

        EXPECT_THAT(wornAcrossSeeds(source, first, 3), ElementsAre(std::set<std::uint32_t>{ 0x1001 }));
        EXPECT_THAT(wornAcrossSeeds(source, first, 12), ElementsAre(std::set<std::uint32_t>{ 0x1002 }));
        EXPECT_THAT(wornAcrossSeeds(source, second, 3), ElementsAre(std::set<std::uint32_t>{ 0x1001 }));
        EXPECT_THAT(wornAcrossSeeds(source, second, 12), ElementsAre(std::set<std::uint32_t>{ 0x1002 }));
    }

    TEST(ESM4WornArmorTest, aLevelledListOfCharactersThatGivesNoneEndsTheChain)
    {
        TestSource source;
        ESM4::LevelledNpc list{};
        list.mId = id(0x2010);
        list.mLvlObject.push_back({ 50, 0, 0x2011, 1, 0 });
        source.mLevelledNpcs[id(0x2010)] = list;
        const ESM4::Npc& npc = source.addNpc(0x2001, {}, ESM4::Npc::Template_UseInventory, 0x2010, 3);

        ESM4::WornArmorTrace trace;
        const auto chain = ESM4::templateChain(source, npc, playerLevel, 1, &trace);

        EXPECT_EQ(chain.size(), 1u);
        EXPECT_EQ(trace.mTemplateListsEmpty, 1u);
    }

    TEST(ESM4WornArmorTest, readsTheLevelOfACharacter)
    {
        ESM4::Npc npc{};
        npc.mIsFONV = true;
        npc.mBaseConfig.fo3.levelOrMult = 12;
        EXPECT_EQ(ESM4::actorLevel(npc, playerLevel), 12);

        // a level of 0 or less is level 1
        npc.mBaseConfig.fo3.levelOrMult = 0;
        EXPECT_EQ(ESM4::actorLevel(npc, playerLevel), 1);

        // the multiplier of the level of the player is in thousandths, kept between the limits
        npc.mBaseConfig.fo3.flags = ESM4::Npc::FO3_PCLevelMult;
        npc.mBaseConfig.fo3.levelOrMult = 1500;
        EXPECT_EQ(ESM4::actorLevel(npc, 10), 15);
        npc.mBaseConfig.fo3.calcMaxlevel = 12;
        EXPECT_EQ(ESM4::actorLevel(npc, 10), 12);
        npc.mBaseConfig.fo3.calcMinlevel = 14;
        npc.mBaseConfig.fo3.calcMaxlevel = 0;
        EXPECT_EQ(ESM4::actorLevel(npc, 4), 14);

        // a record of another game has the level of the player
        ESM4::Npc other{};
        other.mIsTES4 = true;
        EXPECT_EQ(ESM4::actorLevel(other, 7), 7);
    }

    TEST(ESM4WornArmorTest, showsTheFirstOfTwoPiecesThatCoverTheSamePartOfTheBody)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody | leftHand, "suit.nif");
        source.addArmor(0x1002, upperBody, "jacket.nif");
        source.addArmor(0x1003, hat, "hat.nif");
        const std::vector<const ESM4::Armor*> armor
            = { &source.mArmor.at(id(0x1001)), &source.mArmor.at(id(0x1002)), nullptr, &source.mArmor.at(id(0x1003)) };

        const ESM4::WornPieces worn = ESM4::wornPieces(armor, false);

        ASSERT_EQ(worn.mPieces.size(), 2u);
        EXPECT_EQ(worn.mPieces[0].mModel->getOriginal(), "suit.nif");
        EXPECT_EQ(worn.mPieces[0].mSlots, upperBody | leftHand);
        EXPECT_EQ(worn.mPieces[1].mModel->getOriginal(), "hat.nif");
        EXPECT_EQ(worn.mCovered, upperBody | leftHand | hat);
        EXPECT_EQ(worn.mOverlapping, 1u);
    }

    TEST(ESM4WornArmorTest, aWomanWearsTheFemaleModelOrTheMaleOneIfThereIsNone)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody, "male.nif", "female.nif");
        source.addArmor(0x1002, hat, "hat.nif");
        const std::vector<const ESM4::Armor*> armor = { &source.mArmor.at(id(0x1001)), &source.mArmor.at(id(0x1002)) };

        const ESM4::WornPieces woman = ESM4::wornPieces(armor, true);
        ASSERT_EQ(woman.mPieces.size(), 2u);
        EXPECT_EQ(woman.mPieces[0].mModel->getOriginal(), "female.nif");
        EXPECT_EQ(woman.mPieces[1].mModel->getOriginal(), "hat.nif");

        const ESM4::WornPieces man = ESM4::wornPieces(armor, false);
        EXPECT_EQ(man.mPieces[0].mModel->getOriginal(), "male.nif");
    }

    TEST(ESM4WornArmorTest, leavesOutPiecesWithNoModelAndPiecesThatCoverNothing)
    {
        TestSource source;
        source.addArmor(0x1001, upperBody, "");
        source.addArmor(0x1002, 0, "nothing.nif");
        // bits above the parts of the body are not parts of the body
        source.addArmor(0x1003, 0x00400000, "flags.nif");
        source.addArmor(0x1004, head | hair, "helmet.nif");
        const std::vector<const ESM4::Armor*> armor = { &source.mArmor.at(id(0x1001)), &source.mArmor.at(id(0x1002)),
            &source.mArmor.at(id(0x1003)), &source.mArmor.at(id(0x1004)) };

        const ESM4::WornPieces worn = ESM4::wornPieces(armor, false);

        ASSERT_EQ(worn.mPieces.size(), 1u);
        EXPECT_EQ(worn.mPieces[0].mModel->getOriginal(), "helmet.nif");
        EXPECT_EQ(worn.mCovered, head | hair);
        EXPECT_EQ(worn.mWithoutModel, 1u);
        EXPECT_EQ(worn.mWithoutSlots, 2u);
    }

    TEST(ESM4WornArmorTest, readsWhetherARecordTakesAThingFromItsTemplate)
    {
        ESM4::Npc npc{};
        npc.mIsFONV = true;
        npc.mBaseConfig.fo3.templateFlags = ESM4::Npc::Template_UseInventory;
        EXPECT_TRUE(npc.takesFromTemplate(ESM4::Npc::Template_UseInventory));
        EXPECT_FALSE(npc.takesFromTemplate(ESM4::Npc::Template_UseTraits));

        // Oblivion has no template flags
        ESM4::Npc oblivion{};
        oblivion.mIsTES4 = true;
        EXPECT_FALSE(oblivion.takesFromTemplate(ESM4::Npc::Template_UseInventory));
    }
}
