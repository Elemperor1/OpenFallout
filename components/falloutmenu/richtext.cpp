#include "richtext.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace FalloutMenu
{
    namespace
    {
        struct Style
        {
            int mFont = 1;
            std::array<float, 3> mColor{};
        };

        struct Fragment
        {
            bool mImage = false;
            std::string mText;
            std::string mFile;
            Style mStyle;
            float mWidth = 0;
            float mHeight = 0;
        };

        enum class Align
        {
            Left,
            Center,
            Right
        };

        struct Line
        {
            std::vector<Fragment> mFragments;
            float mWidth = 0;
            float mHeight = 0;
            Align mAlign = Align::Left;
            bool mBreakBefore = false;
        };

        bool isSpace(char c)
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
        }

        std::string lowerAscii(std::string_view text)
        {
            std::string result(text);
            for (char& c : result)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        }

        struct Attribute
        {
            std::string mName;
            std::string mValue;
        };

        struct Tag
        {
            std::string mName;
            bool mClosing = false;
            std::vector<Attribute> mAttributes;

            const std::string* find(std::string_view name) const
            {
                for (const Attribute& attribute : mAttributes)
                    if (attribute.mName == name)
                        return &attribute.mValue;
                return nullptr;
            }
        };

        Tag parseTag(std::string_view body)
        {
            Tag tag;
            size_t i = 0;
            const auto skipSpaces = [&] {
                while (i < body.size() && isSpace(body[i]))
                    ++i;
            };
            skipSpaces();
            if (i < body.size() && body[i] == '/')
            {
                tag.mClosing = true;
                ++i;
                skipSpaces();
            }
            const size_t nameStart = i;
            while (i < body.size() && std::isalnum(static_cast<unsigned char>(body[i])))
                ++i;
            tag.mName = lowerAscii(body.substr(nameStart, i - nameStart));
            while (i < body.size())
            {
                skipSpaces();
                if (i >= body.size())
                    break;
                const size_t keyStart = i;
                while (i < body.size() && body[i] != '=' && !isSpace(body[i]) && body[i] != '/')
                    ++i;
                Attribute attribute;
                attribute.mName = lowerAscii(body.substr(keyStart, i - keyStart));
                if (i == keyStart)
                {
                    ++i;
                    continue;
                }
                skipSpaces();
                if (i < body.size() && body[i] == '=')
                {
                    ++i;
                    skipSpaces();
                    if (i < body.size() && (body[i] == '"' || body[i] == '\''))
                    {
                        const char quote = body[i++];
                        const size_t valueStart = i;
                        while (i < body.size() && body[i] != quote)
                            ++i;
                        attribute.mValue = std::string(body.substr(valueStart, i - valueStart));
                        if (i < body.size())
                            ++i;
                    }
                    else
                    {
                        const size_t valueStart = i;
                        while (i < body.size() && !isSpace(body[i]))
                            ++i;
                        attribute.mValue = std::string(body.substr(valueStart, i - valueStart));
                    }
                }
                tag.mAttributes.push_back(std::move(attribute));
            }
            return tag;
        }

        /// Red, green and blue of `RRGGBB` (a `#` before is allowed)
        std::optional<std::array<float, 3>> parseColor(std::string_view value)
        {
            if (!value.empty() && value.front() == '#')
                value.remove_prefix(1);
            if (value.size() != 6)
                return std::nullopt;
            std::array<float, 3> result{};
            for (int channel = 0; channel < 3; ++channel)
            {
                int number = 0;
                for (int digit = 0; digit < 2; ++digit)
                {
                    const char c = value[static_cast<size_t>(channel * 2 + digit)];
                    int part = 0;
                    if (c >= '0' && c <= '9')
                        part = c - '0';
                    else if (c >= 'a' && c <= 'f')
                        part = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F')
                        part = c - 'A' + 10;
                    else
                        return std::nullopt;
                    number = number * 16 + part;
                }
                result[static_cast<size_t>(channel)] = static_cast<float>(number) / 255.f;
            }
            return result;
        }

        bool sameStyle(const Style& a, const Style& b)
        {
            return a.mFont == b.mFont && a.mColor == b.mColor;
        }

        class Layouter
        {
        public:
            Layouter(const RichParams& params, const RichMeasure& measure)
                : mParams(params)
                , mMeasure(measure)
            {
                mStyles.push_back(Style{ std::clamp(params.mFont, 1, 9), params.mColor });
            }

            void run(std::string_view text)
            {
                if (text.empty() || text.front() != '<')
                    runPlain(text);
                else
                    runMarked(text);
                finishLine(false);
                // The blank lines a text ends with make no page of their own
                while (!mLines.empty() && mLines.back().mFragments.empty())
                    mLines.pop_back();
            }

            std::vector<RichPage> pages() const;

        private:
            const Style& style() const { return mStyles.back(); }

            float width(const Style& at, std::string_view text) const
            {
                return mMeasure.mWidth ? mMeasure.mWidth(at.mFont, text) : 0.f;
            }

            float lineHeight(const Style& at) const
            {
                return mMeasure.mLineHeight ? mMeasure.mLineHeight(at.mFont) : 0.f;
            }

            bool wraps() const { return mParams.mWrapWidth >= 2.f; }

            void runPlain(std::string_view text)
            {
                std::string word;
                for (const char c : text)
                {
                    if (c == '\r')
                        continue;
                    if (c == '\n')
                    {
                        flushWord(word);
                        newLines(1);
                    }
                    else if (isSpace(c))
                    {
                        flushWord(word);
                        mPendingSpace = true;
                    }
                    else
                        word.push_back(c);
                }
                flushWord(word);
            }

            void runMarked(std::string_view text)
            {
                std::string word;
                size_t i = 0;
                while (i < text.size())
                {
                    const char c = text[i];
                    if (c == '<')
                    {
                        const size_t close = text.find('>', i);
                        if (close == std::string_view::npos)
                        {
                            ++i;
                            continue;
                        }
                        flushWord(word);
                        handleTag(parseTag(text.substr(i + 1, close - i - 1)));
                        i = close + 1;
                        continue;
                    }
                    if (isSpace(c))
                    {
                        flushWord(word);
                        mPendingSpace = true;
                    }
                    else
                        word.push_back(c);
                    ++i;
                }
                flushWord(word);
            }

            void handleTag(const Tag& tag)
            {
                if (tag.mName == "br")
                    newLines(1);
                else if (tag.mName == "p")
                    newLines(2);
                else if (tag.mName == "hr")
                {
                    if (mParams.mWrapLimit >= 2.f)
                    {
                        finishLine(false);
                        mBreakPending = true;
                    }
                    else
                        newLines(2);
                }
                else if (tag.mName == "div")
                {
                    if (tag.mClosing)
                    {
                        finishLine(false);
                        mAlign = Align::Left;
                    }
                    else if (const std::string* value = tag.find("align"))
                    {
                        finishLine(false);
                        const std::string align = lowerAscii(*value);
                        mAlign = align == "center" ? Align::Center : align == "right" ? Align::Right : Align::Left;
                    }
                }
                else if (tag.mName == "font")
                {
                    if (tag.mClosing)
                    {
                        if (mStyles.size() > 1)
                            mStyles.pop_back();
                        return;
                    }
                    Style next = style();
                    if (const std::string* face = tag.find("face"))
                    {
                        // A number is a font of the game; the names of fonts are not known
                        char* end = nullptr;
                        const long number = std::strtol(face->c_str(), &end, 10);
                        if (end != face->c_str() && *end == '\0' && number >= 1 && number <= 9)
                            next.mFont = static_cast<int>(number);
                    }
                    if (const std::string* color = tag.find("color"))
                        if (const auto parsed = parseColor(*color))
                            next.mColor = *parsed;
                    mStyles.push_back(next);
                }
                else if (tag.mName == "img" && !tag.mClosing)
                {
                    const std::string* source = tag.find("src");
                    if (source == nullptr || source->empty())
                        return;
                    float w = 0;
                    float h = 0;
                    if (const std::string* value = tag.find("width"))
                        w = static_cast<float>(std::atof(value->c_str()));
                    if (const std::string* value = tag.find("height"))
                        h = static_cast<float>(std::atof(value->c_str()));
                    if ((w <= 0 || h <= 0) && mMeasure.mImageSize)
                    {
                        if (const auto size = mMeasure.mImageSize(*source))
                        {
                            if (w <= 0)
                                w = (*size)[0];
                            if (h <= 0)
                                h = (*size)[1];
                        }
                    }
                    if (w <= 0 || h <= 0)
                        return;
                    Fragment fragment;
                    fragment.mImage = true;
                    fragment.mFile = *source;
                    fragment.mStyle = style();
                    fragment.mWidth = w;
                    fragment.mHeight = h;
                    place(std::move(fragment));
                }
            }

            void flushWord(std::string& word)
            {
                if (word.empty())
                    return;
                const Style at = style();
                const float whole = width(at, word);
                if (wraps() && whole > mParams.mWrapWidth)
                {
                    // A word wider than a line goes to a line of its own and is broken where it fills it
                    if (!mLine.mFragments.empty())
                        finishLine(false);
                    size_t start = 0;
                    while (start < word.size())
                    {
                        size_t end = start;
                        size_t last = start;
                        while (end < word.size())
                        {
                            size_t next = end + 1;
                            while (next < word.size() && (static_cast<unsigned char>(word[next]) & 0xC0) == 0x80)
                                ++next;
                            if (width(at, std::string_view(word).substr(start, next - start)) > mParams.mWrapWidth
                                && last > start)
                                break;
                            last = next;
                            end = next;
                        }
                        Fragment fragment;
                        fragment.mText = word.substr(start, last - start);
                        fragment.mStyle = at;
                        fragment.mWidth = width(at, fragment.mText);
                        fragment.mHeight = lineHeight(at);
                        place(std::move(fragment));
                        start = last;
                        if (start < word.size())
                            finishLine(false);
                    }
                    word.clear();
                    return;
                }
                Fragment fragment;
                fragment.mText = word;
                fragment.mStyle = at;
                fragment.mWidth = whole;
                fragment.mHeight = lineHeight(at);
                word.clear();
                place(std::move(fragment));
            }

            /// Put a word or a picture on the line, on the next one when it does not fit
            void place(Fragment fragment)
            {
                const bool spaced = mPendingSpace && !mLine.mFragments.empty();
                mPendingSpace = false;
                const float space = spaced ? width(fragment.mStyle, " ") : 0.f;
                if (wraps() && !mLine.mFragments.empty() && mLine.mWidth + space + fragment.mWidth > mParams.mWrapWidth)
                {
                    // (the space that stood before the word is not carried over)
                    finishLine(false);
                }
                else if (spaced && !fragment.mImage)
                {
                    fragment.mText.insert(fragment.mText.begin(), ' ');
                    fragment.mWidth += space;
                }
                else if (spaced)
                {
                    // A picture after a word: the space is a run of its own
                    Fragment gap;
                    gap.mText = " ";
                    gap.mStyle = fragment.mStyle;
                    gap.mWidth = space;
                    gap.mHeight = lineHeight(fragment.mStyle);
                    add(std::move(gap));
                }
                add(std::move(fragment));
            }

            void add(Fragment fragment)
            {
                if (mLine.mFragments.empty())
                    mLine.mAlign = mAlign;
                mLine.mHeight = std::max(mLine.mHeight, fragment.mHeight);
                mLine.mWidth += fragment.mWidth;
                if (!fragment.mImage && !mLine.mFragments.empty())
                {
                    Fragment& last = mLine.mFragments.back();
                    if (!last.mImage && sameStyle(last.mStyle, fragment.mStyle))
                    {
                        last.mText += fragment.mText;
                        last.mWidth += fragment.mWidth;
                        return;
                    }
                }
                mLine.mFragments.push_back(std::move(fragment));
            }

            /// End the line; an empty one is a blank line when `blank` says so, and nothing otherwise
            void finishLine(bool blank)
            {
                mPendingSpace = false;
                if (mLine.mFragments.empty())
                {
                    if (!blank)
                        return;
                    mLine.mAlign = mAlign;
                    mLine.mHeight = lineHeight(style());
                }
                mLine.mBreakBefore = mBreakPending;
                mBreakPending = false;
                mLines.push_back(std::move(mLine));
                mLine = Line();
            }

            void newLines(int count)
            {
                for (int i = 0; i < count; ++i)
                    finishLine(true);
            }

            const RichParams& mParams;
            const RichMeasure& mMeasure;
            std::vector<Style> mStyles;
            Align mAlign = Align::Left;
            Line mLine;
            std::vector<Line> mLines;
            bool mPendingSpace = false;
            bool mBreakPending = false;
        };

        std::vector<RichPage> Layouter::pages() const
        {
            std::vector<RichPage> result;
            RichPage page;
            bool pageEmpty = true;
            float used = 0;
            const bool paged = mParams.mWrapLimit >= 2.f;
            const float room = mParams.mWrapWidth;
            for (const Line& line : mLines)
            {
                const float pitch = line.mHeight + mParams.mLineGap;
                if (!pageEmpty && (line.mBreakBefore || (paged && used + pitch > mParams.mWrapLimit)))
                {
                    page.mHeight = used;
                    result.push_back(std::move(page));
                    page = RichPage();
                    pageEmpty = true;
                    used = 0;
                }
                float x = 0;
                if (room >= 2.f && line.mAlign == Align::Center)
                    x = std::max(0.f, (room - line.mWidth) / 2.f);
                else if (room >= 2.f && line.mAlign == Align::Right)
                    x = std::max(0.f, room - line.mWidth);
                for (const Fragment& fragment : line.mFragments)
                {
                    RichItem item;
                    item.mImage = fragment.mImage;
                    item.mX = x;
                    // (a picture or a letter smaller than the line stands on the bottom of it)
                    item.mY = used + line.mHeight - fragment.mHeight;
                    item.mWidth = fragment.mWidth;
                    item.mHeight = fragment.mHeight;
                    item.mFont = fragment.mStyle.mFont;
                    item.mColor = fragment.mStyle.mColor;
                    item.mText = fragment.mText;
                    item.mFile = fragment.mFile;
                    x += fragment.mWidth;
                    page.mItems.push_back(std::move(item));
                }
                page.mWidth = std::max(page.mWidth, line.mWidth);
                used += pitch;
                pageEmpty = false;
            }
            page.mHeight = used;
            result.push_back(std::move(page));
            return result;
        }
    }

    std::vector<RichPage> layoutRichText(std::string_view text, const RichParams& params, const RichMeasure& measure)
    {
        Layouter layouter(params, measure);
        layouter.run(text);
        return layouter.pages();
    }
}
