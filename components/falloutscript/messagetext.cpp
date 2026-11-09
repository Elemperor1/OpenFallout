#include "messagetext.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace FalloutScript
{
    namespace
    {
        // Wider than any message needs, and short enough that a token in the data can not make a huge string
        constexpr std::size_t maxField = 99;

        bool isFlag(char c)
        {
            return c == '-' || c == '+' || c == ' ' || c == '#' || c == '0';
        }

        bool isDigit(char c)
        {
            return c >= '0' && c <= '9';
        }

        bool isNameStart(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        }

        bool isNameChar(char c)
        {
            return isNameStart(c) || isDigit(c);
        }

        std::size_t number(std::string_view text, std::size_t& position)
        {
            std::size_t value = 0;
            while (position < text.size() && isDigit(text[position]))
                value = std::min(maxField, value * 10 + static_cast<std::size_t>(text[position++] - '0'));
            return value;
        }
    }

    std::string formatMessage(std::string_view text, const std::vector<double>& values)
    {
        return formatMessage(text, values, TokenResolver());
    }

    std::string formatMessage(std::string_view text, const std::vector<double>& values, const TokenResolver& resolve)
    {
        std::string result;
        std::size_t next = 0;
        for (std::size_t i = 0; i < text.size();)
        {
            if (text[i] == '&' && resolve && i + 1 < text.size())
            {
                // `&-name;` is the form of the games that asks for the bare text (the resolver is given the dash)
                const std::size_t start = text[i + 1] == '-' ? i + 2 : i + 1;
                if (start < text.size() && isNameStart(text[start]))
                {
                    std::size_t end = start + 1;
                    while (end < text.size() && isNameChar(text[end]))
                        ++end;
                    if (end < text.size() && text[end] == ';')
                        if (const std::optional<std::string> value = resolve(text.substr(i + 1, end - i - 1)))
                        {
                            result += *value;
                            i = end + 1;
                            continue;
                        }
                }
            }
            if (text[i] != '%')
            {
                result += text[i++];
                continue;
            }
            if (i + 1 < text.size() && text[i + 1] == '%')
            {
                result += '%';
                i += 2;
                continue;
            }

            // % flags width .precision kind
            std::size_t position = i + 1;
            std::string spec = "%";
            while (position < text.size() && isFlag(text[position]))
                spec += text[position++];
            if (position < text.size() && isDigit(text[position]))
                spec += std::to_string(number(text, position));
            if (position < text.size() && text[position] == '.')
            {
                ++position;
                spec += "." + std::to_string(number(text, position));
            }
            const char kind = position < text.size() ? text[position] : '\0';
            const bool floating
                = kind == 'f' || kind == 'F' || kind == 'g' || kind == 'G' || kind == 'e' || kind == 'E';
            const bool integer = kind == 'd' || kind == 'i' || kind == 'u';
            if (!floating && !integer)
            {
                result += text[i++];
                continue;
            }

            // The games give every token a number, and a call that gave fewer has zeros for the rest
            const double value = next < values.size() ? values[next++] : 0.0;
            char buffer[256];
            int written = 0;
            if (floating)
            {
                spec += kind;
                written = std::snprintf(buffer, sizeof(buffer), spec.c_str(), value);
            }
            else
            {
                spec += "lld";
                // Values the game computes are far from the limits of a long, but a script can ask for anything
                const double whole = std::isnan(value) ? 0.0 : std::clamp(std::trunc(value), -9.0e15, 9.0e15);
                written = std::snprintf(buffer, sizeof(buffer), spec.c_str(), static_cast<long long>(whole));
            }
            if (written > 0)
                result.append(buffer, std::min<std::size_t>(static_cast<std::size_t>(written), sizeof(buffer) - 1));
            i = position + 1;
        }
        return result;
    }

    std::string_view mouseButtonSetting(unsigned button)
    {
        // The buttons of SDL: 1 is the left one, 2 the middle one, 3 the right one, then the extra buttons; the games'
        // table has the left, the right and the middle button first and "Button 3" to "Button 8" after them
        switch (button)
        {
            case 1:
                return "sMouseLeftButton";
            case 2:
                return "sMouseMiddleButton";
            case 3:
                return "sMouseRightButton";
            case 4:
                return "sMouseButton3";
            case 5:
                return "sMouseButton4";
            case 6:
                return "sMouseButton5";
            case 7:
                return "sMouseButton6";
            case 8:
                return "sMouseButton7";
            case 9:
                return "sMouseButton8";
            default:
                return {};
        }
    }

    std::string controlText(std::string_view prefix, std::string_view key, std::string_view none, bool bare)
    {
        // `&-name;` is the bare text of the key, empty for a control that has none
        if (bare)
            return std::string(key);
        std::string text(key.empty() ? none : key);
        if (prefix.empty())
            return text;
        return std::string(prefix) + " " + text;
    }
}
