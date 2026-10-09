#include <components/falloutscript/commandtable.hpp>

#include <sstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;

    TEST(FalloutScriptCommandTableTest, findsCommandsByCodeAndByAnyNameInAnyCase)
    {
        FalloutScript::CommandTable table;
        FalloutScript::CommandInfo info;
        info.mOpcode = 0x1004;
        info.mName = "SetStage";
        info.mShortName = "ss";
        table.add(info);

        ASSERT_NE(table.find(std::uint16_t(0x1004)), nullptr);
        EXPECT_EQ(table.find("setstage")->mOpcode, 0x1004);
        EXPECT_EQ(table.find("SS")->mOpcode, 0x1004);
        EXPECT_EQ(table.find("nothing"), nullptr);
        EXPECT_EQ(table.find(std::uint16_t(0x1005)), nullptr);
        EXPECT_EQ(table.size(), 1u);
    }

    TEST(FalloutScriptCommandTableTest, aReplacedCommandGivesUpItsNames)
    {
        FalloutScript::CommandTable table;
        FalloutScript::CommandInfo info;
        info.mOpcode = 0x1004;
        info.mName = "SetStage";
        table.add(info);
        info.mName = "Other";
        table.add(info);

        EXPECT_EQ(table.size(), 1u);
        EXPECT_EQ(table.find("SetStage"), nullptr);
        EXPECT_EQ(table.find("other")->mOpcode, 0x1004);
    }

    TEST(FalloutScriptCommandTableTest, aNameThatAReplacedCommandHeldGoesToTheCommandItShadowed)
    {
        FalloutScript::CommandTable table;
        FalloutScript::CommandInfo first;
        first.mOpcode = 0x1001;
        first.mName = "Foo";
        FalloutScript::CommandInfo second;
        second.mOpcode = 0x1002;
        second.mName = "Other";
        second.mShortName = "foo";
        table.add(first);
        table.add(second);
        ASSERT_EQ(table.find("foo")->mOpcode, 0x1001);

        first.mName = "Bar";
        table.add(first);

        EXPECT_EQ(table.find("foo")->mOpcode, 0x1002);
        EXPECT_EQ(table.find("bar")->mOpcode, 0x1001);
    }

    TEST(FalloutScriptCommandTableTest, aNameGoesToTheFirstOfTheCommandsThatClaimedItNotTheLowestCode)
    {
        FalloutScript::CommandTable table;
        FalloutScript::CommandInfo info;
        info.mName = "Foo";
        info.mOpcode = 0x1002;
        table.add(info);
        info.mOpcode = 0x1003;
        table.add(info);
        info.mOpcode = 0x1001;
        table.add(info);
        ASSERT_EQ(table.find("foo")->mOpcode, 0x1002);

        info.mOpcode = 0x1002;
        info.mName = "Bar";
        table.add(info);

        EXPECT_EQ(table.find("foo")->mOpcode, 0x1003);
        EXPECT_EQ(table.find("bar")->mOpcode, 0x1002);
    }

    TEST(FalloutScriptCommandTableTest, aReplacedCommandThatKeepsANameKeepsItsPlaceInLine)
    {
        FalloutScript::CommandTable table;
        FalloutScript::CommandInfo info;
        info.mName = "Foo";
        info.mOpcode = 0x1002;
        table.add(info);
        info.mOpcode = 0x1001;
        table.add(info);

        info.mOpcode = 0x1002;
        info.mShortName = "f";
        table.add(info);

        EXPECT_EQ(table.find("foo")->mOpcode, 0x1002);
        EXPECT_EQ(table.find("f")->mOpcode, 0x1002);
    }

    TEST(FalloutScriptCommandTableTest, theFirstCommandToHaveANameKeepsIt)
    {
        FalloutScript::CommandTable table;
        FalloutScript::CommandInfo first;
        first.mOpcode = 0x1001;
        first.mName = "Wait";
        FalloutScript::CommandInfo second;
        second.mOpcode = 0x1002;
        second.mName = "Other";
        second.mShortName = "wait";
        table.add(first);
        table.add(second);
        EXPECT_EQ(table.find("wait")->mOpcode, 0x1001);
        EXPECT_EQ(table.find("other")->mOpcode, 0x1002);
    }

    TEST(FalloutScriptCommandTableTest, readsTheCommandsOfTheExecutablesTable)
    {
        std::istringstream csv(
            "0,0x0000,GameMode,,0,,0x0\n"
            "1,0x0124,SetGameSetting,SetGS,0,0 0,0x0\n"
            "2,0x1002,AddItem,,1,50 1 1?,0x0\n"
            "3,0x1039,SetStage,,0,14 23,0x100\n"
            "4,0x105F,SendAssaultAlarm,,0,6? 17?,0x0\n"
            "4,0x109E,\"MoveToMarker\",MoveTo,1,4 2? 2? 2?,0x100\n");
        FalloutScript::CommandTable table;
        const FalloutScript::CommandCsvResult result = FalloutScript::readCommandTableCsv(csv, table);

        EXPECT_EQ(result.mRows, 6u);
        EXPECT_EQ(result.mCommands, 4u);
        EXPECT_EQ(result.mOther, 2u);
        EXPECT_THAT(result.mProblems, IsEmpty());
        EXPECT_EQ(table.size(), 4u);

        const FalloutScript::CommandInfo* addItem = table.find(std::uint16_t(0x1002));
        ASSERT_NE(addItem, nullptr);
        EXPECT_EQ(addItem->mName, "AddItem");
        EXPECT_TRUE(addItem->mNeedsReference);
        ASSERT_EQ(addItem->mParameters.size(), 3u);
        EXPECT_EQ(addItem->mParameters[0].mType, 50u);
        EXPECT_FALSE(addItem->mParameters[0].mOptional);
        EXPECT_EQ(addItem->mParameters[2].mType, 1u);
        EXPECT_TRUE(addItem->mParameters[2].mOptional);

        const FalloutScript::CommandInfo* setStage = table.find("setstage");
        ASSERT_NE(setStage, nullptr);
        EXPECT_FALSE(setStage->mNeedsReference);
        EXPECT_EQ(setStage->mFlags, 0x100u);
        EXPECT_EQ(setStage->mParameters.size(), 2u);

        const FalloutScript::CommandInfo* moveTo = table.find("MoveTo");
        ASSERT_NE(moveTo, nullptr);
        EXPECT_EQ(moveTo->mName, "MoveToMarker");
        EXPECT_EQ(table.find("SetGS"), nullptr);
    }

    TEST(FalloutScriptCommandTableTest, readsTheParseFunctionWhenTheTableHasIt)
    {
        std::istringstream csv("2,0x1059,ShowMessage,,0,49,0x0,0x4F2A10\r\n");
        FalloutScript::CommandTable table;
        const FalloutScript::CommandCsvResult result = FalloutScript::readCommandTableCsv(csv, table);

        EXPECT_THAT(result.mProblems, IsEmpty());
        ASSERT_NE(table.find(std::uint16_t(0x1059)), nullptr);
        EXPECT_EQ(table.find(std::uint16_t(0x1059))->mParse, 0x4F2A10u);
        EXPECT_EQ(table.find(std::uint16_t(0x1059))->mParameters.size(), 1u);
    }

    TEST(FalloutScriptCommandTableTest, namesTheLinesItCannotRead)
    {
        std::istringstream csv(
            "table,opcode,name,short,parent,parameters,flags\n"
            "2,0x1002,AddItem,,1,50 x,0x0\n"
            "\n"
            "2,0x10000,TooBig,,0,,0x0\n"
            "2,0x1003,SetEssential,,0,25 1?,0x0\n"
            "2,0x1004,Short\n");
        FalloutScript::CommandTable table;
        const FalloutScript::CommandCsvResult result = FalloutScript::readCommandTableCsv(csv, table);

        EXPECT_EQ(result.mRows, 5u);
        EXPECT_EQ(result.mCommands, 1u);
        EXPECT_THAT(result.mProblems,
            ElementsAre(HasSubstr("line 1 "), HasSubstr("line 2 "), HasSubstr("line 4 "), HasSubstr("line 6 ")));
        EXPECT_NE(table.find("SetEssential"), nullptr);
        EXPECT_EQ(table.find("AddItem"), nullptr);
    }
}
