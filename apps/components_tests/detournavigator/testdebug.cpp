#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "settings.hpp"

#include <components/detournavigator/agentbounds.hpp>
#include <components/detournavigator/areatype.hpp>
#include <components/detournavigator/asyncnavmeshupdater.hpp>
#include <components/detournavigator/changetype.hpp>
#include <components/detournavigator/debug.hpp>
#include <components/detournavigator/exceptions.hpp>
#include <components/detournavigator/flags.hpp>
#include <components/detournavigator/recastmesh.hpp>
#include <components/detournavigator/settings.hpp>
#include <components/detournavigator/status.hpp>
#include <components/detournavigator/tilespositionsrange.hpp>
#include <components/detournavigator/version.hpp>

#include <DetourNavMesh.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

// The text that the navigator writes about its values, and the files that it writes of its meshes
namespace DetourNavigator
{
    namespace
    {
        using namespace testing;

        template <class T>
        std::string text(const T& value)
        {
            std::ostringstream stream;
            stream << value;
            return stream.str();
        }

        std::string readFile(const std::string& path)
        {
            std::ifstream file(std::filesystem::path(path), std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }

        TEST(DetourNavigatorDebugTest, writesTheStatuses)
        {
            EXPECT_EQ(text(Status::Success), "DetourNavigator::Status::Success");
            EXPECT_EQ(text(Status::PartialPath), "DetourNavigator::Status::PartialPath");
            EXPECT_EQ(text(Status::NavMeshNotFound), "DetourNavigator::Status::NavMeshNotFound");
            EXPECT_EQ(text(Status::StartPolygonNotFound), "DetourNavigator::Status::StartPolygonNotFound");
            EXPECT_EQ(text(Status::EndPolygonNotFound), "DetourNavigator::Status::EndPolygonNotFound");
            EXPECT_EQ(text(Status::TargetPolygonNotFound), "DetourNavigator::Status::TargetPolygonNotFound");
            EXPECT_EQ(text(Status::MoveAlongSurfaceFailed), "DetourNavigator::Status::MoveAlongSurfaceFailed");
            EXPECT_EQ(text(Status::FindPathOverPolygonsFailed), "DetourNavigator::Status::FindPathOverPolygonsFailed");
            EXPECT_EQ(text(Status::InitNavMeshQueryFailed), "DetourNavigator::Status::InitNavMeshQueryFailed");
            EXPECT_EQ(text(Status::FindStraightPathFailed), "DetourNavigator::Status::FindStraightPathFailed");
            EXPECT_EQ(text(static_cast<Status>(99)), "DetourNavigator::Error::99");
        }

        TEST(DetourNavigatorDebugTest, describesTheStatuses)
        {
            EXPECT_STREQ(getMessage(Status::Success), "success");
            EXPECT_STREQ(getMessage(Status::PartialPath), "partial path is found");
            EXPECT_STREQ(getMessage(Status::NavMeshNotFound), "navmesh is not found");
            EXPECT_STREQ(
                getMessage(Status::StartPolygonNotFound), "polygon for start position is not found on navmesh");
            EXPECT_STREQ(getMessage(Status::EndPolygonNotFound), "polygon for end position is not found on navmesh");
            EXPECT_STREQ(
                getMessage(Status::TargetPolygonNotFound), "polygon for target position is not found on navmesh");
            EXPECT_STREQ(getMessage(Status::MoveAlongSurfaceFailed), "move along surface on navmesh is failed");
            EXPECT_STREQ(getMessage(Status::FindPathOverPolygonsFailed), "path over navmesh polygons is not found");
            EXPECT_STREQ(getMessage(Status::InitNavMeshQueryFailed), "failed to init navmesh query");
            EXPECT_STREQ(getMessage(Status::FindStraightPathFailed), "failed to straight path using polygon path");
            EXPECT_STREQ(getMessage(static_cast<Status>(99)), "unknown error");
        }

        TEST(DetourNavigatorDebugTest, writesTheKindsOfAreasFlagsAndChanges)
        {
            EXPECT_EQ(text(AreaType_null), "null");
            EXPECT_EQ(text(AreaType_water), "water");
            EXPECT_EQ(text(AreaType_door), "door");
            EXPECT_EQ(text(AreaType_pathgrid), "pathgrid");
            EXPECT_EQ(text(AreaType_ground), "ground");
            // the value of an area that is not known is written as the character of its byte
            EXPECT_EQ(text(static_cast<AreaType>('A')), "unknown area type (A)");

            EXPECT_EQ(text(Flag_none), "none");
            EXPECT_EQ(text(Flag_walk), "walk");
            EXPECT_EQ(text(Flag_swim), "swim");
            EXPECT_EQ(text(Flag_openDoor), "openDoor");
            EXPECT_EQ(text(Flag_usePathgrid), "usePathgrid");
            EXPECT_EQ(text(static_cast<Flag>(1 << 8)), "");

            EXPECT_EQ(text(WriteFlags{ Flag_none }), "none");
            EXPECT_EQ(text(WriteFlags{ Flag_walk }), "walk");
            EXPECT_EQ(text(WriteFlags{ Flag_swim | Flag_usePathgrid }), "swim | usePathgrid");
            EXPECT_EQ(text(WriteFlags{ Flag_walk | Flag_swim | Flag_openDoor | Flag_usePathgrid }),
                "walk | swim | openDoor | usePathgrid");

            EXPECT_EQ(text(ChangeType::remove), "ChangeType::remove");
            EXPECT_EQ(text(ChangeType::add), "ChangeType::add");
            EXPECT_EQ(text(ChangeType::update), "ChangeType::update");
            EXPECT_EQ(text(static_cast<ChangeType>(7)), "ChangeType::7");

            EXPECT_EQ(text(JobStatus::Done), "JobStatus::Done");
            EXPECT_EQ(text(JobStatus::Fail), "JobStatus::Fail");
            EXPECT_EQ(text(JobStatus::MemoryCacheMiss), "JobStatus::MemoryCacheMiss");
            EXPECT_EQ(text(static_cast<JobStatus>(9)), "JobStatus::9");
        }

        TEST(DetourNavigatorDebugTest, writesTheStatusesOfDetour)
        {
            EXPECT_EQ(text(WriteDtStatus{ 0 }), "[None] (0x0)");
            EXPECT_EQ(text(WriteDtStatus{ DT_SUCCESS }), "DT_SUCCESS (0x40000000)");
            EXPECT_EQ(
                text(WriteDtStatus{ DT_FAILURE | DT_INVALID_PARAM }), "DT_FAILURE | DT_INVALID_PARAM (0x80000008)");
            EXPECT_EQ(
                text(WriteDtStatus{ DT_SUCCESS | DT_PARTIAL_RESULT }), "DT_SUCCESS | DT_PARTIAL_RESULT (0x40000040)");
        }

        TEST(DetourNavigatorDebugTest, writesTheShapesOfTheAgentsAndTheSettings)
        {
            EXPECT_EQ(text(CollisionShapeType::Aabb), "CollisionShapeType::Aabb");
            EXPECT_EQ(text(CollisionShapeType::RotatingBox), "CollisionShapeType::RotatingBox");
            EXPECT_EQ(text(CollisionShapeType::Cylinder), "CollisionShapeType::Cylinder");
            // the value of a shape that is not known is written as the character of its byte
            EXPECT_EQ(text(static_cast<CollisionShapeType>('B')), "CollisionShapeType::B");
            EXPECT_EQ(text(AgentBounds{ CollisionShapeType::Cylinder, osg::Vec3f(1, 2, 3) }),
                "AgentBounds {CollisionShapeType::Cylinder, {1, 2, 3}}");

            EXPECT_EQ(text(Version{ 3, 4 }), "Version {3, 4}");
            EXPECT_EQ(text(AreaCosts{ 1.5f, 2.5f, 3.5f, 4.5f }),
                "AreaCosts {.mWater = 1.5, .mDoor = 2.5, .mPathgrid = 3.5, .mGround = 4.5}");
            EXPECT_EQ(text(DetourSettings{ 1, 2, 3, 4 }),
                "DetourSettings {.mMaxPolys = 1, .mMaxNavMeshQueryNodes = 2, .mMaxPolygonPathSize = 3, "
                ".mMaxSmoothPathSize = 4}");
        }

        TEST(DetourNavigatorDebugTest, writesTheBoundsTheWaterAndTheHeightfields)
        {
            EXPECT_THAT(text(TileBounds{ osg::Vec2f(1, 2), osg::Vec2f(3, 4) }),
                AllOf(StartsWith("TileBounds {"), EndsWith("}")));
            EXPECT_THAT(text(TilesPositionsRange{ TilePosition(1, 2), TilePosition(3, 4) }),
                AllOf(StartsWith("TilesPositionsRange {"), EndsWith("}")));

            EXPECT_EQ(text(Water{ 8192, 1.5f }), "Water {8192, 1.5}");
            EXPECT_THAT(text(CellWater{ osg::Vec2i(1, 2), Water{ 8192, 1.5f } }),
                AllOf(StartsWith("CellWater {"), EndsWith(", Water {8192, 1.5}}")));
            EXPECT_THAT(text(FlatHeightfield{ osg::Vec2i(1, 2), 8192, 1.5f }),
                AllOf(StartsWith("FlatHeightfield {"), EndsWith(", 8192, 1.5}")));

            const Heightfield heightfield{ osg::Vec2i(1, 2), 8192, 3, 1.f, 2.f, { 1.f, 2.f, 1.5f }, 9, 0, 0 };
            EXPECT_THAT(text(heightfield),
                AllOf(StartsWith("Heightfield {.mCellPosition="), HasSubstr(".mCellSize=8192"), HasSubstr(".mLength=3"),
                    HasSubstr(".mMinHeight=1"), HasSubstr(".mMaxHeight=2"), HasSubstr(".mHeights={1, 2, 1.5, }"),
                    EndsWith(".mOriginalSize=9}")));
        }

        struct DetourNavigatorDebugFilesTest : Test
        {
            const std::string mPrefix = testing::TempDir() + "openfallout_detournavigator_debug_test_";
            const Settings mSettings = Tests::makeSettings();

            ~DetourNavigatorDebugFilesTest()
            {
                std::error_code error;
                std::filesystem::remove(mPrefix + "recastmesh_r1.obj", error);
                std::filesystem::remove(mPrefix + "all_tiles_navmesh_r2.bin", error);
            }
        };

        TEST_F(DetourNavigatorDebugFilesTest, writesTheRecastMeshAsAnObjFile)
        {
            const RecastMesh recastMesh(Version{ 1, 2 },
                Mesh({ 0, 1, 2, 0, 2, 3 }, { 0, 0, 0, 10, 0, 0, 10, 20, 0, 0, 20, 0 },
                    { AreaType_ground, AreaType_water }),
                {}, {}, {}, {});

            writeToFile(recastMesh, mPrefix, "_r1", mSettings.mRecast);

            const std::string content = readFile(mPrefix + "recastmesh_r1.obj");
            std::vector<std::string> lines;
            std::istringstream stream(content);
            for (std::string line; std::getline(stream, line);)
                lines.push_back(line);
            // four vertices and two faces
            ASSERT_EQ(lines.size(), 6u);
            for (std::size_t i = 0; i < 4; ++i)
                EXPECT_THAT(lines[i], StartsWith("v "));
            EXPECT_EQ(lines[4], "f 1 2 3");
            EXPECT_EQ(lines[5], "f 1 3 4");
        }

        TEST_F(DetourNavigatorDebugFilesTest, writesTheHeaderOfANavMeshWithoutTiles)
        {
            const std::unique_ptr<dtNavMesh, decltype(&dtFreeNavMesh)> navMesh(dtAllocNavMesh(), &dtFreeNavMesh);
            dtNavMeshParams params{};
            params.tileWidth = 64;
            params.tileHeight = 64;
            params.maxTiles = 4;
            params.maxPolys = 64;
            ASSERT_TRUE(dtStatusSucceed(navMesh->init(&params)));

            writeToFile(*navMesh, mPrefix, "_r2");

            const std::string content = readFile(mPrefix + "all_tiles_navmesh_r2.bin");
            // the magic MSET, the version, the number of tiles and the parameters, and no tiles
            ASSERT_EQ(content.size(), 3 * sizeof(int) + sizeof(dtNavMeshParams));
            int header[3];
            std::copy_n(content.data(), sizeof(header), reinterpret_cast<char*>(header));
            EXPECT_EQ(header[0], 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T');
            EXPECT_EQ(header[1], 1);
            EXPECT_EQ(header[2], 0);
        }

        TEST_F(DetourNavigatorDebugFilesTest, failsToWriteIntoADirectoryThatIsNotThere)
        {
            const std::string nowhere = mPrefix + "nowhere/";
            const RecastMesh recastMesh(Version{ 1, 2 }, Mesh({}, {}, {}), {}, {}, {}, {});
            EXPECT_THROW(writeToFile(recastMesh, nowhere, "", mSettings.mRecast), std::system_error);

            const std::unique_ptr<dtNavMesh, decltype(&dtFreeNavMesh)> navMesh(dtAllocNavMesh(), &dtFreeNavMesh);
            EXPECT_THROW(writeToFile(*navMesh, nowhere, ""), std::system_error);
        }

        TEST(DetourNavigatorMeshTest, refusesAMeshWhoseAreasDoNotMatchItsTriangles)
        {
            EXPECT_THROW(Mesh({ 0, 1, 2 }, { 0, 0, 0, 1, 0, 0, 1, 1, 0 }, {}), InvalidArgument);
            EXPECT_THROW(Mesh({ 0, 1, 2 }, { 0, 0, 0, 1, 0, 0, 1, 1, 0 }, { AreaType_ground, AreaType_ground }),
                InvalidArgument);
        }
    }
}
