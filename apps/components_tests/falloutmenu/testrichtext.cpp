#include <components/falloutmenu/richtext.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace FalloutMenu;

    // Every character is 10 wide; a line is 20 high in font 1 and 30 high in font 2; a picture of "big" is 100 by 50
    RichMeasure measure()
    {
        RichMeasure result;
        result.mWidth = [](int, std::string_view text) { return 10.f * static_cast<float>(text.size()); };
        result.mLineHeight = [](int font) { return font == 2 ? 30.f : 20.f; };
        result.mImageSize = [](const std::string& file) -> std::optional<std::array<float, 2>> {
            if (file.find("big") != std::string::npos)
                return std::array<float, 2>{ 100.f, 50.f };
            return std::nullopt;
        };
        return result;
    }

    RichParams params(float wrapWidth = 0, float wrapLimit = 0, float lineGap = 0)
    {
        RichParams result;
        result.mWrapWidth = wrapWidth;
        result.mWrapLimit = wrapLimit;
        result.mLineGap = lineGap;
        return result;
    }

    TEST(FalloutRichText, a_text_with_nothing_in_it_is_one_empty_page)
    {
        const auto pages = layoutRichText("", params(100), measure());
        ASSERT_EQ(pages.size(), 1u);
        EXPECT_TRUE(pages[0].mItems.empty());
        EXPECT_FLOAT_EQ(pages[0].mHeight, 0.f);
    }

    TEST(FalloutRichText, a_plain_text_is_broken_at_the_spaces_where_a_word_would_not_fit_and_at_new_lines)
    {
        const auto pages = layoutRichText("aa bb cc\ndd", params(50), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 3u);
        EXPECT_EQ(pages[0].mItems[0].mText, "aa bb");
        EXPECT_FLOAT_EQ(pages[0].mItems[0].mY, 0.f);
        EXPECT_EQ(pages[0].mItems[1].mText, "cc");
        EXPECT_FLOAT_EQ(pages[0].mItems[1].mY, 20.f);
        EXPECT_EQ(pages[0].mItems[2].mText, "dd");
        EXPECT_FLOAT_EQ(pages[0].mItems[2].mY, 40.f);
        EXPECT_FLOAT_EQ(pages[0].mWidth, 50.f);
        EXPECT_FLOAT_EQ(pages[0].mHeight, 60.f);
    }

    TEST(FalloutRichText, a_word_wider_than_the_line_is_broken_where_it_fills_the_line)
    {
        const auto pages = layoutRichText("abcdefghij", params(45), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 3u);
        EXPECT_EQ(pages[0].mItems[0].mText, "abcd");
        EXPECT_EQ(pages[0].mItems[1].mText, "efgh");
        EXPECT_EQ(pages[0].mItems[2].mText, "ij");
    }

    TEST(
        FalloutRichText, a_font_and_a_colour_stay_until_the_end_of_the_mark_and_the_marks_it_does_not_know_are_left_out)
    {
        const auto pages
            = layoutRichText("<b><font face=2 color=FF8000>red</font> plain<br>next</b>", params(0), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 3u);
        EXPECT_EQ(pages[0].mItems[0].mText, "red");
        EXPECT_EQ(pages[0].mItems[0].mFont, 2);
        EXPECT_FLOAT_EQ(pages[0].mItems[0].mColor[0], 1.f);
        EXPECT_NEAR(pages[0].mItems[0].mColor[1], 128.f / 255.f, 1e-4f);
        EXPECT_FLOAT_EQ(pages[0].mItems[0].mColor[2], 0.f);
        // The rest of the line is in the font and colour the text has by itself, with the space that stood before it
        EXPECT_EQ(pages[0].mItems[1].mText, " plain");
        EXPECT_EQ(pages[0].mItems[1].mFont, 1);
        EXPECT_NEAR(pages[0].mItems[1].mColor[0], 117.f / 255.f, 1e-4f);
        EXPECT_FLOAT_EQ(pages[0].mItems[1].mX, 30.f);
        // Letters of a smaller font stand on the bottom of the line the bigger one makes
        EXPECT_FLOAT_EQ(pages[0].mItems[1].mY, 10.f);
        EXPECT_EQ(pages[0].mItems[2].mText, "next");
        EXPECT_FLOAT_EQ(pages[0].mItems[2].mY, 30.f);
    }

    TEST(FalloutRichText, a_paragraph_is_two_lines_down_and_a_break_one)
    {
        const auto pages = layoutRichText("<b>one<p>two<br>three", params(0), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 3u);
        EXPECT_FLOAT_EQ(pages[0].mItems[0].mY, 0.f);
        EXPECT_FLOAT_EQ(pages[0].mItems[1].mY, 40.f);
        EXPECT_FLOAT_EQ(pages[0].mItems[2].mY, 60.f);
    }

    TEST(FalloutRichText, the_height_a_page_may_take_makes_the_pages_and_a_line_that_does_not_fit_goes_to_the_next)
    {
        const auto pages = layoutRichText("<b>a<br>b<br>c<br>d<br>e", params(0, 50), measure());
        ASSERT_EQ(pages.size(), 3u);
        EXPECT_EQ(pages[0].mItems.size(), 2u);
        EXPECT_EQ(pages[1].mItems.size(), 2u);
        ASSERT_EQ(pages[2].mItems.size(), 1u);
        EXPECT_EQ(pages[2].mItems[0].mText, "e");
        EXPECT_FLOAT_EQ(pages[2].mItems[0].mY, 0.f);
        EXPECT_FLOAT_EQ(pages[0].mHeight, 40.f);
    }

    TEST(FalloutRichText, the_gap_between_lines_is_added_to_every_line_and_counts_for_the_page)
    {
        const auto gap = layoutRichText("<b>a<br>b<br>c", params(0, 60, 10), measure());
        // A line is 30 with the gap: two fit in 60
        ASSERT_EQ(gap.size(), 2u);
        ASSERT_EQ(gap[0].mItems.size(), 2u);
        EXPECT_FLOAT_EQ(gap[0].mItems[1].mY, 30.f);
    }

    TEST(FalloutRichText, a_rule_is_a_new_page_when_there_are_pages_and_two_lines_when_there_are_none)
    {
        const auto paged = layoutRichText("<b>a<hr>b", params(0, 1000), measure());
        ASSERT_EQ(paged.size(), 2u);
        EXPECT_EQ(paged[1].mItems[0].mText, "b");
        EXPECT_FLOAT_EQ(paged[1].mItems[0].mY, 0.f);
        const auto whole = layoutRichText("<b>a<hr>b", params(0, 0), measure());
        ASSERT_EQ(whole.size(), 1u);
        ASSERT_EQ(whole[0].mItems.size(), 2u);
        EXPECT_FLOAT_EQ(whole[0].mItems[1].mY, 40.f);
    }

    TEST(FalloutRichText, the_blank_lines_a_text_ends_with_make_no_page_of_their_own)
    {
        const auto pages = layoutRichText("<b>a<br>b<br><br><br>", params(0, 40), measure());
        EXPECT_EQ(pages.size(), 1u);
    }

    TEST(FalloutRichText, a_division_puts_its_lines_in_the_middle_or_at_the_right_of_the_width)
    {
        const auto pages
            = layoutRichText("<div align=center>ab</div><div align=\"right\">cd</div>ef", params(100), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 3u);
        EXPECT_FLOAT_EQ(pages[0].mItems[0].mX, 40.f);
        EXPECT_FLOAT_EQ(pages[0].mItems[1].mX, 80.f);
        EXPECT_FLOAT_EQ(pages[0].mItems[2].mX, 0.f);
    }

    TEST(FalloutRichText, a_picture_takes_the_size_the_text_gives_or_the_file_has_and_makes_its_line_as_high)
    {
        const auto pages
            = layoutRichText("<img src=\"x.dds\" width=30 height=40>one<br><img src=big.dds>two", params(0), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 4u);
        EXPECT_TRUE(pages[0].mItems[0].mImage);
        EXPECT_FLOAT_EQ(pages[0].mItems[0].mWidth, 30.f);
        EXPECT_FLOAT_EQ(pages[0].mItems[0].mHeight, 40.f);
        EXPECT_EQ(pages[0].mItems[0].mFile, "x.dds");
        EXPECT_EQ(pages[0].mItems[1].mText, "one");
        EXPECT_FLOAT_EQ(pages[0].mItems[1].mX, 30.f);
        // The letters stand on the bottom of the line of the picture
        EXPECT_FLOAT_EQ(pages[0].mItems[1].mY, 20.f);
        EXPECT_TRUE(pages[0].mItems[2].mImage);
        EXPECT_FLOAT_EQ(pages[0].mItems[2].mWidth, 100.f);
        EXPECT_FLOAT_EQ(pages[0].mItems[2].mHeight, 50.f);
        EXPECT_FLOAT_EQ(pages[0].mItems[2].mY, 40.f);
        // A picture with no size to be had is left out
        const auto none = layoutRichText("<img src=\"nothing.dds\">x", params(0), measure());
        ASSERT_EQ(none[0].mItems.size(), 1u);
        EXPECT_EQ(none[0].mItems[0].mText, "x");
    }

    // FalloutNV.exe 0xA17390 and 0xA18C79 (Mac round 12AN B9)
    TEST(FalloutRichText, in_marked_up_text_line_ends_are_left_out_and_every_space_stays_but_none_starts_a_line)
    {
        const auto pages = layoutRichText("<b>  one \r\n  two<br>   three", params(0), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 2u);
        EXPECT_EQ(pages[0].mItems[0].mText, "one   two");
        EXPECT_EQ(pages[0].mItems[1].mText, "three");
        // A line end in the middle of a word does not split it, and a tab is a space
        const auto joined = layoutRichText("<b>wo\r\nrd\tx", params(0), measure());
        ASSERT_EQ(joined[0].mItems.size(), 1u);
        EXPECT_EQ(joined[0].mItems[0].mText, "word x");
    }

    TEST(FalloutRichText, in_plain_text_a_line_feed_breaks_the_line_and_a_run_of_spaces_stays)
    {
        const auto pages = layoutRichText("aa  bb\tcc\r\ndd", params(0), measure());
        ASSERT_EQ(pages.size(), 1u);
        ASSERT_EQ(pages[0].mItems.size(), 2u);
        EXPECT_EQ(pages[0].mItems[0].mText, "aa  bb cc");
        EXPECT_EQ(pages[0].mItems[1].mText, "dd");
    }
}
