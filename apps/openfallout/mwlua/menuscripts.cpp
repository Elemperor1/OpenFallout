#include "menuscripts.hpp"

#include <MyGUI_InputManager.h>

#include <components/lua/util.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwstate/character.hpp"
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
        // Whether the console is open: it takes the keys that are typed, and menu scripts are given them as well
        api["consoleOpen"] = []() { return OFBase::Environment::get().getWindowManager()->isConsoleMode(); };
        // Whether a dialog of the engine's own windows is open over the menu (the one that tells a save or a load
        // failed): it has the keys and the buttons of the pad, and menu scripts are given those as well
        api["dialogOpen"] = []() { return MyGUI::InputManager::getInstance().isModalAny(); };
        // The window manager's window for the settings (the game's own settings screens are not drawn from its menu
        // file yet), and whether it is open
        api["openSettings"] = []() {
            OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager();
            if (!windows->isSettingsWindowVisible())
                windows->toggleSettingsWindow();
        };
        api["settingsOpen"] = []() { return OFBase::Environment::get().getWindowManager()->isSettingsWindowVisible(); };

        return LuaUtil::makeReadOnly(api);
    }
}
