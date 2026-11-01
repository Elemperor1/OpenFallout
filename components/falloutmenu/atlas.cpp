#include "atlas.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace FalloutMenu
{
    namespace
    {
        std::string baseName(std::string_view name)
        {
            const std::size_t slash = name.find_last_of("\\/");
            if (slash != std::string_view::npos)
                name.remove_prefix(slash + 1);
            std::string result(name);
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return result;
        }

        std::string_view trimmed(std::string_view text)
        {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
                text.remove_prefix(1);
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
                text.remove_suffix(1);
            return text;
        }
    }

    Atlas Atlas::parse(std::string_view text)
    {
        Atlas atlas;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string_view line = trimmed(text.substr(start, end - start));
            start = end + 1;
            if (line.empty() || line.front() == '#')
                continue;
            // `<filename> <atlas filename>, <idx>, <type>, <woffset>, <hoffset>, <depth>, <width>, <height>`
            const std::size_t split = line.find_first_of(" \t");
            if (split == std::string_view::npos)
                continue;
            const std::string name = baseName(line.substr(0, split));
            std::vector<std::string> fields;
            std::string_view rest = trimmed(line.substr(split));
            std::size_t from = 0;
            while (from <= rest.size())
            {
                std::size_t comma = rest.find(',', from);
                if (comma == std::string_view::npos)
                    comma = rest.size();
                fields.emplace_back(trimmed(rest.substr(from, comma - from)));
                from = comma + 1;
            }
            if (fields.size() < 8)
                continue;
            Entry entry;
            entry.mTexture = fields[0];
            entry.mU = static_cast<float>(std::atof(fields[3].c_str()));
            entry.mV = static_cast<float>(std::atof(fields[4].c_str()));
            entry.mWidth = static_cast<float>(std::atof(fields[6].c_str()));
            entry.mHeight = static_cast<float>(std::atof(fields[7].c_str()));
            atlas.mEntries[name] = std::move(entry);
        }
        return atlas;
    }

    const Atlas::Entry* Atlas::find(std::string_view name) const
    {
        const auto found = mEntries.find(baseName(name));
        return found == mEntries.end() ? nullptr : &found->second;
    }
}
