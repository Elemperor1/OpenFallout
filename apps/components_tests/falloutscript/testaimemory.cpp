#include <components/falloutscript/aimemory.hpp>

#include <gtest/gtest.h>

namespace
{
    using FalloutScript::AiMemory;
    using FalloutScript::AiState;
    using FalloutScript::FormId;
    using FalloutScript::SaveReader;
    using FalloutScript::SaveWriter;

    constexpr FormId guard = 0x00012345;
    constexpr FormId clerk = 0x00022222;
    constexpr FormId patrol = 0x00033333;
    constexpr FormId greet = 0x00044444;

    AiState stateWithPackages()
    {
        AiState state;
        state.mDone = { patrol, greet };
        state.mPackage = patrol;
        state.mChangeTold = true;
        return state;
    }

    TEST(FalloutScriptAiMemoryTest, a_state_with_nothing_in_it_is_not_kept)
    {
        AiMemory memory;
        memory.set(guard, AiState());
        EXPECT_EQ(memory.size(), 0u);
        memory.set(guard, stateWithPackages());
        EXPECT_EQ(memory.size(), 1u);
        memory.set(guard, AiState());
        EXPECT_FALSE(memory.find(guard).has_value());
        // No reference, no state
        memory.set(0, stateWithPackages());
        EXPECT_EQ(memory.size(), 0u);
    }

    TEST(FalloutScriptAiMemoryTest, the_state_of_a_character_is_given_back_as_it_was_kept)
    {
        AiMemory memory;
        AiState follower;
        follower.mFollows = true;
        follower.mFollowsPlayer = true;
        follower.mFollowDistance = 200.f;
        memory.set(guard, stateWithPackages());
        memory.set(clerk, follower);
        ASSERT_TRUE(memory.find(guard).has_value());
        EXPECT_EQ(*memory.find(guard), stateWithPackages());
        EXPECT_EQ(*memory.find(clerk), follower);
        EXPECT_FALSE(memory.find(patrol).has_value());
        memory.clear();
        EXPECT_EQ(memory.size(), 0u);
    }

    TEST(FalloutScriptAiMemoryTest, a_saved_game_brings_the_states_back)
    {
        AiMemory memory;
        AiState follower;
        follower.mFollows = true;
        follower.mFollowTarget = guard;
        follower.mFollowDistance = 128.f;
        memory.set(guard, stateWithPackages());
        memory.set(clerk, follower);

        SaveWriter out;
        FalloutScript::writeAi(memory, out);
        const std::string bytes = out.take();

        AiMemory loaded;
        SaveReader in(bytes);
        FalloutScript::readAi(loaded, in, FalloutScript::aiSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.size(), 2u);
        EXPECT_EQ(*loaded.find(guard), stateWithPackages());
        EXPECT_EQ(*loaded.find(clerk), follower);
    }

    TEST(FalloutScriptAiMemoryTest, what_belongs_to_a_plugin_that_is_gone_is_left_out)
    {
        AiMemory memory;
        AiState follower;
        follower.mFollows = true;
        follower.mFollowTarget = patrol;
        follower.mDone = { greet, guard };
        follower.mPackage = greet;
        follower.mChangeTold = true;
        memory.set(clerk, follower);
        memory.set(guard, stateWithPackages());

        SaveWriter out;
        FalloutScript::writeAi(memory, out);
        const std::string bytes = out.take();

        // The plugin of the forms 0x00033333 and 0x00044444 (the packages and the one followed) is no longer loaded,
        // and so is that of the character 0x00012345
        const SaveReader::FormMapping mapping = [](FormId form) -> std::optional<FormId> {
            if (form == patrol || form == greet || form == guard)
                return std::nullopt;
            return form;
        };
        AiMemory loaded;
        SaveReader in(bytes, mapping);
        FalloutScript::readAi(loaded, in, FalloutScript::aiSectionVersion);
        // The guard is gone with its plugin, and what it did was only packages of the plugin that is gone
        EXPECT_FALSE(loaded.find(guard).has_value());
        ASSERT_TRUE(loaded.find(clerk).has_value());
        const AiState state = *loaded.find(clerk);
        EXPECT_TRUE(state.mDone.empty());
        EXPECT_EQ(state.mPackage, 0u);
        // It followed somebody who is gone: it follows no one
        EXPECT_FALSE(state.mFollows);
    }

    TEST(FalloutScriptAiMemoryTest, a_saved_game_that_is_cut_short_is_refused)
    {
        AiMemory memory;
        memory.set(guard, stateWithPackages());
        SaveWriter out;
        FalloutScript::writeAi(memory, out);
        std::string bytes = out.take();
        bytes.resize(bytes.size() - 3);

        AiMemory loaded;
        SaveReader in(bytes);
        EXPECT_THROW(FalloutScript::readAi(loaded, in, FalloutScript::aiSectionVersion), FalloutScript::SaveError);
    }
}
