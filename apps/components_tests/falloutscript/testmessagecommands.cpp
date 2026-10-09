#include "scriptbuilder.hpp"

#include <components/falloutscript/messagecommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId crate = 0x01000100;
    constexpr FormId scriptOfCrates = 0x01002000;
    constexpr FormId warning = 0x01004000;
    constexpr FormId greeting = 0x01004001;

    // The code of the command in the table of the games
    constexpr std::uint16_t OpShowMessage = 0x1059;

    class FakeScreen : public FalloutScript::MessageWorld
    {
    public:
        std::vector<FormId> mShown;

        void showMessage(FormId message) override { mShown.push_back(message); }
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
        FakeScreen mScreen;

        Runner() { FalloutScript::addMessageCommands(mInterpreter, mScreen); }

        void addScript(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfCrates];
            record.mId = ESM::FormId::fromUint32(scriptOfCrates);
            record.mScript = builder.definition();
        }
    };

    TEST(FalloutScriptMessageCommandsTest, show_message_shows_the_messages_in_the_order_they_are_called)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(warning);
        const std::uint16_t second = builder.form(greeting);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpShowMessage, arguments(1).add(refArg(first)))
            + command(OpShowMessage, arguments(1).add(refArg(second)))
            + command(OpShowMessage, arguments(1).add(refArg(first))) + end());
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_THAT(setup.mScreen.mShown, ElementsAre(warning, greeting, warning));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptMessageCommandsTest, a_message_that_is_no_form_shows_nothing)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(
            scriptName() + begin(BlockType::OnActivate) + command(OpShowMessage, arguments(1).add(intArg(0))) + end());
        setup.addScript(builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_THAT(setup.mScreen.mShown, IsEmpty());
    }
}
