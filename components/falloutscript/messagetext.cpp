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
        std::string result;
        std::size_t next = 0;
        for (std::size_t i = 0; i < text.size();)
        {
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
}
