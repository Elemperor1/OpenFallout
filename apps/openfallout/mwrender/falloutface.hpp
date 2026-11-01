#ifndef GAME_RENDER_FALLOUTFACE_H
#define GAME_RENDER_FALLOUTFACE_H

#include <components/esm4/facegen.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <osg/Image>
#include <osg/Node>
#include <osg/Quat>
#include <osg/Vec3f>

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
    /// many meshes moved, which is none when the vertices of the part are not those of the file. With
    /// `firstOfLongerFile` a part of a single mesh that has fewer vertices than the file moves by the first vertices of
    /// the file: the head files of the games have 1449 (men) or 2289 (women) vertices for a head of 1211, and the
    /// head is the first 1211 (the morphs move neighbouring vertices, and the mirror images of a vertex, alike at
    /// that place of the file and at no other, in the real files).
    std::size_t morphFaceMeshes(osg::Node& part, const ESM4::FaceMorphs& morphs, const std::vector<float>& symmetric,
        const std::vector<float>& asymmetric, bool firstOfLongerFile = false);

    /// Gives the part another image for the diffuse map, on the nodes of this part only. Returns how many nodes it
    /// changed, which is none when the part has no diffuse map.
    std::size_t replaceDiffuseMap(osg::Node& part, osg::Image* image);

    /// What picture a diffuse map is to have instead: given the one it has (null when its texture holds none), the
    /// new one, or null to leave the map as it is.
    using DiffuseChooser = std::function<osg::ref_ptr<osg::Image>(const osg::Image* original)>;

    /// Gives each node of the part that has a diffuse map the picture that `choose` gives for it, on the nodes of this
    /// part only. Returns how many nodes it changed.
    std::size_t replaceDiffuseMap(osg::Node& part, const DiffuseChooser& choose);

    /// The skin with the texture that the editor wrote for a character (its facemods or bodymods file, the base mod)
    /// on it, as the skin shaders of the games do it: twice what the texture has above a half grey is added to each
    /// colour of the skin, and the sum is multiplied by the default of the detail mod, nearly one. The texture is
    /// stretched over the skin, so a texture of 8 by 8 pixels is a few colours spread over the body. The result is a
    /// new picture, shared by whoever asks for the same two files while one has it. Null when either is in a format
    /// that cannot be read (it is not the one of the other characters).
    osg::ref_ptr<osg::Image> tintSkinImage(const osg::Image& skin, const osg::Image& tint);

    /// The same with the average colour of the texture in the place of the texture: for a part of the body whose
    /// picture is laid out otherwise than the texture's (the hands have a picture of their own, the texture is that of
    /// the body).
    osg::ref_ptr<osg::Image> tintSkinImageByAverage(const osg::Image& skin, const osg::Image& tint);

    /// Gives the shapes of a hair, beard or eyebrow its colour. The pictures of the hair of the games are greys that
    /// the colour of the character multiplies, twice over as for the skin, and the colours the nif holds at its
    /// vertices (a shade of one channel, not a hair colour) are not used. `colour` has each channel from 0 to 1. The
    /// shapes get their own material in this part only. Returns how many shapes it changed.
    std::size_t tintHair(osg::Node& part, const osg::Vec3f& colour);

    /// Keeps one of the two shapes that the nif of a hair holds, "Hat" for a head with a hat on it and "NoHat"
    /// for a bare one, and takes the other out of this part. A hair that has only one of them keeps it. Returns how
    /// many shapes it took out.
    std::size_t chooseHairShape(osg::Node& part, bool hat);

    /// The name of the node of the skeleton that a model asks to be attached to: the games put the hair, eyes, mouth
    /// and beard of a character, and the hats, on the bone "Bip01 Head" by the extra data "Prn" of the file, since
    /// these parts are not skinned. Empty when the model names none.
    std::string attachmentName(osg::Node& part);

    /// The turn that the node of the file with the extra data "Prn" has (the identity when the model has none). The
    /// optimizer folds it into the meshes of the model, so the part itself no longer shows it.
    osg::Quat attachmentRotation(osg::Node& part);

    /// The turn to put between a bone and a part that is not skinned, so that the part is in the frame of the head
    /// model (the axes of the world, the origin on the bone) when the bone is at rest. The files of these parts
    /// hold their vertices in that frame, and the eyes and mouth have the turn of the bone undone in their own root
    /// (`partRoot`) while the hair, eyebrows and beards have none; the result is the same for both: the turn that
    /// sits on the bone, then the one of the file, then the meshes give the identity. `boneRest` is the turn of the
    /// bone in the character at rest, `partRoot` the one of `attachmentRotation`.
    osg::Quat attachmentFrame(const osg::Quat& boneRest, const osg::Quat& partRoot);

    /// The morphs of the .egm file that goes with a model (a path as the race and the hair records name it). The file
    /// is read once. Null when there is none, or when it is not valid, which is said once in the log.
    ///
    /// The hair of the games is two shapes in one model ("Hat" and "NoHat"), and the file of each is named after the
    /// model and the shape ("hairmessy01nohat.egm"): `variant` is the end of the name before the extension.
    std::shared_ptr<const ESM4::FaceMorphs> getFaceMorphs(
        const VFS::Manager& vfs, std::string_view model, std::string_view variant = {});

    /// The textures that the archives hold for single characters, found when first asked for
    const ESM4::FaceTextureIndex& getFaceTextures(const VFS::Manager& vfs);
}

#endif
