#include "scriptbuilder.hpp"

#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/playercontrols.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <map>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    using FalloutScript::PlayerControl;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId crate = 0x01000100;
    constexpr FormId scriptOfCrates = 0x01002000;

    // The code of the commands in the table of the games
    constexpr std::uint16_t OpEnablePlayerControls = 0x1060;
    constexpr std::uint16_t OpDisablePlayerControls = 0x1061;

    class FakeControls : public FalloutScript::ControlsWorld
    {
    public:
        // The calls in order: the control and whether it was given
        std::vector<std::pair<PlayerControl, bool>> mCalls;

        void setPlayerControl(PlayerControl control, bool enabled) override { mCalls.emplace_back(control, enabled); }

        // What the player has of each control after the calls, starting with all of them
        std::array<bool, FalloutScript::playerControlCount> state() const
        {
            std::array<bool, FalloutScript::playerControlCount> result;
            result.fill(true);
            for (const auto& [control, enabled] : mCalls)
                result[static_cast<std::size_t>(control)] = enabled;
            return result;
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
        FakeControls mControls;

        Runner() { FalloutScript::addPlayerControlCommands(mInterpreter, mControls); }

        // Runs a script that calls the command with the flags. Each call is a script and an object of its own.
        void call(std::uint16_t opcode, const std::vector<int>& flags)
        {
            const FormId owner = crate + mCalls;
            const FormId script = scriptOfCrates + mCalls;
            ++mCalls;
            ScriptBuilder builder;
            Bytes args = arguments(static_cast<std::uint16_t>(flags.size()));
            for (const int flag : flags)
                args.add(intArg(flag));
            builder.code(scriptName() + begin(BlockType::OnActivate) + command(opcode, args) + end());
            ESM4::Script& record = mScripts[script];
            record.mId = ESM::FormId::fromUint32(script);
            record.mScript = builder.definition();
            mObjects.add(owner, script);
            mObjects.trigger(owner, BlockType::OnActivate);
        }

    private:
        std::uint32_t mCalls = 0;
    };

    using Controls = std::array<bool, FalloutScript::playerControlCount>;

    TEST(FalloutScriptPlayerControlsTest, disabling_with_no_flags_takes_movement_the_pipboy_and_fighting)
    {
        Runner setup;

        setup.call(OpDisablePlayerControls, {});

        EXPECT_EQ(setup.mControls.state(), (Controls{ false, false, false, true, true, true, true }));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPlayerControlsTest, enabling_with_no_flags_gives_all_seven)
    {
        Runner setup;

        setup.call(OpDisablePlayerControls, { 1, 1, 1, 1, 1, 1, 1 });
        EXPECT_EQ(setup.mControls.state(), (Controls{ false, false, false, false, false, false, false }));
        setup.mControls.mCalls.clear();
        setup.call(OpEnablePlayerControls, {});

        EXPECT_EQ(setup.mControls.mCalls.size(), 7u);
        EXPECT_EQ(setup.mControls.state(), (Controls{ true, true, true, true, true, true, true }));
    }

    TEST(FalloutScriptPlayerControlsTest, a_flag_of_zero_leaves_the_control_as_it_is)
    {
        Runner setup;

        // Everything is taken, then only looking is given back
        setup.call(OpDisablePlayerControls, { 1, 1, 1, 1, 1, 1, 1 });
        setup.call(OpEnablePlayerControls, { 0, 0, 0, 0, 1 });

        // Enabling leaves out the last two flags, which default to 1 for it: rollover and sneaking come back as well
        EXPECT_EQ(setup.mControls.state(), (Controls{ false, false, false, false, true, true, true }));
    }

    TEST(FalloutScriptPlayerControlsTest, flags_that_a_disable_call_leaves_out_take_the_defaults_of_the_games)
    {
        Runner setup;

        // Two flags: movement and the Pip-Boy are given, fighting takes its default of 1 and the other four stay
        setup.call(OpDisablePlayerControls, { 0, 1 });

        EXPECT_EQ(setup.mControls.state(), (Controls{ true, false, false, true, true, true, true }));
    }

    TEST(FalloutScriptPlayerControlsTest, controls_are_acted_on_in_the_order_of_the_flags)
    {
        Runner setup;

        setup.call(OpDisablePlayerControls, { 1, 0, 0, 1, 0, 0, 1 });

        ASSERT_EQ(setup.mControls.mCalls.size(), 3u);
        EXPECT_EQ(setup.mControls.mCalls[0], std::make_pair(PlayerControl::Movement, false));
        EXPECT_EQ(setup.mControls.mCalls[1], std::make_pair(PlayerControl::PointOfView, false));
        EXPECT_EQ(setup.mControls.mCalls[2], std::make_pair(PlayerControl::Sneaking, false));
    }
}
