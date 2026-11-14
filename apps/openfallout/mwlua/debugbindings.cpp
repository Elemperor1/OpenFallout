#include "debugbindings.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwscript/falloutscripts.hpp"

#include "../mwinput/actions.hpp"

#include "../mwrender/postprocessor.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/shader/shadermanager.hpp>

#include <components/esm/refid.hpp>
#include <components/lua/luastate.hpp>

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace OFLua
{
    sol::table initDebugPackage(const Context& context)
    {
        auto view = context.sol();
        sol::table api(view, sol::create);

        api["RENDER_MODE"]
            = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, OFRender::RenderMode>(view,
                {
                    { "CollisionDebug", OFRender::Render_CollisionDebug },
                    { "Wireframe", OFRender::Render_Wireframe },
                    { "Pathgrid", OFRender::Render_Pathgrid },
                    { "Water", OFRender::Render_Water },
                    { "Scene", OFRender::Render_Scene },
                    { "NavMesh", OFRender::Render_NavMesh },
                    { "ActorsPaths", OFRender::Render_ActorsPaths },
                    { "RecastMesh", OFRender::Render_RecastMesh },
                }));

        api["toggleRenderMode"] = [context](OFRender::RenderMode value) {
            context.mLuaManager->addAction([value] { OFBase::Environment::get().getWorld()->toggleRenderMode(value); });
        };

        api["toggleGodMode"] = []() { OFBase::Environment::get().getWorld()->toggleGodMode(); };
        api["isGodMode"] = []() { return OFBase::Environment::get().getWorld()->getGodModeState(); };

        api["toggleAI"] = []() { OFBase::Environment::get().getMechanicsManager()->toggleAI(); };
        api["isAIEnabled"] = []() { return OFBase::Environment::get().getMechanicsManager()->isAIActive(); };

        api["toggleCollision"] = []() { OFBase::Environment::get().getWorld()->toggleCollisionMode(); };
        api["isCollisionEnabled"] = []() {
            auto world = OFBase::Environment::get().getWorld();
            return world->isActorCollisionEnabled(world->getPlayerPtr());
        };

        api["toggleMWScript"] = []() { OFBase::Environment::get().getWorld()->toggleScripts(); };
        api["isMWScriptEnabled"] = []() { return OFBase::Environment::get().getWorld()->getScriptsEnabled(); };

        api["reloadLua"] = []() { OFBase::Environment::get().getLuaManager()->reloadAllScripts(); };

        // Calls a command of the script language of Fallout by its name, on a reference ("player", or a form id as text; none
        // for a command that needs none) and with numbers (a form id as text is its number): a tool of the rounds and of
        // developers. It runs on the next frame, in the main thread, and writes the value the command gives to the log
        // ("Command: Name on 0x14 (1, 2) = 3").
        api["scriptCommand"] = [context](std::string_view name, sol::optional<std::string_view> reference,
                                   sol::optional<sol::table> arguments) {
            const auto number = [](std::string_view text) -> double {
                if (text.starts_with("0x"))
                {
                    std::uint32_t value = 0;
                    const char* end = text.data() + text.size();
                    const auto [stop, error] = std::from_chars(text.data() + 2, end, value, 16);
                    if (error != std::errc() || stop != end)
                        throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
                    return value;
                }
                const ESM::RefId refId = ESM::RefId::deserializeText(text);
                const ESM::FormId* id = refId.getIf<ESM::FormId>();
                if (id == nullptr)
                    throw std::runtime_error("\"" + std::string(text) + "\" is not a form id");
                return id->toUint32();
            };
            std::vector<double> numbers;
            if (arguments)
                for (const auto& [key, value] : *arguments)
                {
                    if (value.is<std::string_view>())
                        numbers.push_back(number(value.as<std::string_view>()));
                    else
                        numbers.push_back(value.as<double>());
                }
            const bool player = reference && *reference == "player";
            const double form = reference && !player && !reference->empty() ? number(*reference) : 0.0;
            context.mLuaManager->addAction(
                [command = std::string(name), player, form, numbers = std::move(numbers)] {
                    OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
                    if (scripts == nullptr)
                        return;
                    scripts->debugCommand(command, player ? scripts->playerReference() : static_cast<FalloutScript::FormId>(form), numbers);
                });
        };

        // Gives the player an item of Fallout by its form id ("FormId:0x10e3778"), as the AddItem of a script does
        api["giveItem"] = [context](std::string_view form, sol::optional<int> count) {
            const ESM::RefId refId = ESM::RefId::deserializeText(form);
            const ESM::FormId* id = refId.getIf<ESM::FormId>();
            if (id == nullptr)
                throw std::runtime_error("\"" + std::string(form) + "\" is not a form id");
            context.mLuaManager->addAction([formId = id->toUint32(), number = count.value_or(1)] {
                if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
                    scripts->givePlayerItem(formId, number);
            });
        };

        // Same code path as the screenshot key. Deferred, because the screenshot is captured
        // on the next frame and the input manager is not safe to touch from a Lua thread.
        api["takeScreenshot"] = [context]() {
            context.mLuaManager->addAction(
                [] { OFBase::Environment::get().getInputManager()->executeAction(OFInput::A_Screenshot); });
        };

        api["NAV_MESH_RENDER_MODE"]
            = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, Settings::NavMeshRenderMode>(view,
                {
                    { "AreaType", Settings::NavMeshRenderMode::AreaType },
                    { "UpdateFrequency", Settings::NavMeshRenderMode::UpdateFrequency },
                }));

        api["setNavMeshRenderMode"] = [context](Settings::NavMeshRenderMode value) {
            context.mLuaManager->addAction(
                [value] { OFBase::Environment::get().getWorld()->getRenderingManager()->setNavMeshMode(value); });
        };

        api["triggerShaderReload"] = [context]() {
            context.mLuaManager->addAction([] {
                auto world = OFBase::Environment::get().getWorld();

                world->getRenderingManager()
                    ->getResourceSystem()
                    ->getSceneManager()
                    ->getShaderManager()
                    .triggerShaderReload();
                world->getPostProcessor()->triggerShaderReload();
            });
        };

        api["setShaderHotReloadEnabled"] = [context](bool value) {
            context.mLuaManager->addAction([value] {
                auto world = OFBase::Environment::get().getWorld();
                world->getRenderingManager()
                    ->getResourceSystem()
                    ->getSceneManager()
                    ->getShaderManager()
                    .setHotReloadEnabled(value);
                world->getPostProcessor()->mEnableLiveReload = value;
            });
        };

        return LuaUtil::makeReadOnly(api);
    }
}
