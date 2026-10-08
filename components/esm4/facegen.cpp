#include "facegen.hpp"

#include <algorithm>
#include <bit>
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
        if (bytes.substr(0, 5) != "FREGM")
            return fail(error, "the file is not a FaceGen morph file");

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
        if (directory == "facemods")
        {
            kind = Kind::Face;
            if (!readId(stem, "_0", id))
                return;
        }
        else if (directory == "bodymods")
        {
            if (readId(stem, "modbodymale", id))
                kind = Kind::BodyMale;
            else if (readId(stem, "modbodyfemale", id))
                kind = Kind::BodyFemale;
            else
                return;
        }
        else
            return;

        // Of several files with the same last six digits, the first is kept
        auto& files = mFiles[kind].try_emplace(std::string(plugin)).first->second;
        std::string full(root);
        full.append(directory).append("/").append(plugin).append("/").append(file);
        if (files.try_emplace(id, std::move(full)).second)
            ++mCount;
    }

    const std::string* FaceTextureIndex::find(Kind kind, std::string_view plugin, std::uint32_t formId) const
    {
        const auto kindFiles = mFiles.find(kind);
        if (kindFiles == mFiles.end())
            return nullptr;
        const std::uint32_t id = formId & 0x00FFFFFFu;

        const auto own = kindFiles->second.find(plugin);
        if (own != kindFiles->second.end())
        {
            const auto file = own->second.find(id);
            if (file != own->second.end())
                return &file->second;
        }

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
}
