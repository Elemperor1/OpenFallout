#include "facegen.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

#include <components/misc/strings/algorithm.hpp>

namespace ESM4
{
    static_assert(std::endian::native == std::endian::little);

    namespace
    {
        // "FREGM002", the number of vertices, of morphs for FGGS, of morphs for FGGA and a number that tells what the
        // morphs were made for, then a block of 40 bytes that is not used
        constexpr std::size_t sHeaderSize = 8 + 4 * 4 + 40;
        constexpr std::size_t sMaxMorphs = 4096;

        template <class T>
        T readAt(std::string_view bytes, std::size_t offset)
        {
            T value;
            std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return value;
        }

        // The editor names the folder of a plugin with its extension ("falloutnv.esm"), the load order lists the plugin
        // by its file name, and a stem is easier to be given; the folders are kept by the name without the extension
        std::string_view withoutPluginExtension(std::string_view name)
        {
            for (const std::string_view extension : { ".esm", ".esp", ".esl" })
                if (name.size() > extension.size() && name.ends_with(extension))
                    return name.substr(0, name.size() - extension.size());
            return name;
        }

        bool fail(std::string& error, std::string message)
        {
            error = std::move(message);
            return false;
        }

        bool readMorphs(std::string_view bytes, std::size_t& offset, std::size_t count, std::size_t vertices,
            std::vector<FaceMorph>& out)
        {
            out.resize(count);
            for (FaceMorph& morph : out)
            {
                morph.mScale = readAt<float>(bytes, offset);
                offset += sizeof(float);
                morph.mDeltas.resize(vertices * 3);
                std::memcpy(morph.mDeltas.data(), bytes.data() + offset, vertices * 3 * sizeof(std::int16_t));
                offset += vertices * 3 * sizeof(std::int16_t);
            }
            return true;
        }

        // The value of eight lower case hex digits, or false
        bool readEightDigits(std::string_view text, std::uint32_t& value)
        {
            if (text.size() != 8)
                return false;
            value = 0;
            for (const char c : text)
            {
                std::uint32_t digit;
                if (c >= '0' && c <= '9')
                    digit = static_cast<std::uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    digit = static_cast<std::uint32_t>(c - 'a' + 10);
                else
                    return false;
                value = value << 4 | digit;
            }
            return true;
        }

        // Two ids of eight digits with an underscore between and `rest` after them: the race and the character
        bool readRaceAndId(std::string_view name, std::string_view rest, std::uint32_t& race, std::uint32_t& id)
        {
            if (name.size() != 17 + rest.size() || name[8] != '_' || name.substr(17) != rest)
                return false;
            return readEightDigits(name.substr(0, 8), race) && readEightDigits(name.substr(9, 8), id);
        }

        std::uint64_t raceKey(std::uint32_t race, std::uint32_t id, bool female)
        {
            return (female ? std::uint64_t(1) << 63 : 0) | std::uint64_t(race & 0x00FFFFFFu) << 24
                | std::uint64_t(id & 0x00FFFFFFu);
        }

        // The six hex digits at the end of eight, or false if there are not eight lower case digits before the rest
        bool readId(std::string_view name, std::string_view rest, std::uint32_t& id)
        {
            if (name.size() != 8 + rest.size() || name.substr(8) != rest)
                return false;
            std::uint32_t value = 0;
            for (std::size_t i = 0; i < 8; ++i)
            {
                const char c = name[i];
                std::uint32_t digit;
                if (c >= '0' && c <= '9')
                    digit = static_cast<std::uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    digit = static_cast<std::uint32_t>(c - 'a' + 10);
                else
                    return false;
                value = value << 4 | digit;
            }
            id = value & 0x00FFFFFFu;
            return true;
        }
    }

    bool readFaceMorphs(std::string_view bytes, FaceMorphs& morphs, std::string& error)
    {
        if (bytes.size() < sHeaderSize)
            return fail(error, "the file is shorter than its header");
        if (bytes.substr(0, 8) != "FREGM002")
            return fail(error, "the file is not a FaceGen morph file of the version 002");

        const std::uint32_t vertices = readAt<std::uint32_t>(bytes, 8);
        const std::uint32_t symmetric = readAt<std::uint32_t>(bytes, 12);
        const std::uint32_t asymmetric = readAt<std::uint32_t>(bytes, 16);
        if (vertices == 0 || vertices > 0xFFFF)
            return fail(error, "the file has " + std::to_string(vertices) + " vertices");
        if (symmetric > sMaxMorphs || asymmetric > sMaxMorphs)
            return fail(
                error, "the file has " + std::to_string(symmetric) + " and " + std::to_string(asymmetric) + " morphs");

        // The morphs fill the rest of the file, and the sizes must add up to all of it
        const std::size_t morphSize = sizeof(float) + vertices * 3 * sizeof(std::int16_t);
        const std::size_t morphsSize = (static_cast<std::size_t>(symmetric) + asymmetric) * morphSize;
        if (bytes.size() != sHeaderSize + morphsSize)
            return fail(error,
                "the file has " + std::to_string(bytes.size()) + " bytes, but " + std::to_string(vertices)
                    + " vertices and " + std::to_string(symmetric + asymmetric) + " morphs need "
                    + std::to_string(sHeaderSize + morphsSize));

        FaceMorphs result;
        result.mVertexCount = vertices;
        std::size_t offset = sHeaderSize;
        readMorphs(bytes, offset, symmetric, vertices, result.mSymmetric);
        readMorphs(bytes, offset, asymmetric, vertices, result.mAsymmetric);
        morphs = std::move(result);
        return true;
    }

    namespace
    {
        // "FREGT003", the width and the height of the pictures, the number of modes for FGTS and for FGTA and more
        // numbers that are not used, zeros to 64 bytes
        constexpr std::size_t sTextureHeaderSize = 64;
        constexpr std::uint32_t sMaxTextureSide = 4096;
    }

    bool readFaceTextureMorphs(std::string_view bytes, FaceTextureMorphs& morphs, std::string& error)
    {
        if (bytes.size() < sTextureHeaderSize)
            return fail(error, "the file is shorter than its header");
        if (bytes.substr(0, 8) != "FREGT003")
            return fail(error, "the file is not a FaceGen texture file of the version 003");

        const std::uint32_t width = readAt<std::uint32_t>(bytes, 8);
        const std::uint32_t height = readAt<std::uint32_t>(bytes, 12);
        const std::uint32_t symmetric = readAt<std::uint32_t>(bytes, 16);
        const std::uint32_t asymmetric = readAt<std::uint32_t>(bytes, 20);
        if (width == 0 || height == 0 || width > sMaxTextureSide || height > sMaxTextureSide)
            return fail(error, "the pictures are " + std::to_string(width) + " by " + std::to_string(height));
        if (symmetric > sMaxMorphs || asymmetric > sMaxMorphs)
            return fail(
                error, "the file has " + std::to_string(symmetric) + " and " + std::to_string(asymmetric) + " modes");

        // The modes fill the rest of the file (those for FGTA come after the others and are not used), and the sizes
        // must add up to all of it
        const std::size_t planeSize = static_cast<std::size_t>(width) * height * 3;
        const std::size_t modeSize = sizeof(float) + planeSize;
        const std::size_t modesSize = (static_cast<std::size_t>(symmetric) + asymmetric) * modeSize;
        if (bytes.size() != sTextureHeaderSize + modesSize)
            return fail(error,
                "the file has " + std::to_string(bytes.size()) + " bytes, but pictures of " + std::to_string(width)
                    + " by " + std::to_string(height) + " and " + std::to_string(symmetric + asymmetric)
                    + " modes need " + std::to_string(sTextureHeaderSize + modesSize));

        FaceTextureMorphs result;
        result.mWidth = static_cast<int>(width);
        result.mHeight = static_cast<int>(height);
        result.mScales.resize(symmetric);
        result.mPlanes.resize(static_cast<std::size_t>(symmetric) * planeSize);
        std::size_t offset = sTextureHeaderSize;
        for (std::size_t mode = 0; mode < symmetric; ++mode)
        {
            result.mScales[mode] = readAt<float>(bytes, offset);
            offset += sizeof(float);
            std::memcpy(result.mPlanes.data() + mode * planeSize, bytes.data() + offset, planeSize);
            offset += planeSize;
        }
        morphs = std::move(result);
        return true;
    }

    std::vector<float> addFaceCoefficients(const std::vector<float>& race, const std::vector<float>& character)
    {
        std::vector<float> result(std::max(race.size(), character.size()), 0.f);
        for (std::size_t i = 0; i < race.size(); ++i)
            result[i] += race[i];
        for (std::size_t i = 0; i < character.size(); ++i)
            result[i] += character[i];
        return result;
    }

    std::size_t applyFaceMorphs(const FaceMorphs& morphs, const std::vector<float>& symmetric,
        const std::vector<float>& asymmetric, float* positions, std::size_t vertexCount, std::size_t first)
    {
        if (first >= morphs.mVertexCount)
            return 0;
        const std::size_t count = std::min<std::size_t>(vertexCount, morphs.mVertexCount - first);

        std::size_t moved = 0;
        const auto apply = [&](const std::vector<FaceMorph>& all, const std::vector<float>& coefficients) {
            for (std::size_t i = 0; i < all.size() && i < coefficients.size(); ++i)
            {
                const float factor = all[i].mScale * coefficients[i];
                if (factor == 0.f)
                    continue;
                const std::int16_t* deltas = all[i].mDeltas.data() + first * 3;
                for (std::size_t vertex = 0; vertex < count * 3; ++vertex)
                    positions[vertex] += factor * static_cast<float>(deltas[vertex]);
                ++moved;
            }
        };
        apply(morphs.mSymmetric, symmetric);
        apply(morphs.mAsymmetric, asymmetric);
        return moved;
    }

    std::string faceMorphPath(std::string_view modelPath)
    {
        // The extension is the part of the file name after its last dot
        const std::size_t separator = modelPath.find_last_of("/\\");
        const std::size_t dot = modelPath.find_last_of('.');
        if (dot == std::string_view::npos || dot == 0 || (separator != std::string_view::npos && dot < separator + 2))
            return {};
        std::string result(modelPath.substr(0, dot));
        result += ".egm";
        return result;
    }

    std::string faceTexturePath(std::string_view modelPath)
    {
        std::string result = faceMorphPath(modelPath);
        if (!result.empty())
            result.back() = 't';
        return result;
    }

    void FaceTextureIndex::add(std::string_view path)
    {
        constexpr std::string_view root = "textures/characters/";
        if (path.substr(0, root.size()) != root)
            return;
        path.remove_prefix(root.size());

        const std::size_t firstSlash = path.find('/');
        if (firstSlash == std::string_view::npos)
            return;
        const std::string_view directory = path.substr(0, firstSlash);
        path.remove_prefix(firstSlash + 1);

        const std::size_t secondSlash = path.find('/');
        if (secondSlash == std::string_view::npos)
            return;
        const std::string_view plugin = path.substr(0, secondSlash);
        const std::string_view file = path.substr(secondSlash + 1);
        if (file.find('/') != std::string_view::npos || !file.ends_with(".dds"))
            return;
        const std::string_view stem = file.substr(0, file.size() - 4);

        Kind kind;
        std::uint32_t id = 0;
        std::uint32_t race = 0;
        bool raceFile = false;
        bool female = false;
        if (directory == "facemods")
        {
            kind = Kind::Face;
            if (!readId(stem, "_0", id))
            {
                // The files of a character in a race: the sex, the race and the character
                if (stem.size() > 1 && (stem[0] == 'm' || stem[0] == 'f')
                    && readRaceAndId(stem.substr(1), "_0", race, id))
                {
                    raceFile = true;
                    female = stem[0] == 'f';
                }
                else
                    return;
            }
        }
        else if (directory == "bodymods")
        {
            if (readId(stem, "modbodymale", id))
                kind = Kind::BodyMale;
            else if (readId(stem, "modbodyfemale", id))
                kind = Kind::BodyFemale;
            else if (readRaceAndId(stem, "modbodymale", race, id))
            {
                kind = Kind::BodyMale;
                raceFile = true;
            }
            else if (readRaceAndId(stem, "modbodyfemale", race, id))
            {
                kind = Kind::BodyFemale;
                raceFile = true;
                female = true;
            }
            else
                return;
        }
        else
            return;

        if (raceFile)
        {
            auto& raceFiles = mRaceFiles[kind].try_emplace(std::string(withoutPluginExtension(plugin))).first->second;
            std::string name(root);
            name.append(directory).append("/").append(plugin).append("/").append(file);
            if (raceFiles.try_emplace(raceKey(race, id, female), std::move(name)).second)
                ++mCount;
            return;
        }

        // Of several files with the same last six digits, the first is kept
        auto& files = mFiles[kind].try_emplace(std::string(withoutPluginExtension(plugin))).first->second;
        std::string full(root);
        full.append(directory).append("/").append(plugin).append("/").append(file);
        if (files.try_emplace(id, std::move(full)).second)
            ++mCount;
    }

    const std::string* FaceTextureIndex::findIn(Kind kind, std::string_view plugin, std::uint32_t formId) const
    {
        const auto kindFiles = mFiles.find(kind);
        if (kindFiles == mFiles.end())
            return nullptr;
        const auto own = kindFiles->second.find(withoutPluginExtension(plugin));
        if (own == kindFiles->second.end())
            return nullptr;
        const auto file = own->second.find(formId & 0x00FFFFFFu);
        return file != own->second.end() ? &file->second : nullptr;
    }

    const std::string* FaceTextureIndex::findForRace(
        Kind kind, std::string_view plugin, std::uint32_t race, std::uint32_t formId, bool female) const
    {
        const auto kindFiles = mRaceFiles.find(kind);
        if (kindFiles == mRaceFiles.end())
            return nullptr;
        const std::uint64_t key = raceKey(race, formId, female);
        plugin = withoutPluginExtension(plugin);
        if (const auto own = kindFiles->second.find(plugin); own != kindFiles->second.end())
            if (const auto file = own->second.find(key); file != own->second.end())
                return &file->second;
        const std::string* only = nullptr;
        std::size_t found = 0;
        for (const auto& [name, files] : kindFiles->second)
        {
            if (name == plugin)
                continue;
            const auto file = files.find(key);
            if (file != files.end())
            {
                only = &file->second;
                ++found;
            }
        }
        return found == 1 ? only : nullptr;
    }

    const std::string* FaceTextureIndex::find(Kind kind, std::string_view plugin, std::uint32_t formId) const
    {
        if (const std::string* own = findIn(kind, plugin, formId))
            return own;
        const auto kindFiles = mFiles.find(kind);
        if (kindFiles == mFiles.end())
            return nullptr;
        const std::uint32_t id = formId & 0x00FFFFFFu;
        plugin = withoutPluginExtension(plugin);

        const std::string* only = nullptr;
        std::size_t found = 0;
        for (const auto& [name, files] : kindFiles->second)
        {
            if (name == plugin)
                continue;
            const auto file = files.find(id);
            if (file != files.end())
            {
                only = &file->second;
                ++found;
            }
        }
        return found == 1 ? only : nullptr;
    }
    namespace
    {
        std::uint8_t clampByte(float value)
        {
            return static_cast<std::uint8_t>(std::clamp(value + 0.5f, 0.f, 255.f));
        }

        // The value of a mod that changes nothing: the shaders take the half of 1 off it
        constexpr float sModNeutral = 127.5f;

        // The four colours of a block of S3TC: two of 5:6:5 bits and, between them, two more (when the first is the
        // greater, or always in the blocks of DXT3 and DXT5, which have their alpha before the colours)
        void blockColours(const std::uint8_t* block, bool fourColours, std::uint8_t colours[4][3])
        {
            const auto expand = [](std::uint16_t packed, std::uint8_t* rgb) {
                rgb[0] = static_cast<std::uint8_t>(((packed >> 11) & 31) * 255 / 31);
                rgb[1] = static_cast<std::uint8_t>(((packed >> 5) & 63) * 255 / 63);
                rgb[2] = static_cast<std::uint8_t>((packed & 31) * 255 / 31);
            };
            const std::uint16_t first = static_cast<std::uint16_t>(block[0] | (block[1] << 8));
            const std::uint16_t second = static_cast<std::uint16_t>(block[2] | (block[3] << 8));
            expand(first, colours[0]);
            expand(second, colours[1]);
            for (int channel = 0; channel < 3; ++channel)
            {
                const int a = colours[0][channel];
                const int b = colours[1][channel];
                if (fourColours || first > second)
                {
                    colours[2][channel] = static_cast<std::uint8_t>((2 * a + b) / 3);
                    colours[3][channel] = static_cast<std::uint8_t>((a + 2 * b) / 3);
                }
                else
                {
                    colours[2][channel] = static_cast<std::uint8_t>((a + b) / 2);
                    colours[3][channel] = 0;
                }
            }
        }

        float sampleChannel(const SkinImage& image, float x, float y, int channel)
        {
            // x and y in pixels, the centre of the first pixel at 0.5
            const float fx = std::clamp(x - 0.5f, 0.f, static_cast<float>(image.mWidth - 1));
            const float fy = std::clamp(y - 0.5f, 0.f, static_cast<float>(image.mHeight - 1));
            const int x0 = static_cast<int>(fx);
            const int y0 = static_cast<int>(fy);
            const int x1 = std::min(x0 + 1, image.mWidth - 1);
            const int y1 = std::min(y0 + 1, image.mHeight - 1);
            const float tx = fx - static_cast<float>(x0);
            const float ty = fy - static_cast<float>(y0);
            const auto at = [&](int px, int py) {
                return static_cast<float>(
                    image.mPixels[(static_cast<std::size_t>(py) * image.mWidth + px) * 3 + channel]);
            };
            const float top = at(x0, y0) * (1.f - tx) + at(x1, y0) * tx;
            const float bottom = at(x0, y1) * (1.f - tx) + at(x1, y1) * tx;
            return top * (1.f - ty) + bottom * ty;
        }

        bool fits(const SkinImage& image)
        {
            return image.mWidth > 0 && image.mHeight > 0
                && image.mPixels.size() == static_cast<std::size_t>(image.mWidth) * image.mHeight * 3;
        }
    }

    bool decodeSkinImage(
        SkinPixelFormat format, const std::uint8_t* data, std::size_t size, int width, int height, SkinImage& image)
    {
        constexpr int maxSide = 8192;
        if (data == nullptr || width <= 0 || height <= 0 || width > maxSide || height > maxSide)
            return false;
        const std::size_t pixels = static_cast<std::size_t>(width) * height;

        // What the picture needs is known from its format: a file that declares more than it holds is refused before
        // anything is made for it
        const std::size_t blocks = static_cast<std::size_t>((width + 3) / 4) * ((height + 3) / 4);
        std::size_t required = 0;
        switch (format)
        {
            case SkinPixelFormat::Rgb:
            case SkinPixelFormat::Bgr:
                required = pixels * 3;
                break;
            case SkinPixelFormat::Rgba:
            case SkinPixelFormat::Bgra:
                required = pixels * 4;
                break;
            case SkinPixelFormat::Dxt1:
                required = blocks * 8;
                break;
            case SkinPixelFormat::Dxt3:
            case SkinPixelFormat::Dxt5:
                required = blocks * 16;
                break;
        }
        if (required == 0 || size < required)
            return false;

        image.mWidth = width;
        image.mHeight = height;
        image.mPixels.assign(pixels * 3, 0);

        switch (format)
        {
            case SkinPixelFormat::Rgb:
            case SkinPixelFormat::Bgr:
            case SkinPixelFormat::Rgba:
            case SkinPixelFormat::Bgra:
            {
                const std::size_t step = (format == SkinPixelFormat::Rgb || format == SkinPixelFormat::Bgr) ? 3 : 4;
                if (size < pixels * step)
                    return false;
                const bool swap = format == SkinPixelFormat::Bgr || format == SkinPixelFormat::Bgra;
                for (std::size_t i = 0; i < pixels; ++i)
                {
                    const std::uint8_t* from = data + i * step;
                    image.mPixels[i * 3] = from[swap ? 2 : 0];
                    image.mPixels[i * 3 + 1] = from[1];
                    image.mPixels[i * 3 + 2] = from[swap ? 0 : 2];
                }
                return true;
            }
            case SkinPixelFormat::Dxt1:
            case SkinPixelFormat::Dxt3:
            case SkinPixelFormat::Dxt5:
            {
                const bool hasAlphaBlock = format != SkinPixelFormat::Dxt1;
                const std::size_t blockSize = hasAlphaBlock ? 16 : 8;
                const int blocksX = (width + 3) / 4;
                const int blocksY = (height + 3) / 4;
                if (size < static_cast<std::size_t>(blocksX) * blocksY * blockSize)
                    return false;
                for (int by = 0; by < blocksY; ++by)
                    for (int bx = 0; bx < blocksX; ++bx)
                    {
                        const std::uint8_t* block = data + (static_cast<std::size_t>(by) * blocksX + bx) * blockSize
                            + (hasAlphaBlock ? 8 : 0);
                        std::uint8_t colours[4][3];
                        blockColours(block, hasAlphaBlock, colours);
                        for (int y = 0; y < 4; ++y)
                            for (int x = 0; x < 4; ++x)
                            {
                                const int px = bx * 4 + x;
                                const int py = by * 4 + y;
                                if (px >= width || py >= height)
                                    continue;
                                const int index = (block[4 + y] >> (2 * x)) & 3;
                                std::uint8_t* out = &image.mPixels[(static_cast<std::size_t>(py) * width + px) * 3];
                                out[0] = colours[index][0];
                                out[1] = colours[index][1];
                                out[2] = colours[index][2];
                            }
                    }
                return true;
            }
        }
        return false;
    }

    SkinImage addSkinMod(const SkinImage& skin, const SkinImage& mod, float scale, const std::array<float, 3>& gain)
    {
        if (!fits(skin) || !fits(mod))
            return {};
        SkinImage result;
        result.mWidth = skin.mWidth;
        result.mHeight = skin.mHeight;
        result.mPixels.resize(skin.mPixels.size());
        // The mod is read at the place of each pixel of the skin, in the units of its own pixels
        const float toModX = static_cast<float>(mod.mWidth) / static_cast<float>(skin.mWidth);
        const float toModY = static_cast<float>(mod.mHeight) / static_cast<float>(skin.mHeight);
        for (int y = 0; y < skin.mHeight; ++y)
            for (int x = 0; x < skin.mWidth; ++x)
            {
                const float tx = (static_cast<float>(x) + 0.5f) * toModX;
                const float ty = (static_cast<float>(y) + 0.5f) * toModY;
                const std::size_t at = (static_cast<std::size_t>(y) * skin.mWidth + x) * 3;
                for (int channel = 0; channel < 3; ++channel)
                {
                    const float offset = (sampleChannel(mod, tx, ty, channel) - sModNeutral) * scale;
                    result.mPixels[at + channel]
                        = clampByte((static_cast<float>(skin.mPixels[at + channel]) + offset) * gain[channel]);
                }
            }
        return result;
    }

    std::array<float, 3> averageColour(const SkinImage& image)
    {
        std::array<float, 3> mean{ 0.f, 0.f, 0.f };
        if (!fits(image))
            return mean;
        std::array<double, 3> sum{ 0.0, 0.0, 0.0 };
        const std::size_t pixels = static_cast<std::size_t>(image.mWidth) * image.mHeight;
        for (std::size_t i = 0; i < pixels; ++i)
            for (int channel = 0; channel < 3; ++channel)
                sum[channel] += image.mPixels[i * 3 + channel];
        for (int channel = 0; channel < 3; ++channel)
            mean[channel] = static_cast<float>(sum[channel] / static_cast<double>(pixels));
        return mean;
    }

    SkinImage addSkinMod(
        const SkinImage& skin, const std::array<float, 3>& mod, float scale, const std::array<float, 3>& gain)
    {
        if (!fits(skin))
            return {};
        SkinImage result;
        result.mWidth = skin.mWidth;
        result.mHeight = skin.mHeight;
        result.mPixels.resize(skin.mPixels.size());
        for (std::size_t i = 0; i < skin.mPixels.size(); ++i)
        {
            const std::size_t channel = i % 3;
            result.mPixels[i] = clampByte(
                (static_cast<float>(skin.mPixels[i]) + (mod[channel] - sModNeutral) * scale) * gain[channel]);
        }
        return result;
    }

    SkinImage makeFaceToneImage(const FaceTextureMorphs& morphs, const std::vector<float>& coefficients, bool flipRows)
    {
        SkinImage result;
        if (morphs.mWidth <= 0 || morphs.mHeight <= 0)
            return result;
        const std::size_t width = static_cast<std::size_t>(morphs.mWidth);
        const std::size_t height = static_cast<std::size_t>(morphs.mHeight);
        const std::size_t pixels = width * height;
        if (morphs.mPlanes.size() != morphs.mScales.size() * pixels * 3)
            return result;

        // The whole numbers of the executable: the coefficient and the scale times 256, cut towards zero
        const auto fixed = [](float value) -> std::int64_t {
            if (!std::isfinite(value))
                return 0;
            return static_cast<std::int64_t>(std::clamp(value * 256.f, -1.0e7f, 1.0e7f));
        };
        std::vector<std::int64_t> sum(pixels * 3, 0);
        // At most this many of the modes are summed: the setting uiEGTClampSize of the executable, which no INI file
        // of the games has (Mac round 12BG B1)
        constexpr std::size_t clampSize = 30;
        const std::size_t modes = std::min({ morphs.mScales.size(), coefficients.size(), clampSize });
        for (std::size_t mode = 0; mode < modes; ++mode)
        {
            const std::int64_t weight = fixed(coefficients[mode]) * fixed(morphs.mScales[mode]);
            if (weight == 0)
                continue;
            const std::int8_t* plane = morphs.mPlanes.data() + mode * pixels * 3;
            for (std::size_t i = 0; i < pixels * 3; ++i)
                sum[i] += weight * plane[i];
        }

        result.mWidth = morphs.mWidth;
        result.mHeight = morphs.mHeight;
        result.mPixels.resize(pixels * 3);
        for (std::size_t y = 0; y < height; ++y)
        {
            const std::size_t row = flipRows ? height - 1 - y : y;
            for (std::size_t x = 0; x < width; ++x)
                for (std::size_t channel = 0; channel < 3; ++channel)
                {
                    // The steps this pixel moves, between -255 and 255, and half of them above the mid grey of a mod
                    const float steps = std::clamp(
                        static_cast<float>(static_cast<double>(sum[channel * pixels + row * width + x]) / 65536.0),
                        -255.f, 255.f);
                    result.mPixels[(y * width + x) * 3 + channel] = clampByte(sModNeutral + steps * 0.5f);
                }
        }
        return result;
    }
}
