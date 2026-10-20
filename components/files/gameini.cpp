#include "gameini.hpp"

#include <components/files/conversion.hpp>
#include <components/files/openfile.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

namespace Files
{
    namespace
    {
        std::string_view trim(std::string_view text)
        {
            const char* spaces = " \t\r\n";
            const std::size_t first = text.find_first_not_of(spaces);
            if (first == std::string_view::npos)
                return {};
            return text.substr(first, text.find_last_not_of(spaces) - first + 1);
        }

        std::string makeKey(std::string_view section, std::string_view key)
        {
            return Misc::StringUtils::lowerCase(section) + '\n' + Misc::StringUtils::lowerCase(key);
        }
    }

    GameIni GameIni::parse(std::string_view text)
    {
        // A byte order mark of UTF-8 starts some files
        if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0)
            text.remove_prefix(3);

        GameIni result;
        std::string section;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            const std::string_view line = trim(text.substr(start, end - start));
            start = end + 1;

            if (line.empty() || line.front() == ';')
                continue;
            if (line.front() == '[')
            {
                const std::size_t close = line.find(']');
                if (close != std::string_view::npos)
                    section = std::string(trim(line.substr(1, close - 1)));
                continue;
            }
            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos)
                continue;
            const std::string_view key = trim(line.substr(0, equals));
            if (key.empty())
                continue;
            // A key that appears twice in a section counts once, with its first value: that is what the Windows call the
            // games read their ini files with (GetPrivateProfileString) gives, and the ini of New Vegas depends on it
            // (it lists sTestFile1 as FalloutNV.esm near the top and empty again further down)
            result.mValues.try_emplace(makeKey(section, key), std::string(trim(line.substr(equals + 1))));
        }
        return result;
    }

    std::optional<std::string> GameIni::get(std::string_view section, std::string_view key) const
    {
        const auto found = mValues.find(makeKey(section, key));
        if (found == mValues.end())
            return std::nullopt;
        return found->second;
    }

    void GameIni::merge(const GameIni& other)
    {
        for (const auto& [key, value] : other.mValues)
            mValues[key] = value;
    }

    std::vector<std::string> GameIni::splitList(std::string_view value)
    {
        std::vector<std::string> result;
        std::size_t start = 0;
        while (start <= value.size())
        {
            std::size_t end = value.find(',', start);
            if (end == std::string_view::npos)
                end = value.size();
            const std::string_view entry = trim(value.substr(start, end - start));
            if (!entry.empty())
                result.emplace_back(entry);
            start = end + 1;
        }
        return result;
    }

    GameIni GameIni::load(const std::vector<std::filesystem::path>& paths)
    {
        GameIni result;
        for (const std::filesystem::path& path : paths)
        {
            try
            {
                const auto stream = Files::openBinaryInputFileStream(path);
                const std::string text((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
                result.merge(parse(text));
            }
            catch (const std::exception&)
            {
                // A file that is not there or cannot be read has no settings
            }
        }
        return result;
    }

    std::vector<std::filesystem::path> findGameInis(const std::vector<std::filesystem::path>& dataDirs)
    {
        std::vector<std::filesystem::path> result;
        std::set<std::filesystem::path> seen;
        for (const std::filesystem::path& dataDir : dataDirs)
        {
            const std::filesystem::path folder = std::filesystem::absolute(dataDir).lexically_normal().parent_path();
            if (!seen.insert(folder).second)
                continue;
            std::error_code error;
            std::vector<std::filesystem::path> entries;
            for (const auto& entry : std::filesystem::directory_iterator(folder, error))
                entries.push_back(entry.path());
            for (const char* name : { "Fallout_default.ini", "Fallout.ini", "FalloutPrefs.ini" })
            {
                const auto found = std::find_if(entries.begin(), entries.end(), [&](const std::filesystem::path& path) {
                    return Misc::StringUtils::ciEqual(Files::pathToUnicodeString(path.filename()), name);
                });
                if (found != entries.end())
                    result.push_back(*found);
            }
        }
        return result;
    }
}
