#include <components/esm4/loadnavm.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <exception>
#include <string>
#include <variant>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    // The 32-bit count of an array and its bytes, which can be fewer or more than the count says
    std::string counted(std::uint32_t count, const std::string& elements)
    {
        std::string data;
        append(data, count);
        return data + elements;
    }

    std::string head(std::uint32_t worldSpace = 0x42)
    {
        std::string data;
        append<std::uint32_t>(data, 12); // NVER
        append<std::uint32_t>(data, 0x34); // LCTN
        append<std::uint32_t>(data, worldSpace);
        append<std::uint32_t>(data, 0x1234); // the cell, or the grid of an exterior one
        return data;
    }

    std::string vertices()
    {
        std::string data;
        for (const float value : { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f })
            append(data, value);
        return counted(2, data);
    }

    std::string triangles()
    {
        std::string data;
        for (std::uint16_t value = 1; value <= 8; ++value)
            append(data, value);
        return counted(1, data);
    }

    std::string extConns()
    {
        std::string data;
        append<std::uint32_t>(data, 0xaabbccdd);
        append<std::uint32_t>(data, 0x00010001);
        append<std::uint16_t>(data, 9);
        return counted(1, data);
    }

    std::string doorTriangles()
    {
        std::string data;
        append<std::uint16_t>(data, 7);
        append<std::uint32_t>(data, 0x11223344);
        append<std::uint32_t>(data, 0x00010002);
        return counted(1, data);
    }

    std::string coverTriangles()
    {
        std::string data;
        for (const std::uint16_t value : { 11, 12, 13 })
            append(data, value);
        return counted(3, data);
    }

    // The divisor and the eight floats of the bounds, the part before the segments
    std::string bounds(std::uint32_t divisor)
    {
        std::string data;
        append(data, divisor);
        for (const float value : { 1.5f, 2.5f, -1.f, -2.f, -3.f, 4.f, 5.f, 6.f })
            append(data, value);
        return data;
    }

    std::string segment(const std::vector<std::uint16_t>& indices)
    {
        std::string data;
        append<std::uint32_t>(data, static_cast<std::uint32_t>(indices.size()));
        for (const std::uint16_t index : indices)
            append(data, index);
        return data;
    }

    // Four segments, a divisor of two, one of them empty
    std::string segments()
    {
        return segment({ 1, 2, 3 }) + segment({}) + segment({ 4 }) + segment({ 5, 6 });
    }

    std::string wholeNvnm()
    {
        return head() + vertices() + triangles() + extConns() + doorTriangles() + coverTriangles() + bounds(2)
            + segments();
    }

    std::string emptyNvnm()
    {
        return head() + counted(0, "") + counted(0, "") + counted(0, "") + counted(0, "") + counted(0, "") + bounds(0);
    }

    std::string navmesh(const std::string& subRecords, std::uint32_t id = 5)
    {
        return record("NAVM", id, subRecords);
    }

    std::vector<ESM4::NavMesh> load(const std::string& record, std::size_t cutBytes = 0)
    {
        return loadRecords<ESM4::NavMesh>("NAVM", record, cutBytes);
    }

    std::string loadFailure(const std::string& record, std::size_t cutBytes = 0)
    {
        try
        {
            load(record, cutBytes);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    std::string loadNvnmFailure(const std::string& nvnm)
    {
        return loadFailure(navmesh(subRecord("NVNM", nvnm)));
    }

    TEST(ESM4NavMeshTest, readsEveryField)
    {
        const std::vector<ESM4::NavMesh> records = load(navmesh(subRecord("NVNM", wholeNvnm())));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 5u);
        ASSERT_EQ(records[0].mData.size(), 1u);
        const ESM4::NavMesh::NVNMstruct& nvnm = records[0].mData[0];
        EXPECT_EQ(nvnm.unknownNVER, 12u);
        EXPECT_EQ(nvnm.unknownLCTN, 0x34u);
        EXPECT_EQ(nvnm.worldSpaceId.toUint32(), 0x42u);
        EXPECT_EQ(std::get<ESM::FormId>(nvnm.cellGrid).toUint32(), 0x1234u);

        ASSERT_EQ(nvnm.verticies.size(), 2u);
        EXPECT_EQ(nvnm.verticies[0].x, 1.f);
        EXPECT_EQ(nvnm.verticies[0].z, 3.f);
        EXPECT_EQ(nvnm.verticies[1].x, 4.f);
        EXPECT_EQ(nvnm.verticies[1].z, 6.f);

        ASSERT_EQ(nvnm.triangles.size(), 1u);
        EXPECT_EQ(+nvnm.triangles[0].vertexIndex0, 1);
        EXPECT_EQ(+nvnm.triangles[0].edge0, 4);
        EXPECT_EQ(+nvnm.triangles[0].coverFlags, 8);

        ASSERT_EQ(nvnm.extConns.size(), 1u);
        EXPECT_EQ(+nvnm.extConns[0].unknown, 0xaabbccddu);
        EXPECT_EQ(+nvnm.extConns[0].navMesh, 0x00010001u);
        EXPECT_EQ(+nvnm.extConns[0].triangleIndex, 9);

        ASSERT_EQ(nvnm.doorTriangles.size(), 1u);
        EXPECT_EQ(+nvnm.doorTriangles[0].triangleIndex, 7);
        EXPECT_EQ(+nvnm.doorTriangles[0].unknown, 0x11223344u);
        EXPECT_EQ(+nvnm.doorTriangles[0].doorRef, 0x00010002u);

        EXPECT_THAT(nvnm.coverTriangles, ElementsAre(11, 12, 13));

        EXPECT_EQ(nvnm.divisor, 2u);
        EXPECT_EQ(nvnm.maxXDist, 1.5f);
        EXPECT_EQ(nvnm.maxYDist, 2.5f);
        EXPECT_EQ(nvnm.minX, -1.f);
        EXPECT_EQ(nvnm.minY, -2.f);
        EXPECT_EQ(nvnm.minZ, -3.f);
        EXPECT_EQ(nvnm.maxX, 4.f);
        EXPECT_EQ(nvnm.maxY, 5.f);
        EXPECT_EQ(nvnm.maxZ, 6.f);

        ASSERT_EQ(nvnm.triSegments.size(), 4u);
        EXPECT_THAT(nvnm.triSegments[0], ElementsAre(1, 2, 3));
        EXPECT_THAT(nvnm.triSegments[1], IsEmpty());
        EXPECT_THAT(nvnm.triSegments[2], ElementsAre(4));
        EXPECT_THAT(nvnm.triSegments[3], ElementsAre(5, 6));
    }

    TEST(ESM4NavMeshTest, readsTheGridOfAnExteriorMesh)
    {
        // The grid of Tamriel is two 16-bit numbers, y and then x, where other meshes have the form ID of a cell
        std::string grid;
        append<std::int16_t>(grid, -3);
        append<std::int16_t>(grid, 5);
        const std::string nvnm = head(0x3c).substr(0, 12) + grid + emptyNvnm().substr(16);

        const std::vector<ESM4::NavMesh> records = load(navmesh(subRecord("NVNM", nvnm)));

        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(records[0].mData.size(), 1u);
        const ESM4::Grid& result = std::get<ESM4::Grid>(records[0].mData[0].cellGrid);
        EXPECT_EQ(result.y, -3);
        EXPECT_EQ(result.x, 5);
    }

    TEST(ESM4NavMeshTest, readsAMeshWithNoVerticesAndNoSegments)
    {
        const std::vector<ESM4::NavMesh> records = load(navmesh(subRecord("NVNM", emptyNvnm())));

        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(records[0].mData.size(), 1u);
        const ESM4::NavMesh::NVNMstruct& nvnm = records[0].mData[0];
        EXPECT_THAT(nvnm.verticies, IsEmpty());
        EXPECT_THAT(nvnm.triangles, IsEmpty());
        EXPECT_THAT(nvnm.extConns, IsEmpty());
        EXPECT_THAT(nvnm.doorTriangles, IsEmpty());
        EXPECT_THAT(nvnm.coverTriangles, IsEmpty());
        EXPECT_EQ(nvnm.divisor, 0u);
        EXPECT_THAT(nvnm.triSegments, IsEmpty());
    }

    TEST(ESM4NavMeshTest, readsTheMeshesOfOneRecordInOrder)
    {
        std::string second = wholeNvnm();
        second.replace(12, 4, std::string("\x78\x56\x00\x00", 4)); // the cell
        const std::vector<ESM4::NavMesh> records = load(navmesh(
            subRecord("NVNM", wholeNvnm()) + valueSubRecord<std::uint32_t>("NNAM", 1) + subRecord("NVNM", second)));

        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(records[0].mData.size(), 2u);
        EXPECT_EQ(std::get<ESM::FormId>(records[0].mData[0].cellGrid).toUint32(), 0x1234u);
        EXPECT_EQ(std::get<ESM::FormId>(records[0].mData[1].cellGrid).toUint32(), 0x5678u);
        EXPECT_THAT(records[0].mData[1].triSegments[3], ElementsAre(5, 6));
    }

    TEST(ESM4NavMeshTest, goesOnAfterBytesThatFollowTheSegments)
    {
        // The reader goes on from the end of the sub-record, so a mesh that has more in it than the loader knows does
        // not make the bytes after the segments read as the header of the next sub-record
        std::string second = wholeNvnm();
        second.replace(12, 4, std::string("\x78\x56\x00\x00", 4));
        const std::vector<ESM4::NavMesh> records
            = load(navmesh(subRecord("NVNM", wholeNvnm() + bytePattern(7, 1)) + subRecord("NVNM", second)));

        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(records[0].mData.size(), 2u);
        EXPECT_EQ(std::get<ESM::FormId>(records[0].mData[1].cellGrid).toUint32(), 0x5678u);
        EXPECT_THAT(records[0].mData[1].triSegments[0], ElementsAre(1, 2, 3));
    }

    // Each of the five counted arrays before the bounds, in the order of the file, with the others as they are in a
    // whole mesh
    std::string nvnmWithArray(std::size_t which, const std::string& array)
    {
        std::vector<std::string> arrays = { vertices(), triangles(), extConns(), doorTriangles(), coverTriangles() };
        arrays[which] = array;
        std::string data = head();
        for (const std::string& each : arrays)
            data += each;
        return data + bounds(2) + segments();
    }

    TEST(ESM4NavMeshTest, rejectsAnArrayWithMoreElementsThanTheSubRecordHolds)
    {
        const char* const names[] = { "vertices", "triangles", "external connections", "door triangles", "cover" };
        for (std::size_t which = 0; which < 5; ++which)
        {
            SCOPED_TRACE(names[which]);
            // far more than the sub-record has bytes, and the largest count there is
            for (const std::uint32_t count : { std::uint32_t(100000), std::uint32_t(0xffffffff) })
            {
                SCOPED_TRACE(count);
                EXPECT_EQ(loadNvnmFailure(nvnmWithArray(which, counted(count, ""))),
                    "NVNM array is longer than the rest of the sub-record");
            }
        }
    }

    TEST(ESM4NavMeshTest, rejectsAnArrayOneElementLongerThanTheRestOfTheSubRecord)
    {
        // The last array has the bounds and the segments after it, which the count may not reach into
        const std::string before = head() + vertices() + triangles() + extConns() + doorTriangles();
        const std::string rest = bytePattern(32, 1) + bounds(0); // 68 bytes, 34 elements
        EXPECT_EQ(loadNvnmFailure(before + counted(35, rest)), "NVNM array is longer than the rest of the sub-record");
        // 34 elements fit the sub-record, and leave nothing to read the divisor from
        EXPECT_EQ(loadNvnmFailure(before + counted(34, rest)), "NVNM ends inside a field");
    }

    TEST(ESM4NavMeshTest, rejectsADivisorWhoseSquareOfSegmentsHasNoRoom)
    {
        const std::string before = head() + vertices() + triangles() + extConns() + doorTriangles() + coverTriangles();
        for (const std::uint32_t divisor : { std::uint32_t(3), std::uint32_t(65536), std::uint32_t(0xffffffff) })
        {
            SCOPED_TRACE(divisor);
            EXPECT_EQ(loadNvnmFailure(before + bounds(divisor) + segments()),
                "Triangle segments end before the square of divisor");
        }
    }

    TEST(ESM4NavMeshTest, rejectsASegmentWithMoreIndicesThanTheSubRecordHolds)
    {
        const std::string before = head() + vertices() + triangles() + extConns() + doorTriangles() + coverTriangles();
        // the last of the four segments says three indices and has two
        const std::string cut = segment({ 1, 2, 3 }) + segment({}) + segment({ 4 }) + counted(3, bytePattern(4, 5));
        EXPECT_EQ(loadNvnmFailure(before + bounds(2) + cut), "NVNM array is longer than the rest of the sub-record");
    }

    TEST(ESM4NavMeshTest, rejectsASubRecordThatEndsInsideAField)
    {
        const std::string whole = wholeNvnm();
        // inside the header, inside the count of the vertices, inside the divisor and the bounds
        const std::string beforeBounds
            = head() + counted(0, "") + counted(0, "") + counted(0, "") + counted(0, "") + counted(0, "");
        for (const std::string& data :
            { whole.substr(0, 10), whole.substr(0, 18), beforeBounds + bounds(0).substr(0, 2),
                beforeBounds + bounds(0).substr(0, 20), beforeBounds + bounds(0).substr(0, 35) })
        {
            SCOPED_TRACE(data.size());
            EXPECT_EQ(loadNvnmFailure(data), "NVNM ends inside a field");
        }
    }

    TEST(ESM4NavMeshTest, rejectsAFileThatEndsInsideTheSubRecord)
    {
        const std::string plugin = navmesh(subRecord("NVNM", wholeNvnm()));
        // the last index of the last segment is cut
        EXPECT_EQ(loadFailure(plugin, 2), "NVNM ends inside an array");
        // the indices of the last segment, and half of its count, are cut
        EXPECT_EQ(loadFailure(plugin, 6), "NVNM ends inside a field");
    }
}
