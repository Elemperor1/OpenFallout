#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWGUI_MENUSCENES_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWGUI_MENUSCENES_H

#include <map>
#include <memory>
#include <string>

#include <osg/Group>
#include <osg/ref_ptr>

namespace MyGUIPlatform
{
    class RenderManager;
}

namespace Resource
{
    class ResourceSystem;
}

namespace OFRender
{
    class FacePreview;
    class MenuScene;
}

namespace OFGui
{
    /// The scenes that menus show (the lock and pick of the lockpicking screen): each is made by name, and a widget
    /// that is given the texture `textureName(name)` shows its picture.
    class MenuScenes
    {
    public:
        MenuScenes(
            osg::Group* parent, Resource::ResourceSystem* resourceSystem, MyGUIPlatform::RenderManager* renderManager);
        ~MenuScenes();

        /// The name under which the picture of the scene is a texture of the menus
        static std::string textureName(const std::string& name) { return "scene:" + name; }

        /// Makes the scene (the scene of the same name that there is, is replaced)
        OFRender::MenuScene* create(const std::string& name, int width, int height);
        OFRender::MenuScene* find(const std::string& name);
        /// The head of the screen of the face in the scene (made if the scene has none yet); null when there is no scene
        OFRender::FacePreview* head(const std::string& name);
        /// Takes the scene out of the game's graph; the texture stays, with the last picture
        void destroy(const std::string& name);
        void clear();

    private:
        osg::ref_ptr<osg::Group> mParent;
        Resource::ResourceSystem* mResourceSystem;
        MyGUIPlatform::RenderManager* mRenderManager;
        std::map<std::string, std::unique_ptr<OFRender::MenuScene>> mScenes;
        // Each head is taken down before the scene it is in
        std::map<std::string, std::unique_ptr<OFRender::FacePreview>> mHeads;
    };
}

#endif
