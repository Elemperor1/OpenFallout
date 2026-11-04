#include "menuscenes.hpp"

#include <osg/Group>
#include <osg/Texture2D>

#include <components/myguiplatform/myguirendermanager.hpp>

#include "../mwrender/facepreview.hpp"
#include "../mwrender/menuscene.hpp"

namespace OFGui
{
    MenuScenes::MenuScenes(
        osg::Group* parent, Resource::ResourceSystem* resourceSystem, MyGUIPlatform::RenderManager* renderManager)
        : mParent(parent)
        , mResourceSystem(resourceSystem)
        , mRenderManager(renderManager)
    {
    }

    MenuScenes::~MenuScenes()
    {
        clear();
    }

    OFRender::MenuScene* MenuScenes::create(const std::string& name, int width, int height)
    {
        destroy(name);
        auto scene = std::make_unique<OFRender::MenuScene>(mParent, mResourceSystem, width, height);
        // The entry stays in the render manager for the run: a widget that showed the old picture may still point at it
        mRenderManager->registerTexture(textureName(name), scene->getTexture(), scene->getTextureStateSet());
        return (mScenes[name] = std::move(scene)).get();
    }

    OFRender::MenuScene* MenuScenes::find(const std::string& name)
    {
        const auto it = mScenes.find(name);
        return it == mScenes.end() ? nullptr : it->second.get();
    }

    OFRender::FacePreview* MenuScenes::head(const std::string& name)
    {
        OFRender::MenuScene* scene = find(name);
        if (scene == nullptr)
            return nullptr;
        std::unique_ptr<OFRender::FacePreview>& head = mHeads[name];
        if (head == nullptr)
            head = std::make_unique<OFRender::FacePreview>(*scene, "head", mResourceSystem);
        return head.get();
    }

    void MenuScenes::destroy(const std::string& name)
    {
        mHeads.erase(name);
        const auto it = mScenes.find(name);
        if (it == mScenes.end())
            return;
        it->second->stop();
        mScenes.erase(it);
    }

    void MenuScenes::clear()
    {
        mHeads.clear();
        for (auto& [name, scene] : mScenes)
            scene->stop();
        mScenes.clear();
    }
}
