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
#include <vector>

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
    constexpr FormId crateBase = 0x01000200;
    constexpr FormId worldspace = 0x01004000;
    constexpr FormId otherWorldspace = 0x01004001;
    constexpr FormId player = 0x01000014;
    constexpr FormId scriptOfCrates = 0x01002000;
    constexpr FormId cell = 0x01003000;

    // The types of variables of the SLSD of a script
    constexpr std::uint32_t floatVariable = 0;
    constexpr std::uint32_t integerVariable = 1;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpGetDistance = 0x1001;
    constexpr std::uint16_t OpActivate = 0x100D;
    constexpr std::uint16_t OpGetSecondsPassed = 0x100C;
    constexpr std::uint16_t OpEnable = 0x1021;
    constexpr std::uint16_t OpDisable = 0x1022;
    constexpr std::uint16_t OpGetDisabled = 0x1023;
    constexpr std::uint16_t OpGetInCell = 0x1043;
    constexpr std::uint16_t OpGetIsID = 0x1048;
    constexpr std::uint16_t OpGetInSameCell = 0x1020;
    constexpr std::uint16_t OpPlaceAtMe = 0x1025;
    constexpr std::uint16_t OpGetCurrentTime = 0x1012;
    constexpr std::uint16_t OpGetGlobalValue = 0x104A;
    constexpr std::uint16_t OpGetInWorldspace = 0x1136;
    constexpr std::uint16_t OpGetLinkedRef = 0x116B;
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
        std::map<FormId, FormId> mBases;
        std::map<FormId, FormId> mWorldspaces;
        std::map<std::pair<FormId, FormId>, FormId> mLinks; // by the reference and the keyword

        struct Placement
        {
            FormId mReference;
            FormId mBase;
            int mCount;
            double mDistance;
            int mDirection;
        };
        std::vector<Placement> mPlacements;
        FormId mMade = 0;
        double mHour = 0;

        struct Activation
        {
            FormId mTarget;
            FormId mActivator;
            bool mRunOnActivate;
            bool operator==(const Activation& other) const
            {
                return mTarget == other.mTarget && mActivator == other.mActivator
                    && mRunOnActivate == other.mRunOnActivate;
            }
        };
        std::vector<Activation> mActivations;

        void activate(FormId target, FormId activator, bool runOnActivate) override
        {
            mActivations.push_back({ target, activator, runOnActivate });
        }
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
        bool baseFormOf(FormId reference, FormId& base) override
        {
            const auto found = mBases.find(reference);
            if (found == mBases.end())
                return false;
            base = found->second;
            return true;
        }
        FormId worldspaceOf(FormId reference) override
        {
            const auto found = mWorldspaces.find(reference);
            return found != mWorldspaces.end() ? found->second : 0;
        }
        FormId linkedRef(FormId reference, FormId keyword) override
        {
            const auto found = mLinks.find({ reference, keyword });
            return found != mLinks.end() ? found->second : 0;
        }
        double gameHour() override { return mHour; }
        FormId placeAtMe(FormId reference, FormId base, int count, double distance, int direction) override
        {
            mPlacements.push_back({ reference, base, count, distance, direction });
            return mMade;
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

    TEST(FalloutScriptObjectsTest, get_is_id_compares_the_form_the_object_stands_for)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t baseSlot = builder.form(crateBase);
        const std::uint16_t otherSlot = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetIsID, arguments(1).add(refArg(baseSlot))))
            + setTo(var('s', 2), callToken(OpGetIsID, arguments(1).add(refArg(otherSlot)))) + end());
        builder.variable(1, integerVariable, "isBase").variable(2, integerVariable, "isOther");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mBases[crate] = crateBase;

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 1);
        EXPECT_EQ(setup.variable(crate, 2), 0);
    }

    TEST(FalloutScriptObjectsTest, get_is_id_is_no_for_an_object_the_world_has_no_form_for)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t baseSlot = builder.form(crateBase);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetIsID, arguments(1).add(refArg(baseSlot)))) + end());
        builder.variable(1, integerVariable, "isBase");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 0);
    }

    TEST(FalloutScriptObjectsTest, get_in_same_cell_compares_the_cells_of_two_objects)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t otherSlot = builder.form(otherCrate);
        const std::uint16_t playerSlot = builder.form(player);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetInSameCell, arguments(1).add(refArg(otherSlot))))
            + setTo(var('s', 2), callToken(OpGetInSameCell, arguments(1).add(refArg(playerSlot)))) + end());
        builder.variable(1, integerVariable, "sameCell").variable(2, integerVariable, "otherCell");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mCells[crate] = cell;
        setup.mWorld.mCells[otherCrate] = cell;
        setup.mWorld.mCells[player] = cell + 1;

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 1);
        EXPECT_EQ(setup.variable(crate, 2), 0);
    }

    TEST(FalloutScriptObjectsTest, get_in_same_cell_is_no_when_the_cell_of_the_object_is_not_known)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t otherSlot = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetInSameCell, arguments(1).add(refArg(otherSlot)))) + end());
        builder.variable(1, integerVariable, "sameCell");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);

        // Two objects that are in no known cell are not in the same one
        EXPECT_EQ(setup.variable(crate, 1), 0);
    }

    TEST(FalloutScriptObjectsTest, get_in_worldspace_compares_the_worldspace_of_the_object)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t wasteland = builder.form(worldspace);
        const std::uint16_t other = builder.form(otherWorldspace);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetInWorldspace, arguments(1).add(refArg(wasteland))))
            + setTo(var('s', 2), callToken(OpGetInWorldspace, arguments(1).add(refArg(other)))) + end());
        builder.variable(1, integerVariable, "inWasteland").variable(2, integerVariable, "inOther");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mWorldspaces[crate] = worldspace;

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 1);
        EXPECT_EQ(setup.variable(crate, 2), 0);
    }

    TEST(FalloutScriptObjectsTest, get_linked_ref_tells_the_reference_the_object_is_linked_to)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetLinkedRef, arguments(0))) + end());
        builder.variable(1, integerVariable, "link");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mLinks[{ crate, 0 }] = otherCrate;

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), static_cast<double>(otherCrate));
    }

    TEST(FalloutScriptObjectsTest, get_linked_ref_is_0_for_an_object_with_no_link)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('s', 1), callToken(OpGetLinkedRef, arguments(0))) + end());
        builder.variable(1, integerVariable, "link");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 0);
    }

    TEST(FalloutScriptObjectsTest, place_at_me_asks_for_the_objects_next_to_the_reference_and_tells_the_first)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t base = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('f', 1),
                callToken(
                    OpPlaceAtMe, arguments(4).add(refArg(base)).add(intArg(3)).add(floatArg(120.5)).add(intArg(2))))
            + end());
        builder.variable(1, floatVariable, "made");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mMade = 0xFF000100; // a float variable keeps 24 bits of it, which this one fits in

        setup.mObjects.setLoaded(crate, true);

        ASSERT_EQ(setup.mWorld.mPlacements.size(), 1u);
        const auto& placement = setup.mWorld.mPlacements.front();
        EXPECT_EQ(placement.mReference, crate);
        EXPECT_EQ(placement.mBase, otherCrate);
        EXPECT_EQ(placement.mCount, 3);
        EXPECT_EQ(placement.mDistance, 120.5);
        EXPECT_EQ(placement.mDirection, 2);
        EXPECT_EQ(setup.variable(crate, 1), static_cast<double>(0xFF000100));
    }

    TEST(FalloutScriptObjectsTest, place_at_me_makes_one_object_in_front_without_the_optional_arguments)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t base = builder.form(otherCrate);
        builder.code(
            scriptName() + begin(BlockType::OnLoad) + command(OpPlaceAtMe, arguments(1).add(refArg(base))) + end());
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);

        ASSERT_EQ(setup.mWorld.mPlacements.size(), 1u);
        const auto& placement = setup.mWorld.mPlacements.front();
        EXPECT_EQ(placement.mCount, 1);
        EXPECT_EQ(placement.mDistance, 0.0);
        EXPECT_EQ(placement.mDirection, 0);
    }

    TEST(FalloutScriptObjectsTest, place_at_me_keeps_the_count_within_what_a_script_may_ask_for)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t base = builder.form(otherCrate);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + command(OpPlaceAtMe, arguments(2).add(refArg(base)).add(intArg(100000)))
            + command(OpPlaceAtMe, arguments(2).add(refArg(base)).add(intArg(-4))) + end());
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);

        ASSERT_EQ(setup.mWorld.mPlacements.size(), 2u);
        EXPECT_EQ(setup.mWorld.mPlacements[0].mCount, 100);
        EXPECT_EQ(setup.mWorld.mPlacements[1].mCount, 1);
    }

    TEST(FalloutScriptObjectsTest, get_current_time_tells_the_hour_of_the_clock_of_the_game)
    {
        Runner setup;
        setup.addScript(scriptOfCrates, assigns(BlockType::OnLoad, callToken(OpGetCurrentTime, Bytes())));
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mWorld.mHour = 13.5;

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 13.5);
    }

    TEST(FalloutScriptObjectsTest, get_global_value_tells_the_value_of_a_global_and_0_for_one_that_is_not_there)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t known = builder.form(0x00000500);
        const std::uint16_t unknown = builder.form(0x00000501);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + setTo(var('f', 1), callToken(OpGetGlobalValue, arguments(1).add(refArg(known))))
            + setTo(var('f', 2), callToken(OpGetGlobalValue, arguments(1).add(refArg(unknown)))) + end());
        builder.variable(1, floatVariable, "known").variable(2, floatVariable, "unknown");
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mHost.mGlobals[0x00000500] = 2.5;

        setup.mObjects.setLoaded(crate, true);

        EXPECT_EQ(setup.variable(crate, 1), 2.5);
        EXPECT_EQ(setup.variable(crate, 2), 0);
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
            void activate(FormId, FormId, bool) override {}
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

    TEST(FalloutScriptObjectsTest, activate_names_the_activator_and_whether_the_block_runs)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t who = builder.form(player);
        builder.code(scriptName() + begin(BlockType::OnLoad)
            + command(OpActivate, arguments(2).add(refArg(who)).add(intArg(1))) + end());
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);

        EXPECT_THAT(setup.mWorld.mActivations, ElementsAre(FakeWorld::Activation{ crate, player, true }));
    }

    TEST(FalloutScriptObjectsTest, activate_without_an_activator_takes_the_one_of_the_event)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpActivate, arguments(0)) + end());
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        EXPECT_TRUE(setup.mObjects.trigger(crate, BlockType::OnActivate, otherCrate));

        EXPECT_THAT(setup.mWorld.mActivations, ElementsAre(FakeWorld::Activation{ crate, otherCrate, false }));
    }

    TEST(FalloutScriptObjectsTest, activate_without_an_activator_and_outside_of_an_event_does_nothing)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnLoad) + command(OpActivate, arguments(0)) + end());
        setup.addScript(scriptOfCrates, builder);
        setup.mObjects.add(crate, scriptOfCrates);

        setup.mObjects.setLoaded(crate, true);

        EXPECT_THAT(setup.mWorld.mActivations, IsEmpty());
    }

    TEST(FalloutScriptObjectsTest, every_loaded_reference_is_listed_whatever_blocks_its_script_has)
    {
        Runner setup;
        ScriptBuilder onLoadOnly;
        onLoadOnly.code(scriptName() + begin(BlockType::OnLoad) + end());
        setup.addScript(scriptOfCrates + 1, onLoadOnly);
        setup.addCountingScript();
        setup.mObjects.add(crate, scriptOfCrates);
        setup.mObjects.add(otherCrate, scriptOfCrates + 1);

        setup.mObjects.setLoaded(crate, true);
        setup.mObjects.setLoaded(otherCrate, true);

        EXPECT_THAT(setup.mObjects.loadedReferences(), UnorderedElementsAre(crate, otherCrate));
        EXPECT_THAT(setup.mObjects.updating(), ElementsAre(crate));

        setup.mObjects.setLoaded(otherCrate, false);
        EXPECT_THAT(setup.mObjects.loadedReferences(), ElementsAre(crate));
        setup.mObjects.clear();
        EXPECT_THAT(setup.mObjects.loadedReferences(), IsEmpty());
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
