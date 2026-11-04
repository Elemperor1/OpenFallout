#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOADINGMENU_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_LOADINGMENU_H

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <components/falloutmenu/openmenu.hpp>

#include "loadscreens.hpp"

namespace FalloutScript::LoadScreens
{
    /// The game's own loading menu (`loading_menu.xml`) with what the executable does to it while a place loads (Mac
    /// round 12AR C2): two slides, each a picture with its tip on a black matte, lie over one another and are shown in
    /// turn; the pictures and the tips are the code's, the way they are drawn is the file's
    class LoadingMenu
    {
    public:
        /// The scale of the wheel (fLoadingWheelScale: 2 in the executable)
        static constexpr float wheelScale = 2.f;

        /// Nothing if the file is not in the data, with the reason in `error`
        static std::unique_ptr<LoadingMenu> open(
            std::shared_ptr<FalloutMenu::Resources> resources, float pixelWidth, float pixelHeight, std::string* error);

        /// Put a picture (`icon`, as the record writes it, under Textures) and its tip on a slide (0 or 1), with the
        /// tip laid out by `layout`
        void setSlide(std::size_t slide, const std::string& icon, const std::string& tip, const TipLayout& layout);

        /// Take the pictures and tips off the slides
        void clear();

        /// How much each slide shows (0 to 255; the picture, its tip and the matte behind the tip all show that much)
        /// and how much of the whole menu shows (0 to 1, while it fades in over the last picture of the world)
        void setAlphas(const std::array<float, 2>& slides, float menu);

        /// The size of the window in pixels changed: the tips are laid out again
        void resize(float pixelWidth, float pixelHeight);

        /// Everything to draw, back to front, in pixels of the window
        std::vector<FalloutMenu::Piece> pieces() { return mMenu->pieces(); }

        float pixelWidth() const { return mMenu->pixelWidth(); }
        float pixelHeight() const { return mMenu->pixelHeight(); }
        /// The size of the screen in the units the menu is laid out in
        float designWidth() { return mMenu->menu().designWidth(); }
        float designHeight() { return mMenu->menu().designHeight(); }

        /// The wheel of the loading screen: a model (`loading01.nif`) that the file places by its tile `loading_pinwheel`.
        /// The tile's x and y are where the model's origin stands, in the units of the menu, and the scale is the tile's
        /// `user0` (the executable's fLoadingWheelScale, 2), which the code writes.
        struct Pinwheel
        {
            bool mPresent = false;
            float mX = 0;
            float mY = 0;
            float mScale = 2;
        };
        Pinwheel pinwheel();

        /// The safe zone of the screen: what the tips keep clear at the edges, in units of the menu
        static constexpr float crop = 15.f;

    private:
        struct SlideTiles
        {
            FalloutMenu::Tile* mPicture = nullptr;
            FalloutMenu::Tile* mTip = nullptr;
            FalloutMenu::Tile* mMatte = nullptr;
            bool mShown = false;
            TipLayout mLayout;
        };

        LoadingMenu() = default;
        void layoutTip(SlideTiles& slide);

        std::unique_ptr<FalloutMenu::OpenMenu> mMenu;
        std::array<SlideTiles, 2> mSlides;
        FalloutMenu::Tile* mPinwheel = nullptr;
    };
}

#endif
