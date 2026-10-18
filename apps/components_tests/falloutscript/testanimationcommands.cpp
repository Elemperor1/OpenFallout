#include "scriptbuilder.hpp"

#include <components/falloutscript/animationcommands.hpp>
#include <components/falloutscript/animgroups.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
    using FalloutScript::Game;
    using FalloutScript::ObjectScripts;
    namespace OpenState = FalloutScript::OpenState;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId gate = 0x01000100;
    constexpr FormId lever = 0x01000200;
    constexpr FormId scriptOfGate = 0x01002000;

    constexpr std::uint32_t integerVariable = 1;

    constexpr std::uint16_t OpPlayGroup = 0x1013;
    constexpr std::uint16_t OpGetOpenState = 0x109D;
    constexpr std::uint16_t OpSetOpenState = 0x10DD;
    constexpr std::uint16_t OpIsAnimPlaying = 0x1128;

    constexpr int left = 5;
    constexpr int forward = 3;

    class FakeAnimations : public FalloutScript::AnimationWorld
    {
    public:
        std::map<FormId, std::set<std::string>> mGroups;
        std::set<std::pair<FormId, std::string>> mPlaying;
        std::vector<std::pair<FormId, std::string>> mPlayed;
        std::vector<bool> mRestarted;

        bool hasGroup(FormId reference, std::string_view group) override
        {
            const auto found = mGroups.find(reference);
            return found != mGroups.end() && found->second.count(std::string(group)) != 0;
        }

        bool playGroup(FormId reference, std::string_view group, bool restart) override
        {
            if (!hasGroup(reference, group))
                return false;
            mRestarted.push_back(restart);
            mPlayed.emplace_back(reference, std::string(group));
            mPlaying.emplace(reference, std::string(group));
            return true;
        }

        bool isGroupPlaying(FormId reference, std::string_view group) override
        {
            for (const auto& [owner, name] : mPlaying)
                if (owner == reference && (group.empty() || name == group))
                    return true;
            return false;
        }

        void finish(FormId reference, const std::string& group) { mPlaying.erase({ reference, group }); }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        FakeAnimations mAnimations;
        FalloutScript::OpenStates mStates;

        explicit Runner(Game game = Game::NewVegas)
        {
            FalloutScript::addAnimationCommands(mInterpreter, mStates, mAnimations, game);
        }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGate];
            record.mId = ESM::FormId::fromUint32(scriptOfGate);
            record.mScript = builder.definition();
            mObjects.add(gate, scriptOfGate);
            mObjects.trigger(gate, BlockType::OnActivate);
        }

        double variable(std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(gate);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    // The group is written bare in two bytes, and the flag after it is a number
    Bytes group(int number, int flag)
    {
        return arguments(2).u16(static_cast<std::uint16_t>(number)).add(intArg(flag));
    }

    Bytes groupOnly(int number)
    {
        return arguments(1).u16(static_cast<std::uint16_t>(number));
    }

    Bytes flag(int value)
    {
        return arguments(1).add(intArg(value));
    }

    TEST(FalloutScriptAnimationGroupsTest, names_the_groups_that_scripts_call_by_number)
    {
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 0), "Idle");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::Fallout3, 3), "Forward");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 5), "Left");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::Fallout3, 25), "Unequip");
    }

    TEST(FalloutScriptAnimationGroupsTest, has_the_table_of_each_game_whose_numbers_differ_from_97_on)
    {
        // Read from the executables: 183 groups in Fallout 3, 245 in New Vegas
        EXPECT_EQ(FalloutScript::animationGroupName(Game::Fallout3, 97), "PlaceMine");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 97), "AttackCustom1Power");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 102), "PlaceMine");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::Fallout3, 142), "ReloadA");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::Fallout3, 147), "ReloadF");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 177), "ReloadA");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::Fallout3, 165), "Death");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 224), "Death");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::Fallout3, 182), "JumpLandRight");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 244), "JumpLandRight");
        EXPECT_EQ(FalloutScript::animationGroupName(Game::NewVegas, 169), "stomp");
    }

    TEST(FalloutScriptAnimationGroupsTest, has_nothing_for_a_number_it_does_not_know)
    {
        EXPECT_TRUE(FalloutScript::animationGroupName(Game::NewVegas, -1).empty());
        EXPECT_TRUE(FalloutScript::animationGroupName(Game::Fallout3, 183).empty());
        EXPECT_TRUE(FalloutScript::animationGroupName(Game::NewVegas, 245).empty());
        EXPECT_TRUE(FalloutScript::animationGroupName(Game::Fallout3, 5000).empty());
    }

    TEST(FalloutScriptAnimationGroupsTest, a_variant_has_the_name_and_an_underscore_before_what_it_adds)
    {
        EXPECT_TRUE(FalloutScript::isGroupVariant("Forward_A", "Forward"));
        EXPECT_TRUE(FalloutScript::isGroupVariant("forward_a", "Forward"));
        EXPECT_TRUE(FalloutScript::isGroupVariant("SpecialIdle_Hit", "specialidle"));
        EXPECT_FALSE(FalloutScript::isGroupVariant("Forward", "Forward"));
        EXPECT_FALSE(FalloutScript::isGroupVariant("Forward_", "Forward"));
        EXPECT_FALSE(FalloutScript::isGroupVariant("FastForward_A", "Forward"));
        EXPECT_FALSE(FalloutScript::isGroupVariant("Forwards", "Forward"));
    }

    TEST(FalloutScriptAnimationCommandsTest, a_script_plays_a_group_on_its_object)
    {
        Runner setup;
        setup.mAnimations.mGroups[gate] = { "Left" };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpPlayGroup, group(left, 0))
            + setTo(var('s', 1), callToken(OpIsAnimPlaying, groupOnly(left)))
            + setTo(var('s', 2), callToken(OpIsAnimPlaying, groupOnly(forward)))
            + setTo(var('s', 3), callToken(OpIsAnimPlaying, arguments(0))) + end());
        builder.variable(1, integerVariable, "playing")
            .variable(2, integerVariable, "other")
            .variable(3, integerVariable, "any");

        setup.run(builder);

        EXPECT_THAT(setup.mAnimations.mPlayed, ElementsAre(Pair(gate, "Left")));
        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
        EXPECT_EQ(setup.variable(3), 1);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptAnimationCommandsTest, a_script_plays_a_group_on_another_object)
    {
        Runner setup;
        setup.mAnimations.mGroups[lever] = { "Forward" };
        ScriptBuilder builder;
        const std::uint16_t other = builder.form(lever);
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpPlayGroup, group(forward, 1), other)
            + setTo(var('s', 1), callToken(OpIsAnimPlaying, groupOnly(forward), other))
            + setTo(var('s', 2), callToken(OpIsAnimPlaying, groupOnly(forward))) + end());
        builder.variable(1, integerVariable, "other plays").variable(2, integerVariable, "own plays");

        setup.run(builder);

        EXPECT_THAT(setup.mAnimations.mPlayed, ElementsAre(Pair(lever, "Forward")));
        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
    }

    TEST(FalloutScriptAnimationCommandsTest, the_flag_of_a_group_starts_it_over)
    {
        Runner setup;
        setup.mAnimations.mGroups[gate] = { "Left", "Forward" };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpPlayGroup, group(left, 0))
            + command(OpPlayGroup, group(forward, 1)) + command(OpPlayGroup, group(left, 5)) + end());

        setup.run(builder);

        EXPECT_THAT(setup.mAnimations.mRestarted, ElementsAre(false, true, true));
    }

    TEST(FalloutScriptAnimationCommandsTest, a_group_the_table_does_not_have_plays_nothing)
    {
        Runner setup;
        setup.mAnimations.mGroups[gate] = { "Left" };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpPlayGroup, group(9999, 0))
            + setTo(var('s', 1), callToken(OpIsAnimPlaying, groupOnly(9999))) + end());
        builder.variable(1, integerVariable, "playing");

        setup.run(builder);

        EXPECT_THAT(setup.mAnimations.mPlayed, IsEmpty());
        EXPECT_EQ(setup.variable(1), 0);
    }

    TEST(FalloutScriptAnimationCommandsTest, the_reloads_of_fallout_3_are_groups_there)
    {
        Runner setup(Game::Fallout3);
        setup.mAnimations.mGroups[gate] = { "ReloadB" };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpPlayGroup, group(143, 0)) + end());

        setup.run(builder);

        EXPECT_THAT(setup.mAnimations.mPlayed, ElementsAre(Pair(gate, "ReloadB")));
    }

    TEST(FalloutScriptOpenStatesTest, an_object_is_closed_until_something_opens_it)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open", "Close" };

        EXPECT_EQ(states.stateOf(gate, animations), OpenState::closed);
        EXPECT_FALSE(states.restsOpen(gate));
    }

    TEST(FalloutScriptOpenStatesTest, an_object_that_was_given_a_state_keeps_it)
    {
        FalloutScript::OpenStates states;
        EXPECT_FALSE(states.knows(gate));

        states.rest(gate, true);
        EXPECT_TRUE(states.knows(gate));
        EXPECT_TRUE(states.restsOpen(gate));

        // A door that was open by its record and was then closed is closed, not open again when it is loaded
        states.rest(gate, false);
        EXPECT_TRUE(states.knows(gate));
        EXPECT_FALSE(states.restsOpen(gate));
    }

    TEST(FalloutScriptOpenStatesTest, opening_an_object_takes_it_through_opening_to_open)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open", "Close" };

        EXPECT_TRUE(states.request(gate, true, animations));
        EXPECT_THAT(animations.mPlayed, ElementsAre(Pair(gate, "Open")));
        EXPECT_EQ(states.stateOf(gate, animations), OpenState::opening);

        animations.finish(gate, "Open");
        EXPECT_EQ(states.stateOf(gate, animations), OpenState::open);
    }

    TEST(FalloutScriptOpenStatesTest, closing_an_object_takes_it_through_closing_to_closed)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open", "Close" };
        states.rest(gate, true);

        EXPECT_TRUE(states.request(gate, false, animations));
        EXPECT_THAT(animations.mPlayed, ElementsAre(Pair(gate, "Close")));
        EXPECT_EQ(states.stateOf(gate, animations), OpenState::closing);

        animations.finish(gate, "Close");
        EXPECT_EQ(states.stateOf(gate, animations), OpenState::closed);
    }

    TEST(FalloutScriptOpenStatesTest, an_object_that_is_open_or_opening_is_not_opened_again)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open", "Close" };

        EXPECT_TRUE(states.request(gate, true, animations));
        EXPECT_FALSE(states.request(gate, true, animations));
        animations.finish(gate, "Open");
        EXPECT_FALSE(states.request(gate, true, animations));
        EXPECT_THAT(animations.mPlayed, ElementsAre(Pair(gate, "Open")));
    }

    TEST(FalloutScriptOpenStatesTest, an_object_that_is_closed_or_closing_is_not_closed_again)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open", "Close" };

        EXPECT_FALSE(states.request(gate, false, animations));
        states.rest(gate, true);
        EXPECT_TRUE(states.request(gate, false, animations));
        EXPECT_FALSE(states.request(gate, false, animations));
        EXPECT_THAT(animations.mPlayed, ElementsAre(Pair(gate, "Close")));
    }

    TEST(FalloutScriptOpenStatesTest, an_object_without_the_animations_only_rests)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        // A switch that has an animation of its own and none of opening and closing
        animations.mGroups[lever] = { "Forward", "Left" };

        EXPECT_EQ(states.stateOf(lever, animations), OpenState::closed);
        EXPECT_TRUE(states.request(lever, true, animations));
        EXPECT_EQ(states.stateOf(lever, animations), OpenState::open);
        EXPECT_THAT(animations.mPlayed, IsEmpty());
    }

    TEST(FalloutScriptOpenStatesTest, an_object_with_only_one_of_the_animations_only_rests)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open" };

        EXPECT_TRUE(states.request(gate, true, animations));
        EXPECT_EQ(states.stateOf(gate, animations), OpenState::open);
    }

    TEST(FalloutScriptOpenStatesTest, activating_an_object_opens_it_and_activating_it_again_closes_it)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open", "Close" };

        EXPECT_TRUE(states.toggle(gate, animations));
        animations.finish(gate, "Open");
        EXPECT_FALSE(states.toggle(gate, animations));
        animations.finish(gate, "Close");
        EXPECT_TRUE(states.toggle(gate, animations));
        EXPECT_THAT(animations.mPlayed, ElementsAre(Pair(gate, "Open"), Pair(gate, "Close"), Pair(gate, "Open")));
    }

    TEST(FalloutScriptOpenStatesTest, an_object_that_rests_open_is_open_without_a_word_from_a_script)
    {
        FakeAnimations animations;
        FalloutScript::OpenStates states;
        animations.mGroups[gate] = { "Open", "Close" };

        states.rest(gate, true);

        EXPECT_EQ(states.stateOf(gate, animations), OpenState::open);
        EXPECT_THAT(animations.mPlayed, IsEmpty());
    }

    TEST(FalloutScriptAnimationCommandsTest, a_script_opens_a_door_and_reads_its_state)
    {
        Runner setup;
        setup.mAnimations.mGroups[gate] = { "Open", "Close" };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetOpenState, arguments(0))) + command(OpSetOpenState, flag(1))
            + setTo(var('s', 2), callToken(OpGetOpenState, arguments(0))) + command(OpSetOpenState, flag(1)) + end());
        builder.variable(1, integerVariable, "before").variable(2, integerVariable, "during");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), OpenState::closed);
        EXPECT_EQ(setup.variable(2), OpenState::opening);
        EXPECT_THAT(setup.mAnimations.mPlayed, ElementsAre(Pair(gate, "Open")));

        setup.mAnimations.finish(gate, "Open");
        EXPECT_EQ(setup.mStates.stateOf(gate, setup.mAnimations), OpenState::open);
    }

    TEST(FalloutScriptAnimationCommandsTest, a_script_closes_what_is_open)
    {
        Runner setup;
        setup.mAnimations.mGroups[gate] = { "Open", "Close" };
        setup.mStates.rest(gate, true);
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetOpenState, arguments(0))) + command(OpSetOpenState, flag(0))
            + setTo(var('s', 2), callToken(OpGetOpenState, arguments(0))) + end());
        builder.variable(1, integerVariable, "before").variable(2, integerVariable, "during");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), OpenState::open);
        EXPECT_EQ(setup.variable(2), OpenState::closing);
    }

    // The switches of the reactor room (CG04) have no Open and no Close, and their script tests for a closed state
    TEST(FalloutScriptAnimationCommandsTest, a_switch_without_open_and_close_reads_as_closed)
    {
        Runner setup;
        setup.mAnimations.mGroups[gate] = { "Forward", "Left", "Backward", "SpecialIdle" };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetOpenState, arguments(0))) + command(OpPlayGroup, group(left, 0))
            + setTo(var('s', 2), callToken(OpGetOpenState, arguments(0))) + end());
        builder.variable(1, integerVariable, "before").variable(2, integerVariable, "after");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), OpenState::closed);
        EXPECT_EQ(setup.variable(2), OpenState::closed);
    }
}
