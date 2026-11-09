#include <components/falloutmenu/openmenu.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace
{
    using namespace FalloutMenu;

    constexpr std::size_t headerBytes = 296;
    constexpr std::size_t glyphBytes = 56;

    void putFloat(std::string& bytes, std::size_t at, float value)
    {
        std::memcpy(bytes.data() + at, &value, sizeof(value));
    }

    // A font of 30 high lines whose characters are 10 wide with 1 before and 1 after, cut from a 100 by 100 texture
    std::string testFont()
    {
        std::string bytes(headerBytes + 256 * glyphBytes, '\0');
        putFloat(bytes, 0, 30.f);
        const unsigned int one = 1;
        std::memcpy(bytes.data() + 4, &one, sizeof(one));
        std::memcpy(bytes.data() + 12, "Test_Font_0", 11);
        for (unsigned char c = 33; c < 127; ++c)
        {
            const std::size_t at = bytes.size() - 256 * glyphBytes + c * glyphBytes;
            putFloat(bytes, at + 4, 0.1f);
            putFloat(bytes, at + 8, 0.2f);
            putFloat(bytes, at + 12, 0.2f);
            putFloat(bytes, at + 24, 0.4f);
            putFloat(bytes, at + 36, 10.f);
            putFloat(bytes, at + 40, 20.f);
            putFloat(bytes, at + 44, 1.f);
            putFloat(bytes, at + 48, 1.f);
            putFloat(bytes, at + 52, 20.f);
        }
        return bytes;
    }

    // The first bytes of a DDS picture of a size
    std::string dds(std::uint32_t width, std::uint32_t height)
    {
        std::string bytes(128, '\0');
        std::memcpy(bytes.data(), "DDS ", 4);
        const std::uint32_t header = 124;
        std::memcpy(bytes.data() + 4, &header, 4);
        std::memcpy(bytes.data() + 12, &height, 4);
        std::memcpy(bytes.data() + 16, &width, 4);
        return bytes;
    }

    struct Fixture
    {
        std::map<std::string, std::string> mFiles;
        std::shared_ptr<Resources> mResources;
        std::unique_ptr<OpenMenu> mMenu;

        Fixture()
        {
            mFiles["textures/fonts/glow_monofonto_large.fnt"] = testFont();
            mFiles["textures/fonts/test_font_0.dds"] = dds(100, 100);
            mFiles["textures/interface/interfaceshared.tai"]
                = "small.dds InterfaceShared0.dds, 0, 0, 0.5, 0.25, 0, 0.25, 0.5\n";
            mFiles["textures/interface/interfaceshared0.dds"] = dds(1024, 1024);
            mFiles["textures/interface/main/panel.dds"] = dds(64, 32);
            mFiles["textures/interface/main/strip.dds"] = dds(20, 8);
            mFiles["menus/globals.xml"] = "<globals/>";
        }

        OpenMenu& open(const std::string& menu, float width = 1280.f, float height = 960.f, bool pad = false)
        {
            mFiles["menus/test_menu.xml"] = menu;
            Data data;
            data.mRead = [this](const std::string& path) -> std::optional<std::string> {
                const auto found = mFiles.find(path);
                if (found == mFiles.end())
                    return std::nullopt;
                return found->second;
            };
            data.mReadStart = [this](const std::string& path, std::size_t count) -> std::optional<std::string> {
                const auto found = mFiles.find(path);
                if (found == mFiles.end())
                    return std::nullopt;
                return found->second.substr(0, count);
            };
            data.mSetting = [](std::string_view name) -> std::optional<std::string> {
                if (name == "sHello")
                    return "Hi";
                return std::nullopt;
            };
            mResources = std::make_shared<Resources>(data);
            std::string error;
            mMenu = OpenMenu::open(mResources, "test_menu.xml", width, height, pad, &error);
            EXPECT_TRUE(mMenu) << error;
            return *mMenu;
        }
    };

    TEST(FalloutOpenMenuTest, a_dds_gives_its_size_and_other_bytes_do_not)
    {
        const std::optional<TextSize> size = ddsSize(dds(512, 256));
        ASSERT_TRUE(size);
        EXPECT_FLOAT_EQ(size->mWidth, 512.f);
        EXPECT_FLOAT_EQ(size->mHeight, 256.f);
        EXPECT_FALSE(ddsSize("not a picture at all"));
        EXPECT_FALSE(ddsSize(dds(0, 0)));
    }

    TEST(FalloutOpenMenuTest, a_texture_is_named_as_the_data_names_it)
    {
        EXPECT_EQ(texturePath("Interface\\Main\\Arrow.dds"), "textures/interface/main/arrow.dds");
        EXPECT_EQ(texturePath("  Interface\\Main\\Arrow.dds "), "textures/interface/main/arrow.dds");
        EXPECT_EQ(texturePath("Textures\\Fonts\\a.dds"), "textures/fonts/a.dds");
        EXPECT_EQ(texturePath("Data\\Textures\\a.dds"), "textures/a.dds");
    }

    TEST(FalloutOpenMenuTest, the_fonts_atlas_and_system_colours_come_with_the_resources)
    {
        Fixture f;
        f.open("<menu name=\"M\"/>");
        EXPECT_NE(f.mResources->fonts().get(1), nullptr);
        EXPECT_EQ(f.mResources->atlas().size(), 1u);
        const std::optional<std::array<float, 3>> color = f.mResources->systemColor(2);
        ASSERT_TRUE(color);
        EXPECT_FLOAT_EQ((*color)[0], 255.f);
        EXPECT_FLOAT_EQ((*color)[1], 67.f);
        EXPECT_FALSE(f.mResources->systemColor(7));
    }

    TEST(FalloutOpenMenuTest, new_vegas_makes_the_colours_of_its_display_pip_boy_and_menus_amber)
    {
        Fixture f;
        f.open("<menu name=\"M\"/>");
        // Fallout 3's are the INI's: green for the display, the Pip-Boy and the system, pale green for the main menu
        EXPECT_FLOAT_EQ((*f.mResources->systemColor(1))[0], 26.f);
        EXPECT_FLOAT_EQ((*f.mResources->systemColor(5))[0], 199.f);
        f.mResources->useNewVegasColors();
        for (const int id : { 1, 4, 5, 6 })
        {
            const std::optional<std::array<float, 3>> color = f.mResources->systemColor(id);
            ASSERT_TRUE(color);
            EXPECT_FLOAT_EQ((*color)[0], 255.f);
            EXPECT_FLOAT_EQ((*color)[1], 182.f);
            EXPECT_FLOAT_EQ((*color)[2], 66.f);
        }
        // the alternate display colour and the terminal's are not changed
        EXPECT_FLOAT_EQ((*f.mResources->systemColor(2))[1], 67.f);
        EXPECT_FLOAT_EQ((*f.mResources->systemColor(3))[0], 33.f);
    }

    TEST(FalloutOpenMenuTest, the_colours_wait_until_the_data_says_which_game_runs)
    {
        Fixture f;
        f.open("<menu name=\"M\"/>");
        std::optional<bool> newVegas;
        Data data = f.mResources->data();
        data.mNewVegas = [&newVegas]() { return newVegas; };
        const Resources resources(data);
        // (menus can open while the game still loads: the colours are the INI's until the answer is there)
        EXPECT_FLOAT_EQ((*resources.systemColor(5))[0], 199.f);
        newVegas = true;
        EXPECT_FLOAT_EQ((*resources.systemColor(5))[0], 255.f);
        EXPECT_FLOAT_EQ((*resources.systemColor(5))[1], 182.f);
        EXPECT_FLOAT_EQ((*resources.systemColor(1))[2], 66.f);
        // the answer is taken once
        newVegas = false;
        EXPECT_FLOAT_EQ((*resources.systemColor(5))[0], 255.f);

        std::optional<bool> fallout3 = false;
        Data other = f.mResources->data();
        other.mNewVegas = [&fallout3]() { return fallout3; };
        const Resources green(other);
        EXPECT_FLOAT_EQ((*green.systemColor(5))[0], 199.f);
        EXPECT_FLOAT_EQ((*green.systemColor(1))[0], 26.f);
    }

    TEST(FalloutOpenMenuTest, a_picture_goes_where_the_menu_puts_it_scaled_to_the_window)
    {
        Fixture f;
        // The window is half the design height, so every unit is half a pixel
        OpenMenu& menu = f.open(
            "<menu name=\"M\"><image name=\"I\"><x> 100 </x><y> 40 </y><width> 64 </width>"
            "<height> 32 </height><filename> Interface\\Main\\Panel.dds </filename>"
            "<zoom> &scale; </zoom></image></menu>",
            640.f, 480.f);
        const std::vector<Piece> pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 1u);
        EXPECT_EQ(pieces[0].mTexture, "textures/interface/main/panel.dds");
        EXPECT_EQ(pieces[0].mX, 50);
        EXPECT_EQ(pieces[0].mY, 20);
        EXPECT_EQ(pieces[0].mWidth, 32);
        EXPECT_EQ(pieces[0].mHeight, 16);
        EXPECT_EQ(pieces[0].mSourceWidth, 64);
        EXPECT_EQ(pieces[0].mSourceHeight, 32);
        EXPECT_FALSE(pieces[0].mGlyph);
        EXPECT_EQ(pieces[0].mTile->name(), "I");
    }

    TEST(FalloutOpenMenuTest, a_picture_the_data_lacks_is_left_out_and_told)
    {
        Fixture f;
        OpenMenu& menu = f.open(
            "<menu name=\"M\"><image name=\"I\"><width> 64 </width><height> 32 </height>"
            "<filename> Interface\\Main\\Gone.dds </filename></image></menu>");
        EXPECT_TRUE(menu.pieces().empty());
        EXPECT_EQ(f.mResources->missingTextures().count("textures/interface/main/gone.dds"), 1u);
    }

    TEST(FalloutOpenMenuTest, a_picture_from_the_atlas_takes_its_part_of_the_big_texture)
    {
        Fixture f;
        OpenMenu& menu = f.open(
            "<menu name=\"M\"><image name=\"I\"><x> 10 </x><y> 10 </y><zoom> &scale; </zoom>"
            "<width> 40 </width><height> 20 </height><filename> Interface\\Main\\small.dds"
            "</filename><texatlas> Interface\\InterfaceShared.tai </texatlas></image></menu>");
        const std::vector<Piece> pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 1u);
        EXPECT_EQ(pieces[0].mTexture, "textures/interface/interfaceshared0.dds");
        EXPECT_EQ(pieces[0].mSourceX, 512);
        EXPECT_EQ(pieces[0].mSourceY, 256);
        EXPECT_EQ(pieces[0].mSourceWidth, 256);
        EXPECT_EQ(pieces[0].mSourceHeight, 512);
    }

    TEST(FalloutOpenMenuTest, a_picture_that_repeats_is_cut_into_pieces_at_its_natural_size)
    {
        Fixture f;
        OpenMenu& menu = f.open(
            "<menu name=\"M\"><image name=\"I\"><width> 50 </width><height> 8 </height>"
            "<filename> Interface\\Main\\Strip.dds </filename><repeathorizontal> 1 "
            "</repeathorizontal></image></menu>");
        const std::vector<Piece> pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 3u);
        EXPECT_EQ(pieces[0].mX, 0);
        EXPECT_EQ(pieces[0].mWidth, 20);
        EXPECT_EQ(pieces[1].mX, 20);
        EXPECT_EQ(pieces[2].mX, 40);
        // The last one is cut short, and so is the part of the texture it shows
        EXPECT_EQ(pieces[2].mWidth, 10);
        EXPECT_EQ(pieces[2].mSourceWidth, 10);
        EXPECT_EQ(pieces[0].mSourceWidth, 20);
    }

    TEST(FalloutOpenMenuTest, a_picture_that_scrolls_shows_a_window_of_its_texture_and_goes_on_at_the_left_edge)
    {
        Fixture f;
        // The strip is 20 texels wide; the window is 12 wide and starts at texel 15, so it shows 15..19 and then 0..6
        OpenMenu& menu = f.open(
            "<menu name=\"M\"><image name=\"I\"><x> 3 </x><width> 12 </width><height> 8 </height>"
            "<filename> Interface\\Main\\Strip.dds </filename><_sourcex> 15 </_sourcex></image></menu>");
        std::vector<Piece> pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 2u);
        EXPECT_EQ(pieces[0].mX, 3);
        EXPECT_EQ(pieces[0].mWidth, 5);
        EXPECT_EQ(pieces[0].mSourceX, 15);
        EXPECT_EQ(pieces[0].mSourceWidth, 5);
        EXPECT_EQ(pieces[1].mX, 8);
        EXPECT_EQ(pieces[1].mWidth, 7);
        EXPECT_EQ(pieces[1].mSourceX, 0);
        EXPECT_EQ(pieces[1].mSourceWidth, 7);
        // Scrolled by whole turns and by a negative number it is the same window
        menu.menu().set(*menu.menu().find("I"), "_sourcex", Value(-5.f));
        pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 2u);
        EXPECT_EQ(pieces[0].mSourceX, 15);
        // A window that does not run past the edge is one piece
        menu.menu().set(*menu.menu().find("I"), "_sourcex", Value(4.f));
        pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 1u);
        EXPECT_EQ(pieces[0].mSourceX, 4);
        EXPECT_EQ(pieces[0].mSourceWidth, 12);
    }

    TEST(FalloutOpenMenuTest, a_text_is_one_piece_for_each_character_cut_from_the_font_texture)
    {
        Fixture f;
        OpenMenu& menu = f.open(
            "<menu name=\"M\"><text name=\"T\"><x> 20 </x><y> 10 </y><font> 1 </font>"
            "<string> &-sHello; </string></text></menu>");
        const std::vector<Piece> pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 2u);
        EXPECT_TRUE(pieces[0].mGlyph);
        EXPECT_EQ(pieces[0].mTexture, "textures/fonts/test_font_0.dds");
        // Each character is 10 wide and 20 high, the pen moves 12 for it; the baseline is 20 below the top
        EXPECT_EQ(pieces[0].mWidth, 10);
        EXPECT_EQ(pieces[0].mHeight, 20);
        EXPECT_EQ(pieces[1].mX - pieces[0].mX, 12);
        EXPECT_EQ(pieces[0].mSourceX, 10);
        EXPECT_EQ(pieces[0].mSourceY, 20);
        EXPECT_EQ(pieces[0].mSourceWidth, 10);
        EXPECT_EQ(pieces[0].mSourceHeight, 20);
    }

    TEST(FalloutOpenMenuTest, a_piece_in_a_clip_window_carries_the_window_in_pixels)
    {
        Fixture f;
        OpenMenu& menu = f.open(
            "<menu name=\"M\"><rect name=\"W\"><x> 10 </x><y> 10 </y><width> 100 </width>"
            "<height> 60 </height><clipwindow> 1 </clipwindow><image name=\"I\"><clips> 1 </clips>"
            "<width> 64 </width><height> 32 </height><filename> Interface\\Main\\Panel.dds"
            "</filename><zoom> &scale; </zoom></image></rect></menu>");
        const std::vector<Piece> pieces = menu.pieces();
        ASSERT_EQ(pieces.size(), 1u);
        EXPECT_TRUE(pieces[0].mClipped);
        EXPECT_EQ(pieces[0].mClipX, 10);
        EXPECT_EQ(pieces[0].mClipWidth, 100);
        EXPECT_EQ(pieces[0].mClipHeight, 60);
    }

    TEST(FalloutOpenMenuTest, a_menu_that_is_not_there_is_told_and_the_pad_sets_xbox)
    {
        Fixture f;
        f.open("<menu name=\"M\"><rect name=\"R\"><x> &xbox; </x></rect></menu>");
        std::string error;
        EXPECT_FALSE(OpenMenu::open(f.mResources, "nothing.xml", 1280.f, 960.f, false, &error));
        EXPECT_NE(error.find("menus/nothing.xml"), std::string::npos);
        EXPECT_FLOAT_EQ(f.mMenu->menu().number(*f.mMenu->menu().find("R"), "x"), 0.f);
        f.open("<menu name=\"M\"><rect name=\"R\"><x> &xbox; </x></rect></menu>", 1280.f, 960.f, true);
        EXPECT_FLOAT_EQ(f.mMenu->menu().number(*f.mMenu->menu().find("R"), "x"), 1.f);
    }

    const char* const buttons
        = "<menu name=\"M\">"
          "<hotrect name=\"A\"><x> 0 </x><y> 0 </y><width> 100 </width><height> 50 </height>"
          "<locus> 1 </locus><target> 1 </target><wheelable> 1 </wheelable></hotrect>"
          "<hotrect name=\"B\"><x> 200 </x><y> 0 </y><width> 100 </width><height> 50 </height>"
          "<locus> 1 </locus><target> 1 </target></hotrect></menu>";

    TEST(FalloutOpenMenuTest, the_tile_under_the_pointer_has_mouseover_and_the_one_it_left_does_not)
    {
        Fixture f;
        OpenMenu& open = f.open(buttons, 640.f, 480.f);
        Menu& menu = open.menu();
        Tile* a = menu.find("A");
        Tile* b = menu.find("B");
        EXPECT_EQ(open.pointerMoved(10, 10), a);
        EXPECT_FLOAT_EQ(menu.number(*a, "mouseover"), 1.f);
        EXPECT_EQ(open.pointerMoved(110, 10), b);
        EXPECT_FLOAT_EQ(menu.number(*a, "mouseover"), 0.f);
        EXPECT_FLOAT_EQ(menu.number(*b, "mouseover"), 1.f);
        EXPECT_EQ(open.pointerMoved(5, 400), nullptr);
        EXPECT_FLOAT_EQ(menu.number(*b, "mouseover"), 0.f);
    }

    TEST(FalloutOpenMenuTest, a_click_counts_up_on_the_tile_the_button_went_down_and_up_on)
    {
        Fixture f;
        OpenMenu& open = f.open(buttons, 640.f, 480.f);
        Menu& menu = open.menu();
        Tile* a = menu.find("A");
        open.pointerPressed(10, 10);
        EXPECT_EQ(open.pointerReleased(10, 10), a);
        open.pointerPressed(10, 10);
        EXPECT_EQ(open.pointerReleased(10, 10), a);
        EXPECT_FLOAT_EQ(menu.number(*a, "clicked"), 2.f);
        // Down on one and up on another is no click
        open.pointerPressed(10, 10);
        EXPECT_EQ(open.pointerReleased(110, 10), nullptr);
        EXPECT_FLOAT_EQ(menu.number(*a, "clicked"), 2.f);
        EXPECT_FLOAT_EQ(menu.number(*menu.find("B"), "clicked"), 0.f);
        open.click(*a);
        EXPECT_FLOAT_EQ(menu.number(*a, "clicked"), 3.f);
    }

    TEST(FalloutOpenMenuTest, a_hotrect_around_the_tile_under_the_pointer_is_over_and_clicked_as_well)
    {
        // As a button made by super_text_box: a hotrect with a box in it that takes the pointer too
        Fixture f;
        OpenMenu& open = f.open(
            "<menu name=\"M\"><hotrect name=\"Outer\"><x>10</x><y>10</y><width>100</width>"
            "<height>40</height><target>1</target><rect name=\"Box\"><width>100</width>"
            "<height>40</height><target>1</target></rect></hotrect>"
            "<hotrect name=\"Off\"><x>200</x><y>10</y><width>100</width><height>40</height>"
            "<target>0</target><rect name=\"InOff\"><width>100</width><height>40</height>"
            "<target>1</target></rect></hotrect></menu>",
            640.f, 480.f);
        Menu& menu = open.menu();
        Tile* outer = menu.find("Outer");
        Tile* box = menu.find("Box");
        EXPECT_EQ(open.pointerMoved(20, 20), box);
        EXPECT_FLOAT_EQ(menu.number(*box, "mouseover"), 1.f);
        EXPECT_FLOAT_EQ(menu.number(*outer, "mouseover"), 1.f);
        open.pointerPressed(20, 20);
        EXPECT_EQ(open.pointerReleased(20, 20), box);
        EXPECT_FLOAT_EQ(menu.number(*box, "clicked"), 1.f);
        EXPECT_FLOAT_EQ(menu.number(*outer, "clicked"), 1.f);
        // Out of it, the hotrect is not over any more
        open.pointerMoved(400, 400);
        EXPECT_FLOAT_EQ(menu.number(*box, "mouseover"), 0.f);
        EXPECT_FLOAT_EQ(menu.number(*outer, "mouseover"), 0.f);
        // A hotrect that is not a target (disabled) is not over and not clicked, the tile in it still is
        Tile* off = menu.find("Off");
        Tile* inOff = menu.find("InOff");
        EXPECT_EQ(open.pointerMoved(110, 20), inOff);
        EXPECT_FLOAT_EQ(menu.number(*off, "mouseover"), 0.f);
        open.pointerPressed(110, 20);
        open.pointerReleased(110, 20);
        EXPECT_FLOAT_EQ(menu.number(*off, "clicked"), 0.f);
    }

    TEST(FalloutOpenMenuTest, an_image_or_a_hotrect_that_takes_the_pointer_in_a_hotrect_is_over_and_clicked_alone)
    {
        // As the arrows and the value of a toggle in its row: the files show the arrows when exactly one of the row, an
        // arrow and the value is over, so the row must not stay over with the pointer on one of the others
        Fixture f;
        OpenMenu& open = f.open(
            "<menu name=\"M\"><hotrect name=\"Row\"><x>10</x><y>10</y><width>300</width>"
            "<height>40</height><target>1</target>"
            "<image name=\"Arrow\"><x>10</x><y>5</y><width>20</width><height>20</height><target>1</target></image>"
            "<hotrect name=\"Value\"><x>100</x><y>5</y><width>100</width><height>20</height>"
            "<target>1</target></hotrect></hotrect></menu>",
            640.f, 480.f);
        Menu& menu = open.menu();
        Tile* row = menu.find("Row");
        Tile* arrow = menu.find("Arrow");
        Tile* value = menu.find("Value");
        // the points are those of the file's own units
        const float unit = open.scale();
        const auto move = [&](float x, float y) { return open.pointerMoved(x * unit, y * unit); };
        // on the row but on none of its children
        EXPECT_EQ(move(250, 40), row);
        EXPECT_FLOAT_EQ(menu.number(*row, "mouseover"), 1.f);
        EXPECT_EQ(move(25, 20), arrow);
        EXPECT_FLOAT_EQ(menu.number(*arrow, "mouseover"), 1.f);
        EXPECT_FLOAT_EQ(menu.number(*row, "mouseover"), 0.f);
        EXPECT_EQ(move(150, 20), value);
        EXPECT_FLOAT_EQ(menu.number(*arrow, "mouseover"), 0.f);
        EXPECT_FLOAT_EQ(menu.number(*value, "mouseover"), 1.f);
        EXPECT_FLOAT_EQ(menu.number(*row, "mouseover"), 0.f);
        open.pointerPressed(25 * unit, 20 * unit);
        EXPECT_EQ(open.pointerReleased(25 * unit, 20 * unit), arrow);
        EXPECT_FLOAT_EQ(menu.number(*arrow, "clicked"), 1.f);
        EXPECT_FLOAT_EQ(menu.number(*row, "clicked"), 0.f);
    }

    TEST(FalloutOpenMenuTest, the_wheel_counts_on_the_tile_that_takes_it)
    {
        Fixture f;
        OpenMenu& open = f.open(buttons, 640.f, 480.f);
        Menu& menu = open.menu();
        Tile* a = menu.find("A");
        EXPECT_EQ(open.wheelMoved(10, 10, 1), a);
        EXPECT_EQ(open.wheelMoved(10, 10, 1), a);
        EXPECT_EQ(open.wheelMoved(10, 10, -1), a);
        EXPECT_FLOAT_EQ(menu.number(*a, "wheelmoved"), 1.f);
        EXPECT_EQ(open.wheelMoved(110, 10, 1), nullptr);
    }

    TEST(FalloutOpenMenuTest, a_tile_that_can_be_dragged_follows_the_pointer_in_the_coordinates_of_its_parent)
    {
        Fixture f;
        OpenMenu& open = f.open(
            "<menu name=\"M\"><rect name=\"P\"><x> 100 </x><y> 100 </y><width> 400 </width>"
            "<height> 200 </height><hotrect name=\"D\"><x> 20 </x><y> 10 </y><width> 40 </width>"
            "<height> 20 </height><locus> 1 </locus><target> 1 </target><draggable> 1 </draggable>"
            "<dragstartx> -1 </dragstartx></hotrect></rect></menu>",
            1280.f, 960.f);
        Menu& menu = open.menu();
        Tile* d = menu.find("D");
        EXPECT_EQ(open.pointerPressed(130, 115), d);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragstartx"), 30.f);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragx"), 30.f);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragy"), 15.f);
        // The pointer was 10 and 5 inside the tile
        EXPECT_FLOAT_EQ(menu.number(*d, "dragoffsetx"), 10.f);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragoffsety"), 5.f);
        open.pointerMoved(300, 160);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragx"), 200.f);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragy"), 60.f);
        open.pointerReleased(300, 160);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragstartx"), -1.f);
        // It does not follow any more
        open.pointerMoved(400, 200);
        EXPECT_FLOAT_EQ(menu.number(*d, "dragx"), 200.f);
    }

    TEST(FalloutOpenMenuTest, a_tile_that_is_taken_out_is_let_go_of_by_the_pointer_and_drawn_no_more)
    {
        Fixture f;
        OpenMenu& open = f.open(
            "<menu name=\"M\"><rect name=\"List\"/>"
            "<template name=\"Row\"><width> 100 </width><height> 20 </height>"
            "<locus> 1 </locus><target> 1 </target></template></menu>");
        Menu& menu = open.menu();
        Tile* list = menu.find("List");
        Tile* row = menu.instantiate(*list, "Row", "Row1");
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(open.pointerMoved(10, 10), row);
        const int id = row->uid();
        EXPECT_EQ(menu.byId(id), row);
        open.remove(*row);
        EXPECT_EQ(open.hovered(), nullptr);
        EXPECT_EQ(menu.byId(id), nullptr);
        EXPECT_EQ(open.pointerMoved(10, 10), nullptr);
    }

    // A scrollbar made as the files make them: arrows that count clicks, a marker that is dragged, a wheel
    const char* const scrollbar
        = "<menu name=\"M\"><hotrect name=\"Main\"><x> 0 </x><y> 0 </y><width> 300 </width><height> 100 </height>"
          "<target> 1 </target><wheelable> 1 </wheelable><wheelmoved> 0 </wheelmoved>"
          "<hotrect name=\"Bar\"><_step_size> 1 </_step_size><_number_of_items> 11 </_number_of_items>"
          "<_number_of_visible_items> 1 </_number_of_visible_items><_SetInCode></_SetInCode>"
          "<_wheelmoved><copy src=\"parent()\" trait=\"wheelmoved\"/></_wheelmoved>"
          "<width> 200 </width><height> 20 </height>"
          "<_current_value>"
          "<add><copy src=\"child(Bar_right)\" trait=\"clicked\"/><sub src=\"child(Bar_left)\" trait=\"clicked\"/>"
          "</add>"
          "<add><copy src=\"me()\" trait=\"_wheelmoved\"/></add>"
          "<onlyif><copy src=\"child(Bar_marker)\" trait=\"dragstartx\"/><lt> 0 </lt></onlyif>"
          "<add><copy src=\"child(Bar_marker)\" trait=\"_drag_value\"/><onlyifnot>"
          "<copy src=\"child(Bar_marker)\" trait=\"dragstartx\"/><lt> 0 </lt></onlyifnot></add>"
          "<min><copy src=\"me()\" trait=\"_number_of_items\"/><sub src=\"me()\" trait=\"_number_of_visible_items\"/>"
          "</min><max> 0 </max></_current_value>"
          "<image name=\"Bar_left\"><width> 10 </width></image><image name=\"Bar_right\"><width> 10 </width></image>"
          "<image name=\"Bar_marker\"><target> 1 </target><draggable> 1 </draggable><dragstartx> -1 </dragstartx>"
          "<width> 20 </width><height> 20 </height>"
          "<_drag_value><copy src=\"me()\" trait=\"dragx\"/><div> 20 </div><floor> 0 </floor></_drag_value>"
          "</image></hotrect></hotrect></menu>";

    TEST(FalloutOpenMenuTest, a_scrollbar_keeps_its_value_when_the_marker_is_let_go)
    {
        Fixture f;
        OpenMenu& open = f.open(scrollbar, 1280.f, 960.f);
        Menu& menu = open.menu();
        ASSERT_EQ(open.scrollbars().size(), 1u);
        Tile* bar = menu.find("Bar");
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 0.f);
        // Take hold of the marker at its left end and pull it 100 to the right: five steps of 20
        open.pointerPressed(5, 5);
        open.pointerMoved(105, 5);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 5.f);
        open.pointerReleased(105, 5);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 5.f);
        EXPECT_FLOAT_EQ(menu.number(*menu.find("Bar_right"), "clicked"), 5.f);
    }

    TEST(FalloutOpenMenuTest, the_wheel_moves_a_scrollbar_and_is_not_counted_twice)
    {
        Fixture f;
        OpenMenu& open = f.open(scrollbar, 1280.f, 960.f);
        Menu& menu = open.menu();
        Tile* bar = menu.find("Bar");
        open.wheelMoved(250, 50, 1);
        open.wheelMoved(250, 50, 1);
        open.wheelMoved(250, 50, 1);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 3.f);
        EXPECT_FLOAT_EQ(menu.number(*menu.find("Main"), "wheelmoved"), 0.f);
        // Turned the other way, the value goes down by one for each turn, and stops at 0
        open.wheelMoved(250, 50, -1);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 2.f);
        open.wheelMoved(250, 50, -1);
        open.wheelMoved(250, 50, -1);
        open.wheelMoved(250, 50, -1);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 0.f);
        open.wheelMoved(250, 50, 1);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 1.f);
    }

    TEST(FalloutOpenMenuTest, a_scrollbar_is_put_at_a_value_by_the_screen)
    {
        Fixture f;
        OpenMenu& open = f.open(scrollbar, 1280.f, 960.f);
        Menu& menu = open.menu();
        Tile* bar = menu.find("Bar");
        open.setScrollbar(*bar, 7.f);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 7.f);
        // The arrows still count from there
        open.click(*menu.find("Bar_left"));
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 6.f);
        open.setScrollbar(*bar, 100.f);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 10.f);
    }

    // A list as the files make them: a window that clips, a highlight box, a scrollbar, and a template for the rows
    const char* const listMenu
        = "<menu name=\"M\"><hotrect name=\"List\"><x> 0 </x><y> 0 </y><width> 200 </width><height> 100 </height>"
          "<clipwindow> 1 </clipwindow><_scroll_delta> 20 </_scroll_delta><wheelable> 1 </wheelable>"
          "<hotrect name=\"lb_highlight_box\"><width> 200 </width></hotrect>"
          "<hotrect name=\"lb_scrollbar\"><_step_size> 1 </_step_size><_number_of_items> 1 </_number_of_items>"
          "<_number_of_visible_items> 1 </_number_of_visible_items><_SetInCode></_SetInCode>"
          "<_current_value><add><copy src=\"child(lb_scrollbar_down)\" trait=\"clicked\"/>"
          "<sub src=\"child(lb_scrollbar_up)\" trait=\"clicked\"/></add>"
          "<min><copy src=\"me()\" trait=\"_number_of_items\"/><sub src=\"me()\" trait=\"_number_of_visible_items\"/>"
          "</min><max> 0 </max></_current_value>"
          "<image name=\"lb_scrollbar_up\"/><image name=\"lb_scrollbar_down\"/></hotrect></hotrect>"
          "<template name=\"Row\"><hotrect name=\"Item\"><height> 0 </height><_verticalspacing> 10 </_verticalspacing>"
          "<_y></_y><y><copy src=\"me()\" trait=\"_y\"/><sub><copy src=\"sibling(lb_scrollbar)\" "
          "trait=\"_current_value\"/><mul src=\"parent()\" trait=\"_scroll_delta\"/></sub></y>"
          "<width> 200 </width><target> 1 </target><clips> 1 </clips>"
          "<text name=\"T\"><font> 1 </font><string><copy src=\"parent()\" trait=\"string\"/></string></text>"
          "</hotrect></template></menu>";

    std::vector<std::map<std::string, Value>> rowsOf(int count)
    {
        std::vector<std::map<std::string, Value>> rows;
        for (int i = 0; i < count; ++i)
            rows.push_back({ { "string", Value::fromText("Row " + std::to_string(i + 1)) } });
        return rows;
    }

    TEST(FalloutOpenMenuListTest, rows_are_made_from_the_template_and_stacked_by_the_height_of_their_text)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        Menu& menu = open.menu();
        const int number = open.addList("List", "Row");
        ASSERT_GE(number, 0);
        EXPECT_EQ(open.addList("Nothing", "Row"), -1);
        EXPECT_EQ(open.addList("List", "Nothing"), -1);
        ListBox& list = *open.list(number);
        list.setRows(rowsOf(5));
        ASSERT_EQ(list.rowCount(), 5);
        // Each row is the height of its text (30, one line of the test font) and 10 of room
        for (int row = 0; row < 5; ++row)
        {
            Tile* item = list.item(row);
            ASSERT_NE(item, nullptr);
            EXPECT_EQ(item->name(), "Item");
            EXPECT_FLOAT_EQ(menu.number(*item, "listindex"), static_cast<float>(row));
            EXPECT_FLOAT_EQ(menu.number(*item, "height"), 40.f);
            EXPECT_FLOAT_EQ(menu.number(*item, "_y"), 40.f * static_cast<float>(row));
            EXPECT_EQ(menu.text(*item, "string"), "Row " + std::to_string(row + 1));
        }
        Tile* bar = menu.find("lb_scrollbar");
        // 200 high in all, 100 high to show, 20 to a step
        EXPECT_FLOAT_EQ(menu.number(*bar, "_number_of_items"), 10.f);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_number_of_visible_items"), 5.f);
        EXPECT_FLOAT_EQ(menu.number(*menu.find("List"), "_num_filtered"), 5.f);
        // Made again, there are no more than the new rows
        list.setRows(rowsOf(2));
        EXPECT_EQ(list.rowCount(), 2);
        EXPECT_EQ(menu.find("List")->children().size(), 4u);
    }

    TEST(FalloutOpenMenuListTest, a_list_that_fits_its_first_rows_is_as_high_as_they_are_and_scrolls_by_what_is_left)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        Menu& menu = open.menu();
        const int number = open.addList("List", "Row", 3);
        ASSERT_GE(number, 0);
        ListBox& list = *open.list(number);
        Tile* bar = menu.find("lb_scrollbar");
        // Five rows of 40: the list is as high as the first three (the file's 100 is the least), and the two that are
        // over take two steps of a whole row each
        list.setRows(rowsOf(5));
        EXPECT_FLOAT_EQ(menu.number(*menu.find("List"), "height"), 120.f);
        EXPECT_FLOAT_EQ(menu.number(*menu.find("List"), "_scroll_delta"), 40.f);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_number_of_items"), 3.f);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_number_of_visible_items"), 1.f);
        // The last row is shown by the last step, and it ends where the list does
        list.select(4, true);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 2.f);
        EXPECT_FLOAT_EQ(menu.number(*list.item(4), "y") + menu.number(*list.item(4), "height"), 120.f);
        list.select(0, true);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 0.f);
        // Two rows are lower than the file's least height: nothing to scroll
        list.setRows(rowsOf(2));
        EXPECT_FLOAT_EQ(menu.number(*menu.find("List"), "height"), 100.f);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_number_of_items"), 1.f);
        // A list that does not fit rows keeps the height and the step of its file
        const int plain = open.addList("List", "Row");
        ListBox& other = *open.list(plain);
        other.setRows(rowsOf(2));
        EXPECT_FLOAT_EQ(menu.number(*menu.find("List"), "height"), 100.f);
    }

    TEST(FalloutOpenMenuListTest, a_row_can_name_another_template_than_the_list_makes_its_rows_from)
    {
        Fixture f;
        // The save list of the start menu has a row for a new save that the file makes from its own template
        std::string text = listMenu;
        const std::size_t end = text.rfind("</menu>");
        ASSERT_NE(end, std::string::npos);
        text.insert(end,
            "<template name=\"Other\"><hotrect name=\"OtherItem\"><height> 0 </height><width> 200 </width>"
            "<text name=\"T\"><font> 1 </font><string><copy src=\"parent()\" trait=\"string\"/></string></text>"
            "</hotrect></template>");
        OpenMenu& open = f.open(text.c_str(), 1280.f, 960.f);
        Menu& menu = open.menu();
        const int number = open.addList("List", "Row");
        ASSERT_GE(number, 0);
        ListBox& list = *open.list(number);
        auto rows = rowsOf(3);
        rows[1]["_template"] = Value::fromText("Other");
        list.setRows(rows);
        ASSERT_EQ(list.rowCount(), 3);
        EXPECT_EQ(list.item(0)->name(), "Item");
        EXPECT_EQ(list.item(1)->name(), "OtherItem");
        EXPECT_EQ(list.item(2)->name(), "Item");
        // The traits of the row are set, but the name of the template is not a trait of it
        EXPECT_EQ(menu.text(*list.item(1), "string"), "Row 2");
        EXPECT_FLOAT_EQ(menu.number(*list.item(1), "listindex"), 1.f);
        EXPECT_FALSE(menu.defines(*list.item(1), "_template"));
        // A name the menu has no template for ends the rows, as a list with no such template does
        rows[1]["_template"] = Value::fromText("Nothing");
        list.setRows(rows);
        EXPECT_EQ(list.rowCount(), 1);
    }

    TEST(FalloutOpenMenuListTest, the_list_is_the_tile_of_the_name_that_has_a_scrollbar)
    {
        Fixture f;
        // A heading with the name of the list, before it
        std::string menuText = listMenu;
        menuText.insert(
            menuText.find("<hotrect name=\"List\">"), "<text name=\"List\"><string> Heading </string></text>");
        OpenMenu& open = f.open(menuText, 1280.f, 960.f);
        const int number = open.addList("List", "Row");
        ASSERT_GE(number, 0);
        EXPECT_EQ(open.list(number)->listTile(), open.menu().byId(open.list(number)->listTile())->uid());
        EXPECT_NE(open.menu().find(*open.menu().byId(open.list(number)->listTile()), "lb_scrollbar"), nullptr);
    }

    TEST(FalloutOpenMenuListTest, a_row_can_set_a_trait_of_a_tile_inside_it)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        Menu& menu = open.menu();
        ListBox& list = *open.list(open.addList("List", "Row"));
        std::vector<std::map<std::string, Value>> rows;
        rows.push_back({ { "T.string", Value::fromText("Stimpak (3)") }, { "T.alpha", Value(100.f) } });
        list.setRows(rows);
        Tile* text = menu.find(*list.item(0), "T");
        ASSERT_NE(text, nullptr);
        EXPECT_EQ(menu.text(*text, "string"), "Stimpak (3)");
        EXPECT_FLOAT_EQ(menu.number(*text, "alpha"), 100.f);
    }

    TEST(FalloutOpenMenuListTest, a_row_that_is_there_takes_new_traits_and_the_list_stays_scrolled)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        Menu& menu = open.menu();
        ListBox& list = *open.list(open.addList("List", "Row"));
        list.setRows(rowsOf(5));
        Tile* bar = menu.find("lb_scrollbar");
        list.select(4, true);
        const float scrolled = menu.number(*bar, "_current_value");
        ASSERT_GT(scrolled, 0.f);

        list.updateRow(2, { { "T.string", Value::fromText("Changed") }, { "listindex", Value(7.f) } });
        EXPECT_EQ(menu.text(*menu.find(*list.item(2), "T"), "string"), "Changed");
        EXPECT_FLOAT_EQ(menu.number(*list.item(2), "listindex"), 7.f);
        // the other rows are as they were and the rows are not made again
        EXPECT_EQ(list.rowCount(), 5);
        EXPECT_EQ(menu.text(*menu.find(*list.item(1), "T"), "string"), "Row 2");
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), scrolled);
        EXPECT_EQ(list.selected(), 4);
        // a row that is not there is left alone
        list.updateRow(9, { { "T.string", Value::fromText("Nowhere") } });
        EXPECT_EQ(list.rowCount(), 5);
    }

    TEST(FalloutOpenMenuListTest, a_row_whose_text_changes_height_is_measured_again_and_the_rows_below_it_move)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        Menu& menu = open.menu();
        ListBox& list = *open.list(open.addList("List", "Row"));
        list.setRows(rowsOf(4));
        ASSERT_FLOAT_EQ(menu.number(*list.item(1), "height"), 40.f);
        ASSERT_FLOAT_EQ(menu.number(*list.item(2), "_y"), 80.f);

        list.updateRow(1, { { "string", Value::fromText("Two\nlines") } });
        // 30 for each line of the test font and 10 of room
        EXPECT_FLOAT_EQ(menu.number(*list.item(1), "height"), 70.f);
        EXPECT_FLOAT_EQ(menu.number(*list.item(2), "_y"), 110.f);
        EXPECT_FLOAT_EQ(menu.number(*list.item(3), "_y"), 150.f);
        // and the other way
        list.updateRow(1, { { "string", Value::fromText("One") } });
        EXPECT_FLOAT_EQ(menu.number(*list.item(1), "height"), 40.f);
        EXPECT_FLOAT_EQ(menu.number(*list.item(3), "_y"), 120.f);
    }

    TEST(FalloutOpenMenuListTest, the_highlight_goes_to_the_row_the_pointer_is_on_and_follows_the_scrolling)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        Menu& menu = open.menu();
        ListBox& list = *open.list(open.addList("List", "Row"));
        list.setRows(rowsOf(5));
        Tile* listTile = menu.find("List");
        EXPECT_FLOAT_EQ(menu.number(*listTile, "_highlight_y"), -1.f);
        open.pointerMoved(50, 50);
        EXPECT_EQ(list.selected(), 1);
        EXPECT_FLOAT_EQ(menu.number(*listTile, "_highlight_y"), 40.f);
        EXPECT_FLOAT_EQ(menu.number(*listTile, "_selected_height"), 40.f);
        // The row is found from the text inside it, too
        EXPECT_EQ(list.rowOf(*menu.find(*list.item(3), "T")), 3);
        const std::optional<std::pair<int, int>> found = open.rowOf(*list.item(2));
        ASSERT_TRUE(found);
        EXPECT_EQ(found->second, 2);
        // Scrolled by two steps of 20, the rows and the highlight are 40 higher
        open.setScrollbar(*menu.find("lb_scrollbar"), 2.f);
        list.layout();
        EXPECT_FLOAT_EQ(menu.number(*list.item(1), "y"), 0.f);
        EXPECT_FLOAT_EQ(menu.number(*listTile, "_highlight_y"), 0.f);
    }

    TEST(FalloutOpenMenuListTest, a_row_that_is_picked_is_scrolled_into_view)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        Menu& menu = open.menu();
        ListBox& list = *open.list(open.addList("List", "Row"));
        list.setRows(rowsOf(5));
        Tile* bar = menu.find("lb_scrollbar");
        // The window is 100 high: rows 0 to 2 show (0 to 120 is more than 100, so the third is cut)
        list.select(4, true);
        // The last row ends at 200: the window must start at 100, five steps down
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 5.f);
        list.select(0, true);
        EXPECT_FLOAT_EQ(menu.number(*bar, "_current_value"), 0.f);
        list.select(-1);
        EXPECT_EQ(list.selected(), -1);
    }

    TEST(FalloutOpenMenuListTest, a_row_that_is_outside_the_window_is_cut_and_cannot_be_hit)
    {
        Fixture f;
        OpenMenu& open = f.open(listMenu, 1280.f, 960.f);
        ListBox& list = *open.list(open.addList("List", "Row"));
        list.setRows(rowsOf(5));
        bool cut = false;
        for (const Piece& piece : open.pieces())
            if (piece.mClipped && piece.mClipHeight == 100)
                cut = true;
        EXPECT_TRUE(cut);
        // Row 3 starts at 120, below the window of 100
        EXPECT_EQ(open.pointerMoved(50, 130), nullptr);
    }
}
