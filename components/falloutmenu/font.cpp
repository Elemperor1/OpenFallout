#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace FalloutMenu
{
    namespace
    {
        constexpr std::size_t glyphBytes = 56;
        constexpr std::size_t glyphCount = 256;
        constexpr std::size_t headerBytes = 296;

        float readFloat(std::string_view bytes, std::size_t offset)
        {
            float value;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }

        std::uint32_t readInt(std::string_view bytes, std::size_t offset)
        {
            std::uint32_t value;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
    }

    std::optional<Font> Font::parse(std::string_view bytes)
    {
        if (bytes.size() < headerBytes + glyphCount * glyphBytes)
            return std::nullopt;
        Font font;
        font.mSize = readFloat(bytes, 0);
        if (!(font.mSize > 0 && font.mSize < 1000) || readInt(bytes, 4) != 1)
            return std::nullopt;
        {
            const char* name = bytes.data() + 12;
            std::size_t length = 0;
            while (length < 270 && name[length] != '\0')
                ++length;
            font.mTextureName.assign(name, length);
        }
        // The characters are the last part of the file
        const std::size_t start = bytes.size() - glyphCount * glyphBytes;
        for (std::size_t c = 0; c < glyphCount; ++c)
        {
            const std::size_t at = start + c * glyphBytes;
            Glyph& glyph = font.mGlyphs[c];
            // Four corners (upper left, upper right, lower left, lower right) after a field that is always zero
            glyph.mU0 = readFloat(bytes, at + 4);
            glyph.mV0 = readFloat(bytes, at + 8);
            glyph.mU1 = readFloat(bytes, at + 12);
            glyph.mV1 = readFloat(bytes, at + 24);
            glyph.mWidth = readFloat(bytes, at + 36);
            glyph.mHeight = readFloat(bytes, at + 40);
            glyph.mBefore = readFloat(bytes, at + 44);
            glyph.mAfter = readFloat(bytes, at + 48);
            glyph.mAscent = readFloat(bytes, at + 52);
        }
        // The baseline is as far below the top of a line as the tallest character reaches above it
        float tallest = 0;
        for (unsigned char c = 33; c < 127; ++c)
            tallest = std::max(tallest, font.mGlyphs[c].mAscent);
        font.mBaseline = tallest;
        return font;
    }

    float Font::advance(unsigned char c) const
    {
        const Glyph& g = mGlyphs[c];
        return std::max(0.f, g.mBefore + g.mWidth + g.mAfter);
    }

    float Font::lineWidth(std::string_view text) const
    {
        float width = 0;
        for (const char c : text)
            width += advance(static_cast<unsigned char>(c));
        return width;
    }

    Font::Layout Font::layout(std::string_view text, float wrapWidth, int justify, float blockWidth) const
    {
        // Break the text into the lines it is shown in
        std::vector<std::string_view> lines;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string_view paragraph = text.substr(start, end - start);
            if (!paragraph.empty() && paragraph.back() == '\r')
                paragraph.remove_suffix(1);
            if (wrapWidth > 0)
            {
                std::size_t lineStart = 0;
                std::size_t lastSpace = std::string_view::npos;
                float width = 0;
                for (std::size_t i = 0; i < paragraph.size(); ++i)
                {
                    const unsigned char c = static_cast<unsigned char>(paragraph[i]);
                    if (c == ' ')
                        lastSpace = i;
                    width += advance(c);
                    if (width > wrapWidth && i > lineStart)
                    {
                        std::size_t cut = lastSpace != std::string_view::npos && lastSpace > lineStart ? lastSpace : i;
                        lines.push_back(paragraph.substr(lineStart, cut - lineStart));
                        lineStart = lastSpace != std::string_view::npos && lastSpace > lineStart ? cut + 1 : cut;
                        lastSpace = std::string_view::npos;
                        width = 0;
                        for (std::size_t k = lineStart; k <= i; ++k)
                            width += advance(static_cast<unsigned char>(paragraph[k]));
                    }
                }
                lines.push_back(paragraph.substr(lineStart));
            }
            else
            {
                lines.push_back(paragraph);
            }
            if (end == text.size())
                break;
            start = end + 1;
        }

        Layout layout;
        layout.mLines = static_cast<int>(lines.size());
        // The block the lines are justified in, which is also the width the text reports, so that a tile that is put
        // by its justification point (a text's x) and the lines in it agree where the block starts
        blockWidth = std::max({ blockWidth, wrapWidth, 0.f });
        for (const std::string_view line : lines)
            blockWidth = std::max(blockWidth, lineWidth(line));
        layout.mWidth = blockWidth;
        layout.mHeight = static_cast<float>(lines.size()) * mSize;
        for (std::size_t row = 0; row < lines.size(); ++row)
        {
            const float width = lineWidth(lines[row]);
            float pen = 0;
            if (justify == 2)
                pen = (blockWidth - width) / 2;
            else if (justify == 4)
                pen = blockWidth - width;
            const float baseline = static_cast<float>(row) * mSize + mBaseline;
            for (const char ch : lines[row])
            {
                const unsigned char c = static_cast<unsigned char>(ch);
                const Glyph& g = mGlyphs[c];
                if (g.mWidth > 0 && g.mHeight > 0)
                    layout.mQuads.push_back(
                        { pen + g.mBefore, baseline - g.mAscent, g.mWidth, g.mHeight, g.mU0, g.mV0, g.mU1, g.mV1 });
                pen += advance(c);
            }
        }
        return layout;
    }

    void FontSet::set(int number, Font font)
    {
        if (number >= 1 && number <= count)
            mFonts[number - 1] = std::move(font);
    }

    const Font* FontSet::get(int number) const
    {
        if (number >= 1 && number <= count && mFonts[number - 1])
            return &*mFonts[number - 1];
        for (const std::optional<Font>& font : mFonts)
            if (font)
                return &*font;
        return nullptr;
    }

    bool FontSet::empty() const
    {
        return get(1) == nullptr;
    }
}
