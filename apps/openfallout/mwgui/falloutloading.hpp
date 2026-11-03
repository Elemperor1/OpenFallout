#ifndef OPENFALLOUT_MWGUI_FALLOUTLOADING_H
#define OPENFALLOUT_MWGUI_FALLOUTLOADING_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <components/falloutscript/loadingmenu.hpp>
#include <components/falloutscript/loadscreens.hpp>

namespace MyGUI
{
    class ImageBox;
    class Widget;
}

namespace ESM4
{
    struct LoadScreen;
}

namespace OFWorld
{
    class ESMStore;
}

namespace OFGui
{
    /// The loading screen of Fallout 3 and New Vegas: the pictures and the tips of the game's own load screen records
    /// laid out on the game's own loading menu (`loading_menu.xml`), drawn with widgets over the last frame of the
    /// world (Mac round 12AR C2). It does nothing, and the loading screen of the engine stays what it is, when the data
    /// has no load screens or no loading menu.
    class FalloutLoading
    {
    public:
        FalloutLoading();
        ~FalloutLoading();

        /// The store the load screens are read from (it is filled while the data loads)
        void setStore(const OFWorld::ESMStore& store) { mStore = &store; }

        /// A load begins at `now` (seconds). `mainMenu` is a load from the main menu (its own pictures, a picture every
        /// four seconds), `interior` a load of an interior cell (no picture: the pinwheel alone). False if there is
        /// nothing to show, which leaves the loading screen to the engine.
        bool begin(bool mainMenu, bool interior, double now);
        /// Move the pictures on and draw the menu at `now`
        void update(double now);
        /// The load is over
        void end();

        bool active() const { return mActive; }
        /// How much of the last frame of the world still shows under the menu (1 to 0)
        float worldAlpha() const { return mWorldAlpha; }

    private:
        struct Shape
        {
            MyGUI::ImageBox* mBox = nullptr;
            std::string mKey;
        };

        void readPool();
        bool makeMenu();
        void showPicture(std::size_t slide, std::size_t pool);
        void draw();
        Shape& shape(std::size_t index);

        const OFWorld::ESMStore* mStore = nullptr;
        std::shared_ptr<FalloutMenu::Resources> mResources;
        std::unique_ptr<FalloutScript::LoadScreens::LoadingMenu> mMenu;
        bool mMenuFailed = false;

        std::vector<FalloutScript::LoadScreens::Picture> mPool;
        std::vector<const ESM4::LoadScreen*> mRecords;
        std::size_t mPoolSource = 0;
        FalloutScript::LoadScreens::TipLayout mDefaultLayout;

        std::vector<std::size_t> mChosen;
        FalloutScript::LoadScreens::SlideShow mShow;
        bool mActive = false;
        double mStart = 0;
        float mWorldAlpha = 1.f;

        MyGUI::Widget* mRoot = nullptr;
        std::vector<Shape> mShapes;
    };
}

#endif
