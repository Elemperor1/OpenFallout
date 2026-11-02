#include <components/falloutmenu/xml.hpp>

#include <gtest/gtest.h>

namespace
{
    namespace Xml = FalloutMenu::Xml;

    TEST(FalloutMenuXmlTest, elements_nest_and_names_are_lower_case)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("<Menu Name=\"A\"><Rect><X> 5 </X></Rect></Menu>");
        ASSERT_EQ(nodes.size(), 1u);
        EXPECT_EQ(nodes[0].mName, "menu");
        ASSERT_NE(nodes[0].attribute("name"), nullptr);
        EXPECT_EQ(*nodes[0].attribute("name"), "A");
        ASSERT_EQ(nodes[0].mChildren.size(), 1u);
        ASSERT_EQ(nodes[0].mChildren[0].mChildren.size(), 1u);
        EXPECT_EQ(nodes[0].mChildren[0].mChildren[0].mName, "x");
        EXPECT_EQ(Xml::trim(nodes[0].mChildren[0].mChildren[0].mText), "5");
    }

    TEST(FalloutMenuXmlTest, a_file_of_pieces_has_more_than_one_top_level_element)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("<x> 1 </x><y> 2 </y>");
        ASSERT_EQ(nodes.size(), 2u);
        EXPECT_EQ(nodes[1].mName, "y");
    }

    TEST(FalloutMenuXmlTest, comments_are_skipped_also_with_three_dashes)
    {
        const std::vector<Xml::Node> nodes
            = Xml::parse("<!-- one --><a><!--- two ---><b/><!-- three --></a><!-- <c/> -->");
        ASSERT_EQ(nodes.size(), 1u);
        ASSERT_EQ(nodes[0].mChildren.size(), 1u);
        EXPECT_EQ(nodes[0].mChildren[0].mName, "b");
    }

    TEST(FalloutMenuXmlTest, a_slash_inside_an_attribute_without_quotes_is_part_of_the_value)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("<include src=prefabs/box.xml/><rect name=a/b>");
        ASSERT_EQ(nodes.size(), 2u);
        ASSERT_NE(nodes[0].attribute("src"), nullptr);
        EXPECT_EQ(*nodes[0].attribute("src"), "prefabs/box.xml");
        ASSERT_NE(nodes[1].attribute("name"), nullptr);
        EXPECT_EQ(*nodes[1].attribute("name"), "a/b");
    }

    TEST(FalloutMenuXmlTest, a_comment_between_the_attributes_of_a_tag_is_skipped)
    {
        const std::vector<Xml::Node> nodes
            = Xml::parse("<rect <!-- why --> name=\"Button\" <!-- and --> id='3'><x>1</x></rect>");
        ASSERT_EQ(nodes.size(), 1u);
        ASSERT_NE(nodes[0].attribute("name"), nullptr);
        EXPECT_EQ(*nodes[0].attribute("name"), "Button");
        ASSERT_NE(nodes[0].attribute("id"), nullptr);
        EXPECT_EQ(*nodes[0].attribute("id"), "3");
        ASSERT_EQ(nodes[0].mChildren.size(), 1u);
        EXPECT_EQ(nodes[0].mChildren[0].mName, "x");
        // A comment that is never closed ends the file, not the program
        EXPECT_NO_THROW(Xml::parse("<rect <!-- never ended name=\"a\">"));
    }

    TEST(FalloutMenuXmlTest, a_closing_tag_closes_what_was_left_open_inside_it)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("<a><b><c></a><d/>");
        ASSERT_EQ(nodes.size(), 2u);
        EXPECT_EQ(nodes[0].mName, "a");
        EXPECT_EQ(nodes[1].mName, "d");
        ASSERT_EQ(nodes[0].mChildren.size(), 1u);
        EXPECT_EQ(nodes[0].mChildren[0].mName, "b");
    }

    TEST(FalloutMenuXmlTest, a_closing_tag_that_names_nothing_open_ends_the_element_that_is_open)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("<a><b></zzz><c/></a></a>");
        // </zzz> ends b, </a> ends a, and the second </a> has nothing to end
        ASSERT_EQ(nodes.size(), 1u);
        ASSERT_EQ(nodes[0].mChildren.size(), 2u);
        EXPECT_EQ(nodes[0].mChildren[0].mName, "b");
        EXPECT_TRUE(nodes[0].mChildren[0].mChildren.empty());
        EXPECT_EQ(nodes[0].mChildren[1].mName, "c");
    }

    TEST(FalloutMenuXmlTest, elements_that_are_never_closed_end_with_the_file)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("<a><b> text");
        ASSERT_EQ(nodes.size(), 1u);
        ASSERT_EQ(nodes[0].mChildren.size(), 1u);
        EXPECT_EQ(Xml::trim(nodes[0].mChildren[0].mText), "text");
    }

    TEST(FalloutMenuXmlTest, an_attribute_may_lack_its_opening_quote)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("<rect name=Box\"><x>1</x></rect>");
        ASSERT_EQ(nodes.size(), 1u);
        ASSERT_NE(nodes[0].attribute("name"), nullptr);
        EXPECT_EQ(*nodes[0].attribute("name"), "Box");
        EXPECT_EQ(nodes[0].mChildren.size(), 1u);
    }

    TEST(FalloutMenuXmlTest, a_byte_order_mark_is_skipped)
    {
        const std::vector<Xml::Node> nodes = Xml::parse("\xEF\xBB\xBF<a/>");
        ASSERT_EQ(nodes.size(), 1u);
        EXPECT_EQ(nodes[0].mName, "a");
    }

    TEST(FalloutMenuXmlTest, trim_and_lower_work_on_ascii)
    {
        EXPECT_EQ(Xml::trim(" \t a b \r\n"), "a b");
        EXPECT_EQ(Xml::trim("   "), "");
        EXPECT_EQ(Xml::lower("AbC_1"), "abc_1");
    }
}
