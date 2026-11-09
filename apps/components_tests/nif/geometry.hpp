#ifndef OPENFALLOUT_TEST_SUITE_NIF_GEOMETRY_H
#define OPENFALLOUT_TEST_SUITE_NIF_GEOMETRY_H

#include "niftestfile.hpp"

#include <cstdint>
#include <vector>

namespace NifTest
{
    /// The body of the data of a geometry: the vertices, the normals, the colours and the coordinates of the texture,
    /// as the version of the file writes them. The data flags say how many sets of coordinates and whether the
    /// tangents are there.
    inline Bytes geometryData(
        Builder& file, std::uint16_t numVertices, std::uint16_t dataFlags, bool normals, bool colors)
    {
        const bool fallout = file.profile().mVersion >= 0x14020007;
        Bytes data;
        if (file.profile().mVersion >= 0x0A010072)
            data.i32(0); // group ID
        data.u16(numVertices);
        if (file.profile().mVersion >= 0x0A010000)
            data.u8(0).u8(0); // keep and compress flags
        file.flag(data, true);
        for (std::uint16_t i = 0; i < numVertices; ++i)
            data.vec3(static_cast<float>(i), static_cast<float>(i) + 0.5f, static_cast<float>(i) + 0.25f);
        if (file.profile().mVersion >= 0x0A000100)
            data.u16(dataFlags);
        file.flag(data, normals);
        if (normals)
        {
            for (std::uint16_t i = 0; i < numVertices; ++i)
                data.vec3(0, 0, 1);
            if (dataFlags & 0x1000)
            {
                for (std::uint16_t i = 0; i < numVertices; ++i)
                    data.vec3(1, 0, 0);
                for (std::uint16_t i = 0; i < numVertices; ++i)
                    data.vec3(0, 1, 0);
            }
        }
        data.vec3(1, 2, 3).f32(10); // the bounding sphere
        file.flag(data, colors);
        if (colors)
            for (std::uint16_t i = 0; i < numVertices; ++i)
                data.vec4(1, 0.5f, 0.25f, 1);
        if (file.profile().mVersion <= 0x04020200)
            data.u16(dataFlags);
        const std::uint16_t numUvs = fallout ? (dataFlags & 1) : (dataFlags & 0x3F);
        if (file.profile().mVersion == 0x04000002)
            file.flag(data, numUvs > 0);
        for (std::uint16_t set = 0; set < numUvs; ++set)
            for (std::uint16_t i = 0; i < numVertices; ++i)
                data.vec2(static_cast<float>(i) * 0.5f, static_cast<float>(set));
        if (file.profile().mVersion >= 0x0A000100)
        {
            data.u16(0); // consistency
            if (file.profile().mVersion >= 0x14000004)
                data.i32(-1); // additional data
        }
        return data;
    }

    inline Bytes triShapeData(Builder& file, std::uint16_t numVertices, std::uint16_t dataFlags,
        const std::vector<std::uint16_t>& triangles, bool normals = true, bool colors = true)
    {
        Bytes data = geometryData(file, numVertices, dataFlags, normals, colors);
        data.u16(static_cast<std::uint16_t>(triangles.size() / 3));
        data.u32(static_cast<std::uint32_t>(triangles.size()));
        if (file.profile().mVersion > 0x0A000102)
            file.flag(data, !triangles.empty());
        for (const std::uint16_t index : triangles)
            data.u16(index);
        data.u16(0); // no match groups
        return data;
    }

    inline Bytes geometry(Builder& file, const Builder::Place& at, std::int32_t data, std::int32_t skin = -1)
    {
        Bytes bytes;
        file.place(bytes, at);
        bytes.ref(data);
        if (file.profile().mVersion >= 0x03030D0D)
            bytes.ref(skin);
        if (file.profile().mVersion >= 0x0A000100)
        {
            // the material data: no names
            if (file.profile().mVersion >= 0x14020005)
                bytes.u32(0);
            else if (file.profile().mVersion <= 0x14010003)
                file.flag(bytes, false);
            if (file.profile().mVersion >= 0x14020005)
                bytes.i32(-1); // the active material
            if (file.profile().mVersion >= 0x14020007)
                file.flag(bytes, false); // needs an update
        }
        if (file.profile().mVersion == 0x14020007 && file.profile().mBethVersion > 34)
            bytes.ref(-1).ref(-1); // the shader and the alpha property
        return bytes;
    }
}

#endif
