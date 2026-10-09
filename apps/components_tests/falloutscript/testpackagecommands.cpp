#include "scriptbuilder.hpp"

#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/packagecommands.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId guard = 0x01000100;
    constexpr FormId otherGuard = 0x01000101;
    constexpr FormId scriptOfGuards = 0x01002000;
    constexpr FormId patrol = 0x01003000;
    constexpr FormId rest = 0x01003001;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpEvaluatePackage = 0x105E;
    constexpr std::uint16_t OpAddScriptPackage = 0x1097;
    constexpr std::uint16_t OpRemoveScriptPackage = 0x1098;

    class FakeActors : public FalloutScript::PackageWorld
    {
    public:
        std::map<FormId, std::vector<FormId>> mPackages;
        std::vector<FormId> mEvaluated;

        void addScriptPackage(FormId actor, FormId package) override { mPackages[actor].push_back(package); }
        void removeScriptPackage(FormId actor) override
        {
            if (std::vector<FormId>& packages = mPackages[actor]; !packages.empty())
                packages.pop_back();
        }
        void evaluatePackage(FormId actor) override { mEvaluated.push_back(actor); }
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
        FakeActors mActors;

        Runner() { FalloutScript::addPackageCommands(mInterpreter, mActors); }

        void addScript(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGuards];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards);
            record.mScript = builder.definition();
        }
    };

    TEST(FalloutScriptPackageCommandsTest, a_script_gives_a_package_to_its_object_and_to_another)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t other = builder.form(otherGuard);
        const std::uint16_t package = builder.form(patrol);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpAddScriptPackage, arguments(1).add(refArg(package))) + command(OpEvaluatePackage, arguments(0))
            + command(OpAddScriptPackage, arguments(1).add(refArg(package)), other) + end());
        setup.addScript(builder);
        setup.mObjects.add(guard, scriptOfGuards);

        setup.mObjects.trigger(guard, BlockType::OnActivate);

        EXPECT_THAT(setup.mActors.mPackages,
            UnorderedElementsAre(Pair(guard, ElementsAre(patrol)), Pair(otherGuard, ElementsAre(patrol))));
        EXPECT_THAT(setup.mActors.mEvaluated, ElementsAre(guard));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPackageCommandsTest, remove_script_package_takes_away_the_last_package_given)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(patrol);
        const std::uint16_t second = builder.form(rest);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpAddScriptPackage, arguments(1).add(refArg(first)))
            + command(OpAddScriptPackage, arguments(1).add(refArg(second))) + end() + begin(BlockType::OnLoad)
            + command(OpRemoveScriptPackage, arguments(0)) + end());
        setup.addScript(builder);
        setup.mObjects.add(guard, scriptOfGuards);

        setup.mObjects.trigger(guard, BlockType::OnActivate);
        EXPECT_THAT(setup.mActors.mPackages[guard], ElementsAre(patrol, rest));

        setup.mObjects.trigger(guard, BlockType::OnLoad);
        EXPECT_THAT(setup.mActors.mPackages[guard], ElementsAre(patrol));
    }

    TEST(FalloutScriptPackageCommandsTest, a_package_that_is_not_a_form_is_left_out)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpAddScriptPackage, arguments(1).add(intArg(0))) + end());
        setup.addScript(builder);
        setup.mObjects.add(guard, scriptOfGuards);

        setup.mObjects.trigger(guard, BlockType::OnActivate);

        EXPECT_THAT(setup.mActors.mPackages, IsEmpty());
    }
}
