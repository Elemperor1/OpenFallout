#ifndef OPENFALLOUT_MWGUI_FALLOUTLOADING_H
#define OPENFALLOUT_MWGUI_FALLOUTLOADING_H

#include <array>
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
        /// four seconds); `destination` is where a load of a place goes (pictures of the place come first). False if
        /// there is nothing to show, which leaves the loading screen to the engine.
        bool begin(bool mainMenu, const FalloutScript::LoadScreens::Destination& destination, double now);
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

        /// A picture of the wheel that turns, drawn with a rotating widget
        struct Turning
        {
            MyGUI::ImageBox* mBox = nullptr;
            std::string mTexture;
            int mWidth = 0;
            int mHeight = 0;
        };

        void readPool();
        bool makeMenu();
        void showPicture(std::size_t slide, std::size_t pool);
        void draw();
        Shape& shape(std::size_t index);
        /// The wheel of the loading screen, turning in the corner of the screen (`loading01.nif` of the game: the bars on
        /// both sides, the wheel and the ball over it), at `seconds` of the load and `alpha` of the whole menu
        void drawWheel(double seconds, float alpha);
        /// Puts a picture of the wheel (the file `file`, found in the folders of the game's textures) at `left`, `top` of the
        /// menu, `width` by `height` units, turned by `degrees` about the point `pivotX`, `pivotY` of it (all in units of
        /// the menu; `unitsX` and `unitsY` are the pixels in one unit)
        void turn(Turning& piece, const char* file, float left, float top, float width, float height, float pivotX,
            float pivotY, float degrees, float alpha, double unitsX, double unitsY);

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
        // The bars on the left and the right, the wheel and the ball
        std::array<Turning, 4> mWheel;
    };
}

#endif
