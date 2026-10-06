#include "worldspacestart.hpp"

#include <charconv>
#include <stdexcept>
#include <system_error>

namespace OFWorld
{
    namespace
    {
        // Reads a whole integer from the text, returns false when it is not one.
        bool readInt(std::string_view text, int& value)
        {
            if (text.empty())
                return false;
            const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec == std::errc::result_out_of_range)
                throw std::runtime_error("Cell coordinates out of range.");
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }
    }

    std::optional<WorldspaceStart> parseWorldspaceStart(std::string_view text)
    {
        const std::size_t colon = text.find(':');
        if (colon == std::string_view::npos || colon == 0)
            return std::nullopt;
        const std::string_view coordinates = text.substr(colon + 1);
        const std::size_t comma = coordinates.find(',');
        if (comma == std::string_view::npos)
            return std::nullopt;

        WorldspaceStart result;
        result.mWorldspace = text.substr(0, colon);
        if (!readInt(coordinates.substr(0, comma), result.mX) || !readInt(coordinates.substr(comma + 1), result.mY))
            return std::nullopt;
        return result;
    }
}
