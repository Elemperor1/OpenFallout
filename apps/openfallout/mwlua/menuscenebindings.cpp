#include "menuscenebindings.hpp"

#include <mutex>
#include <set>
#include <string>

#include <osg/Vec3f>
#include <osg/Vec4f>

#include <components/lua/luastate.hpp>
#include <components/lua/util.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwgui/menuscenes.hpp"
#include "../mwrender/menuscene.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"

namespace OFLua
{
    namespace
    {
        // The names of the scenes that are made. The scripts run beside the frame, the scenes are made in it (see
        // LuaManager::addAction), so a script learns that its scene is there with `ready`.
        std::mutex sReadyMutex;
        std::set<std::string> sReady;

        void setReady(const std::string& name, bool ready)
        {
            std::lock_guard<std::mutex> lock(sReadyMutex);
            if (ready)
                sReady.insert(name);
            else
                sReady.erase(name);
        }

        template <class F>
        void onScene(const Context& context, const std::string& name, F&& f)
        {
            context.mLuaManager->addAction([name, f = std::forward<F>(f)] {
                OFGui::MenuScenes* scenes = OFBase::Environment::get().getWindowManager()->getMenuScenes();
                if (scenes == nullptr)
                    return;
                if (OFRender::MenuScene* scene = scenes->find(name))
                    f(*scene);
            });
        }

        osg::Vec3f vec3(const sol::table& table, const char* key, const osg::Vec3f& fallback)
        {
            sol::object value = table[key];
            return value.is<osg::Vec3f>() ? value.as<osg::Vec3f>() : fallback;
        }
    }

    sol::table initCoreMenuSceneBindings(const Context& context)
    {
        sol::state_view lua = context.sol();
        sol::table api(lua, sol::create);

        // Makes the scene `name` with a texture of `width` by `height` pixels (the scene of that name that there is,
        // is replaced). A menu shows the picture with ui.texture{ path = core.menuScene.texture(name) } once
        // `ready(name)` is true.
        api["create"] = [context](const std::string& name, int width, int height) {
            setReady(name, false);
            context.mLuaManager->addAction([name, width, height] {
                OFGui::MenuScenes* scenes = OFBase::Environment::get().getWindowManager()->getMenuScenes();
                if (scenes == nullptr)
                    return;
                scenes->create(name, width, height);
                setReady(name, true);
            });
        };
        // Takes the scene out of the game; the texture keeps the last picture
        api["destroy"] = [context](const std::string& name) {
            setReady(name, false);
            context.mLuaManager->addAction([name] {
                if (OFGui::MenuScenes* scenes = OFBase::Environment::get().getWindowManager()->getMenuScenes())
                    scenes->destroy(name);
            });
        };
        api["ready"] = [](const std::string& name) {
            std::lock_guard<std::mutex> lock(sReadyMutex);
            return sReady.count(name) > 0;
        };
        // The path of the texture of the scene, for ui.texture
        api["texture"] = [](const std::string& name) { return OFGui::MenuScenes::textureName(name); };

        // The planes of the frustum of the camera on its near plane, and its near and far distances
        api["setFrustum"] = [context](const std::string& name, float left, float right, float bottom, float top,
                                float nearPlane, float farPlane) {
            onScene(context, name,
                [=](OFRender::MenuScene& scene) { scene.setFrustum(left, right, bottom, top, nearPlane, farPlane); });
        };
        // Where the camera stands, the way it looks and which way is up in the picture (vectors)
        api["setCamera"] = [context](const std::string& name, const osg::Vec3f& eye, const osg::Vec3f& forward,
                               const osg::Vec3f& up) {
            onScene(context, name, [=](OFRender::MenuScene& scene) { scene.setCamera(eye, forward, up); });
        };
        // The ambient light and the directional light (colours as vectors of red, green, blue from 0 to 1; the
        // direction is the way the light travels)
        api["setLight"] = [context](const std::string& name, const osg::Vec3f& ambient, const osg::Vec3f& diffuse,
                              const osg::Vec3f& direction) {
            onScene(context, name, [=](OFRender::MenuScene& scene) { scene.setLight(ambient, diffuse, direction); });
        };
        api["setClearColour"] = [context](const std::string& name, float r, float g, float b, float a) {
            onScene(context, name, [=](OFRender::MenuScene& scene) { scene.setClearColour(osg::Vec4f(r, g, b, a)); });
        };

        // Puts the model of the file `path` in the scene as `model`
        api["addModel"] = [context](const std::string& name, const std::string& model, const std::string& path) {
            onScene(context, name,
                [model, path](OFRender::MenuScene& scene) { scene.addModel(model, VFS::Path::Normalized(path)); });
        };
        api["removeModel"] = [context](const std::string& name, const std::string& model) {
            onScene(context, name, [model](OFRender::MenuScene& scene) { scene.removeModel(model); });
        };
        // The place of a model: a table with `position` (a vector), `euler` (a vector of the angles of the game's
        // files in degrees: about X, then Y, then Z) and `scale` (a number); what the table leaves out is zero, zero
        // and one
        api["setPlace"] = [context](const std::string& name, const std::string& model, const sol::table& place) {
            const osg::Vec3f position = vec3(place, "position", osg::Vec3f());
            const osg::Vec3f euler = vec3(place, "euler", osg::Vec3f());
            const float scale = place.get_or("scale", 1.f);
            onScene(context, name, [model, position, euler, scale](OFRender::MenuScene& scene) {
                scene.setTransform(model, OFRender::placement(position, euler, scale));
            });
        };
        api["setVisible"] = [context](const std::string& name, const std::string& model, bool visible) {
            onScene(context, name, [model, visible](OFRender::MenuScene& scene) { scene.setVisible(model, visible); });
        };
        // Puts the model at `time` seconds of its sequence `sequence` ("Forward", "Backward", "Left"): the nodes that
        // the sequence moves are where it has them at that time
        api["setSequenceTime"] = [context](const std::string& name, const std::string& model,
                                     const std::string& sequence, float time) {
            onScene(context, name,
                [model, sequence, time](OFRender::MenuScene& scene) { scene.setSequenceTime(model, sequence, time); });
        };

        return LuaUtil::makeReadOnly(api);
    }
}
