#include <components/falloutmenu/atlas.hpp>
#include <components/falloutmenu/font.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <string>

namespace
{
    using FalloutMenu::Atlas;
    using FalloutMenu::Font;
    using FalloutMenu::FontSet;

    constexpr std::size_t headerBytes = 296;
    constexpr std::size_t glyphBytes = 56;

    void putFloat(std::string& bytes, std::size_t at, float value)
    {
        std::memcpy(bytes.data() + at, &value, sizeof(value));
    }

    void putGlyph(
        std::string& bytes, unsigned char c, float width, float height, float before, float after, float ascent)
    {
        const std::size_t at = bytes.size() - 256 * glyphBytes + c * glyphBytes;
        putFloat(bytes, at + 4, 0.25f);
        putFloat(bytes, at + 8, 0.5f);
        putFloat(bytes, at + 12, 0.5f);
        putFloat(bytes, at + 24, 0.75f);
        putFloat(bytes, at + 36, width);
        putFloat(bytes, at + 40, height);
        putFloat(bytes, at + 44, before);
        putFloat(bytes, at + 48, after);
        putFloat(bytes, at + 52, ascent);
    }

    // A font of 30 high lines whose characters are 10 wide with 1 before and 1 after, and a narrow space
    std::string testFont()
    {
        std::string bytes(headerBytes + 256 * glyphBytes, '\0');
        putFloat(bytes, 0, 30.f);
        const unsigned int one = 1;
        std::memcpy(bytes.data() + 4, &one, sizeof(one));
        std::memcpy(bytes.data() + 12, "test_font_0", 11);
        for (unsigned char c = 33; c < 127; ++c)
            putGlyph(bytes, c, 10.f, 20.f, 1.f, 1.f, 20.f);
        putGlyph(bytes, ' ', 0.f, 0.f, 0.f, 6.f, 0.f);
        return bytes;
    }

    TEST(FalloutMenuFontTest, a_file_that_is_too_short_or_has_no_size_is_not_a_font)
    {
        EXPECT_FALSE(Font::parse("short"));
        EXPECT_FALSE(Font::parse(std::string(headerBytes + 256 * glyphBytes, '\0')));
    }

    TEST(FalloutMenuFontTest, a_character_with_a_number_that_is_not_a_number_makes_the_file_not_a_font)
    {
        const float notFinite[] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() };
        // The corners, then the width, height, room before and after, and how far it reaches above the line
        const std::size_t fields[] = { 4, 8, 12, 24, 36, 40, 44, 48, 52 };
        for (const float bad : notFinite)
        {
            for (const std::size_t field : fields)
            {
                std::string bytes = testFont();
                putFloat(bytes, bytes.size() - 256 * glyphBytes + 'A' * glyphBytes + field, bad);
                EXPECT_FALSE(Font::parse(bytes)) << "field " << field << " value " << bad;
            }
        }
        EXPECT_TRUE(Font::parse(testFont()));
    }

    TEST(FalloutMenuFontTest, the_header_gives_the_line_height_and_the_texture_name)
    {
        const std::optional<Font> font = Font::parse(testFont());
        ASSERT_TRUE(font);
        EXPECT_FLOAT_EQ(font->lineHeight(), 30.f);
        EXPECT_EQ(font->textureName(), "test_font_0");
        EXPECT_FLOAT_EQ(font->baseline(), 20.f);
        EXPECT_FLOAT_EQ(font->glyph('A').mWidth, 10.f);
        EXPECT_FLOAT_EQ(font->glyph('A').mU0, 0.25f);
        EXPECT_FLOAT_EQ(font->glyph('A').mV1, 0.75f);
    }

    TEST(FalloutMenuFontTest, a_character_is_as_wide_as_its_picture_and_the_room_around_it)
    {
        const std::optional<Font> font = Font::parse(testFont());
        ASSERT_TRUE(font);
        EXPECT_FLOAT_EQ(font->advance('A'), 12.f);
        EXPECT_FLOAT_EQ(font->advance(' '), 6.f);
        EXPECT_FLOAT_EQ(font->lineWidth("AB A"), 12.f + 12.f + 6.f + 12.f);
    }

    TEST(FalloutMenuFontTest, a_character_of_utf8_is_one_character_of_the_font_not_one_for_each_of_its_bytes)
    {
        std::string bytes = testFont();
        // The letters of the Windows-1252 code page the games use: e with an acute accent (0xE9), a right single quote
        // (0x92)
        putGlyph(bytes, 0xE9, 10.f, 20.f, 1.f, 1.f, 20.f);
        putGlyph(bytes, 0x92, 4.f, 20.f, 1.f, 1.f, 20.f);
        const std::optional<Font> font = Font::parse(bytes);
        ASSERT_TRUE(font);
        // "e acute" in UTF-8 is 0xC3 0xA9; the quote U+2019 is 0xE2 0x80 0x99
        EXPECT_FLOAT_EQ(font->lineWidth("A\xC3\xA9"), 12.f + 12.f);
        EXPECT_FLOAT_EQ(font->lineWidth("\xE2\x80\x99"), 6.f);
        EXPECT_EQ(font->layout("A\xC3\xA9\xE2\x80\x99").mQuads.size(), 3u);
        // A character the code page lacks is a question mark, and a byte that is not UTF-8 is kept as it is
        EXPECT_FLOAT_EQ(font->lineWidth("\xE4\xB8\xAD"), font->advance('?'));
        EXPECT_FLOAT_EQ(font->lineWidth("\xE9"), 12.f);
    }

    TEST(FalloutMenuFontTest, a_line_is_laid_out_character_by_character_from_the_top_left)
    {
        const std::optional<Font> font = Font::parse(testFont());
        ASSERT_TRUE(font);
        const Font::Layout layout = font->layout("A A");
        ASSERT_EQ(layout.mQuads.size(), 2u);
        EXPECT_EQ(layout.mLines, 1);
        EXPECT_FLOAT_EQ(layout.mWidth, 30.f);
        EXPECT_FLOAT_EQ(layout.mHeight, 30.f);
        // The picture starts after the room before it, and its top is as far above the baseline as it reaches (the line
        // is drawn textRise higher than a baseline as far down as the tallest character reaches)
        EXPECT_FLOAT_EQ(layout.mQuads[0].mX, 1.f);
        EXPECT_FLOAT_EQ(layout.mQuads[0].mY, -Font::textRise);
        EXPECT_FLOAT_EQ(layout.mQuads[1].mX, 12.f + 6.f + 1.f);
    }

    TEST(FalloutMenuFontTest, text_breaks_at_new_lines_and_at_the_space_before_a_word_that_does_not_fit)
    {
        const std::optional<Font> font = Font::parse(testFont());
        ASSERT_TRUE(font);
        EXPECT_EQ(font->layout("A\nA").mLines, 2);
        EXPECT_FLOAT_EQ(font->layout("A\nA").mHeight, 60.f);
        // Three words of two characters (24 wide) in a space for 40: one word on each line
        const Font::Layout wrapped = font->layout("AA AA AA", 40.f);
        EXPECT_EQ(wrapped.mLines, 3);
        EXPECT_LE(wrapped.mWidth, 40.f);
        // Without a limit it is one line
        EXPECT_EQ(font->layout("AA AA AA").mLines, 1);
    }

    TEST(FalloutMenuFontTest, a_word_wider_than_the_room_stays_whole)
    {
        const std::optional<Font> font = Font::parse(testFont());
        ASSERT_TRUE(font);
        // Three characters (36 wide) in a space for 20 are one line, as the label of a narrow button is
        const Font::Layout alone = font->layout("AAA", 20.f);
        EXPECT_EQ(alone.mLines, 1);
        EXPECT_EQ(alone.mQuads.size(), 3u);
        EXPECT_FLOAT_EQ(alone.mWidth, 36.f);
        // With more words the long one is a line of its own and the others follow
        const Font::Layout more = font->layout("AAA A A", 20.f);
        EXPECT_EQ(more.mLines, 3);
        EXPECT_FLOAT_EQ(more.mWidth, 36.f);
    }

    TEST(FalloutMenuFontTest, lines_are_justified_inside_the_width_of_the_block)
    {
        const std::optional<Font> font = Font::parse(testFont());
        ASSERT_TRUE(font);
        // "A" is 12 wide and "AAAA" 48: the short line of a centred block starts 18 in, of a right one 36 in
        const Font::Layout centre = font->layout("AAAA\nA", 0.f, 2);
        ASSERT_EQ(centre.mQuads.size(), 5u);
        EXPECT_FLOAT_EQ(centre.mQuads[4].mX, 18.f + 1.f);
        const Font::Layout right = font->layout("AAAA\nA", 0.f, 4);
        EXPECT_FLOAT_EQ(right.mQuads[4].mX, 36.f + 1.f);
        EXPECT_FLOAT_EQ(right.mQuads[4].mY, 30.f - Font::textRise);
    }

    TEST(FalloutMenuFontTest, a_block_wider_than_its_longest_line_is_the_width_the_lines_are_justified_in)
    {
        const std::optional<Font> font = Font::parse(testFont());
        ASSERT_TRUE(font);
        // One "A" (12 wide) wrapped to 100: the block is 100 wide, so a right-justified A ends at 100, a centred one
        // is in the middle, and the width the text reports is the block's, which is where the tile puts its x
        const Font::Layout right = font->layout("A", 100.f, 4);
        EXPECT_FLOAT_EQ(right.mWidth, 100.f);
        EXPECT_FLOAT_EQ(right.mQuads[0].mX, 88.f + 1.f);
        const Font::Layout centre = font->layout("A", 100.f, 2);
        EXPECT_FLOAT_EQ(centre.mQuads[0].mX, 44.f + 1.f);
        // A tile wider than its text justifies in its own width
        const Font::Layout wide = font->layout("A", 0.f, 4, 60.f);
        EXPECT_FLOAT_EQ(wide.mWidth, 60.f);
        EXPECT_FLOAT_EQ(wide.mQuads[0].mX, 48.f + 1.f);
        // and never in less than the text takes
        EXPECT_FLOAT_EQ(font->layout("AAAA", 0.f, 4, 10.f).mWidth, 48.f);
    }

    TEST(FalloutMenuFontTest, the_font_set_gives_a_font_that_is_there_for_a_number_that_is_not)
    {
        FontSet set;
        EXPECT_TRUE(set.empty());
        EXPECT_EQ(set.get(1), nullptr);
        set.set(3, *Font::parse(testFont()));
        EXPECT_FALSE(set.empty());
        EXPECT_NE(set.get(3), nullptr);
        EXPECT_EQ(set.get(5), set.get(3));
        EXPECT_EQ(set.get(0), set.get(3));
        // A number outside the nine is ignored when it is set
        set.set(10, *Font::parse(testFont()));
        EXPECT_EQ(set.get(10), set.get(3));
    }

    TEST(FalloutMenuAtlasTest, each_line_names_a_part_of_the_big_texture)
    {
        const Atlas atlas = Atlas::parse(
            "# comment\n"
            "\n"
            "Solid.dds\t\tBig0.dds, 0, 2D, 0.250000, 0.500000, 0.000000, 0.125000, 0.062500\n"
            "line.dds\t\tBig0.dds, 0, 2D, 0.0, 0.0, 0.0, 1.0, 1.0\r\n"
            "bad line\n");
        EXPECT_EQ(atlas.size(), 2u);
        const Atlas::Entry* solid = atlas.find("Interface\\Shared\\SOLID.dds");
        ASSERT_NE(solid, nullptr);
        EXPECT_EQ(solid->mTexture, "Big0.dds");
        EXPECT_FLOAT_EQ(solid->mU, 0.25f);
        EXPECT_FLOAT_EQ(solid->mV, 0.5f);
        EXPECT_FLOAT_EQ(solid->mWidth, 0.125f);
        EXPECT_FLOAT_EQ(solid->mHeight, 0.0625f);
        EXPECT_NE(atlas.find("line.dds"), nullptr);
        EXPECT_EQ(atlas.find("nothing.dds"), nullptr);
    }
}
