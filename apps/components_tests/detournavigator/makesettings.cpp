#include <components/debug/debuglog.hpp>
#include <components/detournavigator/settings.hpp>
#include <components/misc/constants.hpp>
#include <components/settings/values.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <chrono>

// The navigation mesh gets its limits from the settings, and a tile and a polygon must fit together in 22 bits.
namespace
{
    using namespace DetourNavigator;

    struct DetourNavigatorMakeSettingsTest : testing::Test
    {
        ~DetourNavigatorMakeSettingsTest()
        {
            ::Settings::navigator().mMaxPolygonsPerTile.reset();
            ::Settings::navigator().mMaxTilesNumber.reset();
        }
    };

    TEST_F(DetourNavigatorMakeSettingsTest, copiesTheValuesOfTheGameSettings)
    {
        const auto& navigator = ::Settings::navigator();

        const DetourNavigator::Settings settings = makeSettingsFromSettingsManager(Debug::Verbose);

        EXPECT_EQ(settings.mRecast.mMaxLogLevel, Debug::Verbose);
        EXPECT_EQ(settings.mRecast.mBorderSize, navigator.mBorderSize.get());
        EXPECT_EQ(settings.mRecast.mCellHeight, navigator.mCellHeight.get());
        EXPECT_EQ(settings.mRecast.mCellSize, navigator.mCellSize.get());
        EXPECT_EQ(settings.mRecast.mDetailSampleDist, navigator.mDetailSampleDist.get());
        EXPECT_EQ(settings.mRecast.mDetailSampleMaxError, navigator.mDetailSampleMaxError.get());
        EXPECT_EQ(settings.mRecast.mMaxClimb, Constants::sStepSizeUp);
        EXPECT_EQ(settings.mRecast.mMaxSlope, Constants::sMaxSlope);
        EXPECT_EQ(settings.mRecast.mMaxEdgeLen, navigator.mMaxEdgeLen.get());
        EXPECT_EQ(settings.mRecast.mMaxVertsPerPoly, navigator.mMaxVertsPerPoly.get());
        EXPECT_EQ(settings.mRecast.mRegionMergeArea, navigator.mRegionMergeArea.get());
        EXPECT_EQ(settings.mRecast.mRegionMinArea, navigator.mRegionMinArea.get());
        EXPECT_EQ(settings.mRecast.mTileSize, navigator.mTileSize.get());
        EXPECT_EQ(settings.mRecast.mSwimHeightScale, 0.f);
        EXPECT_EQ(settings.mDetour.mMaxNavMeshQueryNodes, navigator.mMaxNavMeshQueryNodes.get());
        EXPECT_EQ(settings.mDetour.mMaxPolygonPathSize, navigator.mMaxPolygonPathSize.get());
        EXPECT_EQ(settings.mDetour.mMaxSmoothPathSize, navigator.mMaxSmoothPathSize.get());
        EXPECT_EQ(settings.mWaitUntilMinDistanceToPlayer, navigator.mWaitUntilMinDistanceToPlayer.get());
        EXPECT_EQ(settings.mAsyncNavMeshUpdaterThreads, navigator.mAsyncNavMeshUpdaterThreads.get());
        EXPECT_EQ(settings.mMaxNavMeshTilesCacheSize, navigator.mMaxNavMeshTilesCacheSize.get());
        EXPECT_EQ(settings.mEnableWriteRecastMeshToFile, navigator.mEnableWriteRecastMeshToFile.get());
        EXPECT_EQ(settings.mEnableWriteNavMeshToFile, navigator.mEnableWriteNavMeshToFile.get());
        EXPECT_EQ(settings.mRecastMeshPathPrefix, navigator.mRecastMeshPathPrefix.get());
        EXPECT_EQ(settings.mNavMeshPathPrefix, navigator.mNavMeshPathPrefix.get());
        EXPECT_EQ(settings.mEnableRecastMeshFileNameRevision, navigator.mEnableRecastMeshFileNameRevision.get());
        EXPECT_EQ(settings.mEnableNavMeshFileNameRevision, navigator.mEnableNavMeshFileNameRevision.get());
        EXPECT_EQ(settings.mMinUpdateInterval, std::chrono::milliseconds(navigator.mMinUpdateIntervalMs.get()));
        EXPECT_EQ(settings.mEnableNavMeshDiskCache, navigator.mEnableNavMeshDiskCache.get());
        EXPECT_EQ(settings.mWriteToNavMeshDb, navigator.mWriteToNavmeshdb.get());
        EXPECT_EQ(settings.mMaxDbFileSize, navigator.mMaxNavmeshdbFileSize.get());
    }

    TEST_F(DetourNavigatorMakeSettingsTest, roundsThePolygonsOfATileUpToAPowerOfTwo)
    {
        ::Settings::navigator().mMaxPolygonsPerTile.set(1000);

        const DetourNavigator::Settings settings = makeSettingsFromSettingsManager(Debug::Error);

        EXPECT_EQ(settings.mDetour.mMaxPolys, 1024);
        EXPECT_EQ(std::popcount(static_cast<unsigned>(settings.mDetour.mMaxPolys)), 1);
    }

    TEST_F(DetourNavigatorMakeSettingsTest, theBitsOfATileAndItsPolygonsAddUpToTwentyTwo)
    {
        for (const int polygons : { 1, 2, 3, 512, 4096, 5000, 1 << 20, (1 << 21) })
        {
            ::Settings::navigator().mMaxPolygonsPerTile.set(polygons);
            ::Settings::navigator().mMaxTilesNumber.set(1 << 30);

            const DetourNavigator::Settings settings = makeSettingsFromSettingsManager(Debug::Error);

            EXPECT_EQ(settings.mDetour.mMaxPolys, static_cast<int>(std::bit_ceil(static_cast<unsigned>(polygons))))
                << polygons;
            EXPECT_EQ(static_cast<long long>(settings.mDetour.mMaxPolys) * settings.mMaxTilesNumber, 1ll << 22)
                << polygons;
        }
    }

    TEST_F(DetourNavigatorMakeSettingsTest, doesNotAskForMoreTilesThanTheIdentifierCanHold)
    {
        ::Settings::navigator().mMaxPolygonsPerTile.set(1 << 15);
        ::Settings::navigator().mMaxTilesNumber.set(100000);

        const DetourNavigator::Settings settings = makeSettingsFromSettingsManager(Debug::Error);

        EXPECT_EQ(settings.mMaxTilesNumber, 1 << 7);
    }

    TEST_F(DetourNavigatorMakeSettingsTest, keepsTheTilesTheSettingsAskForWhenTheyFit)
    {
        ::Settings::navigator().mMaxPolygonsPerTile.set(1 << 8);
        ::Settings::navigator().mMaxTilesNumber.set(300);

        const DetourNavigator::Settings settings = makeSettingsFromSettingsManager(Debug::Error);

        EXPECT_EQ(settings.mMaxTilesNumber, 300);
    }
}
