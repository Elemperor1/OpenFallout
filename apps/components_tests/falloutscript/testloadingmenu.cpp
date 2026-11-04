#include <components/falloutscript/loadingmenu.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace
{
    using namespace FalloutScript::LoadScreens;

    constexpr std::size_t headerBytes = 296;
    constexpr std::size_t glyphBytes = 56;

    void putFloat(std::string& bytes, std::size_t at, float value)
    {
        std::memcpy(bytes.data() + at, &value, sizeof(value));
    }

    // A font of 30 high lines whose characters are 10 wide, cut from a 100 by 100 texture
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

    const char* slide(const char* name, int id)
    {
        static std::string text;
        text = std::string("<image name=\"") + name + "\"><id>" + std::to_string(id)
            + "</id><filename>Interface\\Shared\\Background\\solid_black.dds</filename><visible>1</visible>"
              "<alpha>0</alpha><width><copy src=\"screen()\" trait=\"width\"/></width>"
              "<height><copy src=\"screen()\" trait=\"height\"/></height><x>0</x><y>0</y></image>";
        return text.c_str();
    }

    std::string loadingMenu()
    {
        std::string xml = "<menu name=\"LoadingMenu\"><x>0</x><y>0</y><alpha>0</alpha>";
        xml += "<image name=\"loading_world_image\"><id>12</id><filename>Interface\\Loading\\loading_text_bar.dds</filename>"
               "<width>10</width><height>10</height></image>";
        xml += "<rect name=\"loading_currobj_rect\"><text name=\"loading_currobj_text\"><font>1</font>"
               "<string>Objective</string></text></rect>";
        xml += slide("loading_tile_slide_01", 32);
        xml += slide("loading_tile_slide_02", 33);
        xml += "<text name=\"tip_text_01\"><id>34</id><visible>0</visible><string></string><font>1</font></text>";
        xml += "<text name=\"tip_text_02\"><id>35</id><visible>0</visible><string></string><font>1</font></text>";
        for (const char* name : { "tip_matte_01", "tip_matte_02" })
        {
            xml += std::string("<image name=\"") + name
                + "\"><filename>Interface\\Loading\\loading_text_bar.dds</filename><visible>0</visible><alpha>0</alpha>"
                  "<width><copy src=\"screen()\" trait=\"width\"/></width><height>0</height><y>0</y></image>";
        }
        xml += "</menu>";
        return xml;
    }

    struct Fixture
    {
        std::map<std::string, std::string> mFiles;
        std::shared_ptr<FalloutMenu::Resources> mResources;
        std::unique_ptr<LoadingMenu> mMenu;
        std::string mError;

        explicit Fixture(const std::string& menuXml = loadingMenu(), float width = 1280.f, float height = 960.f)
        {
            mFiles["textures/fonts/glow_monofonto_large.fnt"] = testFont();
            mFiles["textures/fonts/test_font_0.dds"] = dds(100, 100);
            mFiles["textures/interface/shared/background/solid_black.dds"] = dds(4, 4);
            mFiles["textures/interface/loading/loading_text_bar.dds"] = dds(16, 64);
            mFiles["textures/loading/first.dds"] = dds(1024, 512);
            mFiles["textures/loading/second.dds"] = dds(1024, 512);
            mFiles["menus/globals.xml"] = "<globals/>";
            mFiles["menus/loading_menu.xml"] = menuXml;
            FalloutMenu::Data data;
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
            data.mSetting = [](std::string_view) -> std::optional<std::string> { return std::nullopt; };
            mResources = std::make_shared<FalloutMenu::Resources>(data);
            mMenu = LoadingMenu::open(mResources, width, height, &mError);
        }

        std::vector<FalloutMenu::Piece> piecesOf(const std::string& tile)
        {
            std::vector<FalloutMenu::Piece> found;
            for (const FalloutMenu::Piece& piece : mMenu->pieces())
                if (piece.mTile != nullptr && piece.mTile->name() == tile)
                    found.push_back(piece);
            return found;
        }
    };

    TipLayout newVegasTips()
    {
        TipLayout layout;
        layout.mType = 3;
        layout.mX = 210;
        layout.mY = 100;
        layout.mWidth = 850;
        layout.mHeight = 400;
        layout.mFont = 1;
        layout.mColor = { 255.f, 180.f, 53.f };
        layout.mJustify = 2;
        return layout;
    }

    // The wheel's tile as the game's file places it (loading_menu.xml): centred across the screen and 90 percent down the
    // display zone, from the scale the code writes into user0
    std::string menuWithPinwheel()
    {
        std::string xml = loadingMenu();
        const std::string wheel = R"xml(
<nif name="loading_pinwheel"><id>11</id><filename>Interface\Circular Loading\loading01.nif</filename>
<x><copy src="me()" trait="_x_offset"/><mult src="me()" trait="user0"/>
<add><copy src="screen()" trait="width"/><sub><copy src="me()" trait="width"/><mult src="me()" trait="user0"/></sub><div> 2 </div></add></x>
<y><copy src="me()" trait="_y_offset"/><mult src="me()" trait="user0"/>
<add><copy src="parent()" trait="_y_buffer"/><add><copy src="parent()" trait="_display_zone"/><mult> 0.9 </mult></add>
<sub><copy src="me()" trait="height"/><div> 2 </div></sub></add>
<sub><copy src="me()" trait="height"/><mult src="me()" trait="user0"/></sub></y>
<width> 54 </width><height> 54 </height><_x_offset> 107 </_x_offset><_y_offset> -46 </_y_offset><user0></user0></nif>)xml";
        xml.insert(xml.rfind("</menu>"), wheel);
        xml.insert(xml.find("<alpha>0</alpha>") + 16, "<_y_buffer>30</_y_buffer><_display_zone>900</_display_zone>");
        return xml;
    }

    TEST(FalloutLoadingMenuTest, the_wheel_stands_where_the_file_puts_it_at_the_scale_the_code_gives)
    {
        Fixture f(menuWithPinwheel());
        ASSERT_TRUE(f.mMenu) << f.mError;
        const LoadingMenu::Pinwheel wheel = f.mMenu->pinwheel();
        ASSERT_TRUE(wheel.mPresent);
        EXPECT_FLOAT_EQ(wheel.mScale, 2.f);
        // x = 107 * 2 + (1280 - 54 * 2) / 2 = 800: the model's origin is 80.2 units (at scale 2) to the right of the
        // wheel's middle, which is the middle of the screen
        EXPECT_FLOAT_EQ(wheel.mX, 800.f);
        // y = -46 * 2 + (30 + 900 * 0.9 - 54 / 2) - 54 * 2 = 613
        EXPECT_FLOAT_EQ(wheel.mY, 613.f);
    }

    TEST(FalloutLoadingMenuTest, a_file_without_a_wheel_has_none)
    {
        Fixture f;
        ASSERT_TRUE(f.mMenu) << f.mError;
        EXPECT_FALSE(f.mMenu->pinwheel().mPresent);
    }

    TEST(FalloutLoadingMenuTest, a_file_without_the_tiles_of_a_slide_does_not_open)
    {
        Fixture f("<menu name=\"LoadingMenu\"/>");
        EXPECT_FALSE(f.mMenu);
        EXPECT_NE(f.mError.find("loading_tile_slide_01"), std::string::npos) << f.mError;
    }

    TEST(FalloutLoadingMenuTest, the_world_image_and_the_groups_of_the_other_types_are_not_drawn)
    {
        Fixture f;
        ASSERT_TRUE(f.mMenu) << f.mError;
        f.mMenu->setAlphas({ 255.f, 255.f }, 1.f);
        EXPECT_TRUE(f.piecesOf("loading_world_image").empty());
        EXPECT_TRUE(f.piecesOf("loading_currobj_text").empty());
    }

    TEST(FalloutLoadingMenuTest, nothing_shows_before_a_picture_is_given)
    {
        Fixture f;
        ASSERT_TRUE(f.mMenu) << f.mError;
        f.mMenu->setAlphas({ 255.f, 255.f }, 1.f);
        for (const FalloutMenu::Piece& piece : f.mMenu->pieces())
            EXPECT_FLOAT_EQ(piece.mColor[3], 0.f) << piece.mTile->name();
    }

    TEST(FalloutLoadingMenuTest, a_slide_shows_its_picture_over_the_whole_window_and_its_tip_on_a_matte)
    {
        Fixture f;
        ASSERT_TRUE(f.mMenu) << f.mError;
        f.mMenu->setSlide(0, "loading\\first.dds", "Tip", newVegasTips());
        f.mMenu->setAlphas({ 255.f, 0.f }, 1.f);

        const std::vector<FalloutMenu::Piece> picture = f.piecesOf("loading_tile_slide_01");
        ASSERT_EQ(picture.size(), 1u);
        EXPECT_EQ(picture[0].mTexture, "textures/loading/first.dds");
        EXPECT_FLOAT_EQ(picture[0].mColor[3], 1.f);
        EXPECT_EQ(picture[0].mWidth, 1280);
        EXPECT_EQ(picture[0].mHeight, 960);

        // The matte is a little over and under the box of the tip (100 to 500 on the canvas)
        const std::vector<FalloutMenu::Piece> matte = f.piecesOf("tip_matte_01");
        ASSERT_EQ(matte.size(), 1u);
        EXPECT_EQ(matte[0].mY, 85);
        EXPECT_EQ(matte[0].mHeight, 430);
        EXPECT_FLOAT_EQ(matte[0].mColor[3], 1.f);

        // The tip is three characters in the colour of the record, centred in the box (210 to 1060)
        const std::vector<FalloutMenu::Piece> tip = f.piecesOf("tip_text_01");
        ASSERT_EQ(tip.size(), 3u);
        EXPECT_NEAR(tip[0].mColor[0], 1.f, 1e-4);
        EXPECT_NEAR(tip[0].mColor[1], 180.f / 255.f, 1e-4);
        EXPECT_NEAR(tip[0].mColor[2], 53.f / 255.f, 1e-4);
        EXPECT_GE(tip[0].mX, 210);
        EXPECT_LE(tip[2].mX + tip[2].mWidth, 1060);
        EXPECT_NEAR(0.5 * (tip[0].mX + tip[2].mX + tip[2].mWidth), 635.0, 12.0);
        // (a line of the test font starts a little above the top of its tile)
        EXPECT_NEAR(tip[0].mY, 100, 15);

        // The second slide has nothing yet
        for (const FalloutMenu::Piece& piece : f.mMenu->pieces())
        {
            if (piece.mTile->name() == "loading_tile_slide_02" || piece.mTile->name() == "tip_matte_02")
            {
                EXPECT_FLOAT_EQ(piece.mColor[3], 0.f);
            }
        }
    }

    TEST(FalloutLoadingMenuTest, the_slides_cross_fade_and_the_whole_menu_fades_in_over_them)
    {
        Fixture f;
        ASSERT_TRUE(f.mMenu) << f.mError;
        f.mMenu->setSlide(0, "loading\\first.dds", "One", newVegasTips());
        f.mMenu->setSlide(1, "loading\\second.dds", "Two", newVegasTips());
        f.mMenu->setAlphas({ 51.f, 204.f }, 0.5f);
        EXPECT_NEAR(f.piecesOf("loading_tile_slide_01").at(0).mColor[3], 0.1f, 1e-4);
        EXPECT_NEAR(f.piecesOf("loading_tile_slide_02").at(0).mColor[3], 0.4f, 1e-4);
        EXPECT_NEAR(f.piecesOf("tip_matte_02").at(0).mColor[3], 0.4f, 1e-4);
        EXPECT_NEAR(f.piecesOf("tip_text_02").at(0).mColor[3], 0.4f, 1e-4);
        EXPECT_EQ(f.piecesOf("loading_tile_slide_02").at(0).mTexture, "textures/loading/second.dds");
    }

    TEST(FalloutLoadingMenuTest, a_picture_without_a_tip_has_no_matte)
    {
        Fixture f;
        ASSERT_TRUE(f.mMenu) << f.mError;
        f.mMenu->setSlide(0, "loading\\first.dds", "", newVegasTips());
        f.mMenu->setAlphas({ 255.f, 0.f }, 1.f);
        EXPECT_EQ(f.piecesOf("loading_tile_slide_01").size(), 1u);
        EXPECT_TRUE(f.piecesOf("tip_matte_01").empty());
        EXPECT_TRUE(f.piecesOf("tip_text_01").empty());
    }

    TEST(FalloutLoadingMenuTest, the_tips_are_laid_out_again_when_the_window_changes_size)
    {
        Fixture f;
        ASSERT_TRUE(f.mMenu) << f.mError;
        f.mMenu->setSlide(0, "loading\\first.dds", "Tip", newVegasTips());
        f.mMenu->setAlphas({ 255.f, 0.f }, 1.f);
        // The window is twice as wide as high, so the screen is 1920 units wide: the matte is as wide as the screen
        f.mMenu->resize(1920.f, 960.f);
        EXPECT_FLOAT_EQ(f.mMenu->designWidth(), 1920.f);
        const std::vector<FalloutMenu::Piece> matte = f.piecesOf("tip_matte_01");
        ASSERT_EQ(matte.size(), 1u);
        EXPECT_EQ(matte[0].mWidth, 1920);
        const std::vector<FalloutMenu::Piece> tip = f.piecesOf("tip_text_01");
        ASSERT_EQ(tip.size(), 3u);
        // The box is 210/1280 of the width from the left: 315 of 1920, and 850 wide, so it ends at 1165
        EXPECT_NEAR(0.5 * (tip[0].mX + tip[2].mX + tip[2].mWidth), 740.0, 12.0);
    }
}
