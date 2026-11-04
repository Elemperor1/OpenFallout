#ifndef MWGUI_LOADINGSCREEN_H
#define MWGUI_LOADINGSCREEN_H

#include <cstdint>
#include <memory>

#include <osg/Timer>
#include <osg/ref_ptr>

#include "windowbase.hpp"

#include <components/loadinglistener/loadinglistener.hpp>

namespace osgViewer
{
    class Viewer;
}

namespace osg
{
    class Texture2D;
}

namespace Resource
{
    class ResourceSystem;
}

namespace OFWorld
{
    class ESMStore;
}

namespace OFGui
{
    class BackgroundImage;
    class CopyFramebufferToTextureCallback;
    class FalloutLoading;

    class LoadingScreen : public WindowBase, public Loading::Listener
    {
    public:
        LoadingScreen(Resource::ResourceSystem* resourceSystem, osgViewer::Viewer* viewer);
        virtual ~LoadingScreen();

        /// Overridden from Loading::Listener, see the Loading::Listener documentation for usage details
        void setLabel(const std::string& label, bool important) override;
        void setDestination(std::uint32_t object, std::uint32_t world, bool exterior, int gridX, int gridY) override;
        void loadingOn() override;
        void loadingOff() override;
        void setProgressRange(size_t range) override;
        void setProgress(size_t value) override;
        void increaseProgress(size_t increase = 1) override;

        void setVisible(bool visible) override;

        double getTargetFrameRate() const;

        /// The data of the game has load screens of its own: with them the screen shows their pictures and tips
        void setStore(const OFWorld::ESMStore& store);

    private:
        void findSplashScreens();
        bool needToDrawLoadingScreen();

        void setupCopyFramebufferToTextureCallback();

        Resource::ResourceSystem* mResourceSystem;
        osg::ref_ptr<osgViewer::Viewer> mViewer;

        double mTargetFrameRate;

        double mLastWallpaperChangeTime;
        double mLastRenderTime;
        osg::Timer mTimer;
        double mLoadingOnTime;

        bool mImportantLabel;
        /// Where the load that is about to begin goes (the pictures of a place are for it), until it begins
        struct Destination
        {
            bool mSet = false;
            std::uint32_t mObject = 0;
            std::uint32_t mWorld = 0;
            bool mExterior = false;
            int mGridX = 0;
            int mGridY = 0;
        };
        Destination mDestination;
        std::unique_ptr<FalloutLoading> mFalloutLoading;
        bool mFalloutActive = false;

        int mNestedLoadingCount;

        size_t mProgress;

        bool mShowWallpaper;
        double mOldIcoMin = 0.0;
        unsigned int mOldIcoMax = 0;

        MyGUI::Widget* mLoadingBox;

        MyGUI::TextBox* mLoadingText;
        MyGUI::ScrollBar* mProgressBar;
        BackgroundImage* mSplashImage;
        BackgroundImage* mSceneImage;

        std::vector<std::string> mSplashScreens;

        osg::ref_ptr<osg::Texture2D> mTexture;
        osg::ref_ptr<CopyFramebufferToTextureCallback> mCopyFramebufferToTextureCallback;
        std::unique_ptr<MyGUI::ITexture> mGuiTexture;

        void changeWallpaper();

        void draw();
    };

}

#endif
