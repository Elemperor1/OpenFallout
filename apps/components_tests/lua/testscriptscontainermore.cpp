#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include <components/esm/luascripts.hpp>

#include <components/lua/luastate.hpp>
#include <components/lua/scriptscontainer.hpp>
#include <components/lua/serialization.hpp>
#include <components/lua/util.hpp>

#include <components/testing/util.hpp>

// The parts of the scripts container that the first tests do not reach: the interfaces that scripts override, the
// handlers that fail, the saved data that cannot be read, the timers of every kind and the use of resources
namespace
{
    using namespace testing;
    using namespace TestingOpenMW;

    constexpr VFS::Path::NormalizedView readerPath("reader.lua");

    VFSTestFile readerScript(R"X(
local interfaces = require('openfallout.interfaces')
return {
    eventHandlers = {
        Who = function()
            local chain = interfaces.Chain
            print('chain is', chain and chain.name or 'none')
        end,
    },
}
)X");

    constexpr VFS::Path::NormalizedView iface0Path("iface0.lua");
    constexpr VFS::Path::NormalizedView iface1Path("iface1.lua");
    constexpr VFS::Path::NormalizedView iface2Path("iface2.lua");
    constexpr VFS::Path::NormalizedView iface3Path("iface3.lua");

    VFSTestFile iface0Script(R"X(
local interface = { name = 'i0' }
return {
    interfaceName = 'Chain',
    interface = interface,
    engineHandlers = {
        onInterfaceOverride = function(old) print('i0 overrides', old and old.name) end,
    },
}
)X");

    // has no handler for the override
    VFSTestFile iface1Script(R"X(
local interface = { name = 'i1' }
return {
    interfaceName = 'Chain',
    interface = interface,
}
)X");

    // its handler fails
    VFSTestFile iface2Script(R"X(
local interface = { name = 'i2' }
return {
    interfaceName = 'Chain',
    interface = interface,
    engineHandlers = {
        onInterfaceOverride = function(old) error('nope') end,
    },
}
)X");

    VFSTestFile iface3Script(R"X(
local interface = { name = 'i3' }
return {
    interfaceName = 'Chain',
    interface = interface,
    engineHandlers = {
        onInterfaceOverride = function(old) print('i3 overrides', old and old.name) end,
    },
}
)X");

    constexpr VFS::Path::NormalizedView nameOnlyPath("nameonly.lua");
    constexpr VFS::Path::NormalizedView tableOnlyPath("tableonly.lua");

    VFSTestFile nameOnlyScript("return { interfaceName = 'Orphan' }");
    VFSTestFile tableOnlyScript("return { interface = { value = 1 } }");

    constexpr VFS::Path::NormalizedView failingPath("failing.lua");

    VFSTestFile failingScript(R"X(
return {
    engineHandlers = {
        onInit = function() error('init boom') end,
        onSave = function() error('save boom') end,
        onLoad = function() error('load boom') end,
    },
    eventHandlers = {
        Boom = function() error('event boom') end,
        Fine = function(data) print('fine', data and data.x) end,
    },
}
)X");

    constexpr VFS::Path::NormalizedView invalidPath("invalid.lua");

    VFSTestFile invalidScript("not a script");

    constexpr VFS::Path::NormalizedView emptyPath("empty.lua");

    VFSTestFile emptyScript("");

    constexpr VFS::Path::NormalizedView autoStartPath("autostart.lua");

    VFSTestFile autoStartScript(R"X(
return {
    engineHandlers = {
        onInit = function() print('autostart init') end,
    },
}
)X");

    constexpr VFS::Path::NormalizedView workerPath("worker.lua");

    VFSTestFile workerScript(R"X(
return {
    eventHandlers = {
        -- fills a table of about 20000 values, which takes about 60000 instructions
        Work = function()
            held = {}
            for i = 1, 20000 do
                held[i] = i
            end
        end,
        -- long enough to pass the limit, and short enough to end if the limit does not work
        Spin = function()
            local i = 0
            while i < 10000000 do
                i = i + 1
            end
        end,
        Hog = function()
            held = string.rep('x', 128 * 1024 * 1024)
        end,
    },
}
)X");

    struct LuaScriptsContainerMoreTest : Test
    {
        std::unique_ptr<VFS::Manager> mVFS = createTestVFS({
            { readerPath, &readerScript },
            { iface0Path, &iface0Script },
            { iface1Path, &iface1Script },
            { iface2Path, &iface2Script },
            { iface3Path, &iface3Script },
            { nameOnlyPath, &nameOnlyScript },
            { tableOnlyPath, &tableOnlyScript },
            { failingPath, &failingScript },
            { invalidPath, &invalidScript },
            { emptyPath, &emptyScript },
            { autoStartPath, &autoStartScript },
            { workerPath, &workerScript },
        });

        LuaUtil::ScriptsConfiguration mCfg;
        LuaUtil::LuaState mLua{ mVFS.get(), &mCfg };
        // a state that limits the instructions of a call and the memory, and tracks every allocation over 1 KiB
        LuaUtil::LuaState mLimitedLua{ mVFS.get(), &mCfg,
            LuaUtil::LuaStateSettings{ 200000, 64 * 1024 * 1024, 1024, true } };

        LuaScriptsContainerMoreTest()
        {
            // the instructions of the code that LuaJIT compiles are not counted
            sol::state_view limited = mLimitedLua.unsafeState();
            limited.safe_script("if jit then jit.off() end");

            ESM::LuaScriptsCfg cfg;
            LuaUtil::parseOMWScripts(cfg, R"X(
CUSTOM: reader.lua
CUSTOM: iface0.lua
CUSTOM: iface1.lua
CUSTOM: iface2.lua
CUSTOM: iface3.lua
CUSTOM: nameonly.lua
CUSTOM: tableonly.lua
CUSTOM: failing.lua
CUSTOM: invalid.lua
CUSTOM: empty.lua
CUSTOM, PLAYER: autostart.lua
CUSTOM: worker.lua
)X");
            mCfg.init(std::move(cfg), false);
        }

        int getId(VFS::Path::NormalizedView path) const
        {
            const std::optional<int> id = mCfg.findId(path);
            if (!id.has_value())
                throw std::invalid_argument("Script id is not found: " + std::string(path.value()));
            return *id;
        }

        static std::string who(LuaUtil::ScriptsContainer& scripts)
        {
            testing::internal::CaptureStdout();
            scripts.receiveEvent("Who", "");
            return testing::internal::GetCapturedStdout();
        }

        static std::string receive(LuaUtil::ScriptsContainer& scripts, std::string_view name, std::string_view data)
        {
            testing::internal::CaptureStdout();
            scripts.receiveEvent(name, data);
            return testing::internal::GetCapturedStdout();
        }
    };

    TEST_F(LuaScriptsContainerMoreTest, publishesTheLastOfTheScriptsThatOverrideAnInterface)
    {
        LuaUtil::ScriptsContainer scripts(&mLua, "Test");
        const int id0 = getId(iface0Path);
        const int id1 = getId(iface1Path);
        const int id2 = getId(iface2Path);
        const int id3 = getId(iface3Path);

        testing::internal::CaptureStdout();
        EXPECT_TRUE(scripts.addCustomScript(getId(readerPath)));
        EXPECT_TRUE(scripts.addCustomScript(id1));
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "");
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti1\n");

        // a script before one that has no handler for the override is not told of anything
        testing::internal::CaptureStdout();
        EXPECT_TRUE(scripts.addCustomScript(id0));
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "");
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti1\n");

        // a script after the others is told of the one before it, and its interface is the one that is published
        testing::internal::CaptureStdout();
        EXPECT_TRUE(scripts.addCustomScript(id3));
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "Test[iface3.lua]:\ti3 overrides\ti1\n");
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti3\n");

        // a script between two others is told of the one before it, and tells the one after it. Both can fail
        testing::internal::CaptureStdout();
        EXPECT_TRUE(scripts.addCustomScript(id2));
        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_THAT(output, HasSubstr("Test[iface2.lua] onInterfaceOverride failed: "));
        EXPECT_THAT(output, HasSubstr("nope"));
        EXPECT_THAT(output, HasSubstr("Test[iface3.lua]:\ti3 overrides\ti2\n"));
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti3\n");

        // the interface of the one before is published again when the last script goes away
        scripts.removeScript(id3);
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti2\n");

        // the script after the one that goes away is told of the one before it, and that can fail too
        testing::internal::CaptureStdout();
        scripts.removeScript(id1);
        output = testing::internal::GetCapturedStdout();
        EXPECT_THAT(output, HasSubstr("Test[iface2.lua] onInterfaceOverride failed: "));
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti2\n");

        scripts.removeScript(id2);
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti0\n");

        // the interface is gone when the last script that has it goes away
        scripts.removeScript(id0);
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\tnone\n");

        // the script after the one that goes away is told that there is nothing before it
        testing::internal::CaptureStdout();
        EXPECT_TRUE(scripts.addCustomScript(id3));
        EXPECT_TRUE(scripts.addCustomScript(id0));
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "Test[iface3.lua]:\ti3 overrides\ti0\n");
        testing::internal::CaptureStdout();
        scripts.removeScript(id0);
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "Test[iface3.lua]:\ti3 overrides\tnil\n");
        EXPECT_EQ(who(scripts), "Test[reader.lua]:\tchain is\ti3\n");
    }

    TEST_F(LuaScriptsContainerMoreTest, reportsAnInterfaceThatHasNoNameAndANameThatHasNoInterface)
    {
        LuaUtil::ScriptsContainer scripts(&mLua, "Test");

        testing::internal::CaptureStdout();
        EXPECT_TRUE(scripts.addCustomScript(getId(nameOnlyPath)));
        EXPECT_TRUE(scripts.addCustomScript(getId(tableOnlyPath)));
        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_THAT(
            output, HasSubstr("Test[nameonly.lua]: 'interfaceName' should always be used together with 'interface'"));
        EXPECT_THAT(
            output, HasSubstr("Test[tableonly.lua]: 'interfaceName' should always be used together with 'interface'"));
        EXPECT_TRUE(scripts.hasScript(getId(nameOnlyPath)));
        EXPECT_TRUE(scripts.hasScript(getId(tableOnlyPath)));
        // the scripts can go away like any other
        scripts.removeScript(getId(nameOnlyPath));
        scripts.removeScript(getId(tableOnlyPath));
        EXPECT_FALSE(scripts.hasScript(getId(nameOnlyPath)));
    }

    TEST_F(LuaScriptsContainerMoreTest, survivesHandlersThatFail)
    {
        LuaUtil::ScriptsContainer scripts(&mLua, "Test");
        sol::state_view sol = mLua.unsafeState();
        const int failingId = getId(failingPath);

        testing::internal::CaptureStdout();
        EXPECT_TRUE(scripts.addCustomScript(failingId));
        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_THAT(output, HasSubstr("Test[failing.lua] onInit failed: "));
        EXPECT_THAT(output, HasSubstr("init boom"));

        output = receive(scripts, "Boom", "");
        EXPECT_THAT(output, HasSubstr("Test[failing.lua] eventHandler[Boom] failed. "));
        EXPECT_THAT(output, HasSubstr("event boom"));

        // data that is not Lua data in the serialized format
        output = receive(scripts, "Fine", "\x7f");
        EXPECT_THAT(output, HasSubstr("Test can not parse eventData for 'Fine': "));
        EXPECT_THAT(output, HasSubstr("Incorrect version of Lua serialization format: 127"));

        ESM::LuaScripts data;
        testing::internal::CaptureStdout();
        scripts.save(data);
        output = testing::internal::GetCapturedStdout();
        EXPECT_THAT(output, HasSubstr("Test[failing.lua] onSave failed: "));
        EXPECT_THAT(output, HasSubstr("save boom"));
        ASSERT_EQ(data.mScripts.size(), 1u);
        EXPECT_TRUE(data.mScripts[0].mData.empty());

        // an unloaded container saves what it has loaded
        scripts.load(data);
        ESM::LuaScripts unloadedSave;
        scripts.save(unloadedSave);
        ASSERT_EQ(unloadedSave.mScripts.size(), 1u);
        EXPECT_EQ(unloadedSave.mScripts[0].mScriptId, failingId);

        // the script that is loaded again when an event comes tells that it can not load
        output = receive(scripts, "Fine", LuaUtil::serialize(sol.create_table_with("x", 1)));
        EXPECT_THAT(output, HasSubstr("Test[failing.lua] onLoad failed: "));
        EXPECT_THAT(output, HasSubstr("load boom"));
        EXPECT_THAT(output, HasSubstr("Test[failing.lua]:\tfine\t1\n"));
    }

    TEST_F(LuaScriptsContainerMoreTest, dropsSavedDataAndTimersThatCanNotBeRead)
    {
        LuaUtil::ScriptsContainer scripts(&mLua, "Test");
        const int failingId = getId(failingPath);

        ESM::LuaScripts data;
        // a script that is not registered
        data.mScripts.push_back(ESM::LuaScript{ -1, "", {} });
        // a script with saved data that is not in the serialized format, and a timer with an argument that is not
        ESM::LuaScript failing;
        failing.mScriptId = failingId;
        failing.mData = "\x7f";
        failing.mTimers.push_back(
            ESM::LuaTimer{ ESM::LuaTimer::Type::SIMULATION_TIME, 1.0, "cb", std::string("\x7f") });
        failing.mTimers.push_back(ESM::LuaTimer{ ESM::LuaTimer::Type::GAME_TIME, 2.0, "cb", std::string() });
        data.mScripts.push_back(failing);
        // a script that does not start
        data.mScripts.push_back(ESM::LuaScript{ getId(invalidPath), "", {} });

        testing::internal::CaptureStdout();
        scripts.load(data);
        std::string output = testing::internal::GetCapturedStdout();
        EXPECT_THAT(output, HasSubstr("Test[failing.lua] onLoad failed: "));
        EXPECT_THAT(output, HasSubstr("Incorrect version of Lua serialization format: 127"));
        EXPECT_THAT(output, HasSubstr("Test[failing.lua] can not load timer: "));

        // the script that is not registered is gone, the saved data that could not be read is gone, and so is
        // the timer that could not be read
        ESM::LuaScripts saved;
        scripts.save(saved);
        ASSERT_EQ(saved.mScripts.size(), 2u);
        EXPECT_EQ(saved.mScripts[0].mScriptId, failingId);
        EXPECT_TRUE(saved.mScripts[0].mData.empty());
        ASSERT_EQ(saved.mScripts[0].mTimers.size(), 1u);
        EXPECT_EQ(saved.mScripts[0].mTimers[0].mType, ESM::LuaTimer::Type::GAME_TIME);

        // the other script does not start when the container loads its scripts
        output = receive(scripts, "Nothing", "");
        EXPECT_THAT(output, HasSubstr("Can't start Test[invalid.lua]"));
        EXPECT_THAT(output, HasSubstr("Test[failing.lua] onLoad failed: "));
        EXPECT_TRUE(scripts.hasScript(failingId));
        EXPECT_FALSE(scripts.hasScript(getId(invalidPath)));
    }

    TEST_F(LuaScriptsContainerMoreTest, savesAndLoadsTimersOfEveryKind)
    {
        using TimerType = LuaUtil::ScriptsContainer::TimerType;
        LuaUtil::ScriptsContainer scripts(&mLua, "Test");
        const int emptyId = getId(emptyPath);
        sol::state_view sol = mLua.unsafeState();

        int sum = 0;
        sol::function callback = sol::make_object(sol, [&](int d) { sum += d; });

        EXPECT_TRUE(scripts.addCustomScript(emptyId));
        scripts.registerTimerCallback(emptyId, "cb", callback);
        const double now = LuaUtil::getRealTime();
        scripts.setupSerializableTimer(TimerType::SIMULATION_TIME, 20, emptyId, "cb", sol::make_object(sol, 1));
        scripts.setupSerializableTimer(TimerType::GAME_TIME, 50, emptyId, "cb", sol::make_object(sol, 2));
        scripts.setupSerializableTimer(TimerType::REAL_TIME, now + 100, emptyId, "cb", sol::make_object(sol, 4));
        // a timer of the real time that is already due is saved as one that is due at once
        scripts.setupSerializableTimer(TimerType::REAL_TIME, now - 100, emptyId, "cb", sol::make_object(sol, 8));
        // the timers that can not be saved are not saved
        scripts.setupUnsavableTimer(TimerType::GAME_TIME, 1, emptyId, callback);

        ESM::LuaScripts data;
        scripts.save(data);
        ASSERT_EQ(data.mScripts.size(), 1u);
        const std::vector<ESM::LuaTimer>& timers = data.mScripts[0].mTimers;
        ASSERT_EQ(timers.size(), 4u);
        int simulation = 0;
        int game = 0;
        std::vector<double> realTimes;
        for (const ESM::LuaTimer& timer : timers)
        {
            EXPECT_EQ(timer.mCallbackName, "cb");
            switch (timer.mType)
            {
                case ESM::LuaTimer::Type::SIMULATION_TIME:
                    ++simulation;
                    EXPECT_EQ(timer.mTime, 20.0);
                    break;
                case ESM::LuaTimer::Type::GAME_TIME:
                    ++game;
                    EXPECT_EQ(timer.mTime, 50.0);
                    break;
                case ESM::LuaTimer::Type::REAL_TIME:
                    realTimes.push_back(timer.mTime);
                    break;
            }
        }
        EXPECT_EQ(simulation, 1);
        EXPECT_EQ(game, 1);
        ASSERT_EQ(realTimes.size(), 2u);
        std::sort(realTimes.begin(), realTimes.end());
        EXPECT_EQ(realTimes[0], 0.0);
        EXPECT_NEAR(realTimes[1], 100.0, 10.0);

        // another container gets the timers, and the real time that is left is counted from the time of the load
        LuaUtil::ScriptsContainer restored(&mLua, "Test");
        restored.load(data);
        restored.registerTimerCallback(emptyId, "cb", callback);
        restored.processTimers(0, 0, LuaUtil::getRealTime() + 10);
        EXPECT_EQ(sum, 8);
        restored.processTimers(20, 0, LuaUtil::getRealTime() + 10);
        EXPECT_EQ(sum, 9);
        restored.processTimers(20, 50, LuaUtil::getRealTime() + 10);
        EXPECT_EQ(sum, 11);
        restored.processTimers(20, 50, LuaUtil::getRealTime() + 200);
        EXPECT_EQ(sum, 15);
    }

    TEST_F(LuaScriptsContainerMoreTest, guardsAgainstMisuse)
    {
        LuaUtil::ScriptsContainer scripts(&mLua, "Test");
        sol::state_view sol = mLua.unsafeState();

        // a package must be read-only
        sol::main_object notReadOnly = sol::make_object(sol, 3);
        EXPECT_THROW(scripts.addPackage("bad", notReadOnly), std::logic_error);

        // there is nothing to remove, and nothing to register a callback for
        scripts.removeScript(getId(emptyPath));
        sol::function callback = sol::make_object(sol, [] {});
        EXPECT_THROW(scripts.registerTimerCallback(getId(emptyPath), "cb", callback), std::logic_error);
    }

    TEST_F(LuaScriptsContainerMoreTest, doesNotAddAnAutoStartedScriptAgainWhenTheContainerLoadsIt)
    {
        LuaUtil::ScriptsContainer scripts(&mLua, "Test");
        scripts.setAutoStartConf(mCfg.getPlayerConf());
        scripts.load(ESM::LuaScripts{});

        // the script that the container starts by itself is already there when it loads its scripts
        testing::internal::CaptureStdout();
        EXPECT_FALSE(scripts.addCustomScript(getId(autoStartPath)));
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "Test[autostart.lua]:\tautostart init\n");
        EXPECT_TRUE(scripts.hasScript(getId(autoStartPath)));
    }

    TEST_F(LuaScriptsContainerMoreTest, countsInstructionsAndMemoryOfEveryScript)
    {
        const Debug::Level level = std::exchange(Log::sMinDebugLevel, Debug::All);

        LuaUtil::ScriptsContainer scripts(&mLimitedLua, "Test");
        const int workerId = getId(workerPath);
        EXPECT_TRUE(scripts.addCustomScript(workerId));

        const std::string output = receive(scripts, "Work", "");
        EXPECT_THAT(output, HasSubstr("Test[worker.lua] memory usage 0 -> "));

        std::vector<LuaUtil::ScriptsContainer::ScriptStats> stats;
        scripts.collectStats(stats);
        ASSERT_EQ(stats.size(), mCfg.size());
        const float instructions = stats[workerId].mAvgInstructionCount;
        const int64_t memory = stats[workerId].mMemoryUsage;
        EXPECT_GT(instructions, 100.f);
        EXPECT_GT(memory, 20000);
        EXPECT_EQ(stats[getId(emptyPath)].mMemoryUsage, 0);

        // the average of the instructions goes down with every frame that the script does not run
        scripts.statsNextFrame();
        stats.clear();
        scripts.collectStats(stats);
        const float afterOne = stats[workerId].mAvgInstructionCount;
        EXPECT_LT(afterOne, instructions);
        EXPECT_GT(afterOne, instructions * 0.9f);
        for (int i = 0; i < 3; ++i)
            scripts.statsNextFrame();
        stats.clear();
        scripts.collectStats(stats);
        const float afterFour = stats[workerId].mAvgInstructionCount;
        EXPECT_LT(afterFour, afterOne);
        for (int i = 0; i < 400; ++i)
            scripts.statsNextFrame();
        stats.clear();
        scripts.collectStats(stats);
        EXPECT_EQ(stats[workerId].mAvgInstructionCount, 0.f);
        EXPECT_EQ(stats[workerId].mMemoryUsage, memory);

        // the memory of a script that was removed is still counted, and counted again when the script is added
        scripts.removeScript(workerId);
        stats.clear();
        scripts.collectStats(stats);
        EXPECT_EQ(stats[workerId].mMemoryUsage, memory);
        // what the Lua state frees is taken off the memory of the script that is not there
        lua_gc(mLimitedLua.unsafeState(), LUA_GCCOLLECT, 0);
        stats.clear();
        scripts.collectStats(stats);
        EXPECT_LE(stats[workerId].mMemoryUsage, memory);
        EXPECT_TRUE(scripts.addCustomScript(workerId));
        stats.clear();
        scripts.collectStats(stats);
        EXPECT_LE(stats[workerId].mMemoryUsage, memory);
        EXPECT_GE(stats[workerId].mMemoryUsage, 0);

        Log::sMinDebugLevel = level;
    }

    TEST_F(LuaScriptsContainerMoreTest, stopsScriptsThatUseTooManyInstructionsOrTooMuchMemory)
    {
        LuaUtil::ScriptsContainer scripts(&mLimitedLua, "Test");
        EXPECT_TRUE(scripts.addCustomScript(getId(workerPath)));

        std::string output = receive(scripts, "Spin", "");
        EXPECT_THAT(output, HasSubstr("Test[worker.lua] eventHandler[Spin] failed. "));
        EXPECT_THAT(output, HasSubstr("Lua instruction count exceeded"));

        output = receive(scripts, "Hog", "");
        EXPECT_THAT(output, HasSubstr("is blocked because Lua memory limit"));
        EXPECT_THAT(output, HasSubstr("Test[worker.lua] eventHandler[Hog] failed. "));

        // and the script goes on
        output = receive(scripts, "Work", "");
        EXPECT_THAT(output, Not(HasSubstr("failed")));
    }
}
