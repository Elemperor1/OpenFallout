#include <components/falloutmenu/menu.hpp>
#include <components/falloutmenu/menufile.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace
{
    using namespace FalloutMenu;

    // A menu made from files that are in memory
    struct Fixture
    {
        std::map<std::string, std::string> mFiles;
        std::map<std::string, std::string> mSettings;
        std::unique_ptr<Loader> mLoader;
        std::unique_ptr<Menu> mMenu;
        std::shared_ptr<const TileDef> mDefinition;
        Environment mEnvironment;

        Fixture()
        {
            // Every character of a text is 10 wide and 20 high, an image is 64 by 32
            mEnvironment.mTextSize = [](int, const std::string& text, float) {
                return TextSize{ 10.f * static_cast<float>(text.size()), text.empty() ? 0.f : 20.f };
            };
            mEnvironment.mLineHeight = [](int) { return 20.f; };
            mEnvironment.mImageSize = [](const std::string& file, const std::string&) -> std::optional<TextSize> {
                if (file.find("missing") != std::string::npos)
                    return std::nullopt;
                return TextSize{ 64.f, 32.f };
            };
            mEnvironment.mSystemColor = [](int id) -> std::optional<std::array<float, 3>> {
                if (id == 5)
                    return std::array<float, 3>{ 10.f, 255.f, 100.f };
                return std::nullopt;
            };
        }

        Menu& open(const std::string& menu, float screenWidth = 1280.f, float screenHeight = 960.f)
        {
            mFiles["menus/test_menu.xml"] = menu;
            LoaderEnvironment env;
            env.mReadFile = [this](const std::string& path) -> std::optional<std::string> {
                const auto found = mFiles.find(path);
                if (found == mFiles.end())
                    return std::nullopt;
                return found->second;
            };
            env.mSetting = [this](std::string_view name) -> std::optional<std::string> {
                const auto found = mSettings.find(std::string(name));
                if (found == mSettings.end())
                    return std::nullopt;
                return found->second;
            };
            mLoader = std::make_unique<Loader>(env);
            mDefinition = mLoader->loadMenu("menus/test_menu.xml");
            std::shared_ptr<const TileDef> globals;
            if (mFiles.count("menus/globals.xml") > 0)
                globals = mLoader->loadPlain("menus/globals.xml");
            EXPECT_TRUE(mDefinition);
            mMenu = std::make_unique<Menu>(mDefinition, globals, mLoader->templates(), mEnvironment);
            mMenu->setScreen(screenWidth, screenHeight);
            return *mMenu;
        }
    };

    float numberOf(Menu& menu, const char* tile, const char* trait)
    {
        Tile* found = menu.find(tile);
        EXPECT_NE(found, nullptr) << tile;
        return found == nullptr ? 0.f : menu.number(*found, trait);
    }

    TEST(FalloutMenuLoaderTest, entities_stand_for_numbers_and_settings_for_text)
    {
        Fixture f;
        f.mSettings["sHello"] = "HELLO";
        Menu& menu = f.open(
            "<menu name=\"M\"><text name=\"T\"><justify> &center; </justify><zoom> &scale; </zoom>"
            "<string> &-sHello; </string><_k> &true; </_k></text></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "T", "justify"), 2.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "T", "zoom"), -1.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "T", "_k"), 1.f);
        EXPECT_EQ(menu.text(*menu.find("T"), "string"), "HELLO");
        EXPECT_FLOAT_EQ(builtinEntity("inventorymenu").value_or(0), 1002.f);
        EXPECT_FALSE(builtinEntity("no_such_entity"));
    }

    TEST(FalloutMenuLoaderTest, the_system_colours_are_numbered_as_the_exe_lists_them_and_unknown_words_stay_text)
    {
        EXPECT_EQ(builtinEntity("hudmain"), 1.f);
        EXPECT_EQ(builtinEntity("hudalt"), 2.f);
        EXPECT_EQ(builtinEntity("terminal"), 3.f);
        EXPECT_EQ(builtinEntity("pipboy"), 4.f);
        EXPECT_EQ(builtinEntity("mainmenu"), 5.f);
        EXPECT_EQ(builtinEntity("system"), 6.f);
        EXPECT_EQ(builtinEntity("nosystemcolor"), 0.f);
        EXPECT_EQ(builtinEntity("uselocalcolor"), -1.f);
        // Words the menu files use but the exe has no value for are left alone
        Fixture f;
        Menu& menu = f.open("<menu name=\"M\"><rect name=\"R\"><xup> &prev; </xup></rect></menu>");
        EXPECT_EQ(menu.text(*menu.find("R"), "xup"), "&prev;");
    }

    TEST(FalloutMenuLoaderTest, an_include_is_taken_in_and_what_is_written_after_it_wins)
    {
        Fixture f;
        f.mFiles["menus/prefabs/box.xml"] = "<width> 5 </width><height> 6 </height><x> 9 </x>";
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\"><include src=\"box.xml\"/><width> 7 </width></rect>"
            "<rect name=\"B\"><width> 7 </width><include src=\"box.xml\"/></rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "width"), 7.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "height"), 6.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "B", "width"), 5.f);
        EXPECT_TRUE(f.mLoader->problems().empty());
    }

    TEST(FalloutMenuLoaderTest, an_include_that_is_not_there_is_told_and_does_not_stop_the_menu)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\"><include src=\"nothing.xml\"/><width> 3 </width></rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "width"), 3.f);
        ASSERT_EQ(f.mLoader->problems().size(), 1u);
        EXPECT_NE(f.mLoader->problems()[0].find("nothing.xml"), std::string::npos);
    }

    TEST(FalloutMenuLoaderTest, a_prefab_that_includes_itself_through_a_tile_is_stopped)
    {
        Fixture f;
        f.mFiles["menus/prefabs/loop.xml"] = "<rect name=\"Inner\"><include src=\"loop.xml\"/></rect>";
        Menu& menu = f.open("<menu name=\"M\"><rect name=\"A\"><include src=\"loop.xml\"/></rect></menu>");
        EXPECT_NE(menu.find("A"), nullptr);
        ASSERT_FALSE(f.mLoader->problems().empty());
        EXPECT_NE(f.mLoader->problems()[0].find("too deep"), std::string::npos);
    }

    TEST(FalloutMenuLoaderTest, a_trait_written_with_nothing_in_it_is_an_empty_text_not_zero)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><text name=\"T\"><string></string></text><image name=\"I\"><filename/></image></menu>");
        Tile* text = menu.find("T");
        ASSERT_NE(text, nullptr);
        EXPECT_EQ(menu.text(*text, "string"), "");
        EXPECT_EQ(menu.text(*menu.find("I"), "filename"), "");
    }

    TEST(FalloutMenuEvaluationTest, a_number_that_is_not_finite_is_not_a_number)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\">"
            "<x><copy> 3e38 </copy><mult> 1000 </mult></x>"
            "<width><copy> inf </copy></width>"
            "<height><copy> 1e999 </copy></height>"
            "</rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "x"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "width"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "height"), 0.f);
        EXPECT_TRUE(Value::fromText("nan").isText());
        EXPECT_TRUE(Value::fromText("1e999").isText());
        EXPECT_EQ(Value(3.0e38f).text(), "3e+38");
    }

    TEST(FalloutMenuEvaluationTest, steps_work_on_one_running_value_with_no_precedence)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\">"
            "<x><copy> 10 </copy><add> 5 </add><mult> 2 </mult></x>"
            "<y><copy> 10 </copy><sub> 4 </sub><div> 3 </div><mod> 1 </mod></y>"
            "<width><copy> 7 </copy><min> 3 </min><max> 4 </max></width>"
            "<height><copy> -7.5 </copy><abs/><floor> 0 </floor></height>"
            "</rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "x"), 30.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "y"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "width"), 4.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "height"), 7.f);
    }

    TEST(FalloutMenuEvaluationTest, a_step_can_take_a_whole_expression_as_its_operand)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\"><x><copy> 100 </copy>"
            "<add><copy> 2 </copy><mult> 3 </mult></add></x></rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "x"), 106.f);
    }

    TEST(FalloutMenuEvaluationTest, comparisons_and_conditions)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\">"
            "<_gt><copy> 5 </copy><gt> 3 </gt></_gt>"
            "<_lt><copy> 5 </copy><lt> 3 </lt></_lt>"
            "<_eq><copy> 5 </copy><eq> 5 </eq><and><copy> 1 </copy></and></_eq>"
            "<_not><copy> 0 </copy><not/></_not>"
            "<_only><copy> 7 </copy><onlyif><copy> 0 </copy></onlyif></_only>"
            "<_onlyif><copy> 7 </copy><onlyif><copy> 1 </copy></onlyif></_onlyif>"
            "<_onlyifnot><copy> 7 </copy><onlyifnot><copy> 0 </copy></onlyifnot></_onlyifnot>"
            "<_or><copy> 0 </copy><or><copy> 1 </copy></or></_or>"
            "<_div0><copy> 5 </copy><div> 0 </div></_div0>"
            "</rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_gt"), 1.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_lt"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_eq"), 1.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_not"), 1.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_only"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_onlyif"), 7.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_onlyifnot"), 7.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_or"), 1.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_div0"), 0.f);
    }

    TEST(FalloutMenuEvaluationTest, onlyif_holds_for_a_text_that_has_characters)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"P\">"
            "<text name=\"Full\"><string> some words </string></text>"
            "<text name=\"Empty\"><string></string></text>"
            "<rect name=\"A\">"
            "<_withText><copy> 7 </copy><onlyif src=\"sibling(Full)\" trait=\"string\"/></_withText>"
            "<_withNone><copy> 7 </copy><onlyif src=\"sibling(Empty)\" trait=\"string\"/></_withNone>"
            "<_notText><copy> 7 </copy><onlyifnot src=\"sibling(Full)\" trait=\"string\"/></_notText>"
            "<_notNone><copy> 7 </copy><onlyifnot src=\"sibling(Empty)\" trait=\"string\"/></_notNone>"
            "</rect></rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_withText"), 7.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_withNone"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_notText"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "_notNone"), 7.f);
    }

    TEST(FalloutMenuEvaluationTest, a_trait_can_come_from_the_tiles_around)
    {
        Fixture f;
        f.mFiles["menus/globals.xml"] = "<rect name=\"G\"><_gap> 15 </_gap></rect>";
        Menu& menu = f.open(
            "<menu name=\"M\"><_k> 4 </_k><width> 100 </width>"
            "<rect name=\"P\"><x> 30 </x><width> 50 </width>"
            "<rect name=\"C\">"
            "<x><copy src=\"parent()\" trait=\"x\"/><add src=\"grandparent()\" trait=\"_k\"/></x>"
            "<y><copy src=\"sibling(D)\" trait=\"width\"/></y>"
            "<width><copy src=\"globals()\" trait=\"_gap\"/></width>"
            "<height><copy src=\"screen()\" trait=\"height\"/></height></rect>"
            "<rect name=\"D\"><width> 8 </width>"
            "<_w><copy src=\"parent()\" trait=\"width\"/></_w></rect>"
            "<_cw><copy src=\"child(D)\" trait=\"width\"/></_cw>"
            "<_named><copy src=\"D\" trait=\"width\"/></_named></rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "C", "x"), 34.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "C", "y"), 8.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "C", "width"), 15.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "C", "height"), 960.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "P", "_cw"), 8.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "P", "_named"), 8.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "D", "_w"), 50.f);
    }

    TEST(FalloutMenuEvaluationTest, a_name_that_ends_with_an_underscore_is_completed_with_the_value_so_far)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\"><_pick_1> 10 </_pick_1><_pick_2> 20 </_pick_2>"
            "<_which> 2 </_which>"
            "<x><copy src=\"me()\" trait=\"_which\"/><copy src=\"me()\" trait=\"_pick_\"/></x></rect></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "A", "x"), 20.f);
    }

    TEST(FalloutMenuEvaluationTest, the_screen_is_a_tile_960_high_and_as_wide_as_the_window_is_for_its_height)
    {
        Fixture f;
        Menu& wide
            = f.open("<menu name=\"M\"><width><copy src=\"screen()\" trait=\"width\"/></width></menu>", 1920.f, 1080.f);
        EXPECT_NEAR(wide.designWidth(), 1706.67f, 0.01f);
        EXPECT_FLOAT_EQ(wide.designHeight(), 960.f);
        EXPECT_NEAR(wide.number(wide.root(), "width"), 1706.67f, 0.01f);
        Fixture g;
        Menu& square = g.open("<menu name=\"M\"/>", 1024.f, 768.f);
        EXPECT_FLOAT_EQ(square.designWidth(), 1280.f);
        EXPECT_FLOAT_EQ(square.designHeight(), 960.f);
    }

    TEST(FalloutMenuEvaluationTest, a_window_taller_than_it_is_wide_is_still_960_high)
    {
        Fixture f;
        Menu& tall = f.open("<menu name=\"M\"/>", 600.f, 1000.f);
        EXPECT_FLOAT_EQ(tall.designHeight(), 960.f);
        EXPECT_NEAR(tall.designWidth(), 576.f, 0.01f);
    }

    TEST(FalloutMenuEvaluationTest, the_size_of_a_text_and_of_an_image_follow_from_what_they_show)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><text name=\"T\"><string> abcd </string></text>"
            "<text name=\"E\"/>"
            "<image name=\"I\"><filename> a.dds </filename><zoom> 50 </zoom></image>"
            "<image name=\"S\"><filename> a.dds </filename><zoom> &scale; </zoom><width> 200 </width></image>"
            "<image name=\"X\"><filename> missing.dds </filename></image></menu>");
        EXPECT_FLOAT_EQ(numberOf(menu, "T", "width"), 40.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "T", "height"), 20.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "E", "width"), 0.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "I", "width"), 32.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "I", "height"), 16.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "S", "width"), 200.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "S", "filewidth"), 64.f);
        EXPECT_FLOAT_EQ(numberOf(menu, "X", "width"), 0.f);
    }

    TEST(FalloutMenuEvaluationTest, what_the_screen_sets_wins_until_it_is_cleared)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\"><x> 5 </x><_t><copy src=\"me()\" trait=\"x\"/><add> 1 "
            "</add></_t></rect></menu>");
        Tile& a = *menu.find("A");
        EXPECT_FLOAT_EQ(menu.number(a, "_t"), 6.f);
        menu.set(a, "x", Value(50.f));
        EXPECT_TRUE(menu.isSet(a, "x"));
        EXPECT_FLOAT_EQ(menu.number(a, "x"), 50.f);
        EXPECT_FLOAT_EQ(menu.number(a, "_t"), 51.f);
        menu.clear(a, "x");
        EXPECT_FALSE(menu.isSet(a, "x"));
        EXPECT_FLOAT_EQ(menu.number(a, "_t"), 6.f);
        // A trait the file does not have can be set too
        menu.set(a, "_new", Value::fromText("hello"));
        EXPECT_EQ(menu.text(a, "_new"), "hello");
        EXPECT_TRUE(menu.defines(a, "x"));
        EXPECT_FALSE(menu.defines(a, "_nothing"));
    }

    TEST(FalloutMenuEvaluationTest, a_tile_made_of_a_template_has_its_traits_and_children_and_can_be_taken_out)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"List\"/>"
            "<template name=\"Row\"><height> 26 </height><text name=\"Label\"><string> row </string></text></template>"
            "</menu>");
        Tile* list = menu.find("List");
        ASSERT_NE(list, nullptr);
        Tile* row = menu.instantiate(*list, "row", "Row1");
        ASSERT_NE(row, nullptr);
        EXPECT_EQ(row->name(), "Row1");
        EXPECT_FLOAT_EQ(menu.number(*row, "height"), 26.f);
        EXPECT_EQ(row->children().size(), 1u);
        EXPECT_EQ(list->children().size(), 1u);
        EXPECT_EQ(menu.draw().size(), 1u);
        EXPECT_EQ(menu.instantiate(*list, "none"), nullptr);
        EXPECT_FALSE(menu.problems().empty());
        menu.remove(*row);
        EXPECT_TRUE(list->children().empty());
        menu.instantiate(*list, "row");
        menu.removeChildren(*list);
        EXPECT_TRUE(list->children().empty());
    }

    TEST(FalloutMenuDrawTest, tiles_are_drawn_back_to_front_by_depth_and_what_is_not_visible_is_left_out)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\">"
            "<image name=\"Front\"><filename> a.dds </filename><depth> 5 </depth></image>"
            "<image name=\"Back\"><filename> a.dds </filename><depth> 1 </depth></image>"
            "<rect name=\"Hidden\"><visible> &false; </visible>"
            "<image name=\"Child\"><filename> a.dds </filename></image></rect>"
            "<image name=\"None\"/>"
            "</menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 2u);
        EXPECT_EQ(items[0].mTile->name(), "Back");
        EXPECT_EQ(items[1].mTile->name(), "Front");
        EXPECT_EQ(items[0].mFile, "a.dds");
    }

    TEST(FalloutMenuDrawTest, a_hot_rectangle_that_names_a_file_is_a_picture_and_one_that_does_not_is_not)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\">"
            "<hotrect name=\"Picture\"><filename> map.dds </filename><width> 200 </width><height> 100 "
            "</height></hotrect>"
            "<hotrect name=\"Button\"><width> 50 </width><height> 20 </height></hotrect>"
            "</menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0].mTile->name(), "Picture");
        EXPECT_EQ(items[0].mType, TileType::Image);
        EXPECT_EQ(items[0].mFile, "map.dds");
        EXPECT_FLOAT_EQ(items[0].mWidth, 200.f);
    }

    TEST(FalloutMenuDrawTest, a_hot_rectangle_with_an_alpha_and_no_file_is_a_plain_colour_and_a_rectangle_never_is)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><systemcolor> 5 </systemcolor>"
            // The bar behind the lit row of a list: a box, whose alpha is the `_fill_alpha` of its file
            "<hotrect name=\"Lit\"><_fill_alpha> 51 </_fill_alpha><alpha><copy src=\"me()\" trait=\"_fill_alpha\"/>"
            "</alpha><brightness> 128 </brightness><width> 300 </width><height> 30 </height></hotrect>"
            // No alpha, an alpha of 0, no size: nothing is drawn (the files turn a hotrect off with `alpha 0`)
            "<hotrect name=\"Plain\"><width> 300 </width><height> 30 </height></hotrect>"
            "<hotrect name=\"Off\"><alpha> 0 </alpha><width> 300 </width><height> 30 </height></hotrect>"
            "<hotrect name=\"NoSize\"><alpha> 90 </alpha></hotrect>"
            // A rectangle draws nothing whatever its alpha: it gives the alpha to what is in it
            "<rect name=\"Container\"><alpha> 255 </alpha><width> 300 </width><height> 30 </height></rect>"
            "<rect name=\"Box\"><_fill_alpha> 90 </_fill_alpha><alpha><copy src=\"me()\" "
            "trait=\"_fill_alpha\"/></alpha>"
            "<width> 300 </width><height> 30 </height></rect>"
            "</menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0].mTile->name(), "Lit");
        EXPECT_EQ(items[0].mType, TileType::Image);
        EXPECT_EQ(items[0].mFile, "solid.dds");
        EXPECT_EQ(items[0].mAtlas, "Interface\\InterfaceShared.tai");
        EXPECT_FLOAT_EQ(items[0].mWidth, 300.f);
        EXPECT_FLOAT_EQ(items[0].mHeight, 30.f);
        EXPECT_NEAR(items[0].mColor[3], 0.2f, 1e-4);
    }

    TEST(FalloutMenuDrawTest, a_tile_that_uses_its_local_colour_is_drawn_in_its_own_red_green_and_blue)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><systemcolor> 5 </systemcolor>"
            "<hotrect name=\"Own\"><alpha> 255 </alpha><brightness> &uselocalcolor; </brightness><red> 255 </red>"
            "<green> 128 </green><blue> 0 </blue><width> 30 </width><height> 30 </height></hotrect>"
            "<hotrect name=\"System\"><alpha> 255 </alpha><brightness> 255 </brightness><red> 255 </red>"
            "<green> 128 </green><blue> 0 </blue><width> 30 </width><height> 30 </height></hotrect>"
            "</menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 2u);
        EXPECT_EQ(items[0].mTile->name(), "Own");
        EXPECT_NEAR(items[0].mColor[0], 1.f, 1e-4);
        EXPECT_NEAR(items[0].mColor[1], 128.f / 255.f, 1e-4);
        EXPECT_NEAR(items[0].mColor[2], 0.f, 1e-4);
        EXPECT_NEAR(items[0].mColor[3], 1.f, 1e-4);
        // The same numbers with a brightness of 0 or more are the system colour's (here nothing names one, so these
        // stay)
        EXPECT_EQ(items[1].mTile->name(), "System");
    }

    TEST(FalloutMenuDrawTest, positions_add_up_through_the_tiles_a_tile_is_in)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><x> 100 </x><y> 10 </y><rect name=\"P\"><x> 20 </x><y> 5 </y>"
            "<image name=\"I\"><filename> a.dds </filename><x> 3 </x><y> 4 </y></image></rect></menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 1u);
        EXPECT_FLOAT_EQ(items[0].mX, 123.f);
        EXPECT_FLOAT_EQ(items[0].mY, 19.f);
        EXPECT_FLOAT_EQ(items[0].mWidth, 64.f);
        const std::optional<std::array<float, 4>> rect = menu.rectangle(*menu.find("I"));
        ASSERT_TRUE(rect);
        EXPECT_FLOAT_EQ((*rect)[0], 123.f);
        EXPECT_FLOAT_EQ((*rect)[1], 19.f);
    }

    TEST(FalloutMenuDrawTest, colour_is_the_system_colour_or_the_tiles_own_times_brightness_and_alpha)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><systemcolor> 5 </systemcolor>"
            "<image name=\"A\"><filename> a.dds </filename><brightness> 128 </brightness><alpha> 51 </alpha></image>"
            "<rect name=\"Own\"><systemcolor> 0 </systemcolor>"
            "<image name=\"B\"><filename> a.dds </filename><red> 255 </red><green> 0 </green><blue> 51 "
            "</blue></image></rect>"
            "</menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 2u);
        // The colour comes from the nearest tile above that names a system colour (this one's id is 5)
        EXPECT_NEAR(items[0].mColor[0], 10.f / 255.f * 128.f / 255.f, 1e-4);
        EXPECT_NEAR(items[0].mColor[1], 128.f / 255.f, 1e-4);
        EXPECT_NEAR(items[0].mColor[2], 100.f / 255.f * 128.f / 255.f, 1e-4);
        EXPECT_NEAR(items[0].mColor[3], 0.2f, 1e-4);
        // A system colour of 0 is none: the tile's own colour
        EXPECT_NEAR(items[1].mColor[0], 1.f, 1e-4);
        EXPECT_NEAR(items[1].mColor[1], 0.f, 1e-4);
        EXPECT_NEAR(items[1].mColor[2], 0.2f, 1e-4);
    }

    TEST(FalloutMenuDrawTest, a_text_is_placed_by_the_point_it_is_justified_to)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\">"
            "<text name=\"L\"><string> abcd </string><x> 100 </x></text>"
            "<text name=\"C\"><string> abcd </string><x> 100 </x><justify> &center; </justify></text>"
            "<text name=\"R\"><string> abcd </string><x> 100 </x><justify> &right; </justify></text></menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 3u);
        // Each is 40 wide
        EXPECT_FLOAT_EQ(items[0].mX, 100.f);
        EXPECT_FLOAT_EQ(items[1].mX, 80.f);
        EXPECT_FLOAT_EQ(items[2].mX, 60.f);
        EXPECT_EQ(items[2].mText, "abcd");
        EXPECT_EQ(items[2].mJustify, 4);
    }

    TEST(FalloutMenuDrawTest, a_text_wider_than_its_own_width_stands_by_the_block_it_is_drawn_in)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><text name=\"C\"><string> abcd </string><x> 100 </x><width> 20 </width>"
            "<justify> &center; </justify></text></menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 1u);
        // The text is 40 wide and the lines are justified in a block that is never narrower than that, so the block
        // is centred on the point (a width of 20 would put it 10 further right than the lines are)
        EXPECT_FLOAT_EQ(items[0].mX, 80.f);
    }

    TEST(FalloutMenuDrawTest, the_rectangle_of_a_tile_in_a_text_follows_the_text_it_is_justified_to)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><text name=\"R\"><string> abcd </string><x> 100 </x><justify> &right; </justify>"
            "<image name=\"I\"><filename> a.dds </filename><x> 5 </x></image></text></menu>");
        // The text is 40 wide, so it starts at 60 and its image 5 further; the rectangle is where the image is drawn
        const auto rect = menu.rectangle(*menu.find("I"));
        ASSERT_TRUE(rect.has_value());
        EXPECT_FLOAT_EQ((*rect)[0], 65.f);
        float drawn = -1.f;
        for (const DrawItem& item : menu.draw())
            if (item.mType == TileType::Image)
                drawn = item.mX;
        EXPECT_FLOAT_EQ(drawn, (*rect)[0]);
    }

    TEST(FalloutMenuDrawTest, a_clip_window_cuts_what_is_in_it_that_asks_for_it)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"W\"><x> 10 </x><y> 20 </y><width> 100 </width><height> 50 </height>"
            "<clipwindow> &true; </clipwindow>"
            "<image name=\"In\"><filename> a.dds </filename><clips> &true; </clips></image>"
            "<image name=\"Free\"><filename> a.dds </filename></image></rect></menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 2u);
        EXPECT_TRUE(items[0].mClipped);
        EXPECT_FLOAT_EQ(items[0].mClipX, 10.f);
        EXPECT_FLOAT_EQ(items[0].mClipY, 20.f);
        EXPECT_FLOAT_EQ(items[0].mClipWidth, 100.f);
        EXPECT_FLOAT_EQ(items[0].mClipHeight, 50.f);
        EXPECT_FALSE(items[1].mClipped);
    }

    TEST(FalloutMenuHitTest, the_pointer_is_on_the_topmost_visible_tile_that_is_a_target)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\">"
            "<rect name=\"Back\"><locus> &true; </locus><target> &true; </target><width> 100 </width><height> 100 "
            "</height></rect>"
            "<rect name=\"Front\"><locus> &true; </locus><target> &true; </target><depth> 3 </depth>"
            "<x> 50 </x><width> 100 </width><height> 100 </height></rect>"
            "<rect name=\"NoTarget\"><locus> &true; </locus><depth> 9 </depth><width> 400 </width><height> 400 "
            "</height></rect>"
            "<rect name=\"Off\"><locus> &true; </locus><target> &true; </target><visible> &false; </visible><depth> 9 "
            "</depth>"
            "<width> 400 </width><height> 400 </height></rect></menu>");
        ASSERT_NE(menu.tileAt(10, 10), nullptr);
        EXPECT_EQ(menu.tileAt(10, 10)->name(), "Back");
        EXPECT_EQ(menu.tileAt(60, 10)->name(), "Front");
        EXPECT_EQ(menu.tileAt(140, 99)->name(), "Front");
        EXPECT_EQ(menu.tileAt(300, 300), nullptr);
    }

    TEST(FalloutMenuHitTest, the_wheel_goes_to_the_tile_that_takes_it)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"List\"><wheelable> &true; </wheelable><width> 100 </width><height> 100 "
            "</height></rect></menu>");
        ASSERT_NE(menu.wheelTileAt(5, 5), nullptr);
        EXPECT_EQ(menu.wheelTileAt(5, 5)->name(), "List");
        EXPECT_EQ(menu.wheelTileAt(500, 5), nullptr);
        EXPECT_EQ(menu.tileAt(5, 5), nullptr);
    }

    TEST(FalloutMenuHitTest, a_tile_that_clips_cannot_be_hit_outside_its_window)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"W\"><width> 50 </width><height> 50 </height><clipwindow> &true; "
            "</clipwindow>"
            "<rect name=\"Row\"><locus> &true; </locus><target> &true; </target><clips> &true; </clips>"
            "<width> 200 </width><height> 200 </height></rect></rect></menu>");
        EXPECT_NE(menu.tileAt(10, 10), nullptr);
        EXPECT_EQ(menu.tileAt(100, 100), nullptr);
    }

    // super_text_box: a hotrect that takes its position from the text box it holds, which has the same position
    TEST(FalloutMenuLayoutTest, a_hotrect_that_takes_its_position_from_its_child_does_not_move_the_child_again)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"Bar\"><x> 50 </x><y> 100 </y>"
            "<hotrect name=\"Button\"><_x> 30 </_x><_y> -5 </_y>"
            "<x><copy src=\"child(Box)\" trait=\"x\"/></x><y><copy src=\"child(Box)\" trait=\"y\"/></y>"
            "<rect name=\"Box\"><x><copy src=\"parent()\" trait=\"_x\"/></x><y><copy src=\"parent()\" "
            "trait=\"_y\"/></y>"
            "<image name=\"Line\"><filename> a.dds </filename></image></rect></hotrect>"
            "<hotrect name=\"Plain\"><x> 30 </x><y> 20 </y><rect name=\"In\"><x> 4 </x>"
            "<image name=\"Pic\"><filename> a.dds </filename></image></rect></hotrect></rect></menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 2u);
        // The button stands once at 30 right of the bar, as the child says, not at 60
        EXPECT_EQ(items[0].mTile->name(), "Line");
        EXPECT_FLOAT_EQ(items[0].mX, 50.f + 30.f);
        EXPECT_FLOAT_EQ(items[0].mY, 100.f - 5.f);
        // Any other hotrect is a frame like every tile
        EXPECT_EQ(items[1].mTile->name(), "Pic");
        EXPECT_FLOAT_EQ(items[1].mX, 50.f + 30.f + 4.f);
        EXPECT_FLOAT_EQ(items[1].mY, 100.f + 20.f);
        const std::optional<std::array<float, 4>> rect = menu.rectangle(*menu.find("Line"));
        ASSERT_TRUE(rect);
        EXPECT_FLOAT_EQ((*rect)[0], 80.f);
    }

    TEST(FalloutMenuLayoutTest, a_reference_names_the_tile_and_trait_a_button_of_the_pad_stands_for)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><xbuttonx><ref src=\"Drop\" trait=\"clicked\"/></xbuttonx>"
            "<_plain> 4 </_plain><rect name=\"Drop\"/></menu>");
        const auto reference = menu.reference(menu.root(), "xbuttonx");
        ASSERT_TRUE(reference);
        EXPECT_EQ(reference->first, menu.find("Drop"));
        EXPECT_EQ(reference->second, "clicked");
        EXPECT_FALSE(menu.reference(menu.root(), "_plain"));
        EXPECT_FALSE(menu.reference(menu.root(), "_missing"));
    }

    TEST(FalloutMenuLayoutTest, a_trait_that_asks_for_itself_does_not_run_away)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"A\"><x><copy src=\"me()\" trait=\"y\"/></x>"
            "<y><copy src=\"me()\" trait=\"x\"/><add> 1 </add></y></rect></menu>");
        // Either answer will do; what matters is that this comes back
        (void)numberOf(menu, "A", "x");
        SUCCEED();
    }

    TEST(FalloutMenuLayoutTest, a_text_with_marks_has_pages_and_draws_the_one_that_pagenum_names)
    {
        Fixture f;
        Menu& menu = f.open(
            "<menu name=\"M\"><rect name=\"R\"><x> 100 </x><y> 200 </y>"
            "<text name=\"T\"><ishtml> &true; </ishtml><x> 5 </x><y> 7 </y>"
            "<wrapwidth> 100 </wrapwidth><wraplimit> 50 </wraplimit><pagenum> 1 </pagenum></text>"
            "</rect></menu>");
        Tile& text = *menu.find("T");
        menu.set(text, "string", Value::fromText("<font color=00FF00>one</font><br>two<br>three<br>four<br>five"));
        // Five lines of 20 are three pages of at most 50
        EXPECT_FLOAT_EQ(menu.number(text, "pagecount"), 3.f);
        // The size of the text is that of the page that is shown: two lines, the widest has five letters
        EXPECT_FLOAT_EQ(menu.number(text, "width"), 50.f);
        EXPECT_FLOAT_EQ(menu.number(text, "height"), 40.f);
        std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 2u);
        EXPECT_EQ(items[0].mText, "three");
        EXPECT_FLOAT_EQ(items[0].mX, 105.f);
        EXPECT_FLOAT_EQ(items[0].mY, 207.f);
        EXPECT_EQ(items[1].mText, "four");
        EXPECT_FLOAT_EQ(items[1].mY, 227.f);
        // The letters have the colour of the book where the text does not give one
        EXPECT_NEAR(items[0].mColor[0], 117.f / 255.f, 1e-4f);

        menu.set(text, "pagenum", Value(0.f));
        items = menu.draw();
        ASSERT_EQ(items.size(), 2u);
        EXPECT_EQ(items[0].mText, "one");
        EXPECT_FLOAT_EQ(items[0].mColor[0], 0.f);
        EXPECT_FLOAT_EQ(items[0].mColor[1], 1.f);

        // A number that is no page shows nothing, and so does the pages' end
        menu.set(text, "pagenum", Value(3.f));
        EXPECT_TRUE(menu.draw().empty());
        menu.set(text, "pagenum", Value(-1.f));
        EXPECT_TRUE(menu.draw().empty());

        // Another text makes other pages
        menu.set(text, "pagenum", Value(0.f));
        menu.set(text, "string", Value::fromText("<b>short"));
        EXPECT_FLOAT_EQ(menu.number(text, "pagecount"), 1.f);
    }

    TEST(FalloutMenuLayoutTest, a_text_without_ishtml_is_drawn_as_one_text)
    {
        Fixture f;
        Menu& menu = f.open("<menu name=\"M\"><text name=\"T\"><string> hello </string></text></menu>");
        const std::vector<DrawItem> items = menu.draw();
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0].mText, "hello");
        EXPECT_FLOAT_EQ(menu.number(*menu.find("T"), "pagecount"), 0.f);
    }
}
