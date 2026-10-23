#include "scriptbuilder.hpp"

#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/packagecommands.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>
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
    constexpr std::uint16_t OpGetIsCurrentPackage = 0x10A1;
    constexpr std::uint16_t OpResetAI = 0x11FA;
    constexpr std::uint16_t OpGetCurrentAIPackage = 0x106E;
    constexpr std::uint16_t OpGetOffersServicesNow = 0x10FF;

    class FakeActors : public FalloutScript::PackageWorld
    {
    public:
        std::map<FormId, std::vector<FormId>> mPackages;
        std::vector<FormId> mEvaluated;
        std::vector<FormId> mReset;
        std::map<FormId, FormId> mCurrent;
        std::map<FormId, int> mTypes;
        std::set<FormId> mOffering;

        void addScriptPackage(FormId actor, FormId package) override { mPackages[actor].push_back(package); }
        void removeScriptPackage(FormId actor) override
        {
            if (std::vector<FormId>& packages = mPackages[actor]; !packages.empty())
                packages.pop_back();
        }
        void evaluatePackage(FormId actor) override { mEvaluated.push_back(actor); }
        void resetAI(FormId actor) override { mReset.push_back(actor); }
        FormId currentPackage(FormId actor) override
        {
            const auto found = mCurrent.find(actor);
            return found != mCurrent.end() ? found->second : 0;
        }
        int currentPackageType(FormId actor) override
        {
            const auto found = mTypes.find(actor);
            return found != mTypes.end() ? found->second : -1;
        }
        bool currentPackageOffersServices(FormId actor) override { return mOffering.count(actor) != 0; }
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

    TEST(FalloutScriptPackageCommandsTest, reset_ai_starts_the_ai_of_the_character_over)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpResetAI, arguments(0)) + end());
        setup.addScript(builder);
        setup.mObjects.add(guard, scriptOfGuards);

        setup.mObjects.trigger(guard, BlockType::OnActivate);

        EXPECT_THAT(setup.mActors.mReset, ElementsAre(guard));
        EXPECT_THAT(setup.mActors.mEvaluated, IsEmpty());
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

    TEST(FalloutScriptPackageCommandsTest, get_is_current_package_tells_whether_a_package_is_the_one_followed_now)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t other = builder.form(otherGuard);
        const std::uint16_t first = builder.form(patrol);
        const std::uint16_t second = builder.form(rest);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetIsCurrentPackage, arguments(1).add(refArg(first))))
            + setTo(var('s', 2), callToken(OpGetIsCurrentPackage, arguments(1).add(refArg(second))))
            + setTo(var('s', 3), callToken(OpGetIsCurrentPackage, arguments(1).add(refArg(first)), other)) + end());
        builder.variable(1, 0, "first").variable(2, 0, "second").variable(3, 0, "other");
        setup.addScript(builder);
        setup.mObjects.add(guard, scriptOfGuards);
        setup.mActors.mCurrent[guard] = patrol;

        setup.mObjects.trigger(guard, BlockType::OnActivate);

        FalloutScript::Instance* instance = setup.mObjects.instance(guard);
        ASSERT_NE(instance, nullptr);
        EXPECT_EQ(instance->variable(1), 1);
        EXPECT_EQ(instance->variable(2), 0);
        // someone who follows nothing follows no package
        EXPECT_EQ(instance->variable(3), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptPackageCommandsTest, the_kind_of_the_current_package_is_told_in_the_numbers_of_the_games)
    {
        Runner run;
        const auto kind
            = [&run](FormId actor) { return run.mInterpreter.call(OpGetCurrentAIPackage, actor, true, {}); };
        EXPECT_EQ(kind(guard), -1.0);
        run.mActors.mTypes[guard] = 0;
        EXPECT_EQ(kind(guard), 0.0);
        run.mActors.mTypes[guard] = 5;
        EXPECT_EQ(kind(guard), 13.0);
        run.mActors.mTypes[guard] = 12;
        EXPECT_EQ(kind(guard), 36.0);
        run.mActors.mTypes[guard] = 17;
        EXPECT_EQ(kind(guard), -1.0);
        run.mActors.mTypes[guard] = 31;
        EXPECT_EQ(kind(guard), 32.0);
        run.mActors.mTypes[guard] = 32;
        EXPECT_EQ(kind(guard), -1.0);
        EXPECT_EQ(run.mInterpreter.call(OpGetCurrentAIPackage, 0, false, {}), -1.0);
    }

    TEST(FalloutScriptPackageCommandsTest, a_character_offers_its_services_when_its_package_does)
    {
        Runner run;
        const auto offers
            = [&run](FormId actor) { return run.mInterpreter.call(OpGetOffersServicesNow, actor, true, {}); };
        EXPECT_EQ(offers(guard), 0.0);
        run.mActors.mOffering.insert(guard);
        EXPECT_EQ(offers(guard), 1.0);
        EXPECT_EQ(offers(otherGuard), 0.0);
    }
}
