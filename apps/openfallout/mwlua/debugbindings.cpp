#include "debugbindings.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwinput/actions.hpp"

#include "../mwrender/postprocessor.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/shader/shadermanager.hpp>

#include <components/lua/luastate.hpp>

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
