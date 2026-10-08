#include "falloutanimation.hpp"

#include <algorithm>

#include <components/misc/strings/lower.hpp>

namespace OFRender
{
    namespace
    {
        constexpr std::string_view sExtension = ".kf";

        /// The name of a file in the folder without its extension, empty for a file of a folder below or not a .kf
        std::string_view stemInFolder(std::string_view folder, std::string_view path)
        {
            if (!path.starts_with(folder) || !path.ends_with(sExtension))
                return {};
            std::string_view name = path.substr(folder.size());
            name.remove_suffix(sExtension.size());
            if (name.empty() || name.find('/') != std::string_view::npos)
                return {};
            return name;
        }
    }

    std::string falloutAnimationFolder(std::string_view skeleton)
    {
        std::string folder = Misc::StringUtils::lowerCase(skeleton);
        std::ranges::replace(folder, '\\', '/');
        const std::size_t slash = folder.find_last_of('/');
        if (slash == std::string::npos)
            return {};
        folder.resize(slash + 1);
        return folder;
    }

    std::string chooseFalloutIdle(std::string_view folder, const std::vector<std::string>& files)
    {
        static constexpr std::string_view preferred[] = { "mtidle", "idle", "h2hidle" };
        for (std::string_view stem : preferred)
        {
            const std::string path = std::string(folder) + std::string(stem) + std::string(sExtension);
            if (std::ranges::find(files, path) != files.end())
                return path;
        }

        std::string best;
        std::string_view bestStem;
        for (const std::string& path : files)
        {
            const std::string_view stem = stemInFolder(folder, path);
            if (stem.empty() || stem.find("idle") == std::string_view::npos)
                continue;
            if (best.empty() || stem < bestStem)
            {
                best = path;
                bestStem = stemInFolder(folder, best);
            }
        }
        return best;
    }
}
