#include "waterframes.hpp"

#include <iomanip>
#include <sstream>

#include <components/vfs/manager.hpp>

namespace OFRender
{
    std::vector<VFS::Path::Normalized> findWaterFrames(
        const VFS::Manager& vfs, std::string_view texture, int frameCount)
    {
        std::vector<VFS::Path::Normalized> frames;
        for (int i = 0; i < frameCount; ++i)
        {
            std::ostringstream name;
            name << "textures/water/" << texture << std::setw(2) << std::setfill('0') << i << ".dds";
            frames.emplace_back(name.str());
        }
        if (!frames.empty() && !vfs.exists(frames.front()))
            frames.clear();
        return frames;
    }
}
