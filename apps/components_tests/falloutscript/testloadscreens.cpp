#include <components/falloutscript/loadscreens.hpp>

#include <gtest/gtest.h>

#include <cstring>

namespace
{
    namespace LoadScreens = FalloutScript::LoadScreens;
    using LoadScreens::Picture;
    using LoadScreens::SlideShow;

    // The one load screen type of New Vegas: tips, in a box of 850 by 400 at (210, 100), font 6, orange, centred
    std::array<std::uint8_t, 88> tipData()
    {
        std::array<std::uint8_t, 88> data{};
        const auto put32 = [&data](std::size_t offset, std::uint32_t value) {
            std::memcpy(data.data() + offset, &value, sizeof(value));
        };
        const auto putFloat
            = [&data](std::size_t offset, float value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
        put32(0x00, 3);
        put32(0x04, 210);
        put32(0x08, 100);
        put32(0x0C, 850);
        put32(0x10, 400);
        putFloat(0x14, 0.f);
        put32(0x18, 6);
        putFloat(0x1C, 255.f);
        putFloat(0x20, 180.f);
        putFloat(0x24, 53.f);
        put32(0x28, 2);
        return data;
    }

    TEST(FalloutScriptLoadScreensTest, the_data_of_the_tip_type_gives_the_box_the_font_and_the_colour)
    {
        const auto layout = LoadScreens::readTipLayout(tipData());
        ASSERT_TRUE(layout.has_value());
        EXPECT_FLOAT_EQ(layout->mX, 210.f);
        EXPECT_FLOAT_EQ(layout->mWidth, 850.f);
        EXPECT_FLOAT_EQ(layout->mHeight, 400.f);
        EXPECT_EQ(layout->mFont, 6);
        EXPECT_EQ(layout->mJustify, 2);
        EXPECT_FLOAT_EQ(layout->mColor[1], 180.f);
    }

    TEST(FalloutScriptLoadScreensTest, a_type_that_is_not_the_tips_gives_no_layout)
    {
        std::array<std::uint8_t, 88> data = tipData();
        data[0] = 4;
        EXPECT_FALSE(LoadScreens::readTipLayout(data).has_value());
    }

    TEST(FalloutScriptLoadScreensTest, the_tip_box_is_stretched_to_the_screen_and_kept_in_the_safe_zone)
    {
        const auto layout = *LoadScreens::readTipLayout(tipData());
        // A screen of 1280 by 960 units with the zone of 15: the box is where the record says
        LoadScreens::Window window = LoadScreens::tipWindow(layout, 1280.f, 960.f, 15.f, 15.f);
        EXPECT_FLOAT_EQ(window.mLeft, 210.f);
        EXPECT_FLOAT_EQ(window.mTop, 100.f);
        EXPECT_FLOAT_EQ(window.mRight, 1060.f);
        EXPECT_FLOAT_EQ(window.mBottom, 500.f);
        EXPECT_FLOAT_EQ(LoadScreens::tipX(window, 2), 635.f);
        EXPECT_FLOAT_EQ(LoadScreens::tipX(window, 1), 210.f);
        EXPECT_FLOAT_EQ(LoadScreens::tipX(window, 4), 1060.f);
        // A wider screen moves the left edge with the width (not the width of the box)
        window = LoadScreens::tipWindow(layout, 1707.f, 960.f, 15.f, 15.f);
        EXPECT_NEAR(window.mLeft, 280.f, 0.2f);
        EXPECT_NEAR(window.width(), 850.f, 0.2f);
        // A box that does not fit is cut at the edge of the zone
        LoadScreens::TipLayout wide = layout;
        wide.mX = 0;
        wide.mWidth = 5000;
        window = LoadScreens::tipWindow(wide, 1280.f, 960.f, 15.f, 15.f);
        EXPECT_FLOAT_EQ(window.mLeft, 15.f);
        EXPECT_FLOAT_EQ(window.mRight, 1265.f);
    }

    std::vector<Picture> pool()
    {
        std::vector<Picture> pictures;
        const auto add = [&pictures](const char* icon, bool mainMenu, bool local) {
            Picture picture;
            picture.mIcon = icon;
            picture.mText = icon;
            picture.mMainMenu = mainMenu;
            if (local)
                picture.mPlaces.push_back({ 0x100, 0, 0, 0 });
            pictures.push_back(picture);
        };
        add("interface\\loading\\a.dds", false, false);
        add("interface\\loading\\b.dds", true, false);
        add("interface\\loading\\c.dds", false, true);
        add("Interface\\Loading\\A.dds", false, false);
        add("interface\\loading\\d.dds", false, false);
        add("interface\\loading\\e.dds", false, false);
        add("interface\\loading\\f.dds", false, false);
        add("", false, false);
        return pictures;
    }

    TEST(FalloutScriptLoadScreensTest, the_pictures_are_general_ones_in_a_random_order_with_no_file_twice)
    {
        const std::vector<Picture> pictures = pool();
        // The rolls take the first of what is left each time
        const auto first = [](int low, int) { return low; };
        const std::vector<std::size_t> chosen = LoadScreens::choosePictures(pictures, false, 4, first);
        // a, b (the local c and the one with no file are not candidates), the other a is the same file as the first and
        // is dropped when it comes, then d and e
        ASSERT_EQ(chosen.size(), 4u);
        EXPECT_EQ(chosen[0], 0u);
        EXPECT_EQ(chosen[1], 1u);
        EXPECT_EQ(chosen[2], 4u);
        EXPECT_EQ(chosen[3], 5u);
        // The last of what is left each time
        const auto last = [](int, int high) { return high; };
        const std::vector<std::size_t> other = LoadScreens::choosePictures(pictures, false, 2, last);
        ASSERT_EQ(other.size(), 2u);
        EXPECT_EQ(other[0], 6u);
        EXPECT_EQ(other[1], 5u);
    }

    // A pool of pictures for places: g and h are general, i is for the interior 0x500, j for the world 0x900, k for the grid
    // cell (3, -4) of the world 0x900, l for the interior 0x500 too (and with the same file as i)
    std::vector<Picture> placePool()
    {
        std::vector<Picture> pictures;
        const auto add = [&pictures](const char* icon, std::vector<LoadScreens::Place> places) {
            Picture picture;
            picture.mIcon = icon;
            picture.mText = icon;
            picture.mPlaces = std::move(places);
            pictures.push_back(std::move(picture));
        };
        add("g.dds", {});
        add("h.dds", {});
        add("i.dds", { { 0x500, 0, 0, 0 } });
        add("j.dds", { { 0x900, 0, 0, 0 } });
        add("k.dds", { { 0, 0x900, 3, -4 } });
        add("I.dds", { { 0x500, 0, 0, 0 } });
        add("m.dds", {});
        add("n.dds", {});
        return pictures;
    }

    TEST(FalloutScriptLoadScreensTest, a_picture_is_for_the_cell_of_an_interior_or_the_world_of_an_exterior)
    {
        const std::vector<Picture> pictures = placePool();
        LoadScreens::Destination interior;
        interior.mSet = true;
        interior.mObject = 0x500;
        EXPECT_FALSE(LoadScreens::isForPlace(pictures[0], interior));
        EXPECT_TRUE(LoadScreens::isForPlace(pictures[2], interior));
        EXPECT_FALSE(LoadScreens::isForPlace(pictures[3], interior));
        LoadScreens::Destination exterior;
        exterior.mSet = true;
        exterior.mObject = 0x900;
        exterior.mWorld = 0x900;
        exterior.mExterior = true;
        exterior.mGridX = 3;
        exterior.mGridY = -4;
        EXPECT_TRUE(LoadScreens::isForPlace(pictures[3], exterior));
        // The grid cell of the world matches when the player arrives in it
        EXPECT_TRUE(LoadScreens::isForPlace(pictures[4], exterior));
        exterior.mGridX = 4;
        EXPECT_FALSE(LoadScreens::isForPlace(pictures[4], exterior));
        // A destination that was not set is no place
        EXPECT_FALSE(LoadScreens::isForPlace(pictures[2], LoadScreens::Destination()));
        // A picture whose record names the interior cell in the indirect field is for it as well
        Picture indirect;
        indirect.mPlaces = { { 0, 0x500, 0, 0 } };
        EXPECT_TRUE(LoadScreens::isForPlace(indirect, interior));
        interior.mObject = 0x501;
        EXPECT_FALSE(LoadScreens::isForPlace(indirect, interior));
        // A grid does not match in an interior
        LoadScreens::Destination other;
        other.mSet = true;
        other.mObject = 0x777;
        other.mWorld = 0x900;
        other.mGridX = 3;
        other.mGridY = -4;
        EXPECT_FALSE(LoadScreens::isForPlace(pictures[4], other));
    }

    TEST(FalloutScriptLoadScreensTest, a_load_to_a_place_takes_one_picture_of_the_place_and_general_ones_for_the_rest)
    {
        const std::vector<Picture> pictures = placePool();
        const auto first = [](int low, int) { return low; };
        LoadScreens::Destination interior;
        interior.mSet = true;
        interior.mObject = 0x500;
        const std::vector<std::size_t> chosen = LoadScreens::choosePictures(pictures, false, interior, 4, 1, first);
        // one for the place (the other with the same file is not taken), then the general ones in order
        ASSERT_EQ(chosen.size(), 4u);
        EXPECT_EQ(chosen[0], 2u);
        EXPECT_EQ(chosen[1], 0u);
        EXPECT_EQ(chosen[2], 1u);
        EXPECT_EQ(chosen[3], 6u);
        // More pictures of the place when the executable's setting says so (the file taken is not taken twice)
        const std::vector<std::size_t> more = LoadScreens::choosePictures(pictures, false, interior, 4, 3, first);
        ASSERT_EQ(more.size(), 4u);
        EXPECT_EQ(more[0], 2u);
        EXPECT_EQ(more[1], 0u);
        // No place picture for the place: general ones only
        LoadScreens::Destination nowhere;
        nowhere.mSet = true;
        nowhere.mObject = 0x777;
        const std::vector<std::size_t> general = LoadScreens::choosePictures(pictures, false, nowhere, 3, 1, first);
        ASSERT_EQ(general.size(), 3u);
        EXPECT_EQ(general[0], 0u);
        // A load that goes nowhere takes general pictures
        const std::vector<std::size_t> none
            = LoadScreens::choosePictures(pictures, false, LoadScreens::Destination(), 4, 1, first);
        ASSERT_EQ(none.size(), 4u);
        EXPECT_EQ(none[0], 0u);
        EXPECT_EQ(none[3], 7u);
        // The main menu takes its own whatever the place
        std::vector<Picture> menu = placePool();
        menu[1].mMainMenu = true;
        const std::vector<std::size_t> fromMenu = LoadScreens::choosePictures(menu, true, interior, 20, 1, first);
        ASSERT_EQ(fromMenu.size(), 1u);
        EXPECT_EQ(fromMenu[0], 1u);
    }

    TEST(FalloutScriptLoadScreensTest, the_main_menu_takes_the_pictures_that_are_marked_for_it)
    {
        const std::vector<Picture> pictures = pool();
        const auto first = [](int low, int) { return low; };
        const std::vector<std::size_t> chosen = LoadScreens::choosePictures(pictures, true, 20, first);
        ASSERT_EQ(chosen.size(), 1u);
        EXPECT_EQ(chosen[0], 1u);
        EXPECT_TRUE(LoadScreens::choosePictures({}, false, 4, first).empty());
    }

    TEST(FalloutScriptLoadScreensTest,
        the_first_picture_is_shown_at_once_and_the_next_fades_in_over_it_after_ten_seconds)
    {
        SlideShow show;
        show.start(100.0);
        EXPECT_EQ(show.count(), 1u);
        EXPECT_FLOAT_EQ(show.alphas(100.0)[0], 255.f);
        EXPECT_FLOAT_EQ(show.alphas(100.0)[1], 0.f);
        EXPECT_FALSE(show.advance(109.9));
        EXPECT_TRUE(show.advance(110.0));
        EXPECT_EQ(show.count(), 2u);
        // The second picture is on the other tile and begins from nothing
        EXPECT_FLOAT_EQ(show.alphas(110.0)[0], 255.f);
        EXPECT_FLOAT_EQ(show.alphas(110.0)[1], 0.f);
        // Halfway through the 2.222 s the two are even
        const std::array<float, 2> half = show.alphas(110.0 + LoadScreens::crossFadeSeconds / 2);
        EXPECT_NEAR(half[0], 127.5f, 0.5f);
        EXPECT_NEAR(half[1], 127.5f, 0.5f);
        const std::array<float, 2> done = show.alphas(113.0);
        EXPECT_FLOAT_EQ(done[0], 0.f);
        EXPECT_FLOAT_EQ(done[1], 255.f);
        // The third goes back to the first tile
        EXPECT_TRUE(show.advance(120.0));
        EXPECT_FLOAT_EQ(show.alphas(125.0)[0], 255.f);
        EXPECT_FLOAT_EQ(show.alphas(125.0)[1], 0.f);
        EXPECT_EQ(show.newest(), 0u);
    }

    TEST(FalloutScriptLoadScreensTest, a_slide_show_that_was_not_started_shows_nothing)
    {
        SlideShow show;
        EXPECT_FALSE(show.advance(1000.0));
        EXPECT_FLOAT_EQ(show.alphas(1000.0)[0], 0.f);
        EXPECT_FLOAT_EQ(show.alphas(1000.0)[1], 0.f);
    }
}
