#include <apps/openfallout/mwrender/skyfallback.hpp>
#include <apps/openfallout/mwrender/skyutil.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <osg/Math>

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
}
