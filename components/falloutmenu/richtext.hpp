#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_RICHTEXT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_RICHTEXT_H

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FalloutMenu
{
    /// What the layout of a marked up text asks of the fonts and the files of the game
    struct RichMeasure
    {
        /// The width of a line of text in a font (1 to 9)
        std::function<float(int font, std::string_view text)> mWidth;
        /// The distance from one line to the next in a font
        std::function<float(int font)> mLineHeight;
        /// The size of a picture of the text, by the path the text gives (width and height)
        std::function<std::optional<std::array<float, 2>>(const std::string& file)> mImageSize;
    };

    /// What a text tile says about its layout (the traits of the tile, in the units of the menu)
    struct RichParams
    {
        /// The font of the text where it does not change it (1 to 9)
        int mFont = 1;
        /// The width the lines are broken at (nothing under 2 breaks them)
        float mWrapWidth = 0;
        /// The height a page may take (nothing under 2 makes pages: the whole text is one)
        float mWrapLimit = 0;
        /// What is added to the height of every line
        float mLineGap = 0;
        /// The colour of the letters where the text does not give one, red, green and blue from 0 to 1
        std::array<float, 3> mColor{ 117.f / 255.f, 59.f / 255.f, 33.f / 255.f };
    };

    /// A piece of a page: a run of letters in one font and colour, or a picture
    struct RichItem
    {
        bool mImage = false;
        /// From the top left of the page
        float mX = 0;
        float mY = 0;
        /// A picture's size
        float mWidth = 0;
        float mHeight = 0;
        int mFont = 1;
        std::array<float, 3> mColor{ 1.f, 1.f, 1.f };
        std::string mText;
        std::string mFile;
    };

    struct RichPage
    {
        std::vector<RichItem> mItems;
        /// The widest line, and the height of the lines together
        float mWidth = 0;
        float mHeight = 0;
    };

    /// The pages of a text with the marks the books and the notes of the games have (the way the executable lays it
    /// out, Mac round 12AM B4). The marks are IMG (SRC, WIDTH, HEIGHT), DIV (ALIGN=LEFT, CENTER or RIGHT), FONT (FACE,
    /// COLOR=RRGGBB) with its /FONT, BR (the text goes on a line below), P (two lines below) and HR (a new page when
    /// the layout makes pages, two lines below when it does not); other marks are left out. A text that does not begin
    /// with a mark is plain: it is broken where it has a new line. Whitespace between words is one space. There is
    /// always at least one page.
    std::vector<RichPage> layoutRichText(std::string_view text, const RichParams& params, const RichMeasure& measure);
}

#endif
