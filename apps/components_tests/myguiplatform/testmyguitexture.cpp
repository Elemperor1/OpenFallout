#include <components/myguiplatform/myguitexture.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/vfs/manager.hpp>

#include <gtest/gtest.h>

#include <osg/Image>
#include <osg/Texture2D>

namespace
{
    using namespace testing;

    struct MyGUIPlatformTextureTest : Test
    {
        const VFS::Manager mVfs;
        Resource::ImageManager mImageManager{ &mVfs, 0.0 };
    };

    TEST_F(MyGUIPlatformTextureTest, aTextureThatNoDataFileHasIsBlankAndNotTheWarningImage)
    {
        MyGUIPlatform::OSGTexture texture("textures/target.dds", &mImageManager);
        texture.loadFromFile("textures/target.dds");

        const osg::Texture2D* const loaded = texture.getTexture();
        ASSERT_NE(loaded, nullptr);
        ASSERT_NE(loaded->getImage(), nullptr);
        EXPECT_NE(loaded->getImage(), mImageManager.getWarningImage());
        EXPECT_GT(texture.getWidth(), 0);
        EXPECT_GT(texture.getHeight(), 0);
        for (int y = 0; y < loaded->getImage()->t(); ++y)
            for (int x = 0; x < loaded->getImage()->s(); ++x)
                EXPECT_FLOAT_EQ(loaded->getImage()->getColor(x, y).a(), 0.f) << x << ' ' << y;
    }
}
