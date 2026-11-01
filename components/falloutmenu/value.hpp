#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_VALUE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_VALUE_H

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace FalloutMenu
{
    /// What a trait holds: a number, or a text. A text that reads as a number is both, because the menu files do not
    /// say which they mean (`<string> 5 </string>` is a text, `<width> 5 </width>` a number).
    class Value
    {
    public:
        Value() = default;
        Value(float number)
            : mNumber(number)
        {
        }

        /// The value of a piece of text of a menu file: a number if all of it is one, else a text
        static Value fromText(std::string_view text)
        {
            Value value;
            value.mString = std::string(text);
            if (!text.empty())
            {
                const std::string copy(text);
                char* end = nullptr;
                const double parsed = std::strtod(copy.c_str(), &end);
                if (end != copy.c_str() && *end == '\0')
                    value.mNumber = static_cast<float>(parsed);
                else
                    value.mIsText = true;
            }
            return value;
        }

        /// A text of no characters, which is not the number 0: what a text trait is until something sets it
        static Value emptyText()
        {
            Value value;
            value.mIsText = true;
            return value;
        }

        float number() const { return mNumber; }
        bool truth() const { return mNumber != 0.f; }
        /// A text that is not a number
        bool isText() const { return mIsText; }
        bool isEmpty() const { return mString.empty() && (mIsText || mNumber == 0.f); }

        std::string text() const
        {
            if (mIsText || !mString.empty())
                return mString;
            if (mNumber == static_cast<float>(static_cast<long long>(mNumber)))
                return std::to_string(static_cast<long long>(mNumber));
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(mNumber));
            return buffer;
        }

        friend bool operator==(const Value& a, const Value& b)
        {
            return a.mNumber == b.mNumber && a.mIsText == b.mIsText && a.mString == b.mString;
        }

    private:
        float mNumber = 0.f;
        std::string mString;
        bool mIsText = false;
    };
}

#endif
