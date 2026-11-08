#ifndef OPENFALLOUT_COMPONENTS_ESM4_FACEGEN_H
#define OPENFALLOUT_COMPONENTS_ESM4_FACEGEN_H

#include <array>
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

    // The coefficients of a face are the numbers of its race for the sex of the character, and those of the character
    // itself on top (the numbers of the characters in the games centre on 0, the numbers of the races do not, and a
    // character with all of them 0 has the face of its race). Either may be shorter or empty; the result has as many
    // numbers as the longer.
    std::vector<float> addFaceCoefficients(const std::vector<float>& race, const std::vector<float>& character);

    // The .egm file that goes with a model: the same file name with the extension .egm
    // ("meshes/characters/head/headhuman.nif" has "meshes/characters/head/headhuman.egm"). Empty if the file name has
    // no extension.
    std::string faceMorphPath(std::string_view modelPath);

    // How the colour of the face can change: the .egt file beside the model ("FREGT003") holds, for each coefficient of
    // FGTS, a scale and three pictures (red, green and blue) of signed bytes. The coefficient times the scale times the
    // byte of a pixel is how many of the 256 steps of that colour the face moves there.
    struct FaceTextureMorphs
    {
        int mWidth = 0;
        int mHeight = 0;
        std::vector<float> mScales; // one for each coefficient
        std::vector<std::int8_t>
            mPlanes; // for each coefficient the red, green and blue pictures, one row after the other
    };

    // Reads a .egt file. False if it is not one that fits together, and then error says what is wrong.
    bool readFaceTextureMorphs(std::string_view bytes, FaceTextureMorphs& morphs, std::string& error);

    // The .egt file that goes with a model (as faceMorphPath names the .egm file). Empty if the file name has no
    // extension.
    std::string faceTexturePath(std::string_view modelPath);

    // The colours of a character's skin. The texture that the editor writes for a character (see FaceTextureIndex) is
    // not a picture of the skin: its colours sit around mid grey (the averages of the real files are 84..141 of 255),
    // so in the place of the skin it paints the whole character grey. It is a factor of two on the skin of the race
    // (the head and body textures of the engine), so that mid grey changes nothing and the rest shifts the colour of
    // the skin. These are the pixels, to make that product on the processor.

    // The formats of the pictures that a skin is made of
    enum class SkinPixelFormat
    {
        Rgb,
        Rgba,
        Bgr,
        Bgra,
        Dxt1,
        Dxt3,
        Dxt5,
    };

    // A picture of 3 bytes for each pixel (red, green, blue), the first row first
    struct SkinImage
    {
        int mWidth = 0;
        int mHeight = 0;
        std::vector<std::uint8_t> mPixels;
    };

    // Unpacks the first level of a picture (a compressed one is in blocks of 4 by 4 pixels, the top row of blocks
    // first, as DDS files hold it). False when the size or the data do not fit the picture.
    bool decodeSkinImage(
        SkinPixelFormat format, const std::uint8_t* data, std::size_t size, int width, int height, SkinImage& image);

    // The base mod of a face that the game makes while it runs, for the coefficients of FGTS (those of the race and of
    // the character added): the sum over the coefficients of coefficient * scale * the picture of the mode, which the
    // executable takes in whole numbers (the coefficient and the scale times 256 cut towards zero, the products added
    // and divided by 65536, as BSFaceGenModel at 0x65B410 does), is how many steps each colour of the skin moves, and
    // the mod keeps half of it above mid grey so that the skin shader can double it (the steps are held between -255
    // and 255). The pictures are stored the way the file has them; `flipRows` turns them over. An empty picture when
    // the morphs hold no picture.
    SkinImage makeFaceToneImage(const FaceTextureMorphs& morphs, const std::vector<float>& coefficients, bool flipRows);

    // The skin as the skin shaders of the games draw it with the base mod of a character (its facemods or bodymods
    // file): the shader adds `scale` times (mod - 127.5) to each channel of the skin (the shaders: skin + 2 * (mod -
    // 0.5)) and multiplies the sum by `gain` for each channel (the detail mod, which no character here has a file for).
    // The mod is read at the same place of its own picture, between its pixels as a stretched picture is. A channel is
    // held between 0 and 255.
    SkinImage addSkinMod(const SkinImage& skin, const SkinImage& mod, float scale, const std::array<float, 3>& gain);

    // The average colour of a picture, each channel from 0 to 255; black for an empty one
    std::array<float, 3> averageColour(const SkinImage& image);

    // The same with one colour (each channel from 0 to 255) in the place of the mod
    SkinImage addSkinMod(
        const SkinImage& skin, const std::array<float, 3>& mod, float scale, const std::array<float, 3>& gain);

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

        // The texture in the folder of the plugin only, which is where the editor puts the texture of a character in
        // the plugin that last saved the character
        const std::string* findIn(Kind kind, std::string_view plugin, std::uint32_t formId) const;

        // The texture that the editor wrote for a character in a race, for the characters that have no file under their
        // own id because they get their race when the game makes them (facemods "m<race>_<character>_0.dds" for a man,
        // "f..." for a woman, bodymods "<race>_<character>modbodymale.dds"). The ids are matched by their last six
        // digits and the folder of the plugin is chosen as `find` chooses it.
        const std::string* findForRace(
            Kind kind, std::string_view plugin, std::uint32_t race, std::uint32_t formId, bool female) const;

        std::size_t size() const { return mCount; }

    private:
        // The files of each kind, by the name of the folder of the plugin and then by the last six digits of the id
        std::map<Kind, std::map<std::string, std::map<std::uint32_t, std::string>, std::less<>>> mFiles;
        // The same for the files of a character in a race, by the race and then the character (last six digits of each,
        // the race in the high bits), and for the face by the sex too in the top bit
        std::map<Kind, std::map<std::string, std::map<std::uint64_t, std::string>, std::less<>>> mRaceFiles;
        std::size_t mCount = 0;
    };
}

#endif
