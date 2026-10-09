#include "scriptbuilder.hpp"

#include <components/falloutscript/objectcommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::Instance;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId crate = 0x01000100;
    constexpr FormId otherCrate = 0x01000101;
    constexpr FormId player = 0x01000014;
    constexpr FormId scriptOfCrates = 0x01002000;
    constexpr FormId cell = 0x01003000;

    // The types of variables of the SLSD of a script
    constexpr std::uint32_t floatVariable = 0;
    constexpr std::uint32_t integerVariable = 1;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpGetDistance = 0x1001;
    constexpr std::uint16_t OpGetSecondsPassed = 0x100C;
    constexpr std::uint16_t OpEnable = 0x1021;
    constexpr std::uint16_t OpDisable = 0x1022;
    constexpr std::uint16_t OpGetDisabled = 0x1023;
    constexpr std::uint16_t OpGetInCell = 0x1043;
    constexpr std::uint16_t OpGetRandomPercent = 0x104D;
    constexpr std::uint16_t OpIsActionRef = 0x1069;
    constexpr std::uint16_t OpGetIsReference = 0x1088;
    constexpr std::uint16_t OpGetActionRef = 0x10CD;
    constexpr std::uint16_t OpGetSelf = 0x10CE;

    class FakeWorld : public FalloutScript::ObjectWorld
    {
    public:
        std::set<FormId> mDisabled;
        std::map<FormId, FormId> mCells;
        std::map<std::pair<FormId, FormId>, double> mDistances;

        bool isDisabled(FormId reference) override { return mDisabled.count(reference) != 0; }
        void setDisabled(FormId reference, bool disabled) override
        {
            if (disabled)
                mDisabled.insert(reference);
            else
                mDisabled.erase(reference);
        }
        bool distance(FormId first, FormId second, double& result) override
        {
            auto found = mDistances.find({ first, second });
            if (found == mDistances.end())
                found = mDistances.find({ second, first });
            if (found == mDistances.end())
                return false;
            result = found->second;
            return true;
        }
        FormId cellOf(FormId reference) override
        {
            const auto found = mCells.find(reference);
            return found != mCells.end() ? found->second : 0;
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
        FakeWorld mWorld;
        int mRandom = 42;

        Runner()
        {
            FalloutScript::addObjectCommands(mInterpreter, mObjects, mWorld, [this] { return mRandom; });
        }

        void addScript(FormId id, const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[id];
            record.mId = ESM::FormId::fromUint32(id);
            record.mScript = builder.definition();
        }

        /// A script with two counters: the first counts the frames, the second the times the object was loaded
        void addCountingScript(FormId id = scriptOfCrates)
        {
            ScriptBuilder builder;
            builder.code(scriptName() + begin(BlockType::GameMode)
                + setTo(var('s', 1), var('s', 1) + number("1") + op("+")) + end() + begin(BlockType::OnLoad)
                + setTo(var('s', 2), var('s', 2) + number("1") + op("+")) + end());
            builder.variable(1, integerVariable, "frames").variable(2, integerVariable, "loads");
            addScript(id, builder);
        }

        double variable(FormId reference, std::uint32_t index)
        {
            Instance* instance = mObjects.instance(reference);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    /// A script with a block of one event, which sets the first variable to the value of an expression
    ScriptBuilder assigns(std::uint16_t blockType, const Bytes& expression)
    {
        ScriptBuilder builder;
        builder.code(scriptName() + begin(blockType) + setTo(var('f', 1), expression) + end());
        builder.variable(1, floatVariable, "result");
        return builder;
    }

    TEST(FalloutScriptObjectsTest, a_reference_that_is_loaded_runs_its_game_mode_every_frame)
    {
        Runner setup;
        setup.addCountingScript();
        ASSERT_NE(setup.mObjects.add(crate, scriptOfCrates), nullptr);

        setup.mObjects.update();
        EXPECT_EQ(setup.variable(crate, 1), 0);

        setup.mObjects.setLoaded(crate, true);
        setup.mObjects.update();
        setup.mObjects.update();
        EXPECT_EQ(setup.variable(crate, 1), 2);

        setup.mObjects.setLoaded(crate, false);
        setup.mObjects.update();
        EXPECT_EQ(setup.variable(crate, 1), 2);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptObjectsTest, on_load_runs_each_time_the_reference_becomes_loaded)
    {
        Runner setup;
        setup.addCountingScript();
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);
        EXPECT_EQ(setup.variable(crate, 2), 1);
        setup.mObjects.setLoaded(crate, true); // already loaded
        EXPECT_EQ(setup.variable(crate, 2), 1);
        setup.mObjects.setLoaded(crate, false);
        setup.mObjects.setLoaded(crate, true);
        EXPECT_EQ(setup.variable(crate, 2), 2);
        EXPECT_TRUE(setup.mObjects.loaded(crate));
    }

    TEST(FalloutScriptObjectsTest, a_reference_keeps_its_variables_when_it_is_unloaded)
    {
        Runner setup;
        setup.addCountingScript();
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.setLoaded(crate, true);
        setup.mObjects.update();
        setup.mObjects.setLoaded(crate, false);

        // given the script again, as when the cell is loaded again
        Instance* again = setup.mObjects.add(crate, scriptOfCrates);

        ASSERT_NE(again, nullptr);
        EXPECT_EQ(again->variable(1), 1);
        EXPECT_EQ(setup.mObjects.size(), 1u);
    }

    TEST(FalloutScriptObjectsTest, references_of_one_script_have_variables_of_their_own)
    {
        Runner setup;
        setup.addCountingScript();
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.add(otherCrate, scriptOfCrates);
        setup.mObjects.setLoaded(crate, true);

        setup.mObjects.update();
        setup.mObjects.update();

        EXPECT_EQ(setup.variable(crate, 1), 2);
        EXPECT_EQ(setup.variable(otherCrate, 1), 0);
        EXPECT_EQ(setup.mObjects.instance(crate)->owner(), crate);
        EXPECT_EQ(setup.mObjects.instance(otherCrate)->owner(), otherCrate);
    }

    TEST(FalloutScriptObjectsTest, a_script_that_is_not_known_or_cannot_run_is_reported_once)
    {
        Runner setup;
        ScriptBuilder broken;
        broken.code(Bytes().u16(0x10).u16(100)); // a statement that runs past the end of the code
        setup.addScript(scriptOfCrates, broken);

        EXPECT_EQ(setup.mObjects.add(crate, scriptOfCrates), nullptr);
        EXPECT_EQ(setup.mObjects.add(otherCrate, scriptOfCrates), nullptr);
        EXPECT_EQ(setup.mObjects.add(crate, 0x01009999), nullptr);
        EXPECT_EQ(setup.mObjects.add(crate, 0x01009999), nullptr);
        EXPECT_EQ(setup.mObjects.add(0, scriptOfCrates), nullptr);

        EXPECT_THAT(setup.mHost.mLog, SizeIs(2));
        EXPECT_FALSE(setup.mObjects.has(crate));
        EXPECT_EQ(setup.mObjects.size(), 0u);
    }

    TEST(FalloutScriptObjectsTest, an_event_runs_the_blocks_of_that_type_with_the_action_reference)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetActionRef, Bytes())) + end());
        builder.variable(1, integerVariable, "activator");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        EXPECT_TRUE(setup.mObjects.hasBlock(crate, BlockType::OnActivate));
        EXPECT_FALSE(setup.mObjects.hasBlock(crate, BlockType::OnLoad));
        EXPECT_FALSE(setup.mObjects.trigger(crate, BlockType::OnLoad));
        EXPECT_FALSE(setup.mObjects.trigger(otherCrate, BlockType::OnActivate));
        EXPECT_EQ(setup.mObjects.actionReference(), 0u);

        EXPECT_TRUE(setup.mObjects.trigger(crate, BlockType::OnActivate, player));

        EXPECT_EQ(setup.variable(crate, 1), static_cast<double>(player));
        EXPECT_EQ(setup.mObjects.actionReference(), 0u);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptObjectsTest, is_action_ref_tells_whether_the_activator_is_that_reference)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t playerSlot = builder.form(player);
        const std::uint16_t crateSlot = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsActionRef, arguments(1).add(refArg(playerSlot))))
            + setTo(var('s', 2), callToken(OpIsActionRef, arguments(1).add(refArg(crateSlot)))) + end());
        builder.variable(1, integerVariable, "player").variable(2, integerVariable, "crate");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate, player);

        EXPECT_EQ(setup.variable(crate, 1), 1);
        EXPECT_EQ(setup.variable(crate, 2), 0);
    }

    TEST(FalloutScriptObjectsTest, a_script_disables_and_enables_itself_and_other_references)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t other = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpDisable, arguments(0))
            + command(OpDisable, arguments(0), other) + end() + begin(BlockType::OnLoad)
            + command(OpEnable, arguments(0), other) + end());
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.trigger(crate, BlockType::OnActivate);
        EXPECT_THAT(setup.mWorld.mDisabled, UnorderedElementsAre(crate, otherCrate));

        setup.mObjects.setLoaded(crate, true);
        EXPECT_THAT(setup.mWorld.mDisabled, UnorderedElementsAre(crate));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptObjectsTest, get_disabled_tells_the_state_of_the_reference)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t other = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnActivate) + setTo(var('s', 1), callToken(OpGetDisabled, Bytes()))
            + setTo(var('s', 2), callToken(OpGetDisabled, Bytes(), other)) + end());
        builder.variable(1, integerVariable, "self").variable(2, integerVariable, "other");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mDisabled.insert(otherCrate);

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_EQ(setup.variable(crate, 1), 0);
        EXPECT_EQ(setup.variable(crate, 2), 1);
    }

    TEST(FalloutScriptObjectsTest, get_distance_measures_between_the_script_and_a_reference)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t playerSlot = builder.form(player);
        const std::uint16_t other = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('f', 1), callToken(OpGetDistance, arguments(1).add(refArg(playerSlot))))
            + setTo(var('f', 2), callToken(OpGetDistance, arguments(1).add(refArg(other)))) + end());
        builder.variable(1, floatVariable, "near").variable(2, floatVariable, "far");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mDistances[{ crate, player }] = 125.5;

        setup.mObjects.trigger(crate, BlockType::OnActivate);

        EXPECT_EQ(setup.variable(crate, 1), 125.5);
        // a distance that can not be told is a far one, so that "is it near" says no
        EXPECT_EQ(setup.variable(crate, 2), static_cast<double>(std::numeric_limits<float>::max()));
    }

    TEST(FalloutScriptObjectsTest, get_in_cell_and_get_is_reference_ask_about_the_object_the_script_belongs_to)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t cellSlot = builder.form(cell);
        const std::uint16_t crateSlot = builder.form(crate);
        const std::uint16_t otherSlot = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetInCell, arguments(1).add(refArg(cellSlot))))
            + setTo(var('s', 2), callToken(OpGetIsReference, arguments(1).add(refArg(crateSlot))))
            + setTo(var('s', 3), callToken(OpGetIsReference, arguments(1).add(refArg(otherSlot)))) + end());
        builder.variable(1, integerVariable, "inCell")
            .variable(2, integerVariable, "isSelf")
            .variable(3, integerVariable, "isOther");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mCells[crate] = cell;

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 1);
        EXPECT_EQ(setup.variable(crate, 2), 1);
        EXPECT_EQ(setup.variable(crate, 3), 0);
    }

    TEST(FalloutScriptObjectsTest, get_self_tells_the_reference_and_nothing_for_the_script_of_a_quest)
    {
        Runner setup;
        setup.addScript(scriptOfCrates, assigns(BlockType::OnLoad, callToken(OpGetSelf, Bytes())));
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.setLoaded(crate, true);
        EXPECT_EQ(setup.variable(crate, 1), static_cast<double>(crate));

        // an instance that is not the script of an object, such as that of a quest
        Instance quest(
            prepare(assigns(BlockType::GameMode, callToken(OpGetSelf, Bytes())), setup.mCommands), 0x01005000);
        setup.mInterpreter.run(quest, BlockType::GameMode);
        EXPECT_EQ(quest.variable(1), 0);
    }

    TEST(FalloutScriptObjectsTest, seconds_passed_and_random_percent_come_from_the_frame_and_the_generator)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::GameMode)
            + setTo(var('f', 1), callToken(OpGetSecondsPassed, Bytes()))
            + setTo(var('s', 2), callToken(OpGetRandomPercent, Bytes())) + end());
        builder.variable(1, floatVariable, "seconds").variable(2, integerVariable, "random");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.setLoaded(crate, true);
        setup.mRandom = 17;

        setup.mObjects.setSecondsPassed(0.25f);
        setup.mObjects.update();

        EXPECT_EQ(setup.variable(crate, 1), 0.25);
        EXPECT_EQ(setup.variable(crate, 2), 17);
        EXPECT_EQ(setup.mObjects.secondsPassed(), 0.25f);
    }

    TEST(FalloutScriptObjectsTest, a_script_that_unloads_another_reference_does_not_upset_the_frame)
    {
        Runner setup;
        // the first reference (the lower form id) runs, and its script unloads the second through the world
        class Unloader : public FalloutScript::ObjectWorld
        {
        public:
            ObjectScripts* mObjects = nullptr;
            bool isDisabled(FormId) override { return false; }
            void setDisabled(FormId, bool) override { mObjects->setLoaded(otherCrate, false); }
            bool distance(FormId, FormId, double&) override { return false; }
            FormId cellOf(FormId) override { return 0; }
        } unloader;
        unloader.mObjects = &setup.mObjects;
        FalloutScript::addObjectCommands(setup.mInterpreter, setup.mObjects, unloader, [] { return 0; });

        ScriptBuilder first;
        first.code(scriptName() + begin(BlockType::GameMode) + command(OpDisable, arguments(0)) + end());
        setup.addScript(scriptOfCrates, first);
        setup.addCountingScript(scriptOfCrates + 1);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.add(otherCrate, scriptOfCrates + 1);
        setup.mObjects.setLoaded(crate, true);
        setup.mObjects.setLoaded(otherCrate, true);

        setup.mObjects.update();

        EXPECT_FALSE(setup.mObjects.loaded(otherCrate));
        EXPECT_EQ(setup.variable(otherCrate, 1), 0);
    }

    TEST(FalloutScriptObjectsTest, clear_forgets_every_reference)
    {
        Runner setup;
        setup.addCountingScript();
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.setLoaded(crate, true);

        setup.mObjects.clear();
        setup.mObjects.update();

        EXPECT_FALSE(setup.mObjects.has(crate));
        EXPECT_EQ(setup.mObjects.size(), 0u);
        EXPECT_EQ(setup.mObjects.instance(crate), nullptr);
    }
}
