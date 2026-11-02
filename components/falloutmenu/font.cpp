#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace FalloutMenu
{
    namespace
    {
        constexpr std::size_t glyphBytes = 56;
        constexpr std::size_t glyphCount = 256;
        constexpr std::size_t headerBytes = 296;

        // The byte of Windows-1252 for a character of Unicode, or '?'
        unsigned char codePageByte(std::uint32_t code)
        {
            if (code < 0x100)
                return static_cast<unsigned char>(code);
            static constexpr std::pair<std::uint32_t, unsigned char> special[] = {
                { 0x20AC, 0x80 },
                { 0x201A, 0x82 },
                { 0x0192, 0x83 },
                { 0x201E, 0x84 },
                { 0x2026, 0x85 },
                { 0x2020, 0x86 },
                { 0x2021, 0x87 },
                { 0x02C6, 0x88 },
                { 0x2030, 0x89 },
                { 0x0160, 0x8A },
                { 0x2039, 0x8B },
                { 0x0152, 0x8C },
                { 0x017D, 0x8E },
                { 0x2018, 0x91 },
                { 0x2019, 0x92 },
                { 0x201C, 0x93 },
                { 0x201D, 0x94 },
                { 0x2022, 0x95 },
                { 0x2013, 0x96 },
                { 0x2014, 0x97 },
                { 0x02DC, 0x98 },
                { 0x2122, 0x99 },
                { 0x0161, 0x9A },
                { 0x203A, 0x9B },
                { 0x0153, 0x9C },
                { 0x017E, 0x9E },
                { 0x0178, 0x9F },
            };
            for (const auto& [unicode, byte] : special)
                if (unicode == code)
                    return byte;
            return '?';
        }

        // The text as the bytes the font is indexed by: the UTF-8 characters of the text become their bytes of the code
        // page, and a byte that does not start a UTF-8 character is kept
        std::string fontBytes(std::string_view text)
        {
            std::string out;
            out.reserve(text.size());
            for (std::size_t i = 0; i < text.size();)
            {
                const unsigned char lead = static_cast<unsigned char>(text[i]);
                std::size_t length = 1;
                std::uint32_t code = lead;
                if (lead >= 0xC2 && lead <= 0xDF)
                {
                    length = 2;
                    code = lead & 0x1Fu;
                }
                else if (lead >= 0xE0 && lead <= 0xEF)
                {
                    length = 3;
                    code = lead & 0x0Fu;
                }
                else if (lead >= 0xF0 && lead <= 0xF4)
                {
                    length = 4;
                    code = lead & 0x07u;
                }
                bool valid = length > 1 && i + length <= text.size();
                for (std::size_t k = 1; valid && k < length; ++k)
                {
                    const unsigned char next = static_cast<unsigned char>(text[i + k]);
                    valid = (next & 0xC0) == 0x80;
                    code = (code << 6) | (next & 0x3Fu);
                }
                if (valid)
                {
                    out.push_back(static_cast<char>(codePageByte(code)));
                    i += length;
                }
                else
                {
                    out.push_back(text[i]);
                    ++i;
                }
            }
            return out;
        }

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
            // A file with a number that is not a number would put that into every line that uses the character
            for (const float number : { glyph.mU0, glyph.mV0, glyph.mU1, glyph.mV1, glyph.mWidth, glyph.mHeight,
                     glyph.mBefore, glyph.mAfter, glyph.mAscent })
            {
                if (!std::isfinite(number))
                    return std::nullopt;
            }
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

    float Font::bytesWidth(std::string_view bytes) const
    {
        float width = 0;
        for (const char c : bytes)
            width += advance(static_cast<unsigned char>(c));
        return width;
    }

    float Font::lineWidth(std::string_view text) const
    {
        return bytesWidth(fontBytes(text));
    }

    Font::Layout Font::layout(std::string_view utf8, float wrapWidth, int justify, float blockWidth) const
    {
        const std::string converted = fontBytes(utf8);
        const std::string_view text = converted;
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
                    // A line breaks at a space only: a word that is wider than the room stays whole on a line of its
                    // own (the three letters of a short button label are wider than the room its fixed width leaves)
                    if (width > wrapWidth && lastSpace != std::string_view::npos && lastSpace > lineStart)
                    {
                        lines.push_back(paragraph.substr(lineStart, lastSpace - lineStart));
                        lineStart = lastSpace + 1;
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
        {
            const float lineWidth = bytesWidth(line);
            layout.mLongest = std::max(layout.mLongest, lineWidth);
            blockWidth = std::max(blockWidth, lineWidth);
        }
        layout.mWidth = blockWidth;
        layout.mHeight = static_cast<float>(lines.size()) * mSize;
        for (std::size_t row = 0; row < lines.size(); ++row)
        {
            const float width = bytesWidth(lines[row]);
            float pen = 0;
            if (justify == 2)
                pen = (blockWidth - width) / 2;
            else if (justify == 4)
                pen = blockWidth - width;
            const float baseline = static_cast<float>(row) * mSize + mBaseline - textRise;
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
