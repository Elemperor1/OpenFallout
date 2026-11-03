#include "loadingmenu.hpp"

#include <algorithm>

namespace FalloutScript::LoadScreens
{
    namespace
    {
        FalloutMenu::Tile* named(FalloutMenu::Menu& menu, std::string_view name)
        {
            return menu.find(name);
        }

        void put(FalloutMenu::Menu& menu, FalloutMenu::Tile& tile, std::string_view trait, float value)
        {
            menu.set(tile, trait, FalloutMenu::Value(value));
        }

        // The tile of a slide fades with the slide: the picture, the tip and the matte show as much as the slide does
        float shown(float alpha, float menu)
        {
            return std::clamp(alpha, 0.f, 255.f) * std::clamp(menu, 0.f, 1.f);
        }
    }

    std::unique_ptr<LoadingMenu> LoadingMenu::open(
        std::shared_ptr<FalloutMenu::Resources> resources, float pixelWidth, float pixelHeight, std::string* error)
    {
        std::unique_ptr<FalloutMenu::OpenMenu> menu = FalloutMenu::OpenMenu::open(
            std::move(resources), "loading_menu.xml", pixelWidth, pixelHeight, false, error);
        if (!menu)
            return nullptr;

        std::unique_ptr<LoadingMenu> result(new LoadingMenu);
        result->mMenu = std::move(menu);
        FalloutMenu::Menu& file = result->mMenu->menu();
        const std::array<std::array<std::string_view, 3>, 2> names{
            { { "loading_tile_slide_01", "tip_text_01", "tip_matte_01" },
                { "loading_tile_slide_02", "tip_text_02", "tip_matte_02" } }
        };
        for (std::size_t slide = 0; slide < 2; ++slide)
        {
            SlideTiles& tiles = result->mSlides[slide];
            tiles.mPicture = named(file, names[slide][0]);
            tiles.mTip = named(file, names[slide][1]);
            tiles.mMatte = named(file, names[slide][2]);
            if (tiles.mPicture == nullptr || tiles.mTip == nullptr || tiles.mMatte == nullptr)
            {
                if (error != nullptr)
                    *error = "loading_menu.xml has no tile named " + std::string(names[slide][0]) + ", "
                        + std::string(names[slide][1]) + " or " + std::string(names[slide][2]);
                return nullptr;
            }
        }
        // The world image is the last frame of the world, which the engine draws under the menu; the groups of text for
        // the other types of load screen (the objective, the level, the stats, the plate) are built hidden and the tips
        // are all that the data of the games asks for
        for (const std::string_view name : { "loading_world_image", "loading_info_rect", "loading_currobj_rect",
                 "loading_stat_rect", "loading_levelprog_rect", "loading_plateno_rect" })
        {
            if (FalloutMenu::Tile* tile = named(file, name))
                file.set(*tile, "visible", FalloutMenu::Value(0.f));
        }
        return result;
    }

    void LoadingMenu::layoutTip(SlideTiles& slide)
    {
        FalloutMenu::Menu& file = mMenu->menu();
        const Window window = tipWindow(slide.mLayout, file.designWidth(), file.designHeight(), crop, crop);
        FalloutMenu::Tile& tip = *slide.mTip;
        // (the executable's layout of the tips, 0x78C770: x by the justification, the box of the wrap and the limit,
        // the font, the colour of the record and the brightness that lets the colour show)
        put(file, tip, "x", tipX(window, slide.mLayout.mJustify));
        put(file, tip, "y", window.mTop);
        put(file, tip, "width", window.width());
        put(file, tip, "height", window.height());
        put(file, tip, "wrapwidth", window.width());
        put(file, tip, "wraplimit", window.height());
        put(file, tip, "rotateangle", slide.mLayout.mRotate);
        put(file, tip, "font", static_cast<float>(slide.mLayout.mFont));
        put(file, tip, "justify", static_cast<float>(slide.mLayout.mJustify));
        put(file, tip, "red", slide.mLayout.mColor[0]);
        put(file, tip, "green", slide.mLayout.mColor[1]);
        put(file, tip, "blue", slide.mLayout.mColor[2]);
        put(file, tip, "brightness", -1.f);
        // The matte is a little above and below the box (not the text in it)
        put(file, *slide.mMatte, "y", window.mTop - 15.f);
        put(file, *slide.mMatte, "height", window.height() + 30.f);
    }

    void LoadingMenu::setSlide(
        std::size_t index, const std::string& icon, const std::string& tip, const TipLayout& layout)
    {
        if (index >= mSlides.size())
            return;
        SlideTiles& slide = mSlides[index];
        FalloutMenu::Menu& file = mMenu->menu();
        file.set(*slide.mPicture, "filename", FalloutMenu::Value::fromText("Textures\\" + icon));
        file.set(*slide.mTip, "string", FalloutMenu::Value::fromText(tip));
        slide.mLayout = layout;
        slide.mShown = true;
        // A picture that has no tip has no matte either
        const float hasTip = tip.empty() ? 0.f : 1.f;
        put(file, *slide.mTip, "visible", hasTip);
        put(file, *slide.mMatte, "visible", hasTip);
        layoutTip(slide);
    }

    void LoadingMenu::clear()
    {
        FalloutMenu::Menu& file = mMenu->menu();
        for (SlideTiles& slide : mSlides)
        {
            slide.mShown = false;
            put(file, *slide.mTip, "visible", 0.f);
            put(file, *slide.mMatte, "visible", 0.f);
            put(file, *slide.mPicture, "alpha", 0.f);
        }
    }

    void LoadingMenu::setAlphas(const std::array<float, 2>& slides, float menu)
    {
        FalloutMenu::Menu& file = mMenu->menu();
        for (std::size_t index = 0; index < mSlides.size(); ++index)
        {
            SlideTiles& slide = mSlides[index];
            const float alpha = slide.mShown ? shown(slides[index], menu) : 0.f;
            put(file, *slide.mPicture, "alpha", alpha);
            put(file, *slide.mTip, "alpha", alpha);
            put(file, *slide.mMatte, "alpha", alpha);
        }
    }

    void LoadingMenu::resize(float pixelWidth, float pixelHeight)
    {
        mMenu->resize(pixelWidth, pixelHeight);
        for (SlideTiles& slide : mSlides)
            if (slide.mShown)
                layoutTip(slide);
    }
}
