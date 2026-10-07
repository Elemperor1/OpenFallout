#ifndef OPENFALLOUT_COMPONENTS_ESMTERRAIN_TEXTUREPATH_H
#define OPENFALLOUT_COMPONENTS_ESMTERRAIN_TEXTUREPATH_H

#include <string_view>

#include <components/vfs/pathutil.hpp>

namespace VFS
{
    class Manager;
}

namespace ESMTerrain
{
    /// The file of a texture that a landscape texture or a texture set record names. Oblivion names it relative to the
    /// directory of the landscape textures (`landscape` is true), Fallout and Skyrim relative to the directory of the
    /// textures, and the records of some plugins have that directory in the path too. The first of these places that
    /// has the file is the result, and the place the oldest games would have it in when none does (the textures
    /// directory for a texture set, the landscape directory for the filename of an Oblivion record).
    VFS::Path::Normalized findTexturePath(const VFS::Manager* vfs, std::string_view path, bool landscape);
}

#endif
