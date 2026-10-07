#include "texturepath.hpp"

#include <string>
#include <vector>

#include <components/vfs/manager.hpp>

namespace ESMTerrain
{
    VFS::Path::Normalized findTexturePath(const VFS::Manager* vfs, std::string_view path, bool landscape)
    {
        constexpr std::string_view textures = "textures/";
        constexpr std::string_view landscapeTextures = "textures/landscape/";

        const VFS::Path::Normalized file(path);
        std::string_view relative = file.view();
        while (relative.starts_with('/'))
            relative.remove_prefix(1);

        std::vector<VFS::Path::Normalized> candidates;
        if (relative.starts_with(textures))
            candidates.emplace_back(std::string(relative));
        if (landscape)
            candidates.emplace_back(std::string(landscapeTextures) + std::string(relative));
        candidates.emplace_back(std::string(textures) + std::string(relative));

        if (vfs != nullptr)
        {
            for (VFS::Path::Normalized& candidate : candidates)
                if (vfs->exists(candidate))
                    return std::move(candidate);
        }
        return std::move(candidates.front());
    }
}
