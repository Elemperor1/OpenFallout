#include <apps/openfallout/mwrender/skyfallback.hpp>
#include <apps/openfallout/mwrender/skyutil.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <osg/Math>
#include <osg/Matrixf>
#include <osg/Uniform>

#include <components/testing/util.hpp>
#include <components/vfs/manager.hpp>

namespace
{
    using namespace testing;
    using namespace OFRender;

    TEST(OpenFalloutRenderSkyFallbackAtmosphereAlpha, isTransparentAtTheHorizonAndOpaqueOverhead)
    {
        EXPECT_FLOAT_EQ(atmosphereAlpha(0.f), 0.f);
        EXPECT_FLOAT_EQ(atmosphereAlpha(osg::PIf / 2.f), 1.f);
    }

    TEST(OpenFalloutRenderSkyFallbackAtmosphereAlpha, risesWithTheAngleAboveTheHorizon)
    {
        float last = -1.f;
        // Straight up it is 1 for a float to the last digits, so only the angles before that rise for certain.
        for (int degrees = 0; degrees <= 80; ++degrees)
        {
            const float alpha = atmosphereAlpha(osg::DegreesToRadians(static_cast<float>(degrees)));
            EXPECT_GT(alpha, last) << degrees;
            last = alpha;
        }
        EXPECT_GE(atmosphereAlpha(osg::DegreesToRadians(89.f)), last);
    }

    TEST(OpenFalloutRenderSkyFallbackAtmosphereAlpha, staysWithinZeroAndOneForAnglesOutsideTheDome)
    {
        EXPECT_FLOAT_EQ(atmosphereAlpha(-1.f), 0.f);
        EXPECT_FLOAT_EQ(atmosphereAlpha(3.f), 1.f);
    }

    TEST(OpenFalloutRenderSkyFallbackRadius, isTheFullRadiusWhenTheViewDistanceIsFarEnough)
    {
        EXPECT_FLOAT_EQ(generatedSkyRadius(7168.f), 1500.f);
        EXPECT_FLOAT_EQ(generatedSkyRadius(81920.f), 1500.f);
    }

    TEST(OpenFalloutRenderSkyFallbackRadius, staysInsideTheFarPlaneOfAShortViewDistance)
    {
        for (const float viewDistance : { 10.f, 500.f, 1000.f, 1600.f })
        {
            EXPECT_LT(generatedSkyRadius(viewDistance), viewDistance) << viewDistance;
            EXPECT_GT(generatedSkyRadius(viewDistance), 0.5f * viewDistance) << viewDistance;
        }
    }

    TEST(OpenFalloutRenderSkyFallbackDome, hasAllItsVerticesOnASphereOfTheRadiusAboveTheHorizon)
    {
        const osg::ref_ptr<osg::Geometry> dome = createAtmosphereDome(1500.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(dome->getVertexArray());
        ASSERT_NE(vertices, nullptr);
        ASSERT_FALSE(vertices->empty());
        for (const osg::Vec3f& vertex : *vertices)
        {
            EXPECT_NEAR(vertex.length(), 1500.f, 0.01f);
            EXPECT_GE(vertex.z(), -0.01f);
        }
    }

    TEST(OpenFalloutRenderSkyFallbackDome, hasTheAlphaOfItsAngleAtEveryVertexAndNoColour)
    {
        const osg::ref_ptr<osg::Geometry> dome = createAtmosphereDome(100.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(dome->getVertexArray());
        const auto* colours = dynamic_cast<const osg::Vec4Array*>(dome->getColorArray());
        ASSERT_NE(vertices, nullptr);
        ASSERT_NE(colours, nullptr);
        ASSERT_EQ(colours->size(), vertices->size());
        EXPECT_EQ(dome->getColorBinding(), osg::Array::BIND_PER_VERTEX);
        for (std::size_t i = 0; i < vertices->size(); ++i)
        {
            const float elevation = std::asin(std::clamp((*vertices)[i].z() / 100.f, 0.f, 1.f));
            EXPECT_NEAR((*colours)[i].a(), atmosphereAlpha(elevation), 1e-4f) << i;
            EXPECT_EQ((*colours)[i].r(), 0.f);
            EXPECT_EQ((*colours)[i].g(), 0.f);
            EXPECT_EQ((*colours)[i].b(), 0.f);
        }
    }

    TEST(OpenFalloutRenderSkyFallbackDome, hasTheHorizonTransparentAndTheTopOpaque)
    {
        const osg::ref_ptr<osg::Geometry> dome = createAtmosphereDome(100.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(dome->getVertexArray());
        const auto* colours = dynamic_cast<const osg::Vec4Array*>(dome->getColorArray());
        float lowest = 1.f;
        float highest = 0.f;
        for (std::size_t i = 0; i < vertices->size(); ++i)
        {
            if (std::abs((*vertices)[i].z()) < 0.001f)
            {
                EXPECT_EQ((*colours)[i].a(), 0.f);
            }
            lowest = std::min(lowest, (*colours)[i].a());
            highest = std::max(highest, (*colours)[i].a());
        }
        EXPECT_EQ(lowest, 0.f);
        EXPECT_EQ(highest, 1.f);
        EXPECT_EQ(colours->back().a(), 1.f); // the top
    }

    TEST(OpenFalloutRenderSkyFallbackDome, hasOnlyTrianglesOfItsVerticesAndNoGaps)
    {
        const osg::ref_ptr<osg::Geometry> dome = createAtmosphereDome(100.f);
        const std::size_t count = dome->getVertexArray()->getNumElements();
        ASSERT_EQ(dome->getNumPrimitiveSets(), 1u);
        const auto* triangles = dynamic_cast<const osg::DrawElementsUShort*>(dome->getPrimitiveSet(0));
        ASSERT_NE(triangles, nullptr);
        EXPECT_EQ(triangles->getMode(), static_cast<GLenum>(osg::PrimitiveSet::TRIANGLES));
        EXPECT_EQ(triangles->size() % 3, 0u);
        std::vector<bool> used(count, false);
        for (const unsigned short index : *triangles)
        {
            ASSERT_LT(index, count);
            used[index] = true;
        }
        EXPECT_EQ(std::count(used.begin(), used.end(), false), 0);
    }

    TEST(OpenFalloutRenderSkyFallbackDome, hasNoFaceCullingBecauseItIsSeenFromTheInside)
    {
        const osg::ref_ptr<osg::Geometry> dome = createAtmosphereDome(100.f);
        ASSERT_NE(dome->getStateSet(), nullptr);
        EXPECT_EQ(dome->getStateSet()->getMode(GL_CULL_FACE) & osg::StateAttribute::ON, 0u);
    }

    // The alpha of the pixel in the column x of the row y.
    unsigned char alphaAt(const osg::Image& image, int x, int y)
    {
        return image.data(x, y)[3];
    }

    TEST(OpenFalloutRenderSkyFallbackSun, isASquareImageOfWhitePixels)
    {
        const osg::ref_ptr<osg::Image> image = createSunImage(64);
        EXPECT_EQ(image->s(), 64);
        EXPECT_EQ(image->t(), 64);
        EXPECT_EQ(image->getPixelFormat(), static_cast<GLenum>(GL_RGBA));
        EXPECT_EQ(image->getDataType(), static_cast<GLenum>(GL_UNSIGNED_BYTE));
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                for (int channel = 0; channel < 3; ++channel)
                    ASSERT_EQ(image->data(x, y)[channel], 255) << x << "," << y;
    }

    TEST(OpenFalloutRenderSkyFallbackSun, hasADiscInTheMiddleAndNothingAtTheEdges)
    {
        const osg::ref_ptr<osg::Image> image = createSunImage(128);
        EXPECT_EQ(alphaAt(*image, 64, 64), 255);
        EXPECT_EQ(alphaAt(*image, 63, 63), 255);
        for (int i = 0; i < 128; ++i)
        {
            EXPECT_EQ(alphaAt(*image, i, 0), 0) << "top " << i;
            EXPECT_EQ(alphaAt(*image, i, 127), 0) << "bottom " << i;
            EXPECT_EQ(alphaAt(*image, 0, i), 0) << "left " << i;
            EXPECT_EQ(alphaAt(*image, 127, i), 0) << "right " << i;
        }
    }

    TEST(OpenFalloutRenderSkyFallbackSun, fadesOutwardsFromTheCentre)
    {
        const osg::ref_ptr<osg::Image> image = createSunImage(128);
        for (int x = 64; x < 127; ++x)
            EXPECT_GE(alphaAt(*image, x, 64), alphaAt(*image, x + 1, 64)) << x;
        // A glow around the disc: pixels outside of it are not empty.
        EXPECT_GT(alphaAt(*image, 64 + 20, 64), 0);
    }

    TEST(OpenFalloutRenderSkyFallbackSun, isTheSameInAllDirections)
    {
        const osg::ref_ptr<osg::Image> image = createSunImage(128);
        for (int offset = 0; offset < 60; ++offset)
        {
            const int value = alphaAt(*image, 64 + offset, 64);
            EXPECT_EQ(alphaAt(*image, 63 - offset, 64), value) << offset;
            EXPECT_EQ(alphaAt(*image, 64, 64 + offset), value) << offset;
            EXPECT_EQ(alphaAt(*image, 64, 63 - offset), value) << offset;
        }
    }

    TEST(OpenFalloutRenderSkyFallbackSunFlash, isAGlowThatFadesToNothingAtTheEdge)
    {
        const osg::ref_ptr<osg::Image> image = createSunFlashImage(128);
        EXPECT_GT(alphaAt(*image, 64, 64), 100);
        EXPECT_LT(alphaAt(*image, 64, 64), 255);
        for (int x = 64; x < 127; ++x)
            EXPECT_GE(alphaAt(*image, x, 64), alphaAt(*image, x + 1, 64)) << x;
        EXPECT_EQ(alphaAt(*image, 127, 64), 0);
        EXPECT_EQ(alphaAt(*image, 0, 0), 0);
    }

    class OpenFalloutRenderMoonFiles : public Test
    {
    protected:
        static constexpr std::array<const char*, 8> sPhases
            = { "new", "one_wax", "half_wax", "three_wax", "one_wan", "half_wan", "three_wan", "full" };

        /// The files of a moon, without the ones in the list, in a game.
        std::unique_ptr<VFS::Manager> makeVfs(const std::vector<std::string>& moons, const std::string& without = {})
        {
            VFS::FileMap files;
            for (const std::string& moon : moons)
            {
                std::vector<std::string> names = { "textures/tx_mooncircle_full_" + moon.substr(0, 1) + ".dds" };
                for (const char* phase : sPhases)
                    names.push_back(
                        "textures/tx_" + std::string(moon == "m" ? "masser" : "secunda") + "_" + phase + ".dds");
                for (const std::string& name : names)
                {
                    if (name != without)
                        mNames.push_back(name);
                }
            }
            for (const std::string& name : mNames)
                files.emplace(VFS::Path::Normalized(name), &mFile);
            return TestingOpenMW::createTestVFS(std::move(files));
        }

        TestingOpenMW::VFSTestFile mFile{ "" };
        std::vector<std::string> mNames;
    };

    TEST_F(OpenFalloutRenderMoonFiles, isFalseWhenTheGameHasNoMoonFiles)
    {
        const auto vfs = makeVfs({});
        EXPECT_FALSE(Moon::hasFiles(*vfs, Moon::Type_Masser));
        EXPECT_FALSE(Moon::hasFiles(*vfs, Moon::Type_Secunda));
    }

    TEST_F(OpenFalloutRenderMoonFiles, isTrueForTheMoonsThatTheGameHasFilesFor)
    {
        const auto vfs = makeVfs({ "m", "s" });
        EXPECT_TRUE(Moon::hasFiles(*vfs, Moon::Type_Masser));
        EXPECT_TRUE(Moon::hasFiles(*vfs, Moon::Type_Secunda));
    }

    TEST_F(OpenFalloutRenderMoonFiles, judgesEachMoonOnItsOwnFiles)
    {
        const auto vfs = makeVfs({ "m" });
        EXPECT_TRUE(Moon::hasFiles(*vfs, Moon::Type_Masser));
        EXPECT_FALSE(Moon::hasFiles(*vfs, Moon::Type_Secunda));
    }

    TEST_F(OpenFalloutRenderMoonFiles, isFalseWhenAPhaseIsMissing)
    {
        const auto vfs = makeVfs({ "m", "s" }, "textures/tx_secunda_three_wan.dds");
        EXPECT_TRUE(Moon::hasFiles(*vfs, Moon::Type_Masser));
        EXPECT_FALSE(Moon::hasFiles(*vfs, Moon::Type_Secunda));
    }

    TEST_F(OpenFalloutRenderMoonFiles, isFalseWhenTheCircleIsMissing)
    {
        const auto vfs = makeVfs({ "m", "s" }, "textures/tx_mooncircle_full_m.dds");
        EXPECT_FALSE(Moon::hasFiles(*vfs, Moon::Type_Masser));
        EXPECT_TRUE(Moon::hasFiles(*vfs, Moon::Type_Secunda));
    }

    // The layout of the cloud mesh of Morrowind, which the sky manager gives the alpha of its vertices by their index:
    // a vertex at the middle, then four rings of 16, the last two with the alpha 0.25 and 0.
    TEST(OpenFalloutRenderSkyFallbackClouds, hasTheVerticesOfTheCloudMeshOfMorrowind)
    {
        const osg::ref_ptr<osg::Geometry> clouds = createCloudDome(1000.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(clouds->getVertexArray());
        ASSERT_NE(vertices, nullptr);
        ASSERT_EQ(vertices->size(), 65u);
        EXPECT_NEAR((*vertices)[0].x(), 0.f, 1e-3f);
        EXPECT_NEAR((*vertices)[0].y(), 0.f, 1e-3f);
        // every ring is farther from the middle than the one before
        for (std::size_t i = 1; i + 16 < vertices->size(); ++i)
        {
            const osg::Vec2f near((*vertices)[i].x(), (*vertices)[i].y());
            const osg::Vec2f far((*vertices)[i + 16].x(), (*vertices)[i + 16].y());
            EXPECT_LT(near.length(), far.length()) << i;
        }
    }

    TEST(OpenFalloutRenderSkyFallbackClouds, isOneFlatDiscWithItsFarthestRingAtTheRadius)
    {
        const osg::ref_ptr<osg::Geometry> clouds = createCloudDome(1000.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(clouds->getVertexArray());
        ASSERT_NE(vertices, nullptr);
        ASSERT_EQ(vertices->size(), 65u);
        const float height = (*vertices)[0].z();
        EXPECT_NEAR(height, 1000.f * std::sin(osg::DegreesToRadians(5.f)), 1e-2f);
        for (const osg::Vec3f& vertex : *vertices)
            EXPECT_NEAR(vertex.z(), height, 1e-3f);
        // the rings are on the lines of sight at 70, 45, 25 and 5 degrees above the horizon
        const std::array<float, 4> degrees = { 70.f, 45.f, 25.f, 5.f };
        for (std::size_t ring = 0; ring < degrees.size(); ++ring)
        {
            for (std::size_t i = 0; i < 16; ++i)
            {
                const osg::Vec3f& vertex = (*vertices)[1 + ring * 16 + i];
                const float distance = std::hypot(vertex.x(), vertex.y());
                EXPECT_NEAR(osg::RadiansToDegrees(std::atan2(vertex.z(), distance)), degrees[ring], 1e-2f)
                    << ring << "," << i;
            }
        }
        const osg::Vec3f& farthest = (*vertices)[1 + 3 * 16];
        EXPECT_NEAR(farthest.length(), 1000.f, 1e-2f);
    }

    TEST(OpenFalloutRenderSkyFallbackClouds, hasTheAlphaOfTheCloudMeshOfMorrowindAtEveryIndex)
    {
        const osg::ref_ptr<osg::Geometry> clouds = createCloudDome(1000.f);
        const auto* colours = dynamic_cast<const osg::Vec4Array*>(clouds->getColorArray());
        ASSERT_NE(colours, nullptr);
        ASSERT_EQ(colours->size(), 65u);
        for (std::size_t i = 0; i < colours->size(); ++i)
        {
            float expected = 1.f;
            if (i >= 49)
                expected = 0.f;
            else if (i >= 33)
                expected = 0.25098f;
            EXPECT_FLOAT_EQ((*colours)[i].a(), expected) << i;
        }
    }

    // A texture that is laid on the disc without being stretched in a direction: the coordinates are the same multiple
    // of the position at every vertex, so a cloud has the same shape on the disc wherever it is. (They were the ones of
    // the plane under a dome once, and the clouds near the horizon were up to 11 times taller than wide.)
    TEST(OpenFalloutRenderSkyFallbackClouds, laysTheTextureOnTheDiscWithoutStretchingIt)
    {
        const osg::ref_ptr<osg::Geometry> clouds = createCloudDome(1000.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(clouds->getVertexArray());
        const auto* coordinates = dynamic_cast<const osg::Vec2Array*>(clouds->getTexCoordArray(0));
        ASSERT_NE(coordinates, nullptr);
        ASSERT_EQ(coordinates->size(), vertices->size());
        EXPECT_NEAR((*coordinates)[0].x(), 0.5f, 1e-4f);
        EXPECT_NEAR((*coordinates)[0].y(), 0.5f, 1e-4f);
        // straight above, the texture repeats one and a half times in a radian of the sky
        const float height = (*vertices)[0].z();
        const float scale = 1.5f / height;
        for (std::size_t i = 1; i < vertices->size(); ++i)
        {
            EXPECT_NEAR((*coordinates)[i].x() - 0.5f, (*vertices)[i].x() * scale, 1e-3f) << i;
            EXPECT_NEAR((*coordinates)[i].y() - 0.5f, (*vertices)[i].y() * scale, 1e-3f) << i;
        }
    }

    TEST(OpenFalloutRenderSkyFallbackClouds, hasOnlyTrianglesOfItsVertices)
    {
        const osg::ref_ptr<osg::Geometry> clouds = createCloudDome(1000.f);
        const auto* triangles = dynamic_cast<const osg::DrawElementsUShort*>(clouds->getPrimitiveSet(0));
        ASSERT_NE(triangles, nullptr);
        EXPECT_EQ(triangles->size() % 3, 0u);
        std::vector<bool> used(65, false);
        for (const unsigned short index : *triangles)
        {
            ASSERT_LT(index, 65);
            used[index] = true;
        }
        EXPECT_EQ(std::count(used.begin(), used.end(), false), 0);
    }

    TEST(OpenFalloutRenderSkyFallbackHorizonBand, isARingOnTheSkyFromTheHorizonToAboutTwentyTwoDegrees)
    {
        const osg::ref_ptr<osg::Geometry> band = createHorizonBand(1000.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(band->getVertexArray());
        ASSERT_NE(vertices, nullptr);
        ASSERT_FALSE(vertices->empty());
        float lowest = 90.f;
        float highest = 0.f;
        for (const osg::Vec3f& vertex : *vertices)
        {
            EXPECT_NEAR(vertex.length(), 1000.f, 0.01f);
            const float degrees = osg::RadiansToDegrees(std::asin(vertex.z() / 1000.f));
            lowest = std::min(lowest, degrees);
            highest = std::max(highest, degrees);
        }
        EXPECT_NEAR(lowest, 0.f, 0.01f);
        EXPECT_NEAR(highest, 22.f, 0.01f);
    }

    TEST(OpenFalloutRenderSkyFallbackHorizonBand, putsTheFirstBandOfTheStripFourTimesAroundTheSky)
    {
        const osg::ref_ptr<osg::Geometry> band = createHorizonBand(1000.f);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(band->getVertexArray());
        const auto* coordinates = dynamic_cast<const osg::Vec2Array*>(band->getTexCoordArray(0));
        ASSERT_NE(coordinates, nullptr);
        ASSERT_EQ(coordinates->size(), vertices->size());
        float widest = 0.f;
        for (std::size_t i = 0; i < vertices->size(); ++i)
        {
            EXPECT_GE((*coordinates)[i].x(), 0.f) << i;
            EXPECT_LE((*coordinates)[i].x(), 4.f + 1e-4f) << i;
            // the top of the band is the top of the image and the horizon the middle of it (the strip has two bands)
            const float degrees = osg::RadiansToDegrees(std::asin((*vertices)[i].z() / 1000.f));
            EXPECT_NEAR((*coordinates)[i].y(), 0.5f * (1.f - degrees / 22.f), 1e-3f) << i;
            widest = std::max(widest, (*coordinates)[i].x());
        }
        EXPECT_NEAR(widest, 4.f, 1e-4f);
    }

    TEST(OpenFalloutRenderSkyFallbackHorizonBand, hasOnlyTrianglesOfItsVerticesAndAnOpaqueAlpha)
    {
        const osg::ref_ptr<osg::Geometry> band = createHorizonBand(1000.f);
        const std::size_t count = band->getVertexArray()->getNumElements();
        const auto* triangles = dynamic_cast<const osg::DrawElementsUShort*>(band->getPrimitiveSet(0));
        const auto* colours = dynamic_cast<const osg::Vec4Array*>(band->getColorArray());
        ASSERT_NE(triangles, nullptr);
        ASSERT_NE(colours, nullptr);
        EXPECT_EQ(triangles->size() % 3, 0u);
        std::vector<bool> used(count, false);
        for (const unsigned short index : *triangles)
        {
            ASSERT_LT(index, count);
            used[index] = true;
        }
        EXPECT_EQ(std::count(used.begin(), used.end(), false), 0);
        for (const osg::Vec4f& colour : *colours)
            EXPECT_EQ(colour.a(), 1.f);
    }

    TEST(OpenFalloutRenderSkyFallbackHorizonBand, doesNotDriftWithTheClouds)
    {
        // The layer of clouds moves its texture along one axis, which is up and down the band: the band sets the
        // matrix of its own, which is the identity
        const osg::ref_ptr<osg::Geometry> band = createHorizonBand(1000.f);
        ASSERT_NE(band->getStateSet(), nullptr);
        const osg::Uniform* matrix = band->getStateSet()->getUniform("texMat0");
        ASSERT_NE(matrix, nullptr);
        osg::Matrixf value;
        value(3, 0) = 1.f;
        ASSERT_TRUE(matrix->get(value));
        EXPECT_EQ(value, osg::Matrixf());
    }

    TEST(OpenFalloutRenderSkyFallbackHorizonTexture, isAFileThatIsNamedAHorizonStrip)
    {
        EXPECT_TRUE(isHorizonCloudTexture("sky\\WastelandCloudHorizon01.dds"));
        EXPECT_TRUE(isHorizonCloudTexture("textures/sky/wastelandcloudhorizon02.dds"));
        EXPECT_TRUE(isHorizonCloudTexture("HORIZON.dds"));
    }

    TEST(OpenFalloutRenderSkyFallbackHorizonTexture, isNotAFileOfAnotherNameInADirectoryOfThatName)
    {
        EXPECT_FALSE(isHorizonCloudTexture({}));
        EXPECT_FALSE(isHorizonCloudTexture("sky\\NVCloudlight.dds"));
        EXPECT_FALSE(isHorizonCloudTexture("sky\\WastelandCloudCloudyUpper01.dds"));
        EXPECT_FALSE(isHorizonCloudTexture("sky\\horizon\\clouds.dds"));
    }

    TEST(OpenFalloutRenderSkyFallbackCloudLayer, showsTheDiscUntilThereIsAHorizonStrip)
    {
        const osg::ref_ptr<osg::Group> layer = createCloudLayer(1000.f);
        ASSERT_EQ(layer->getNumChildren(), 2u);
        const auto shown
            = [&] { return std::pair(layer->getChild(0)->getNodeMask() != 0, layer->getChild(1)->getNodeMask() != 0); };
        EXPECT_EQ(shown(), std::pair(true, false));
        selectCloudShape(*layer, "sky\\WastelandCloudHorizon01.dds");
        EXPECT_EQ(shown(), std::pair(false, true));
        selectCloudShape(*layer, "sky\\WastelandCloudCloudyUpper01.dds");
        EXPECT_EQ(shown(), std::pair(true, false));
        selectCloudShape(*layer, "sky\\WastelandCloudHorizon01.dds");
        selectCloudShape(*layer, {});
        EXPECT_EQ(shown(), std::pair(true, false));
    }

    TEST(OpenFalloutRenderSkyFallbackStars, hasAQuadForEachStar)
    {
        const osg::ref_ptr<osg::Geometry> stars = createStarField(1000.f, 50);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(stars->getVertexArray());
        const auto* coordinates = dynamic_cast<const osg::Vec2Array*>(stars->getTexCoordArray(0));
        const auto* colours = dynamic_cast<const osg::Vec4Array*>(stars->getColorArray());
        const auto* triangles = dynamic_cast<const osg::DrawElementsUShort*>(stars->getPrimitiveSet(0));
        ASSERT_NE(vertices, nullptr);
        ASSERT_NE(coordinates, nullptr);
        ASSERT_NE(colours, nullptr);
        ASSERT_NE(triangles, nullptr);
        EXPECT_EQ(vertices->size(), 200u);
        EXPECT_EQ(coordinates->size(), 200u);
        EXPECT_EQ(colours->size(), 200u);
        EXPECT_EQ(triangles->size(), 300u);
        for (const unsigned short index : *triangles)
            ASSERT_LT(index, 200);
    }

    TEST(OpenFalloutRenderSkyFallbackStars, hasEveryStarFacingTheOriginAtTheRadiusAndAboveTheHorizon)
    {
        const osg::ref_ptr<osg::Geometry> stars = createStarField(1000.f, 100);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(stars->getVertexArray());
        ASSERT_NE(vertices, nullptr);
        for (std::size_t star = 0; star < vertices->size() / 4; ++star)
        {
            const osg::Vec3f centre = ((*vertices)[star * 4] + (*vertices)[star * 4 + 1] + (*vertices)[star * 4 + 2]
                                          + (*vertices)[star * 4 + 3])
                / 4.f;
            EXPECT_NEAR(centre.length(), 1000.f, 0.5f) << star;
            EXPECT_GE(centre.z(), -0.11f * 1000.f) << star;
            // the corners are in the plane that faces the origin
            for (std::size_t corner = 0; corner < 4; ++corner)
            {
                const osg::Vec3f offset = (*vertices)[star * 4 + corner] - centre;
                EXPECT_NEAR(offset * centre, 0.f, 0.5f) << star;
                EXPECT_GT(offset.length(), 0.f);
                EXPECT_LT(offset.length(), 10.f);
            }
        }
    }

    TEST(OpenFalloutRenderSkyFallbackStars, hasBrightnessesBetweenAThirdAndOneAndNoColour)
    {
        const osg::ref_ptr<osg::Geometry> stars = createStarField(1000.f, 100);
        const auto* colours = dynamic_cast<const osg::Vec4Array*>(stars->getColorArray());
        float lowest = 1.f;
        float highest = 0.f;
        for (const osg::Vec4f& colour : *colours)
        {
            EXPECT_EQ(colour.r(), 0.f);
            lowest = std::min(lowest, colour.a());
            highest = std::max(highest, colour.a());
        }
        EXPECT_GE(lowest, 0.34f);
        EXPECT_LE(highest, 1.f);
        EXPECT_GT(highest - lowest, 0.2f);
    }

    TEST(OpenFalloutRenderSkyFallbackStars, isTheSameSkyEveryTime)
    {
        const osg::ref_ptr<osg::Geometry> first = createStarField(1000.f, 100);
        const osg::ref_ptr<osg::Geometry> second = createStarField(1000.f, 100);
        const auto* one = dynamic_cast<const osg::Vec3Array*>(first->getVertexArray());
        const auto* other = dynamic_cast<const osg::Vec3Array*>(second->getVertexArray());
        ASSERT_EQ(one->size(), other->size());
        for (std::size_t i = 0; i < one->size(); ++i)
            EXPECT_EQ((*one)[i], (*other)[i]) << i;
    }

    TEST(OpenFalloutRenderSkyFallbackStarImage, isADotThatFadesToTheEdge)
    {
        const osg::ref_ptr<osg::Image> image = createStarImage(16);
        EXPECT_EQ(image->s(), 16);
        EXPECT_GE(alphaAt(*image, 8, 8), 200);
        EXPECT_EQ(alphaAt(*image, 0, 0), 0);
        EXPECT_EQ(alphaAt(*image, 15, 8), 0);
        for (int x = 8; x < 15; ++x)
            EXPECT_GE(alphaAt(*image, x, 8), alphaAt(*image, x + 1, 8)) << x;
    }

    TEST(OpenFalloutRenderSkyFallbackTransparentImage, isOnePixelThatShowsNothing)
    {
        const osg::ref_ptr<osg::Image> image = createTransparentImage();
        EXPECT_EQ(image->s(), 1);
        EXPECT_EQ(image->t(), 1);
        EXPECT_EQ(alphaAt(*image, 0, 0), 0);
    }
}
