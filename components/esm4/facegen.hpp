#ifndef OPENFALLOUT_COMPONENTS_ESM4_FACEGEN_H
#define OPENFALLOUT_COMPONENTS_ESM4_FACEGEN_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ESM4
{
    // FaceGen in Fallout 3 and New Vegas. The face of a character is the head of its race, moved by the coefficients
    // that the NPC_ record holds (FGGS, 50 numbers, and FGGA, 30 numbers). The race's head model has a .egm file beside
    // it that holds, for each coefficient, how far every vertex of the model moves when the coefficient is 1. The
    // skin of the face is a texture that the editor wrote for the character, if it did.

    // One way the face can change: every vertex moves by its three deltas times the scale times the coefficient
    struct FaceMorph
    {
        float mScale = 0.f;
        std::vector<std::int16_t> mDeltas; // x, y and z of each vertex
    };

    struct FaceMorphs
    {
        std::uint32_t mVertexCount = 0;
        std::vector<FaceMorph> mSymmetric; // those of FGGS
        std::vector<FaceMorph> mAsymmetric; // those of FGGA
    };

    // Reads a .egm file. False if it is not one that fits together, and then error says what is wrong.
    bool readFaceMorphs(std::string_view bytes, FaceMorphs& morphs, std::string& error);

    // Adds the morphs, scaled by the coefficients, to the positions of vertexCount vertices (three floats each) that
    // are the vertices from `first` of the file on. Coefficients that the file has no morph for are left out, as are
    // morphs that have no coefficient. Returns how many morphs moved the vertices (those with a coefficient that is
    // not zero).
    std::size_t applyFaceMorphs(const FaceMorphs& morphs, const std::vector<float>& symmetric,
        const std::vector<float>& asymmetric, float* positions, std::size_t vertexCount, std::size_t first = 0);

    // The .egm file that goes with a model: the same file name with the extension .egm
    // ("meshes/characters/head/headhuman.nif" has "meshes/characters/head/headhuman.egm"). Empty if the file name has
    // no extension.
    std::string faceMorphPath(std::string_view modelPath);

    // The textures that the editor wrote for single characters, under textures/characters/facemods/<plugin>/ (the skin
    // of the face: <8 hex digits>_0.dds) and textures/characters/bodymods/<plugin>/ (the skin of the body:
    // <8 hex digits>modbodymale.dds and <8 hex digits>modbodyfemale.dds). The 8 digits are the form id of the character
    // as the editor knew it, whose first two digits are not those of the load order of the game, so a file is matched
    // by the plugin's folder and the last six digits.
    class FaceTextureIndex
    {
    public:
        enum class Kind
        {
            Face,
            BodyMale,
            BodyFemale,
        };

        // Takes a path as the file system of the game lists it (lower case, with slashes). Paths that are not of such
        // a texture are left out, and so are the odd names with a letter and two ids in them.
        void add(std::string_view path);

        // The texture of the character with the form id (the load order digits do not matter) that a plugin made. The
        // folder of the plugin is tried first (the editor names it with the extension of the plugin, "falloutnv.esm",
        // and the name is taken with or without it); if it has no file for the character, a file in a folder of another
        // plugin is taken only when no two folders have one.
        const std::string* find(Kind kind, std::string_view plugin, std::uint32_t formId) const;

        std::size_t size() const { return mCount; }

    private:
        // The files of each kind, by the name of the folder of the plugin and then by the last six digits of the id
        std::map<Kind, std::map<std::string, std::map<std::uint32_t, std::string>, std::less<>>> mFiles;
        std::size_t mCount = 0;
    };
}

#endif
