#include <components/esm4/loadnavi.hpp>
#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string formIds(const std::vector<std::uint32_t>& ids)
    {
        std::string data;
        append<std::uint32_t>(data, static_cast<std::uint32_t>(ids.size()));
        for (const std::uint32_t id : ids)
            append(data, id);
        return data;
    }

    // The common start of a NVMI subrecord of Oblivion, up to the doors that are linked
    std::string navMeshInfoStart(std::uint32_t form, std::uint32_t flags)
    {
        std::string data;
        append(data, form);
        append(data, flags);
        for (const float coordinate : { 100.f, 200.f, 300.f })
            append(data, coordinate);
        append<std::uint32_t>(data, 1); // flag of the preferred merges
        data += formIds({ 0x00000901, 0x00000902 }); // merged
        data += formIds({ 0x00000903 }); // preferred merges
        append<std::uint32_t>(data, 2); // linked doors
        for (const std::uint32_t door : { 0x00000904u, 0x00000905u })
        {
            append<std::uint32_t>(data, 77);
            append(data, door);
        }
        return data;
    }

    std::string islandInfo()
    {
        std::string data;
        for (const float coordinate : { -1.f, -2.f, -3.f, 1.f, 2.f, 3.f })
            append(data, coordinate);
        append<std::uint32_t>(data, 2); // triangles
        for (const std::uint16_t index : { 0, 1, 2, 2, 1, 3 })
            append(data, index);
        append<std::uint32_t>(data, 4); // vertices
        for (const float coordinate : { 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 5.f })
            append(data, coordinate);
        return data;
    }

    ESM4::Navigation loadNavigation(const std::string& data, std::uint32_t modIndex = 0)
    {
        const std::vector<ESM4::Navigation> result
            = loadRecords<ESM4::Navigation>("NAVI", record("NAVI", 0x801, data), 0, nullptr, modIndex);
        EXPECT_EQ(result.size(), 1u);
        return result.empty() ? ESM4::Navigation{} : result[0];
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadNavigation(data);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    // A NVPP subrecord: the total of chunks, which has ten added, a chunk of a node with two paths, one of the same
    // node with another two, the separator with a last node, and ten pairs of a node and an index
    std::string preferredPaths(std::uint32_t total = 12)
    {
        std::string data;
        append<std::uint32_t>(data, total);
        append<std::uint32_t>(data, 1);
        append<std::uint32_t>(data, 0x100);
        data += formIds({ 0x0000000A, 0x0000000B });
        data += formIds({ 0x0000000C, 0x0000000D });
        append<std::uint32_t>(data, 1);
        append<std::uint32_t>(data, 0x200);
        append<std::uint32_t>(data, 10);
        for (std::uint32_t i = 0; i < 10; ++i)
        {
            append<std::uint32_t>(data, 0x300 + i);
            append<std::uint32_t>(data, 40 + i);
        }
        return data;
    }

    TEST(ESM4NavigationTest, readsTheEditorIdAndTheVersionOfTheNavigationData)
    {
        const ESM4::Navigation navigation
            = loadNavigation(zString("EDID", "NaviEdid") + valueSubRecord<std::uint32_t>("NVER", 12)
                + subRecord("NVSI", bytePattern(8, 1)) + subRecord("NVCI", bytePattern(8, 1)));

        EXPECT_EQ(navigation.mEditorId, "NaviEdid");
        EXPECT_TRUE(navigation.mNavMeshInfo.empty());
    }

    TEST(ESM4NavigationTest, readsTheNavMeshesOfACellOfTheWorldWithTheirDoorsAndMerges)
    {
        std::string data = navMeshInfoStart(0x00000801, 0x20);
        append<std::uint8_t>(data, 1); // an island
        data += islandInfo();
        append<std::uint32_t>(data, 0x55); // location marker
        append<std::uint32_t>(data, 0x00000a01); // the world space is not Skyrim's or Morrowind's
        append<std::uint32_t>(data, 0x00000a02); // the cell

        const ESM4::Navigation navigation = loadNavigation(subRecord("NVMI", data));

        ASSERT_EQ(navigation.mNavMeshInfo.size(), 1u);
        const ESM4::Navigation::NavMeshInfo& info = navigation.mNavMeshInfo[0];
        EXPECT_EQ(info.formId.toUint32(), 0x801u);
        EXPECT_EQ(info.flags, 0x20u);
        EXPECT_EQ(info.x, 100.f);
        EXPECT_EQ(info.y, 200.f);
        EXPECT_EQ(info.z, 300.f);
        EXPECT_EQ(info.flagPrefMerges, 1u);
        ASSERT_EQ(info.formIdMerged.size(), 2u);
        EXPECT_EQ(info.formIdMerged[1].toUint32(), 0x902u);
        ASSERT_EQ(info.formIdPrefMerged.size(), 1u);
        EXPECT_EQ(info.formIdPrefMerged[0].toUint32(), 0x903u);
        ASSERT_EQ(info.linkedDoors.size(), 2u);
        EXPECT_EQ(info.linkedDoors[0].unknown, 77u);
        EXPECT_EQ(info.linkedDoors[1].formId, 0x905u);
        ASSERT_EQ(info.islandInfo.size(), 1u);
        EXPECT_EQ(info.islandInfo[0].minX, -1.f);
        EXPECT_EQ(info.islandInfo[0].maxZ, 3.f);
        ASSERT_EQ(info.islandInfo[0].triangles.size(), 2u);
        EXPECT_EQ(info.islandInfo[0].triangles[1].vertexIndex2, 3);
        ASSERT_EQ(info.islandInfo[0].verticies.size(), 4u);
        EXPECT_EQ(info.islandInfo[0].verticies[3].z, 5.f);
        EXPECT_EQ(info.locationMarker, 0x55u);
        EXPECT_EQ(info.worldSpaceId.toUint32(), 0xa01u);
        ASSERT_TRUE(std::holds_alternative<ESM::FormId>(info.cellGrid));
        EXPECT_EQ(std::get<ESM::FormId>(info.cellGrid).toUint32(), 0xa02u);
    }

    TEST(ESM4NavigationTest, readsTheGridOfACellInTheWorldOfSkyrimBackwards)
    {
        std::string data = navMeshInfoStart(0x00000802, 0x40);
        append<std::uint8_t>(data, 0); // no island
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0x0000003c); // Tamriel
        append<std::int16_t>(data, -7); // y, then x
        append<std::int16_t>(data, 12);

        const ESM4::Navigation navigation = loadNavigation(subRecord("NVMI", data));

        ASSERT_EQ(navigation.mNavMeshInfo.size(), 1u);
        EXPECT_TRUE(navigation.mNavMeshInfo[0].islandInfo.empty());
        ASSERT_TRUE(std::holds_alternative<ESM4::Grid>(navigation.mNavMeshInfo[0].cellGrid));
        EXPECT_EQ(std::get<ESM4::Grid>(navigation.mNavMeshInfo[0].cellGrid).x, 12);
        EXPECT_EQ(std::get<ESM4::Grid>(navigation.mNavMeshInfo[0].cellGrid).y, -7);
    }

    TEST(ESM4NavigationTest, readsTheGridOfACellInTheWorldOfMorrowindToo)
    {
        std::string data;
        append<std::uint32_t>(data, 0x00000803);
        append<std::uint32_t>(data, 0);
        for (const float coordinate : { 1.f, 2.f, 3.f })
            append(data, coordinate);
        append<std::uint32_t>(data, 0);
        data += formIds({});
        data += formIds({});
        data += formIds({});
        append<std::uint8_t>(data, 0);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0x01380000); // the world space of Morrowind, in the plugin that is the second
        append<std::int16_t>(data, 4);
        append<std::int16_t>(data, 5);

        const ESM4::Navigation navigation = loadNavigation(subRecord("NVMI", data), 1);

        ASSERT_EQ(navigation.mNavMeshInfo.size(), 1u);
        ASSERT_TRUE(std::holds_alternative<ESM4::Grid>(navigation.mNavMeshInfo[0].cellGrid));
        EXPECT_EQ(std::get<ESM4::Grid>(navigation.mNavMeshInfo[0].cellGrid).y, 4);
        EXPECT_EQ(std::get<ESM4::Grid>(navigation.mNavMeshInfo[0].cellGrid).x, 5);
        EXPECT_TRUE(navigation.mNavMeshInfo[0].formIdMerged.empty());
        EXPECT_TRUE(navigation.mNavMeshInfo[0].linkedDoors.empty());
    }

    TEST(ESM4NavigationTest, collectsEveryNavMeshOfTheRecordInOrder)
    {
        std::string data;
        for (const std::uint32_t form : { 0x00000811u, 0x00000812u })
        {
            data += subRecord("NVMI", [&] {
                std::string nvmi = navMeshInfoStart(form, 0);
                append<std::uint8_t>(nvmi, 0);
                append<std::uint32_t>(nvmi, 0);
                append<std::uint32_t>(nvmi, 0x00000a01);
                append<std::uint32_t>(nvmi, 0x00000a02);
                return nvmi;
            }());
        }

        const ESM4::Navigation navigation = loadNavigation(data);

        ASSERT_EQ(navigation.mNavMeshInfo.size(), 2u);
        EXPECT_EQ(navigation.mNavMeshInfo[0].formId.toUint32(), 0x811u);
        EXPECT_EQ(navigation.mNavMeshInfo[1].formId.toUint32(), 0x812u);
    }

    TEST(ESM4NavigationTest, skipsTheNavMeshesOfTheGamesThatReadThemFromTheNavMeshRecords)
    {
        const std::string data = subRecord("NVMI", bytePattern(8, 1)) + zString("EDID", "AfterTheNavMeshInfo");

        const std::pair<float, int> games[]
            = { { 0.94f, 15 }, { 1.32f, 15 }, { 1.34f, 15 }, { 1.7f, 44 }, { 1.f, 131 } };
        for (const auto& [header, form] : games)
        {
            const ESM4::Navigation navigation
                = loadVersionedRecord<ESM4::Navigation>("NAVI", header, static_cast<std::uint16_t>(form), data);

            EXPECT_TRUE(navigation.mNavMeshInfo.empty()) << header;
            EXPECT_EQ(navigation.mEditorId, "AfterTheNavMeshInfo") << header;
        }
    }

    TEST(ESM4NavigationTest, readsThePreferredPathsOfNodes)
    {
        const ESM4::Navigation navigation = loadNavigation(subRecord("NVPP", preferredPaths()));

        ASSERT_EQ(navigation.mPreferredPaths.size(), 3u);
        EXPECT_EQ(navigation.mPreferredPaths[0].first, 0x100u);
        ASSERT_EQ(navigation.mPreferredPaths[0].second.size(), 2u);
        EXPECT_EQ(navigation.mPreferredPaths[0].second[0].toUint32(), 0xAu);
        EXPECT_EQ(navigation.mPreferredPaths[0].second[1].toUint32(), 0xBu);
        // the next chunk has no node of its own and takes the one before
        EXPECT_EQ(navigation.mPreferredPaths[1].first, 0x100u);
        ASSERT_EQ(navigation.mPreferredPaths[1].second.size(), 2u);
        EXPECT_EQ(navigation.mPreferredPaths[1].second[1].toUint32(), 0xDu);
        // the separator has a node with no paths
        EXPECT_EQ(navigation.mPreferredPaths[2].first, 0x200u);
        EXPECT_TRUE(navigation.mPreferredPaths[2].second.empty());
        ASSERT_EQ(navigation.mPathIndexMap.size(), 10u);
        EXPECT_EQ(navigation.mPathIndexMap.at(ESM::FormId::fromUint32(0x300)), 40u);
        EXPECT_EQ(navigation.mPathIndexMap.at(ESM::FormId::fromUint32(0x309)), 49u);
    }

    TEST(ESM4NavigationTest, takesAnEmptyPreferredPathSubrecordAsOneWithoutNodes)
    {
        std::string data;
        append<std::uint32_t>(data, 0); // no chunks
        append<std::uint32_t>(data, 0);

        const ESM4::Navigation navigation = loadNavigation(subRecord("NVPP", data) + zString("EDID", "Next"));

        EXPECT_TRUE(navigation.mPreferredPaths.empty());
        EXPECT_EQ(navigation.mEditorId, "Next");
    }

    TEST(ESM4NavigationTest, doesNotReadPreferredPathsThatAreMalformed)
    {
        EXPECT_THAT(loadFailure(subRecord("NVPP", preferredPaths(5))), HasSubstr("chunks >= 0xa"));

        // the first chunk has no node
        std::string noNode;
        append<std::uint32_t>(noNode, 11);
        append<std::uint32_t>(noNode, 2);
        noNode += bytePattern(8, 1);
        EXPECT_THAT(loadFailure(subRecord("NVPP", noNode)), HasSubstr("node not found"));

        // the separator is not 1, and the number of pairs is not 10
        std::string badSeparator = preferredPaths();
        badSeparator.replace(4 + 4 + 4 + 4 + 8 + 4 + 8, 4, std::string("\x02\0\0\0", 4));
        EXPECT_THAT(loadFailure(subRecord("NVPP", badSeparator)), HasSubstr("expected separator"));
        std::string badPairs = preferredPaths();
        badPairs.replace(4 + 4 + 4 + 4 + 8 + 4 + 8 + 4 + 4, 4, std::string("\x09\0\0\0", 4));
        EXPECT_THAT(loadFailure(subRecord("NVPP", badPairs)), HasSubstr("expected 0xa"));
    }

    TEST(ESM4NavigationTest, skipsThePreferredPathsOfFallout4)
    {
        const ESM4::Navigation navigation = loadVersionedRecord<ESM4::Navigation>(
            "NAVI", 0.95f, 131, subRecord("NVPP", preferredPaths(5)) + zString("EDID", "Next"));

        EXPECT_TRUE(navigation.mPreferredPaths.empty());
        EXPECT_EQ(navigation.mEditorId, "Next");
    }

    TEST(ESM4NavigationTest, doesNotReadASubrecordItDoesNotKnow)
    {
        EXPECT_THAT(loadFailure(subRecord("XXYZ", "")), HasSubstr("Unknown subrecord XXYZ"));
    }
}
