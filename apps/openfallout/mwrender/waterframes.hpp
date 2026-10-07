#ifndef OPENFALLOUT_MWRENDER_WATERFRAMES_H
#define OPENFALLOUT_MWRENDER_WATERFRAMES_H

#include <string_view>
#include <vector>

#include <components/vfs/pathutil.hpp>

namespace VFS
{
    class Manager;
}

namespace OFRender
{
    /// The files of the frames of an animated texture of the water, in the order of the animation: the name of the
    /// texture and the number of the frame in two digits (textures/water/water00.dds). Nothing when the game has no
    /// file for the first frame, which is what the games of Fallout have of the textures of the water of Morrowind: a
    /// missing frame would be drawn as the magenta warning image, and the water would have the colour of that, and a
    /// line in the log for each frame.
    std::vector<VFS::Path::Normalized> findWaterFrames(
        const VFS::Manager& vfs, std::string_view texture, int frameCount);
}

#endif
