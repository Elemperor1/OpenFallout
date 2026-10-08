#ifndef OPENFALLOUT_MWRENDER_FALLOUTANIMATION_H
#define OPENFALLOUT_MWRENDER_FALLOUTANIMATION_H

#include <string>
#include <string_view>
#include <vector>

namespace OFRender
{
    /// The folder with the animation files (.kf) of a character of Fallout 3 or New Vegas: the folder of its skeleton,
    /// with a slash at the end, lower case. The path is one under the virtual file system (`meshes/...`). A path with
    /// no folder gives an empty one.
    std::string falloutAnimationFolder(std::string_view skeleton);

    /// The animation a character plays when it stands, chosen among the paths of the files in the folder of its
    /// skeleton (lower case, as the virtual file system lists them, files of folders below that one included): the file
    /// `mtidle.kf`, else `idle.kf`, else `h2hidle.kf`, else the first of the files in the folder itself (in the order
    /// of their names) that has `idle` in its name. Empty when there is none.
    std::string chooseFalloutIdle(std::string_view folder, const std::vector<std::string>& files);
}

#endif
