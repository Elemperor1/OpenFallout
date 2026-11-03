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
            picture.mLocal = local;
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
