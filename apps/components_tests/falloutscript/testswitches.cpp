#include "scriptbuilder.hpp"

#include <components/falloutscript/extendercommands.hpp>
#include <components/falloutscript/switches.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;

    constexpr std::uint16_t OpSetHardcore = 0x124B;
    constexpr std::uint16_t OpIsHardcore = 0x124A;
    constexpr std::uint16_t OpIsXBox = 0x1135;
    constexpr std::uint16_t OpForceActiveQuest = 0x11DD;
    constexpr std::uint16_t OpPlayBink = 0x1114;
    constexpr std::uint16_t OpKillQuestUpdates = 0x1231;

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FalloutScript::Switches mSwitches;

        Runner() { FalloutScript::addSwitchCommands(mInterpreter, mSwitches); }

        double call(std::uint16_t opcode, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, 0, false, std::move(arguments));
        }
    };

    TEST(FalloutScriptSwitchesTest, the_mode_of_play_is_answered_as_it_was_set)
    {
        Runner run;
        EXPECT_EQ(run.call(OpIsHardcore), 0.0);
        run.call(OpSetHardcore, { 1 });
        EXPECT_EQ(run.call(OpIsHardcore), 1.0);
        run.call(OpSetHardcore, { 0 });
        EXPECT_EQ(run.call(OpIsHardcore), 0.0);
    }

    TEST(FalloutScriptSwitchesTest, a_switch_without_an_argument_is_on_and_a_form_is_kept_by_its_id)
    {
        Runner run;
        run.call(OpKillQuestUpdates);
        EXPECT_EQ(run.mSwitches.get("KillQuestUpdates"), 1);
        run.call(OpForceActiveQuest, { 0x00100001 });
        EXPECT_EQ(run.mSwitches.get("ForceActiveQuest"), 0x00100001);
    }

    TEST(FalloutScriptSwitchesTest, a_command_that_does_nothing_says_so_once_and_gives_nothing)
    {
        Runner run;
        EXPECT_EQ(run.call(OpPlayBink, {}), 0.0);
        EXPECT_EQ(run.call(OpPlayBink, {}), 0.0);
        ASSERT_EQ(run.mHost.mLog.size(), 1u);
        EXPECT_THAT(run.mHost.mLog[0], HasSubstr("PlayBink"));
        EXPECT_THAT(run.mHost.mLog[0], HasSubstr("does nothing"));
        EXPECT_EQ(run.call(OpIsXBox), 0.0);
    }

    TEST(FalloutScriptSwitchesTest, every_command_that_does_nothing_is_one_the_games_have)
    {
        FalloutScript::CommandTable newVegas = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        FalloutScript::addExtenderCommands(newVegas, FalloutScript::Game::NewVegas);
        FalloutScript::CommandTable fallout3 = FalloutScript::vanillaCommands(FalloutScript::Game::Fallout3);
        std::size_t count = 0;
        const FalloutScript::IgnoredCommand* commands = FalloutScript::ignoredCommands(count);
        ASSERT_GT(count, 0u);
        for (std::size_t i = 0; i < count; ++i)
            EXPECT_TRUE(newVegas.find(commands[i].mName) != nullptr || fallout3.find(commands[i].mName) != nullptr)
                << commands[i].mName;
    }

    TEST(FalloutScriptSwitchesTest, a_saved_game_brings_the_switches_back)
    {
        FalloutScript::Switches switches;
        switches.set("SetHardcore", 1);
        switches.set("SetPCYoung", 0);
        switches.set("ForceActiveQuest", 0x00100001);
        FalloutScript::SaveWriter out;
        FalloutScript::writeSwitches(switches, out);
        const std::string bytes = out.take();

        FalloutScript::Switches loaded;
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readSwitches(loaded, in, FalloutScript::switchesSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.get("SetHardcore"), 1);
        EXPECT_EQ(loaded.get("ForceActiveQuest"), 0x00100001);
        // A switch that is off is the same as one never set
        EXPECT_EQ(loaded.all().count("SetPCYoung"), 0u);
        loaded.clear();
        EXPECT_TRUE(loaded.all().empty());
    }
}
