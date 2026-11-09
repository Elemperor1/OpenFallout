#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <components/lua/asyncpackage.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/scriptscontainer.hpp>
#include <components/lua/storage.hpp>
#include <components/testing/util.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    using namespace testing;

    template <typename T>
    T get(sol::state_view& lua, const std::string& luaCode)
    {
        return lua.safe_script("return " + luaCode).get<T>();
    }

    /// Sets up an async package the callbacks of which belong to a script that is there (`async`) and a script that was
    /// removed (`deadAsync`).
    void setUpAsync(sol::state_view& lua)
    {
        // The package is made once, because making it again replaces the type of its values
        const auto initializer = LuaUtil::getAsyncPackageInitializer(
            lua.lua_state(), []() { return 0.0; }, []() { return 0.0; });
        const auto asyncOf = [&](bool scriptIsThere) {
            sol::table hiddenData(lua, sol::create);
            hiddenData[LuaUtil::ScriptsContainer::sScriptIdKey] = LuaUtil::ScriptId{};
            hiddenData[LuaUtil::ScriptsContainer::sScriptDebugNameKey] = "test.lua";
            initializer(hiddenData);
            // The script is removed after its package is made, as it is in the game
            if (!scriptIsThere)
                hiddenData[LuaUtil::ScriptsContainer::sScriptIdKey] = sol::nil;
            return LuaUtil::AsyncPackageId{ nullptr, 0, hiddenData };
        };
        lua["async"] = asyncOf(true);
        lua["deadAsync"] = asyncOf(false);
    }

    struct RecordingListener : LuaUtil::LuaStorage::Listener
    {
        mutable std::vector<std::string> mEvents;

        void valueChanged(std::string_view section, std::string_view key, const sol::object& value) const override
        {
            mEvents.push_back(
                std::string(section) + "." + std::string(key) + (value == sol::nil ? " removed" : " set"));
        }

        void sectionReplaced(std::string_view section, const sol::optional<sol::table>& values) const override
        {
            mEvents.push_back(std::string(section) + (values ? " replaced" : " cleared"));
        }
    };

    TEST(LuaUtilStorageSectionsTest, aListenerIsToldWhatChangesInASection)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            RecordingListener listener;
            storage.setActive(true);
            storage.setListener(&listener);
            lua["section"] = storage.getMutableSection(lua, "options");

            lua.safe_script("section:set('x', 1)");
            lua.safe_script("section:set('x', nil)");
            // A key that is not there is removed without a change of the values
            lua.safe_script("section:set('missing', nil)");
            lua.safe_script("section:reset({ y = 2 })");
            lua.safe_script("section:reset()");

            EXPECT_THAT(listener.mEvents,
                ElementsAre("options.x set", "options.x removed", "options.missing removed", "options replaced",
                    "options cleared"));
            EXPECT_TRUE(get<bool>(lua, "section:get('x') == nil"));
        });
    }

    TEST(LuaUtilStorageSectionsTest, aValueIsReadBackAsACopyOrAsReadOnlyData)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            lua["section"] = storage.getMutableSection(lua, "options");

            lua.safe_script("section:set('list', { 1, 2, 3 })");

            EXPECT_TRUE(get<bool>(lua, "section:get('missing') == nil"));
            EXPECT_TRUE(get<bool>(lua, "section:getCopy('missing') == nil"));
            EXPECT_EQ(get<int>(lua, "#section:get('list')"), 3);
            // The same read-only table is given every time
            EXPECT_TRUE(get<bool>(lua, "section:get('list') == section:get('list')"));
            EXPECT_TRUE(get<bool>(lua, "section:getCopy('list') ~= section:getCopy('list')"));
        });
    }

    TEST(LuaUtilStorageSectionsTest, aReadOnlySectionCannotBeChanged)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            lua["ro"] = storage.getReadOnlySection(lua, "options");

            for (const char* call : { "ro:set('x', 1)", "ro:reset()", "ro:removeOnExit()", "ro:setLifeTime(0)" })
                EXPECT_THROW(lua.safe_script(call), std::exception) << call;
        });
    }

    TEST(LuaUtilStorageSectionsTest, aStorageThatIsNotActiveRefusesAccess)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;

            EXPECT_THROW(storage.getMutableSection(lua, "options"), std::logic_error);
            storage.setActive(true);
            lua["section"] = storage.getMutableSection(lua, "options");
            storage.setActive(false);
            EXPECT_THROW(lua.safe_script("section:get('x')"), std::exception);
        });
    }

    TEST(LuaUtilStorageSectionsTest, aHandlerThatChangesItsOwnSectionDoesNotLoopForever)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            setUpAsync(lua);
            lua["section"] = storage.getMutableSection(lua, "options");
            lua["calls"] = 0;

            lua.safe_script(R"(
                section:subscribe(async:callback(function(name, key)
                    calls = calls + 1
                    section:set('again', true)
                end))
            )");
            lua.safe_script("section:set('x', 1)");

            // The handler ran once, and its own change was refused
            EXPECT_EQ(get<int>(lua, "calls"), 1);
            EXPECT_TRUE(get<bool>(lua, "section:get('again') == nil"));
            EXPECT_EQ(get<int>(lua, "section:get('x')"), 1);
        });
    }

    TEST(LuaUtilStorageSectionsTest, aChainOfHandlersThatChangeEachOtherIsCutShort)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            setUpAsync(lua);

            // Section n sets the value x of section n + 1 when it changes
            lua["sections"] = lua.create_table();
            for (int n = 1; n <= 16; ++n)
                lua["sections"][n] = storage.getMutableSection(lua, "chain" + std::to_string(n));
            lua.safe_script(R"(
                for n = 1, 15 do
                    sections[n]:subscribe(async:callback(function(name, key)
                        sections[n + 1]:set('x', 1)
                    end))
                end
                sections[1]:set('x', 1)
            )");

            // A chain that is too long is cut where the nesting gets too deep
            EXPECT_EQ(get<int>(lua, "sections[11]:get('x')"), 1);
            EXPECT_TRUE(get<bool>(lua, "sections[12]:get('x') == nil"));
        });
    }

    TEST(LuaUtilStorageSectionsTest, theHandlersOfScriptsThatWereRemovedAreDropped)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            setUpAsync(lua);
            lua["section"] = storage.getMutableSection(lua, "options");
            lua["menuSection"] = storage.getMutableSection(lua, "options", true);
            lua["calls"] = 0;

            // Enough of them for the list of handlers to be pruned when it is full
            lua.safe_script(R"(
                for i = 1, 6 do
                    section:subscribe(deadAsync:callback(function() calls = calls + 100 end))
                    menuSection:subscribe(deadAsync:callback(function() calls = calls + 100 end))
                end
                section:subscribe(async:callback(function() calls = calls + 1 end))
                menuSection:subscribe(async:callback(function() calls = calls + 10 end))
                section:set('x', 1)
            )");

            EXPECT_EQ(get<int>(lua, "calls"), 11);
        });
    }

    TEST(LuaUtilStorageSectionsTest, theLifeTimeOfASectionDecidesWhatIsSavedAndWhatIsKept)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            setUpAsync(lua);
            lua["saved"] = storage.getMutableSection(lua, "saved");
            lua["session"] = storage.getMutableSection(lua, "session");
            lua["temporary"] = storage.getMutableSection(lua, "temporary");
            lua["menu"] = storage.getMutableSection(lua, "temporary", true);
            lua["calls"] = 0;
            lua.safe_script("session:setLifeTime(1)");
            lua.safe_script("temporary:setLifeTime(2)");
            lua.safe_script("saved:set('a', 1)");
            lua.safe_script("session:set('b', 2)");
            lua.safe_script("temporary:set('c', 3)");
            lua.safe_script("saved:subscribe(async:callback(function() calls = calls + 1 end))");
            lua.safe_script("menu:subscribe(async:callback(function() calls = calls + 10 end))");

            const std::filesystem::path path = TestingOpenMW::outputFilePath("test_storage_lifetime.bin");
            storage.save(lua, path);
            storage.clearTemporaryAndRemoveCallbacks();

            // The sections that are not temporary stay, the callbacks of scripts do not
            lua["saved"] = storage.getMutableSection(lua, "saved");
            lua["session"] = storage.getMutableSection(lua, "session");
            lua["temporary"] = storage.getMutableSection(lua, "temporary");
            EXPECT_EQ(get<int>(lua, "saved:get('a')"), 1);
            EXPECT_EQ(get<int>(lua, "session:get('b')"), 2);
            EXPECT_TRUE(get<bool>(lua, "temporary:get('c') == nil"));
            lua.safe_script("saved:set('a', 5)");
            EXPECT_EQ(get<int>(lua, "calls"), 0);

            // Only the sections that are saved have their values in the file
            LuaUtil::LuaStorage loaded;
            loaded.setActive(true);
            loaded.load(lua, path);
            lua["saved"] = loaded.getMutableSection(lua, "saved");
            lua["session"] = loaded.getMutableSection(lua, "session");
            EXPECT_EQ(get<int>(lua, "saved:get('a')"), 1);
            EXPECT_TRUE(get<bool>(lua, "session:get('b') == nil"));
        });
    }

    TEST(LuaUtilStorageSectionsTest, aStorageFileThatCannotBeReadLeavesTheStorageEmpty)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);

            const std::filesystem::path empty = TestingOpenMW::outputFilePath("test_storage_empty.bin");
            std::ofstream(empty, std::ios::binary).close();
            const std::filesystem::path missing = TestingOpenMW::outputFilePath("test_storage_missing.bin");

            for (const std::filesystem::path& path : { empty, missing })
            {
                LuaUtil::LuaStorage storage;
                storage.setActive(true);
                EXPECT_NO_THROW(storage.load(lua, path)) << path;
                EXPECT_EQ(storage.getAllSections(lua).size(), 0u) << path;
            }
        });
    }

    TEST(LuaUtilStorageSectionsTest, theSectionsCanBeListedAsReadOnlyOrMutable)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            storage.setActive(true);
            storage.getMutableSection(lua, "first");
            storage.getMutableSection(lua, "second");

            lua["mutableSections"] = storage.getAllSections(lua);
            lua["readOnlySections"] = storage.getAllSections(lua, true);

            EXPECT_NO_THROW(lua.safe_script("mutableSections.first:set('x', 1)"));
            EXPECT_THROW(lua.safe_script("readOnlySections.first:set('x', 2)"), std::exception);
            EXPECT_EQ(get<int>(lua, "readOnlySections.first:get('x')"), 1);
            EXPECT_TRUE(get<bool>(lua, "readOnlySections.second ~= nil"));
        });
    }

    // ----------------------------------------------------------------------------------------------------------------
    // The packages that scripts get

    TEST(LuaUtilStoragePackagesTest, aGlobalScriptHasTheGlobalSectionsAndTheirLifeTimes)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage global;
            global.setActive(true);
            lua["storage"] = LuaUtil::LuaStorage::initGlobalPackage(view, &global);

            lua.safe_script("options = storage.globalSection('options')");
            lua.safe_script("options:set('volume', 7)");
            lua.safe_script("options:setLifeTime(storage.LIFE_TIME.Temporary)");

            EXPECT_EQ(get<int>(lua, "storage.globalSection('options'):get('volume')"), 7);
            EXPECT_EQ(get<int>(lua, "storage.allGlobalSections().options:get('volume')"), 7);
            EXPECT_TRUE(get<bool>(lua, "storage.LIFE_TIME.Persistent ~= storage.LIFE_TIME.GameSession"));
            EXPECT_TRUE(get<bool>(lua, "storage.LIFE_TIME.GameSession ~= storage.LIFE_TIME.Temporary"));
            EXPECT_THROW(lua.safe_script("storage.LIFE_TIME.Other = 1"), std::exception);

            global.clearTemporaryAndRemoveCallbacks();
            EXPECT_EQ(global.getAllSections(lua).size(), 0u);
        });
    }

    TEST(LuaUtilStoragePackagesTest, aLocalScriptHasTheGlobalSectionsToReadOnly)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage global;
            global.setActive(true);
            global.setSingleValue("options", "volume", sol::make_object(lua, 7));
            lua["storage"] = LuaUtil::LuaStorage::initLocalPackage(view, &global);

            EXPECT_EQ(get<int>(lua, "storage.globalSection('options'):get('volume')"), 7);
            EXPECT_THROW(lua.safe_script("storage.globalSection('options'):set('volume', 1)"), std::exception);
            EXPECT_TRUE(get<bool>(lua, "storage.LIFE_TIME ~= nil"));
        });
    }

    TEST(LuaUtilStoragePackagesTest, aPlayerScriptHasThePlayerSectionsToChangeAndTheGlobalOnesToRead)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage global;
            LuaUtil::LuaStorage player;
            global.setActive(true);
            player.setActive(true);
            global.setSingleValue("world", "tide", sol::make_object(lua, 3));
            lua["storage"] = LuaUtil::LuaStorage::initPlayerPackage(view, &global, &player);

            lua.safe_script("storage.playerSection('settings'):set('fov', 90)");

            EXPECT_EQ(get<int>(lua, "storage.playerSection('settings'):get('fov')"), 90);
            EXPECT_EQ(get<int>(lua, "storage.allPlayerSections().settings:get('fov')"), 90);
            EXPECT_EQ(get<int>(lua, "storage.globalSection('world'):get('tide')"), 3);
            EXPECT_THROW(lua.safe_script("storage.globalSection('world'):set('tide', 1)"), std::exception);
            // The same section of the player storage as the one of the menu
            EXPECT_EQ(get<int>(lua, "storage.allPlayerSections().settings:get('fov')"), 90);
        });
    }

    TEST(LuaUtilStoragePackagesTest, aMenuScriptHasThePlayerSectionsAndKeepsItsHandlers)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage global;
            LuaUtil::LuaStorage player;
            global.setActive(true);
            player.setActive(true);
            global.setSingleValue("world", "tide", sol::make_object(lua, 3));
            setUpAsync(lua);
            lua["storage"] = LuaUtil::LuaStorage::initMenuPackage(view, &global, &player);
            lua["calls"] = 0;

            lua.safe_script("settings = storage.playerSection('settings')");
            lua.safe_script("settings:subscribe(async:callback(function() calls = calls + 1 end))");
            lua.safe_script("settings:set('fov', 90)");
            // Starting a game removes the handlers of the scripts of the game, and not the ones of the menu
            player.clearTemporaryAndRemoveCallbacks();
            lua.safe_script("settings:set('fov', 100)");

            EXPECT_EQ(get<int>(lua, "calls"), 2);
            EXPECT_EQ(get<int>(lua, "storage.allPlayerSections().settings:get('fov')"), 100);
            EXPECT_EQ(get<int>(lua, "storage.globalSection('world'):get('tide')"), 3);
            EXPECT_THROW(lua.safe_script("storage.globalSection('world'):set('tide', 1)"), std::exception);
        });
    }

    TEST(LuaUtilStoragePackagesTest, aScriptThatIsLoadingHasThePlayerSectionsToRead)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage player;
            player.setActive(true);
            player.setSingleValue("settings", "fov", sol::make_object(lua, 90));
            lua["storage"] = LuaUtil::LuaStorage::initLoadPackage(view, &player);

            EXPECT_EQ(get<int>(lua, "storage.playerSection('settings'):get('fov')"), 90);
            EXPECT_THROW(lua.safe_script("storage.playerSection('settings'):set('fov', 1)"), std::exception);
        });
    }

    TEST(LuaUtilStoragePackagesTest, theValuesOfASectionCanBeReplacedFromOutsideOfLua)
    {
        LuaUtil::LuaState luaState{ nullptr, nullptr };
        luaState.protectedCall([](LuaUtil::LuaView& view) {
            sol::state_view& lua = view.sol();
            LuaUtil::LuaStorage::initLuaBindings(view);
            LuaUtil::LuaStorage storage;
            RecordingListener listener;
            storage.setActive(true);
            storage.setListener(&listener);
            sol::table values(lua, sol::create);
            values["a"] = 1;
            values["b"] = "two";

            storage.setSectionValues("options", values);
            lua["section"] = storage.getReadOnlySection(lua, "options");
            EXPECT_EQ(get<int>(lua, "section:get('a')"), 1);
            EXPECT_EQ(get<std::string>(lua, "section:get('b')"), "two");

            storage.setSectionValues("options", sol::nullopt);
            EXPECT_TRUE(get<bool>(lua, "section:get('a') == nil"));
            EXPECT_THAT(listener.mEvents, ElementsAre("options replaced", "options cleared"));
        });
    }
}
