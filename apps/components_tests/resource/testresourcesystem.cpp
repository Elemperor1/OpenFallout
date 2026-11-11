#include <components/files/configurationmanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/toutf8/toutf8.hpp>
#include <components/vfs/manager.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

namespace
{
    using namespace testing;

    // The shaders of the scene manager find their files and know the defines the models are loaded with
    void prepareShaders(Resource::ResourceSystem& resourceSystem)
    {
        Resource::SceneManager* sceneManager = resourceSystem.getSceneManager();
        const Files::ConfigurationManager configurationManager;
        sceneManager->setShaderPath(configurationManager.getLocalPath() / "resources/shaders");

        auto defines = Shader::getDefaultDefines();

        auto shadowDefines = SceneUtil::ShadowManager::getShadowsDisabledDefines();

        osg::ref_ptr<SceneUtil::LightManager> lightManager
            = new SceneUtil::LightManager(SceneUtil::LightSettings{}, &resourceSystem);
        auto lightDefines = lightManager->getLightDefines();

        for (const auto& define : shadowDefines)
            defines[define.first] = define.second;
        for (const auto& define : lightDefines)
            defines[define.first] = define.second;

        sceneManager->getShaderManager().setGlobalDefines(defines);
    }

    TEST(ResourceResourceSystem, scenemanager_getinstance_should_be_thread_safe)
    {
        const VFS::Manager vfsManager;
        const ToUTF8::Utf8Encoder encoder(ToUTF8::WINDOWS_1252);
        Resource::ResourceSystem resourceSystem(&vfsManager, 1.0, &encoder.getStatelessEncoder());
        Resource::SceneManager* sceneManager = resourceSystem.getSceneManager();
        prepareShaders(resourceSystem);

        constexpr VFS::Path::NormalizedView noSuchPath("meshes/whatever.nif");
        std::vector<std::thread> threads;

        for (int i = 0; i < 50; ++i)
        {
            threads.emplace_back([=]() { sceneManager->getInstance(noSuchPath); });
        }
        for (std::thread& thread : threads)
            thread.join();
    }

    TEST(ResourceResourceSystem, a_model_whose_shapes_are_kept_is_loaded_apart_from_then_on)
    {
        const VFS::Manager vfsManager;
        const ToUTF8::Utf8Encoder encoder(ToUTF8::WINDOWS_1252);
        Resource::ResourceSystem resourceSystem(&vfsManager, 1.0, &encoder.getStatelessEncoder());
        Resource::SceneManager* sceneManager = resourceSystem.getSceneManager();
        prepareShaders(resourceSystem);

        constexpr VFS::Path::NormalizedView card("meshes/clutter/card.nif");
        constexpr VFS::Path::NormalizedView other("meshes/clutter/other.nif");
        const osg::ref_ptr<const osg::Node> ordinary = sceneManager->getTemplate(card);
        const osg::ref_ptr<const osg::Node> otherOrdinary = sceneManager->getTemplate(other);
        const osg::ref_ptr<const osg::Node> apart = sceneManager->getTemplate(card, true, true);
        ASSERT_NE(ordinary.get(), nullptr);
        ASSERT_NE(apart.get(), nullptr);
        EXPECT_NE(ordinary.get(), apart.get());
        // Asked for as before, the model of the file is the one that keeps no shapes apart
        EXPECT_EQ(sceneManager->getTemplate(card).get(), ordinary.get());

        sceneManager->keepShapesOf(card);
        EXPECT_EQ(sceneManager->getTemplate(card).get(), apart.get());
        EXPECT_EQ(sceneManager->getTemplate(card, true, true).get(), apart.get());
        // Only the model that was named
        EXPECT_EQ(sceneManager->getTemplate(other).get(), otherOrdinary.get());
    }
}
