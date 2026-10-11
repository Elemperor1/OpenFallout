#include "scriptbuilder.hpp"

#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/referencestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>
#include <utility>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    using FalloutScript::ReferenceState;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId door = 0x01000100;
    constexpr FormId chest = 0x01000101;
    constexpr FormId scriptOfDoors = 0x01002000;

    constexpr std::uint32_t integerVariable = 1;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpGetLocked = 0x1005;
    constexpr std::uint16_t OpGetLockLevel = 0x1041;
    constexpr std::uint16_t OpLock = 0x1072;
    constexpr std::uint16_t OpUnLock = 0x1073;
    constexpr std::uint16_t OpGetDestroyed = 0x10CB;
    constexpr std::uint16_t OpSetDestroyed = 0x10CC;

    class FakeRecords : public FalloutScript::ReferenceRecords
    {
    public:
        std::map<FormId, std::pair<bool, int>> mLocks;
        int mQuestions = 0;

        bool initialLock(FormId reference, bool& locked, int& level) override
        {
            ++mQuestions;
            const auto found = mLocks.find(reference);
            if (found == mLocks.end())
                return false;
            locked = found->second.first;
            level = found->second.second;
            return true;
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
        FakeRecords mRecords;
        ReferenceState mState{ mRecords };

        Runner() { FalloutScript::addReferenceStateCommands(mInterpreter, mState); }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfDoors];
            record.mId = ESM::FormId::fromUint32(scriptOfDoors);
            record.mScript = builder.definition();
            mObjects.add(door, scriptOfDoors);
            mObjects.trigger(door, BlockType::OnActivate);
        }

        double variable(std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(door);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    TEST(FalloutScriptReferenceStateTest, a_reference_is_locked_as_its_record_says_until_a_script_changes_it)
    {
        FakeRecords records;
        records.mLocks[door] = { true, 75 };
        ReferenceState state(records);

        EXPECT_TRUE(state.isLocked(door));
        EXPECT_EQ(state.lockLevel(door), 75);
        state.unlock(door);
        EXPECT_FALSE(state.isLocked(door));
        // The level stays with the lock
        EXPECT_EQ(state.lockLevel(door), 75);
        state.lock(door, -1);
        EXPECT_TRUE(state.isLocked(door));
        EXPECT_EQ(state.lockLevel(door), 75);
    }

    TEST(FalloutScriptReferenceStateTest, a_reference_without_a_lock_is_not_locked)
    {
        FakeRecords records;
        records.mLocks[chest] = { false, 0 };
        ReferenceState state(records);

        EXPECT_FALSE(state.isLocked(chest));
        // A reference that no record has is not locked either
        EXPECT_FALSE(state.isLocked(door));
        EXPECT_EQ(state.lockLevel(door), 0);
    }

    TEST(FalloutScriptReferenceStateTest, a_script_locks_a_reference_at_the_level_it_names)
    {
        FakeRecords records;
        ReferenceState state(records);

        state.lock(chest, 50);
        EXPECT_TRUE(state.isLocked(chest));
        EXPECT_EQ(state.lockLevel(chest), 50);
        state.lock(chest, 0);
        EXPECT_EQ(state.lockLevel(chest), 0);
        // No level: the lock keeps the one it has
        state.lock(chest, -1);
        EXPECT_EQ(state.lockLevel(chest), 0);
    }

    TEST(FalloutScriptReferenceStateTest, what_a_script_did_before_the_records_could_be_read_still_holds_after)
    {
        FakeRecords records;
        ReferenceState state(records);
        // The record is unknown now: the script locks the reference at a level, and unlocks another
        state.lock(door, 30);
        records.mLocks[door] = { false, 99 };
        EXPECT_TRUE(state.isLocked(door));
        EXPECT_EQ(state.lockLevel(door), 30);

        // Only the lock was said: the level is that of the record when it can be read
        state.unlock(chest);
        records.mLocks[chest] = { true, 80 };
        EXPECT_FALSE(state.isLocked(chest));
        EXPECT_EQ(state.lockLevel(chest), 80);
    }

    TEST(FalloutScriptReferenceStateTest, a_reference_is_destroyed_until_a_script_repairs_it)
    {
        FakeRecords records;
        ReferenceState state(records);

        EXPECT_FALSE(state.isDestroyed(door));
        state.setDestroyed(door, true);
        EXPECT_TRUE(state.isDestroyed(door));
        EXPECT_FALSE(state.isDestroyed(chest));
        state.setDestroyed(door, false);
        EXPECT_FALSE(state.isDestroyed(door));
    }

    TEST(FalloutScriptReferenceStateTest, clearing_forgets_what_scripts_did)
    {
        FakeRecords records;
        records.mLocks[door] = { true, 10 };
        ReferenceState state(records);
        state.unlock(door);
        state.setDestroyed(door, true);

        state.clear();

        EXPECT_TRUE(state.isLocked(door));
        EXPECT_FALSE(state.isDestroyed(door));
    }

    TEST(FalloutScriptReferenceCommandsTest, a_script_works_with_the_lock_of_its_object)
    {
        Runner setup;
        setup.mRecords.mLocks[door] = { true, 60 };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetLocked, arguments(0)))
            + setTo(var('s', 2), callToken(OpGetLockLevel, arguments(0))) + command(OpUnLock, arguments(0))
            + setTo(var('s', 3), callToken(OpGetLocked, arguments(0))) + command(OpLock, arguments(1).add(intArg(90)))
            + setTo(var('s', 4), callToken(OpGetLocked, arguments(0)))
            + setTo(var('s', 5), callToken(OpGetLockLevel, arguments(0))) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "level")
            .variable(3, integerVariable, "unlocked")
            .variable(4, integerVariable, "relocked")
            .variable(5, integerVariable, "newLevel");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 60);
        EXPECT_EQ(setup.variable(3), 0);
        EXPECT_EQ(setup.variable(4), 1);
        EXPECT_EQ(setup.variable(5), 90);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptReferenceCommandsTest, a_lock_with_no_level_keeps_the_level_of_the_record)
    {
        Runner setup;
        setup.mRecords.mLocks[door] = { false, 40 };
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpLock, arguments(0))
            + setTo(var('s', 1), callToken(OpGetLocked, arguments(0)))
            + setTo(var('s', 2), callToken(OpGetLockLevel, arguments(0))) + end());
        builder.variable(1, integerVariable, "locked").variable(2, integerVariable, "level");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 40);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptReferenceCommandsTest, a_script_destroys_and_repairs_its_object)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetDestroyed, arguments(0)))
            + command(OpSetDestroyed, arguments(1).add(intArg(1)))
            + setTo(var('s', 2), callToken(OpGetDestroyed, arguments(0)))
            + command(OpSetDestroyed, arguments(1).add(intArg(0)))
            + setTo(var('s', 3), callToken(OpGetDestroyed, arguments(0))) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "destroyed")
            .variable(3, integerVariable, "repaired");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }
}
