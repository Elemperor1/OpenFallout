#include "scriptbuilder.hpp"

#include <components/falloutscript/levelledlists.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::LevelledAddition;
    using FalloutScript::LevelledLists;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;
    using Kind = FalloutScript::LevelledListRecords::Kind;

    constexpr FormId guard = 0x01000100;
    constexpr FormId scriptOfGuards = 0x01002000;
    constexpr FormId itemList = 0x01000300;
    constexpr FormId npcList = 0x01000301;
    constexpr FormId creatureList = 0x01000302;
    constexpr FormId pistol = 0x01000400;
    constexpr FormId rifle = 0x01000401;
    constexpr FormId raider = 0x01000402;
    constexpr FormId mutant = 0x01000403;

    constexpr std::uint16_t OpAddNPCToLeveledList = 0x1234;
    constexpr std::uint16_t OpAddCreatureToLeveledList = 0x1235;
    constexpr std::uint16_t OpAddItemToLeveledList = 0x1236;

    ESM4::LVLO entry(int level, std::uint32_t item, int count = 1)
    {
        return { static_cast<std::int16_t>(level), 0, item, static_cast<std::int16_t>(count), 0 };
    }

    class FakeRecords : public FalloutScript::LevelledListRecords
    {
    public:
        Kind kindOf(FormId form) override
        {
            switch (form)
            {
                case itemList:
                    return Kind::Item;
                case npcList:
                    return Kind::Character;
                case creatureList:
                    return Kind::Creature;
                default:
                    return Kind::None;
            }
        }

        bool isEntryOf(Kind kind, FormId form) override
        {
            switch (kind)
            {
                case Kind::Item:
                    return form == pistol || form == rifle;
                case Kind::Character:
                    return form == raider;
                case Kind::Creature:
                    return form == mutant;
                default:
                    return false;
            }
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        FakeRecords mRecords;
        LevelledLists mLists;

        Runner() { FalloutScript::addLevelledListCommands(mInterpreter, mLists, mRecords); }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGuards];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards);
            record.mScript = builder.definition();
            mObjects.add(guard, scriptOfGuards);
            mObjects.trigger(guard, BlockType::OnActivate);
        }
    };

    TEST(FalloutScriptLevelledListsTest, anAddedEntryIsMergedBehindTheEntriesOfItsLevel)
    {
        LevelledLists lists;
        lists.add(itemList, { pistol, 5, 2, 100.f });
        lists.add(itemList, { rifle, 1, 1, 100.f });
        const std::vector<ESM4::LVLO> record{ entry(1, 0x10), entry(5, 0x11), entry(9, 0x12) };

        std::vector<ESM4::LVLO> merged;
        ASSERT_TRUE(lists.merge(itemList, record, merged));
        ASSERT_EQ(merged.size(), 5u);
        EXPECT_EQ(merged[0].item, 0x10u);
        EXPECT_EQ(merged[1].item, rifle); // behind the entry of level 1
        EXPECT_EQ(merged[2].item, 0x11u);
        EXPECT_EQ(merged[3].item, pistol); // behind the entry of level 5
        EXPECT_EQ(merged[3].count, 2);
        EXPECT_EQ(merged[4].item, 0x12u);
    }

    TEST(FalloutScriptLevelledListsTest, aListNothingWasAddedToIsLeftAlone)
    {
        LevelledLists lists;
        lists.add(itemList, { pistol, 5, 1, 100.f });
        std::vector<ESM4::LVLO> merged;
        EXPECT_FALSE(lists.merge(npcList, { entry(1, 0x10) }, merged));
        EXPECT_TRUE(merged.empty());
        // nothing is added for no list or no form
        lists.add(0, { pistol, 5, 1, 100.f });
        lists.add(npcList, { 0, 5, 1, 100.f });
        EXPECT_EQ(lists.listCount(), 1u);
    }

    TEST(FalloutScriptLevelledListsTest, theAddedEntryTakesPartInTheChoiceOfALaterRoll)
    {
        LevelledLists lists;
        const std::vector<ESM4::LVLO> record{ entry(1, 0x10) };
        const auto pick = [&](int level) {
            std::vector<ESM4::LVLO> merged;
            const std::vector<ESM4::LVLO>& entries
                = lists.merge(itemList, record, merged) ? merged : record;
            ESM4::LevelledRandom random(3);
            std::vector<ESM4::LevelledChoice> chosen;
            ESM4::chooseLevelledEntries(entries, {}, level, random, chosen);
            return chosen.empty() ? 0u : chosen[0].mItem.toUint32();
        };
        EXPECT_EQ(pick(20), 0x10u);
        lists.add(itemList, { pistol, 10, 1, 100.f });
        EXPECT_EQ(pick(20), pistol); // the highest level that is not above 20
        EXPECT_EQ(pick(5), 0x10u); // below the level of the new entry
    }

    TEST(FalloutScriptLevelledListsTest, whatWasAddedSurvivesASave)
    {
        LevelledLists lists;
        lists.add(itemList, { pistol, 5, 2, 75.f });
        lists.add(itemList, { rifle, 7, 1, 100.f });
        lists.add(npcList, { raider, 9, 1, 100.f });
        FalloutScript::SaveWriter out;
        lists.write(out);

        LevelledLists loaded;
        FalloutScript::SaveReader in(out.bytes());
        loaded.read(in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.additions(itemList), lists.additions(itemList));
        EXPECT_EQ(loaded.additions(npcList), lists.additions(npcList));
        EXPECT_EQ(loaded.additions(itemList)[0].mHealth, 75.f);
        loaded.clear();
        EXPECT_EQ(loaded.listCount(), 0u);
    }

    TEST(FalloutScriptLevelledListCommandsTest, aScriptAddsAnItemAnAndACharacterAndACreature)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t item = builder.form(itemList);
        const std::uint16_t gun = builder.form(pistol);
        const std::uint16_t npcs = builder.form(npcList);
        const std::uint16_t person = builder.form(raider);
        const std::uint16_t creatures = builder.form(creatureList);
        const std::uint16_t beast = builder.form(mutant);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpAddItemToLeveledList,
                arguments(5).add(refArg(item)).add(refArg(gun)).add(intArg(7)).add(intArg(3)).add(floatArg(50)))
            + command(OpAddNPCToLeveledList, arguments(3).add(refArg(npcs)).add(refArg(person)).add(intArg(4)))
            + command(OpAddCreatureToLeveledList,
                arguments(3).add(refArg(creatures)).add(refArg(beast)).add(intArg(11)))
            + end());

        setup.run(builder);

        ASSERT_EQ(setup.mLists.additions(itemList).size(), 1u);
        EXPECT_EQ(setup.mLists.additions(itemList)[0], (LevelledAddition{ pistol, 7, 3, 50.f }));
        EXPECT_EQ(setup.mLists.additions(npcList), (std::vector<LevelledAddition>{ { raider, 4, 1, 100.f } }));
        EXPECT_EQ(setup.mLists.additions(creatureList), (std::vector<LevelledAddition>{ { mutant, 11, 1, 100.f } }));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptLevelledListCommandsTest, nothingIsAddedForACountOfZeroAListOfAnotherKindOrAnEntryOfAnotherKind)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t item = builder.form(itemList);
        const std::uint16_t gun = builder.form(pistol);
        const std::uint16_t npcs = builder.form(npcList);
        const std::uint16_t beast = builder.form(mutant);
        const std::uint16_t person = builder.form(raider);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            // a count of 0
            + command(OpAddItemToLeveledList,
                arguments(5).add(refArg(item)).add(refArg(gun)).add(intArg(7)).add(intArg(0)).add(floatArg(100)))
            // a creature in a list of characters, and a character in a list of items
            + command(OpAddNPCToLeveledList, arguments(3).add(refArg(npcs)).add(refArg(beast)).add(intArg(4)))
            + command(OpAddItemToLeveledList,
                arguments(5).add(refArg(item)).add(refArg(person)).add(intArg(7)).add(intArg(1)).add(floatArg(100)))
            + end());

        setup.run(builder);

        EXPECT_EQ(setup.mLists.listCount(), 0u);
    }

    TEST(FalloutScriptLevelledListCommandsTest, levelAndCountAreKeptAsSixteenBits)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t item = builder.form(itemList);
        const std::uint16_t gun = builder.form(pistol);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpAddItemToLeveledList,
                arguments(5).add(refArg(item)).add(refArg(gun)).add(intArg(-1)).add(intArg(65537)).add(floatArg(100)))
            + end());

        setup.run(builder);

        ASSERT_EQ(setup.mLists.additions(itemList).size(), 1u);
        EXPECT_EQ(setup.mLists.additions(itemList)[0].mLevel, 65535);
        EXPECT_EQ(setup.mLists.additions(itemList)[0].mCount, 1);
    }
}
