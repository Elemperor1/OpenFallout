#ifndef GAME_RENDER_FALLOUTFACE_H
#define GAME_RENDER_FALLOUTFACE_H

#include <components/esm4/facegen.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include <osg/Image>
#include <osg/Node>

namespace VFS
{
    class Manager;
}

namespace OFRender
{
    /// Moves the vertices of the meshes of a part of a character by the morphs of a FaceGen file and the coefficients
    /// of the character. The meshes of the part are shared with every other character that wears it, so each mesh that
    /// moves is replaced by a copy in this part only. A file for a single mesh moves the mesh that has as many vertices
    /// as the file; a file for several meshes moves them one after the other, in the order of the part. Returns how
    /// many meshes moved, which is none when the vertices of the part are not those of the file.
    std::size_t morphFaceMeshes(osg::Node& part, const ESM4::FaceMorphs& morphs, const std::vector<float>& symmetric,
        const std::vector<float>& asymmetric);

    /// Gives the part another image for the diffuse map, on the nodes of this part only. Returns how many nodes it
    /// changed, which is none when the part has no diffuse map.
    std::size_t replaceDiffuseMap(osg::Node& part, osg::Image* image);

    /// The morphs of the .egm file that goes with a model (a path as the race and the hair records name it). The file
    /// is read once. Null when there is none, or when it is not valid, which is said once in the log.
    std::shared_ptr<const ESM4::FaceMorphs> getFaceMorphs(const VFS::Manager& vfs, std::string_view model);

    /// The textures that the archives hold for single characters, found when first asked for
    const ESM4::FaceTextureIndex& getFaceTextures(const VFS::Manager& vfs);
}

#endif
