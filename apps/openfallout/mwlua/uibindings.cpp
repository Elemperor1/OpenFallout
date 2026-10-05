#include "uibindings.hpp"

#include <components/lua/util.hpp>
#include <components/lua_ui/alignment.hpp>
#include <components/lua_ui/content.hpp>
#include <components/lua_ui/element.hpp>
#include <components/lua_ui/layers.hpp>
#include <components/lua_ui/registerscriptsettings.hpp>
#include <components/lua_ui/resources.hpp>
#include <components/lua_ui/util.hpp>

#include <components/misc/finitevalues.hpp>
#include <components/misc/strings/format.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/manager.hpp>

#include <cmath>
#include <numbers>

#include "context.hpp"
#include "luamanagerimp.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include <format>

namespace sol
{
    template <>
    struct is_automagical<LuaUi::Layer> : std::false_type
    {
    };
}

namespace OFLua
{
    namespace
    {
        constexpr int sMaxCursorSize = 128;

        const std::unordered_map<OFGui::GuiMode, std::string_view> modeToName{
            { OFGui::GM_Inventory, "Interface" },
            { OFGui::GM_Container, "Container" },
            { OFGui::GM_Companion, "Companion" },
            { OFGui::GM_MainMenu, "MainMenu" },
            { OFGui::GM_Journal, "Journal" },
            { OFGui::GM_Scroll, "Scroll" },
            { OFGui::GM_Book, "Book" },
            { OFGui::GM_Alchemy, "Alchemy" },
            { OFGui::GM_Repair, "Repair" },
            { OFGui::GM_Dialogue, "Dialogue" },
            { OFGui::GM_Barter, "Barter" },
            { OFGui::GM_Rest, "Rest" },
            { OFGui::GM_SpellBuying, "SpellBuying" },
            { OFGui::GM_Travel, "Travel" },
            { OFGui::GM_SpellCreation, "SpellCreation" },
            { OFGui::GM_Enchanting, "Enchanting" },
            { OFGui::GM_Recharge, "Recharge" },
            { OFGui::GM_Training, "Training" },
            { OFGui::GM_MerchantRepair, "MerchantRepair" },
            { OFGui::GM_Levelup, "LevelUp" },
            { OFGui::GM_Name, "ChargenName" },
            { OFGui::GM_Race, "ChargenRace" },
            { OFGui::GM_Birth, "ChargenBirth" },
            { OFGui::GM_Class, "ChargenClass" },
            { OFGui::GM_ClassGenerate, "ChargenClassGenerate" },
            { OFGui::GM_ClassPick, "ChargenClassPick" },
            { OFGui::GM_ClassCreate, "ChargenClassCreate" },
            { OFGui::GM_Review, "ChargenClassReview" },
            { OFGui::GM_Loading, "Loading" },
            { OFGui::GM_LoadingWallpaper, "LoadingWallpaper" },
            { OFGui::GM_Jail, "Jail" },
            { OFGui::GM_QuickKeysMenu, "QuickKeysMenu" },
        };

        const auto nameToMode = [] {
            std::unordered_map<std::string_view, OFGui::GuiMode> res;
            for (const auto& [mode, name] : modeToName)
                res[name] = mode;
            return res;
        }();
    }

    sol::table registerUiApi(const Context& context)
    {
        sol::state_view lua = context.sol();
        bool menu = context.mType == Context::Menu;

        OFBase::WindowManager* windowManager = OFBase::Environment::get().getWindowManager();

        sol::table api(lua, sol::create);
        api["CursorMode"]
            = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, OFBase::CursorMode>(lua,
                { { "CONFINED", OFBase::CursorMode::Confined }, { "FREE", OFBase::CursorMode::Free },
                    { "LOCKED", OFBase::CursorMode::Locked } }));
        api["setCursorMode"] = [luaManager = context.mLuaManager](OFBase::CursorMode mode) {
            luaManager->addAction([mode] { OFBase::Environment::get().getInputManager()->setCursorMode(mode); });
        };
        api["getCursorMode"] = []() { return OFBase::Environment::get().getInputManager()->getCursorMode(); };
        api["setCursorPosition"] = [luaManager = context.mLuaManager](const Misc::FiniteVec2f& position) {
            luaManager->addAction(
                [position] { OFBase::Environment::get().getInputManager()->setCursorPosition(position); });
        };
        api["getCursorPosition"] = []() { return OFBase::Environment::get().getInputManager()->getCursorPosition(); };
        api["setCursorVisible"] = [luaManager = context.mLuaManager](bool visible) {
            luaManager->addAction(
                [visible] { OFBase::Environment::get().getWindowManager()->setCursorVisible(visible); });
        };
        api["getCursorVisible"] = [windowManager]() { return windowManager->getCursorVisible(); };
        api["_setHudVisibility"] = [luaManager = context.mLuaManager](bool state) {
            luaManager->addAction([state] { OFBase::Environment::get().getWindowManager()->setHudVisibility(state); });
        };
        api["_isHudVisible"] = []() -> bool { return OFBase::Environment::get().getWindowManager()->isHudVisible(); };
        api["_getDefaultFontSize"] = []() -> int { return Settings::gui().mFontSize; };
        api["showMessage"]
            = [luaManager = context.mLuaManager](std::string_view message, const sol::optional<sol::table>& options) {
                  OFGui::ShowInDialogueMode mode = OFGui::ShowInDialogueMode_IfPossible;
                  if (options.has_value())
                  {
                      auto showInDialogue = options->get<sol::optional<bool>>("showInDialogue");
                      if (showInDialogue.has_value())
                      {
                          if (*showInDialogue)
                              mode = OFGui::ShowInDialogueMode_Only;
                          else
                              mode = OFGui::ShowInDialogueMode_Never;
                      }
                  }
                  luaManager->addUIMessage(message, mode);
              };

        api["_showInteractiveMessage"] = [windowManager](std::string_view message, sol::optional<sol::table>) {
            windowManager->interactiveMessageBox(message, { "#{Interface:OK}" });
        };
        api["CONSOLE_COLOR"] = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string, Misc::Color>(lua,
            {
                { "Default", Misc::Color::fromHex(OFBase::WindowManager::sConsoleColor_Default.substr(1)) },
                { "Error", Misc::Color::fromHex(OFBase::WindowManager::sConsoleColor_Error.substr(1)) },
                { "Success", Misc::Color::fromHex(OFBase::WindowManager::sConsoleColor_Success.substr(1)) },
                { "Info", Misc::Color::fromHex(OFBase::WindowManager::sConsoleColor_Info.substr(1)) },
            }));
        api["printToConsole"] = [luaManager = context.mLuaManager](std::string_view message, const Misc::Color& color) {
            luaManager->addInGameConsoleMessage(std::format("{}\n", message), color);
        };
        api["setConsoleMode"] = [luaManager = context.mLuaManager, windowManager](std::string_view mode) {
            luaManager->addAction([mode = std::string(mode), windowManager] { windowManager->setConsoleMode(mode); });
        };
        api["getConsoleMode"] = [windowManager]() -> std::string_view { return windowManager->getConsoleMode(); };
        api["setConsoleSelectedObject"] = [luaManager = context.mLuaManager, windowManager](const sol::object& obj) {
            if (obj == sol::nil)
                luaManager->addAction([windowManager] { windowManager->setConsoleSelectedObject(OFWorld::Ptr()); });
            else
            {
                if (!obj.is<LObject>())
                    throw std::runtime_error("Game object expected");
                luaManager->addAction(
                    [windowManager, obj = obj.as<LObject>()] { windowManager->setConsoleSelectedObject(obj.ptr()); });
            }
        };
        api["content"] = LuaUi::loadContentConstructor(context.mLua);

        api["create"]
            = [luaManager = context.mLuaManager, menu](const sol::table& layout, sol::optional<sol::table> options) {
                  auto element = LuaUi::Element::make(layout, menu, options);
                  luaManager->addAction([element] { element->create(); }, "Create UI");
                  return element;
              };

        api["getElements"] = [menu](sol::this_state thisState, sol::optional<std::string_view> layer) {
            sol::table res(thisState, sol::create);
            size_t index = 1;
            LuaUi::Element::forEachShared(menu, [&](const std::shared_ptr<LuaUi::Element>& element) {
                if ((element->mState != LuaUi::Element::Created && element->mState != LuaUi::Element::Update)
                    || element->mRoot == nullptr)
                    return;

                MyGUI::ILayer* layerNode = nullptr;
                for (LuaUi::WidgetExtension* ext = element->mRoot; ext != nullptr && layerNode == nullptr;
                     ext = ext->getParent())
                    layerNode = ext->widget()->getLayer();

                if (!layerNode)
                    return;

                if (layer.has_value() && layerNode->getName() != *layer)
                    return;

                res[index++] = element;
            });
            return res;
        };

        api["updateAll"] = [luaManager = context.mLuaManager, menu]() {
            LuaUi::Element::forEach(menu, [](LuaUi::Element* e) {
                if (e->mState == LuaUi::Element::Created)
                    e->mState = LuaUi::Element::Update;
            });
            luaManager->addAction([menu]() { LuaUi::Element::forEach(menu, [](LuaUi::Element* e) { e->update(); }); },
                "Update all menu UI elements");
        };
        api["_getMenuTransparency"] = []() -> float { return Settings::gui().mMenuTransparency; };

        sol::table layersTable(lua, sol::create);
        layersTable["indexOf"] = [](std::string_view name) -> sol::optional<size_t> {
            size_t index = LuaUi::Layer::indexOf(name);
            if (index == LuaUi::Layer::count())
                return sol::nullopt;
            else
                return LuaUtil::toLuaIndex(index);
        };
        layersTable["insertAfter"] = [context](std::string afterName, std::string name, const sol::object& opt) {
            LuaUi::Layer::Options options;
            options.mInteractive = LuaUtil::getValueOrDefault(LuaUtil::getFieldOrNil(opt, "interactive"), true);
            context.mLuaManager->addAction(
                [afterName = std::move(afterName), name = std::move(name), options]() {
                    size_t index = LuaUi::Layer::indexOf(afterName);
                    if (index == LuaUi::Layer::count())
                        throw std::logic_error(std::format("Couldn't insert after non-existent layer {}", afterName));
                    LuaUi::Layer::insert(index + 1, name, options);
                },
                "Insert after UI layer");
        };
        layersTable["insertBefore"] = [context](std::string beforeName, std::string name, const sol::object& opt) {
            LuaUi::Layer::Options options;
            options.mInteractive = LuaUtil::getValueOrDefault(LuaUtil::getFieldOrNil(opt, "interactive"), true);
            context.mLuaManager->addAction(
                [beforeName = std::move(beforeName), name = std::move(name), options]() {
                    size_t index = LuaUi::Layer::indexOf(beforeName);
                    if (index == LuaUi::Layer::count())
                        throw std::logic_error(std::format("Couldn't insert before non-existent layer {}", beforeName));
                    LuaUi::Layer::insert(index, name, options);
                },
                "Insert before UI layer");
        };
        sol::table layers = LuaUtil::makeReadOnly(layersTable);
        sol::table layersMeta = layers[sol::metatable_key];
        layersMeta[sol::meta_function::length] = []() { return LuaUi::Layer::count(); };
        layersMeta[sol::meta_function::index] = sol::overload(
            [](const sol::object& self, size_t index) {
                index = LuaUtil::fromLuaIndex(index);
                return LuaUi::Layer(index);
            },
            [layersTable](
                const sol::object& self, std::string_view key) { return layersTable.raw_get<sol::object>(key); });
        {
            auto pairs = [layers](const sol::object&) {
                auto next = [](const sol::table& l, size_t i) -> sol::optional<std::tuple<size_t, LuaUi::Layer>> {
                    if (i < LuaUi::Layer::count())
                        return std::make_tuple(i + 1, LuaUi::Layer(i));
                    else
                        return sol::nullopt;
                };
                return std::make_tuple(next, layers, 0);
            };
            layersMeta[sol::meta_function::pairs] = pairs;
            layersMeta[sol::meta_function::ipairs] = pairs;
        }
        api["layers"] = layers;

        sol::table typeTable(lua, sol::create);
        for (const auto& it : LuaUi::widgetTypeToName())
            typeTable.set(it.second, it.first);
        api["TYPE"] = LuaUtil::makeStrictReadOnly(typeTable);

        api["ALIGNMENT"] = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, LuaUi::Alignment>(lua,
            { { "Start", LuaUi::Alignment::Start }, { "Center", LuaUi::Alignment::Center },
                { "End", LuaUi::Alignment::End } }));

        api["registerSettingsPage"] = &LuaUi::registerSettingsPage;
        api["removeSettingsPage"] = &LuaUi::removeSettingsPage;

        api["texture"] = [luaManager = context.mLuaManager](const sol::table& options) {
            LuaUi::TextureData data;
            sol::object path = LuaUtil::getFieldOrNil(options, "path");
            if (path.is<std::string>())
                data.mPath = VFS::Path::Normalized(path.as<std::string>());
            if (data.mPath.empty())
                throw std::logic_error("Invalid texture path");
            sol::object offset = LuaUtil::getFieldOrNil(options, "offset");
            if (offset.is<osg::Vec2f>())
                data.mOffset = offset.as<osg::Vec2f>();
            sol::object size = LuaUtil::getFieldOrNil(options, "size");
            if (size.is<osg::Vec2f>())
                data.mSize = size.as<osg::Vec2f>();
            return luaManager->uiResourceManager()->registerTexture(std::move(data));
        };

        api["cursor"] = [luaManager = context.mLuaManager, menu](const sol::table& options) {
            LuaUi::CursorData data;
            sol::object path = LuaUtil::getFieldOrNil(options, "path");
            if (path.is<std::string>())
                data.mPath = VFS::Path::Normalized(path.as<std::string>());
            if (data.mPath.empty())
                throw std::logic_error("Invalid cursor path");
            if (!OFBase::Environment::get().getResourceSystem()->getVFS()->exists(data.mPath))
                throw std::logic_error("Cursor texture does not exist: " + std::string(data.mPath));

            sol::object size = LuaUtil::getFieldOrNil(options, "size");
            sol::object hotspot = LuaUtil::getFieldOrNil(options, "hotspot");
            if (!size.is<osg::Vec2f>() || !hotspot.is<osg::Vec2f>())
                throw std::logic_error("Cursor size and hotspot must be vectors");
            data.mSize = size.as<osg::Vec2f>();
            data.mHotspot = hotspot.as<osg::Vec2f>();
            data.mPersistent = menu;

            sol::optional<Misc::FiniteDouble> rotation = options.get<sol::optional<Misc::FiniteDouble>>("rotation");
            if (rotation)
            {
                constexpr double fullTurn = 2 * std::numbers::pi;
                data.mRotation = std::fmod(static_cast<double>(*rotation), fullTurn);
                if (data.mRotation < 0)
                    data.mRotation += fullTurn;
            }

            auto integral = [](float value) { return std::isfinite(value) && std::floor(value) == value; };
            if (!integral(data.mSize.x()) || !integral(data.mSize.y()) || !integral(data.mHotspot.x())
                || !integral(data.mHotspot.y()) || data.mSize.x() <= 0 || data.mSize.y() <= 0 || data.mHotspot.x() < 0
                || data.mHotspot.y() < 0 || data.mHotspot.x() >= data.mSize.x() || data.mHotspot.y() >= data.mSize.y())
                throw std::logic_error("Invalid cursor size or hotspot");
            if (data.mSize.x() > sMaxCursorSize || data.mSize.y() > sMaxCursorSize)
                throw std::logic_error("Cursor size cannot exceed 128 pixels");

            auto cursor = luaManager->uiResourceManager()->registerCursor(std::move(data));
            luaManager->addAction([cursor] {
                OFBase::Environment::get().getWindowManager()->createLuaCursor(cursor->mName,
                    std::string(cursor->mPath), static_cast<int>(cursor->mSize.x()),
                    static_cast<int>(cursor->mSize.y()), static_cast<int>(cursor->mHotspot.x()),
                    static_cast<int>(cursor->mHotspot.y()), -cursor->mRotation * 180.0 / std::numbers::pi);
            });
            return cursor;
        };

        api["setCursor"] = [luaManager = context.mLuaManager](
                               sol::optional<std::shared_ptr<LuaUi::CursorResource>> cursor) {
            luaManager->addAction([cursor = std::move(cursor)] {
                OFBase::Environment::get().getWindowManager()->setLuaCursorOverride(cursor ? (*cursor)->mName : "");
            });
        };
        api["getCursor"] = [luaManager = context.mLuaManager, windowManager]() {
            return luaManager->uiResourceManager()->findCursor(windowManager->getCurrentCursorName());
        };

        api["screenSize"] = []() {
            return osg::Vec2f(
                static_cast<float>(Settings::video().mResolutionX), static_cast<float>(Settings::video().mResolutionY));
        };

        api["_getAllUiModes"] = [](sol::this_state thisState) {
            sol::table res(thisState, sol::create);
            for (const auto& [_, name] : modeToName)
                res[name] = name;
            return res;
        };
        api["_getUiModeStack"] = [windowManager](sol::this_state thisState) {
            sol::table res(thisState, sol::create);
            int i = 1;
            for (OFGui::GuiMode m : windowManager->getGuiModeStack())
                res[i++] = modeToName.at(m);
            return res;
        };
        api["_setUiModeStack"]
            = [windowManager, luaManager = context.mLuaManager](sol::table modes, sol::optional<LObject> arg) {
                  std::vector<OFGui::GuiMode> newStack(modes.size());
                  for (unsigned i = 0; i < newStack.size(); ++i)
                      newStack[i] = nameToMode.at(LuaUtil::cast<std::string_view>(modes[LuaUtil::toLuaIndex(i)]));
                  luaManager->addAction(
                      [windowManager, newStack = std::move(newStack), arg = std::move(arg)]() {
                          OFWorld::Ptr ptr;
                          if (arg.has_value())
                              ptr = arg->ptr();
                          const std::vector<OFGui::GuiMode>& stack = windowManager->getGuiModeStack();
                          size_t common = 0;
                          while (common < std::min(stack.size(), newStack.size()) && stack[common] == newStack[common])
                              common++;
                          // TODO: Maybe disallow opening/closing special modes (main menu, settings, loading screen)
                          // from player scripts. Add new Lua context "menu" that can do it.
                          for (size_t i = stack.size() - common; i > 0; i--)
                              windowManager->popGuiMode(true);
                          if (common == newStack.size() && !newStack.empty() && arg.has_value())
                              windowManager->pushGuiMode(newStack.back(), ptr);
                          for (size_t i = common; i < newStack.size(); ++i)
                              windowManager->pushGuiMode(newStack[i], ptr);
                      },
                      "Set UI modes");
              };
        api["_getAllWindowIds"] = [windowManager](sol::this_state thisState) {
            sol::table res(thisState, sol::create);
            for (std::string_view name : windowManager->getAllWindowIds())
                res[name] = name;
            return res;
        };
        api["_getAllowedWindows"] = [windowManager](sol::this_state thisState, std::string_view mode) {
            sol::table res(thisState, sol::create);
            for (std::string_view name : windowManager->getAllowedWindowIds(nameToMode.at(mode)))
                res[name] = name;
            return res;
        };
        api["_setWindowDisabled"]
            = [windowManager, luaManager = context.mLuaManager](std::string window, bool disabled) {
                  luaManager->addAction(
                      [=, window = std::move(window)]() { windowManager->setDisabledByLua(window, disabled); });
              };
        api["_isWindowVisible"]
            = [windowManager](std::string_view window) { return windowManager->isWindowVisible(window); };

        // TODO
        // api["_showMouseCursor"] = [](bool) {};

        return api;
    }

    sol::table initUserInterfacePackage(const Context& context)
    {
        if (context.initializeOnce("openfallout_ui_usertypes"))
        {
            auto textureResource = context.sol().new_usertype<LuaUi::TextureResource>("TextureResource");
            textureResource[sol::meta_function::to_string] = [](const LuaUi::TextureResource& resource) {
                return std::format("TextureResource[{}]", resource.mPath.value());
            };
            textureResource["path"] = sol::readonly_property(
                [](const LuaUi::TextureResource& resource) -> std::string_view { return resource.mPath; });
            textureResource["offset"]
                = sol::readonly_property([](const LuaUi::TextureResource& resource) { return resource.mOffset; });
            textureResource["size"]
                = sol::readonly_property([](const LuaUi::TextureResource& resource) { return resource.mSize; });

            auto cursorResource = context.sol().new_usertype<LuaUi::CursorResource>("CursorResource");
            cursorResource[sol::meta_function::to_string] = [](const LuaUi::CursorResource& resource) {
                return "CursorResource[" + resource.mPath.value() + "]";
            };
            cursorResource["path"] = sol::readonly_property(
                [](const LuaUi::CursorResource& resource) -> std::string_view { return resource.mPath; });
            cursorResource["size"]
                = sol::readonly_property([](const LuaUi::CursorResource& resource) { return resource.mSize; });
            cursorResource["hotspot"]
                = sol::readonly_property([](const LuaUi::CursorResource& resource) { return resource.mHotspot; });
            cursorResource["rotation"]
                = sol::readonly_property([](const LuaUi::CursorResource& resource) { return resource.mRotation; });

            auto uiElement = context.sol().new_usertype<LuaUi::Element>("UiElement");
            uiElement[sol::meta_function::to_string] = [](const LuaUi::Element& element) {
                std::stringstream res;
                res << "UiElement";
                if (element.mLayer != "")
                    res << "[" << element.mLayer << "]";
                return res.str();
            };
            uiElement["layout"] = sol::property([](const LuaUi::Element& element) { return element.mLayout; },
                [](LuaUi::Element& element, const sol::main_table& layout) { element.mLayout = layout; });
            uiElement["update"] = [luaManager = context.mLuaManager](const std::shared_ptr<LuaUi::Element>& element) {
                if (element->mState != LuaUi::Element::Created)
                    return;
                luaManager->addAction([element] { element->update(); }, "Update UI");
                element->mState = LuaUi::Element::Update;
            };
            uiElement["destroy"] = [luaManager = context.mLuaManager](const std::shared_ptr<LuaUi::Element>& element) {
                if (element->mState == LuaUi::Element::Destroyed)
                    return;
                luaManager->addAction([element] { LuaUi::Element::erase(element.get()); }, "Destroy UI");
                element->mState = LuaUi::Element::Destroy;
            };

            auto uiLayer = context.sol().new_usertype<LuaUi::Layer>("UiLayer");
            uiLayer["name"]
                = sol::readonly_property([](LuaUi::Layer& self) -> std::string_view { return self.name(); });
            uiLayer["size"] = sol::readonly_property([](LuaUi::Layer& self) { return self.size(); });
            uiLayer[sol::meta_function::to_string]
                = [](LuaUi::Layer& self) { return std::format("UiLayer({})", self.name()); };
        }

        sol::object cached = context.getTypePackage("openfallout_ui");
        if (cached != sol::nil)
            return cached;
        else
        {
            sol::table api = LuaUtil::makeReadOnly(registerUiApi(context));
            return context.setTypePackage(api, "openfallout_ui");
        }
    }
}
