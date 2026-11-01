#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_FONT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_FONT_H

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FalloutMenu
{
    /// A font of the games: a `.fnt` file that says where each of 256 characters is in a texture and how it is placed,
    /// and the texture of the same name. Fonts are bitmaps cut tight around each character, so every character has its
    /// own width and the text is as wide as its characters are.
    class Font
    {
    public:
        struct Glyph
        {
            /// The corners of the character in the texture, 0 to 1
            float mU0 = 0;
            float mV0 = 0;
            float mU1 = 0;
            float mV1 = 0;
            /// The size of the character's picture in pixels
            float mWidth = 0;
            float mHeight = 0;
            /// The room left of the picture and after it that the pen moves over; after is negative for the glowing
            /// fonts, whose pictures have a halo that overlaps the next one
            float mBefore = 0;
            float mAfter = 0;
            /// How far the picture's top edge is above the baseline (the picture is drawn from baseline - ascent
            /// downwards)
            float mAscent = 0;
        };

        /// One character of a laid out text, from the top left of the text
        struct Quad
        {
            float mX = 0;
            float mY = 0;
            float mWidth = 0;
            float mHeight = 0;
            float mU0 = 0;
            float mV0 = 0;
            float mU1 = 0;
            float mV1 = 0;
        };

        struct Layout
        {
            std::vector<Quad> mQuads;
            float mWidth = 0;
            float mHeight = 0;
            /// How many lines the text took
            int mLines = 0;
        };

        /// The bytes of a `.fnt` file; nothing if they are not one
        static std::optional<Font> parse(std::string_view bytes);

        /// The name of the texture, as the file gives it, without a folder or an extension
        const std::string& textureName() const { return mTextureName; }
        /// The distance from one line to the next
        float lineHeight() const { return mSize; }
        float baseline() const { return mBaseline; }
        const Glyph& glyph(unsigned char c) const { return mGlyphs[c]; }
        /// How far the pen moves over for a character
        float advance(unsigned char c) const;

        /// The width a line of text takes
        float lineWidth(std::string_view text) const;

        /// Lay a text out: broken into lines at the new lines and, when `wrapWidth` is more than zero, at the spaces
        /// before a word that would not fit. `justify` is 1 for left, 2 for centre and 4 for right, and says where each
        /// line is in the width of the whole text.
        Layout layout(std::string_view text, float wrapWidth = 0, int justify = 1) const;

    private:
        float mSize = 0;
        float mBaseline = 0;
        std::string mTextureName;
        std::array<Glyph, 256> mGlyphs{};
    };

    /// The fonts a menu asks for by number (1 to 9)
    class FontSet
    {
    public:
        static constexpr int count = 9;

        void set(int number, Font font);
        /// The font with this number; the first one that is there if the number is not one
        const Font* get(int number) const;
        bool empty() const;

    private:
        std::array<std::optional<Font>, count> mFonts;
    };
}

#endif
