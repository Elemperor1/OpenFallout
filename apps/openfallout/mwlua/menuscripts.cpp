#include "menuscripts.hpp"

#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

#include <components/esm/records.hpp>
#include <components/falloutscript/loadscreens.hpp>
#include <components/lua/util.hpp>
#include <components/misc/rng.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwscript/falloutscripts.hpp"
#include "../mwstate/character.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"

namespace OFLua
{
    namespace
    {
        const OFState::Character* findCharacter(std::string_view characterDir)
        {
            OFBase::StateManager* manager = OFBase::Environment::get().getStateManager();
            for (auto it = manager->characterBegin(); it != manager->characterEnd(); ++it)
                if (it->getPath().filename() == characterDir)
                    return &*it;
            return nullptr;
        }

        const OFState::Slot* findSlot(const OFState::Character* character, std::string_view slotName)
        {
            if (!character)
                return nullptr;
            for (const OFState::Slot& slot : *character)
                if (slot.mPath.filename() == slotName)
                    return &slot;
            return nullptr;
        }

        // The settings that the game's own settings screens (the file start_menu.xml) write, by the name that a script
        // gives them: the screens only reach these, and a setting that the engine has no counterpart of is not here
        // (the script keeps what the player picked for it)
        struct Option
        {
            std::string_view mName;
            std::string_view mCategory;
            std::string_view mSetting;
            bool mIsBool;
        };

        constexpr Option screenOptions[] = {
            { "masterVolume", "Sound", "master volume", false },
            { "musicVolume", "Sound", "music volume", false },
            { "footstepsVolume", "Sound", "footsteps volume", false },
            { "voiceVolume", "Sound", "voice volume", false },
            { "effectsVolume", "Sound", "sfx volume", false },
            { "gamma", "Video", "gamma", false },
            { "mouseSensitivity", "Input", "camera sensitivity", false },
            { "invertYAxis", "Input", "invert y axis", true },
            { "crosshair", "HUD", "crosshair", true },
            { "subtitles", "GUI", "subtitles", true },
            { "rumble", "Input", "gamepad rumble", true },
        };

        const Option& findOption(std::string_view name)
        {
            for (const Option& option : screenOptions)
                if (option.mName == name)
                    return option;
            throw std::runtime_error("Not a setting of the menu: " + std::string(name));
        }

        // The two items of the Gameplay screen that belong to the game being played rather than to the settings file:
        // the difficulty (0 very easy to 4 very hard) and the hardcore mode (New Vegas). The rules keep them in the
        // switches of the game, which a save holds, and they exist while a game of a Fallout runs.
        enum class GameItem
        {
            None,
            Difficulty,
            Hardcore,
        };

        GameItem gameItem(std::string_view name)
        {
            if (name == "difficulty")
                return GameItem::Difficulty;
            if (name == "hardcore")
                return GameItem::Hardcore;
            return GameItem::None;
        }

        OFScript::FalloutScripts* runningGame()
        {
            const OFBase::Environment& environment = OFBase::Environment::get();
            if (!environment.hasFalloutScripts()
                || environment.getStateManager()->getState() != OFBase::StateManager::State_Running)
                return nullptr;
            return environment.getFalloutScripts();
        }

        bool shouldBypass(const sol::optional<sol::table>& options)
        {
            if (options.has_value())
                return options->get_or("bypass", false);

            return false;
        }
    }

    sol::table initMenuPackage(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        api["STATE"]
            = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, OFBase::StateManager::State>(lua,
                {
                    { "NoGame", OFBase::StateManager::State_NoGame },
                    { "Running", OFBase::StateManager::State_Running },
                    { "Ended", OFBase::StateManager::State_Ended },
                }));

        api["getState"] = []() -> int { return OFBase::Environment::get().getStateManager()->getState(); };

        api["newGame"] = [](const sol::optional<sol::table>& options) {
            OFBase::Environment::get().getStateManager()->requestNewGame(shouldBypass(options));
        };

        api["loadGame"] = [](std::string_view dir, std::string_view slotName) {
            const OFState::Character* character = findCharacter(dir);
            const OFState::Slot* slot = findSlot(character, slotName);
            if (!slot)
                throw std::runtime_error("Save game slot not found: " + std::string(dir) + "/" + std::string(slotName));
            OFBase::Environment::get().getStateManager()->requestLoad(character, slot->mPath);
        };

        api["deleteGame"] = [](std::string_view dir, std::string_view slotName) {
            const OFState::Character* character = findCharacter(dir);
            const OFState::Slot* slot = findSlot(character, slotName);
            if (!slot)
                throw std::runtime_error("Save game slot not found: " + std::string(dir) + "/" + std::string(slotName));
            OFBase::Environment::get().getStateManager()->deleteGame(character, slot);
        };

        api["getCurrentSaveDir"] = []() -> sol::optional<std::string> {
            OFBase::StateManager* manager = OFBase::Environment::get().getStateManager();
            const OFState::Character* character = manager->getCurrentCharacter();
            if (character)
                return character->getPath().filename().string();
            else
                return sol::nullopt;
        };

        api["saveGame"] = [context](std::string_view description, sol::optional<std::string_view> slotName) {
            if (!context.mLuaManager->isSynchronizedUpdateRunning())
                throw std::runtime_error("menu.saveGame can only be used during engine or event handler processing");
            OFBase::StateManager* manager = OFBase::Environment::get().getStateManager();
            const OFState::Character* character = manager->getCurrentCharacter();
            const OFState::Slot* slot = nullptr;
            if (slotName)
                slot = findSlot(character, *slotName);
            manager->saveGame(description, slot);
        };

        auto getSaves = [](sol::state_view currentState, const OFState::Character& character) {
            sol::table saves(currentState, sol::create);
            for (const OFState::Slot& slot : character)
            {
                sol::table slotInfo(currentState, sol::create);
                slotInfo["description"] = slot.mProfile.mDescription;
                slotInfo["playerName"] = slot.mProfile.mPlayerName;
                slotInfo["playerLevel"] = slot.mProfile.mPlayerLevel;
                slotInfo["timePlayed"] = slot.mProfile.mTimePlayed;
                sol::table contentFiles(currentState, sol::create);
                for (size_t i = 0; i < slot.mProfile.mContentFiles.size(); ++i)
                    contentFiles[LuaUtil::toLuaIndex(i)] = Misc::StringUtils::lowerCase(slot.mProfile.mContentFiles[i]);

                {
                    const auto systemTime = std::chrono::system_clock::now()
                        - (std::filesystem::file_time_type::clock::now() - slot.mTimeStamp);
                    slotInfo["creationTime"] = std::chrono::duration<double>(systemTime.time_since_epoch()).count();
                }

                slotInfo["contentFiles"] = contentFiles;
                saves[slot.mPath.filename().string()] = slotInfo;
            }
            return saves;
        };

        api["getSaves"] = [getSaves](sol::this_state thisState, std::string_view dir) -> sol::table {
            const OFState::Character* character = findCharacter(dir);
            if (!character)
                throw std::runtime_error("Saves not found: " + std::string(dir));
            return getSaves(thisState, *character);
        };

        api["getAllSaves"] = [getSaves](sol::this_state thisState) -> sol::table {
            sol::table saves(thisState, sol::create);
            OFBase::StateManager* manager = OFBase::Environment::get().getStateManager();
            for (auto it = manager->characterBegin(); it != manager->characterEnd(); ++it)
                saves[it->getPath().filename().string()] = getSaves(thisState, *it);
            return saves;
        };

        api["quit"] = []() { OFBase::Environment::get().getStateManager()->requestQuit(); };

        // The menu that opens with Escape, which a script can draw from the game's own menu file (start_menu.xml)
        // instead of the window manager's buttons. Whether it is open (the screen on top is the menu):
        api["mainMenuOpen"]
            = []() { return OFBase::Environment::get().getWindowManager()->getMode() == OFGui::GM_MainMenu; };
        // The script draws the menu (true), or the window manager does (false, as before). The script says it every
        // frame: a claim that is a second old is no longer one, so that a script that fails gives the menu back. With
        // `escape` the key that leaves the menu (Escape, the B button) is the script's, which goes back a step in a
        // screen of its own, and the window manager does not close the menu with it.
        api["claimMainMenu"] = [](bool drawn, sol::optional<bool> escape) {
            OFBase::Environment::get().getWindowManager()->setMainMenuDrawnByLua(drawn, escape.value_or(false));
        };
        // Close the menu and go on with the game (nothing when no game is running)
        api["resume"] = []() {
            if (OFBase::Environment::get().getStateManager()->getState() == OFBase::StateManager::State_Running)
                OFBase::Environment::get().getWindowManager()->removeGuiMode(OFGui::GM_MainMenu);
        };
        // Whether the game can be saved now, as the window manager's menu decides it to show its button for saving: a
        // game that runs, whose character is made and which is not in a place that cannot be saved in
        api["canSave"] = []() {
            return OFBase::Environment::get().getStateManager()->getState() == OFBase::StateManager::State_Running
                && OFBase::Environment::get().getWorld()->getGlobalInt(OFWorld::Globals::sCharGenState) == -1
                && OFBase::Environment::get().getWindowManager()->isSavingAllowed();
        };
        // Whether the console or a dialog of the engine's own windows (the one that tells a save or a load failed) had
        // the key or the button that the script is being given: they take the keys and the buttons of the pad, and menu
        // scripts are given those as well. Asked in a handler of a key or of a button of the pad
        api["inputTaken"] = [context]() { return context.mLuaManager->isMenuInputTaken(); };
        // The window manager's window for the settings (the game's own settings screens are not drawn from its menu
        // file yet), and whether it is open
        api["openSettings"] = []() {
            OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager();
            if (!windows->isSettingsWindowVisible())
                windows->toggleSettingsWindow();
        };
        api["settingsOpen"] = []() { return OFBase::Environment::get().getWindowManager()->isSettingsWindowVisible(); };

        // The settings that the screens of the game's settings write (the names are in `screenOptions` above): what a
        // setting is now (a number, or a boolean for those that are switches), and a change of it, which the engine
        // takes at once
        api["getOption"] = [](sol::this_state thisState, std::string_view name) -> sol::object {
            // (nil while no game runs: the script then shows what the player picked)
            if (const GameItem item = gameItem(name); item != GameItem::None)
            {
                OFScript::FalloutScripts* game = runningGame();
                if (game == nullptr)
                    return sol::nil;
                if (item == GameItem::Difficulty)
                    return sol::make_object(thisState, game->gameDifficulty());
                return sol::make_object(thisState, game->hardcoreMode());
            }
            const Option& option = findOption(name);
            if (option.mIsBool)
                return sol::make_object(thisState, Settings::get<bool>(option.mCategory, option.mSetting).get());
            return sol::make_object(thisState, Settings::get<float>(option.mCategory, option.mSetting).get());
        };
        api["setOption"] = [](std::string_view name, sol::object value) {
            if (const GameItem item = gameItem(name); item != GameItem::None)
            {
                // (with no game running there is nothing to change: the script keeps what the player picked)
                OFScript::FalloutScripts* game = runningGame();
                if (item == GameItem::Difficulty)
                {
                    if (!value.is<double>())
                        throw std::runtime_error("The setting is a number: " + std::string(name));
                    if (game != nullptr)
                        game->setGameDifficulty(static_cast<int>(std::lround(value.as<double>())));
                }
                else
                {
                    if (!value.is<bool>())
                        throw std::runtime_error("The setting is a switch (true or false): " + std::string(name));
                    if (game != nullptr)
                        game->setHardcoreMode(value.as<bool>());
                }
                return;
            }
            const Option& option = findOption(name);
            if (option.mIsBool)
            {
                if (!value.is<bool>())
                    throw std::runtime_error("The setting is a switch (true or false): " + std::string(name));
                Settings::get<bool>(option.mCategory, option.mSetting).set(value.as<bool>());
            }
            else
            {
                if (!value.is<float>())
                    throw std::runtime_error("The setting is a number: " + std::string(name));
                Settings::get<float>(option.mCategory, option.mSetting).set(value.as<float>());
            }
            OFBase::Environment::get().getWindowManager()->applyChangedSettings();
        };
        // Whether a game controller is plugged in (the game's settings leave out the items of the pad when there is
        // none)
        api["controllerConnected"] = []() {
            for (int index = 0; index < SDL_NumJoysticks(); ++index)
                if (SDL_IsGameController(index) == SDL_TRUE)
                    return true;
            return false;
        };
        // The pictures the title screen turns through behind its list: the load screens of the data that carry the
        // main menu's flag (0x400), picked as the game's loading menu picks them when a game starts from the main menu
        // (every picture once, in a random order, at most 20), as the paths of the picture files (without "textures")
        api["mainMenuPictures"] = [](sol::this_state thisState) {
            namespace LS = FalloutScript::LoadScreens;
            constexpr std::uint32_t mainMenuFlag = 0x400;
            constexpr std::size_t most = 20;
            std::vector<LS::Picture> pool;
            for (const ESM4::LoadScreen& record : OFBase::Environment::get().getESMStore()->get<ESM4::LoadScreen>())
            {
                if (record.mIcon.empty())
                    continue;
                LS::Picture picture;
                picture.mIcon = record.mIcon;
                picture.mMainMenu = (record.mFlags & mainMenuFlag) != 0;
                pool.push_back(std::move(picture));
            }
            const std::vector<std::size_t> chosen = LS::choosePictures(
                pool, true, most, [](int low, int high) { return low + Misc::Rng::rollDice(high - low + 1); });
            sol::table result(thisState, sol::create);
            for (const std::size_t index : chosen)
                result.add(pool[index].mIcon);
            return result;
        };
        // The settings are written to the user's file (when the screens of the settings are left)
        api["saveOptions"] = []() { OFBase::Environment::get().getWindowManager()->saveSettings(); };

        return LuaUtil::makeReadOnly(api);
    }
}
