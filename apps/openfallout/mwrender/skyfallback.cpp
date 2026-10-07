#include "skyfallback.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>

#include <osg/Math>
#include <osg/Vec3f>

#include <components/misc/strings/algorithm.hpp>
#include <components/sceneutil/texmat.hpp>

namespace OFRender
{
    namespace
    {
        // Degrees above the horizon of the rings of the dome. The alpha rises fast near the horizon, so there are more
        // rings there.
        constexpr std::array<float, 10> sRingElevations = { 0.f, 5.f, 10.f, 15.f, 20.f, 30.f, 40.f, 50.f, 60.f, 75.f };

        constexpr int sSegments = 32;

        // Where the generated sky is when the far plane of the camera is further away than that
        constexpr float sFullRadius = 1500.f;

        float smoothstep(float low, float high, float value)
        {
            const float t = std::clamp((value - low) / (high - low), 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        }

        /// A square image of white pixels with the alpha that alphaAt gives for the distance from the centre, in half
        /// the width of the image (1 at the middle of an edge, a bit more than 1.4 in a corner).
        template <class Alpha>
        osg::ref_ptr<osg::Image> createWhiteImage(int size, Alpha alphaAt)
        {
            osg::ref_ptr<osg::Image> image = new osg::Image;
            image->allocateImage(size, size, 1, GL_RGBA, GL_UNSIGNED_BYTE);
            image->setInternalTextureFormat(GL_RGBA8);
            const float half = size / 2.f;
            for (int y = 0; y < size; ++y)
            {
                std::uint8_t* row = image->data(0, y);
                for (int x = 0; x < size; ++x)
                {
                    const float dx = (x + 0.5f - half) / half;
                    const float dy = (y + 0.5f - half) / half;
                    const float alpha = std::clamp(alphaAt(std::sqrt(dx * dx + dy * dy)), 0.f, 1.f);
                    std::uint8_t* pixel = row + x * 4;
                    pixel[0] = pixel[1] = pixel[2] = 255;
                    pixel[3] = static_cast<std::uint8_t>(std::lround(alpha * 255.f));
                }
            }
            return image;
        }
    }

    float generatedSkyRadius(float viewDistance)
    {
        return std::min(sFullRadius, 0.9f * viewDistance);
    }

    float atmosphereAlpha(float elevation)
    {
        const float up = std::sin(std::clamp(elevation, 0.f, osg::PIf / 2.f));
        return 1.f - (1.f - up) * (1.f - up);
    }

    osg::ref_ptr<osg::Geometry> createAtmosphereDome(float radius)
    {
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec4Array> colours = new osg::Vec4Array;

        for (const float degrees : sRingElevations)
        {
            const float elevation = osg::DegreesToRadians(degrees);
            const float alpha = atmosphereAlpha(elevation);
            for (int i = 0; i < sSegments; ++i)
            {
                const float angle = 2.f * osg::PIf * i / sSegments;
                vertices->push_back(osg::Vec3f(radius * std::cos(elevation) * std::cos(angle),
                    radius * std::cos(elevation) * std::sin(angle), radius * std::sin(elevation)));
                colours->push_back(osg::Vec4f(0.f, 0.f, 0.f, alpha));
            }
        }
        const int apex = static_cast<int>(vertices->size());
        vertices->push_back(osg::Vec3f(0.f, 0.f, radius));
        colours->push_back(osg::Vec4f(0.f, 0.f, 0.f, 1.f));

        osg::ref_ptr<osg::DrawElementsUShort> triangles = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES);
        const int rings = static_cast<int>(sRingElevations.size());
        for (int ring = 0; ring < rings; ++ring)
        {
            for (int i = 0; i < sSegments; ++i)
            {
                const int next = (i + 1) % sSegments;
                const int low = ring * sSegments;
                if (ring + 1 < rings)
                {
                    const int high = (ring + 1) * sSegments;
                    triangles->push_back(low + i);
                    triangles->push_back(low + next);
                    triangles->push_back(high + i);
                    triangles->push_back(low + next);
                    triangles->push_back(high + next);
                    triangles->push_back(high + i);
                }
                else
                {
                    triangles->push_back(low + i);
                    triangles->push_back(low + next);
                    triangles->push_back(apex);
                }
            }
        }

        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setName("Sky Dome");
        geometry->setVertexArray(vertices);
        geometry->setColorArray(colours, osg::Array::BIND_PER_VERTEX);
        geometry->addPrimitiveSet(triangles);
        // The shader looks up nothing but the alpha, there is no texture, no normal and no lighting to set up, and the
        // dome is seen from the inside.
        geometry->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        return geometry;
    }

    osg::ref_ptr<osg::Geometry> createCloudDome(float radius)
    {
        constexpr int rings = 4;
        constexpr int segments = 16;
        // The angle above the horizon of each ring, from the top, and the alpha it has in the mesh of Morrowind
        constexpr std::array<float, rings> elevations = { 70.f, 45.f, 25.f, 5.f };
        constexpr std::array<float, rings> alphas = { 1.f, 1.f, 0.25098f, 0.f };
        // How many times the texture repeats along the distance of one radian, seen from the middle, straight above
        constexpr float repeatsPerRadian = 1.5f;

        // Like the mesh of Morrowind, the clouds are one flat disc over the viewer, not a dome: the rings are where
        // the lines of sight at those angles meet the disc, and the farthest one is at the radius. The texture lies on
        // the disc without distortion, so the view of it is in perspective, the clouds are squeezed together towards
        // the horizon. (On a dome whose texture coordinates were the ones of the plane under it, a cloud near the
        // horizon was drawn up to 11 times taller than wide, as streaks.)
        const float height = radius * std::sin(osg::DegreesToRadians(elevations[rings - 1]));

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec2Array> coordinates = new osg::Vec2Array;
        osg::ref_ptr<osg::Vec4Array> colours = new osg::Vec4Array;

        const auto add = [&](float distance, float angle, float alpha) {
            const float x = distance * std::cos(angle);
            const float y = distance * std::sin(angle);
            vertices->push_back(osg::Vec3f(x, y, height));
            coordinates->push_back(
                osg::Vec2f(0.5f + x / height * repeatsPerRadian, 0.5f + y / height * repeatsPerRadian));
            colours->push_back(osg::Vec4f(0.f, 0.f, 0.f, alpha));
        };

        add(0.f, 0.f, 1.f);
        for (int ring = 0; ring < rings; ++ring)
        {
            const float distance = height / std::tan(osg::DegreesToRadians(elevations[ring]));
            for (int i = 0; i < segments; ++i)
                add(distance, 2.f * osg::PIf * i / segments, alphas[ring]);
        }

        osg::ref_ptr<osg::DrawElementsUShort> triangles = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES);
        for (int i = 0; i < segments; ++i)
        {
            const int next = (i + 1) % segments;
            triangles->push_back(0);
            triangles->push_back(1 + i);
            triangles->push_back(1 + next);
        }
        for (int ring = 0; ring + 1 < rings; ++ring)
        {
            for (int i = 0; i < segments; ++i)
            {
                const int next = (i + 1) % segments;
                const int high = 1 + ring * segments;
                const int low = high + segments;
                triangles->push_back(high + i);
                triangles->push_back(low + i);
                triangles->push_back(high + next);
                triangles->push_back(high + next);
                triangles->push_back(low + i);
                triangles->push_back(low + next);
            }
        }

        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setName("Sky Clouds");
        geometry->setVertexArray(vertices);
        geometry->setTexCoordArray(0, coordinates, osg::Array::BIND_PER_VERTEX);
        geometry->setColorArray(colours, osg::Array::BIND_PER_VERTEX);
        geometry->addPrimitiveSet(triangles);
        geometry->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        return geometry;
    }

    osg::ref_ptr<osg::Geometry> createHorizonBand(float radius)
    {
        constexpr int segments = 48;
        constexpr int rows = 4;
        // The strip of the games is two bands in an image four times as wide as one band is tall. Round the sky it
        // repeats four times, which puts it at a height of 22 degrees.
        constexpr float topElevation = 22.f;
        constexpr float repeatsAround = 4.f;

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec2Array> coordinates = new osg::Vec2Array;
        osg::ref_ptr<osg::Vec4Array> colours = new osg::Vec4Array;
        for (int row = 0; row <= rows; ++row)
        {
            const float fraction = static_cast<float>(row) / rows;
            const float elevation = osg::DegreesToRadians(topElevation * fraction);
            for (int i = 0; i <= segments; ++i)
            {
                const float turn = static_cast<float>(i) / segments;
                const float angle = 2.f * osg::PIf * turn;
                vertices->push_back(osg::Vec3f(std::cos(elevation) * std::cos(angle),
                                        std::cos(elevation) * std::sin(angle), std::sin(elevation))
                    * radius);
                // The first band of the image has its top at the top of the band and its bottom at the horizon.
                coordinates->push_back(osg::Vec2f(turn * repeatsAround, 0.5f * (1.f - fraction)));
                colours->push_back(osg::Vec4f(0.f, 0.f, 0.f, 1.f));
            }
        }

        osg::ref_ptr<osg::DrawElementsUShort> triangles = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES);
        for (int row = 0; row < rows; ++row)
        {
            for (int i = 0; i < segments; ++i)
            {
                const int low = row * (segments + 1) + i;
                const int high = low + segments + 1;
                triangles->push_back(low);
                triangles->push_back(low + 1);
                triangles->push_back(high);
                triangles->push_back(low + 1);
                triangles->push_back(high + 1);
                triangles->push_back(high);
            }
        }

        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setName("Sky Horizon Clouds");
        geometry->setVertexArray(vertices);
        geometry->setTexCoordArray(0, coordinates, osg::Array::BIND_PER_VERTEX);
        geometry->setColorArray(colours, osg::Array::BIND_PER_VERTEX);
        geometry->addPrimitiveSet(triangles);
        osg::StateSet* stateset = geometry->getOrCreateStateSet();
        stateset->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        // The clouds of the layer drift along the texture, which is up the band here: this one stays where it is
        SceneUtil::setupTexMatForStateSet(*stateset, 0, osg::Matrixf{});
        return geometry;
    }

    bool isHorizonCloudTexture(std::string_view path)
    {
        const std::size_t slash = path.find_last_of("/\\");
        const std::string_view file = slash == std::string_view::npos ? path : path.substr(slash + 1);
        constexpr std::string_view word = "horizon";
        for (std::size_t i = 0; i + word.size() <= file.size(); ++i)
            if (Misc::StringUtils::ciEqual(file.substr(i, word.size()), word))
                return true;
        return false;
    }

    osg::ref_ptr<osg::Group> createCloudLayer(float radius)
    {
        osg::ref_ptr<osg::Group> layer = new osg::Group;
        layer->addChild(createCloudDome(radius));
        layer->addChild(createHorizonBand(radius));
        selectCloudShape(*layer, {});
        return layer;
    }

    void selectCloudShape(osg::Group& layer, std::string_view texture)
    {
        const bool horizon = isHorizonCloudTexture(texture);
        layer.getChild(0)->setNodeMask(horizon ? 0u : ~0u);
        layer.getChild(1)->setNodeMask(horizon ? ~0u : 0u);
    }

    osg::ref_ptr<osg::Geometry> createStarField(float radius, int count)
    {
        // The angle across a star, in the brightest and the faintest
        constexpr float sizeLow = 0.22f;
        constexpr float sizeHigh = 0.5f;

        std::mt19937 random(0x57A125);
        std::uniform_real_distribution<float> unit(0.f, 1.f);

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec2Array> coordinates = new osg::Vec2Array;
        osg::ref_ptr<osg::Vec4Array> colours = new osg::Vec4Array;
        osg::ref_ptr<osg::DrawElementsUShort> quads = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES);

        for (int i = 0; i < count; ++i)
        {
            // Uniform on the sphere, but not below a little under the horizon
            const float z = -0.1f + 1.1f * unit(random);
            const float angle = 2.f * osg::PIf * unit(random);
            const float ring = std::sqrt(1.f - z * z);
            const osg::Vec3f direction(ring * std::cos(angle), ring * std::sin(angle), z);

            osg::Vec3f first = direction ^ osg::Vec3f(0.f, 0.f, 1.f);
            if (first.length2() < 1e-6f)
                first = osg::Vec3f(1.f, 0.f, 0.f);
            first.normalize();
            const osg::Vec3f second = direction ^ first;

            // The fainter stars are the many
            const float brightness = 0.35f + 0.65f * std::pow(unit(random), 3.f);
            const float half
                = radius * std::tan(osg::DegreesToRadians(sizeLow + (sizeHigh - sizeLow) * brightness) / 2.f);

            const unsigned short base = static_cast<unsigned short>(vertices->size());
            const osg::Vec3f centre = direction * radius;
            vertices->push_back(centre - first * half - second * half);
            vertices->push_back(centre + first * half - second * half);
            vertices->push_back(centre + first * half + second * half);
            vertices->push_back(centre - first * half + second * half);
            coordinates->push_back(osg::Vec2f(0.f, 0.f));
            coordinates->push_back(osg::Vec2f(1.f, 0.f));
            coordinates->push_back(osg::Vec2f(1.f, 1.f));
            coordinates->push_back(osg::Vec2f(0.f, 1.f));
            for (int corner = 0; corner < 4; ++corner)
                colours->push_back(osg::Vec4f(0.f, 0.f, 0.f, brightness));
            for (const unsigned short index : { 0, 1, 2, 0, 2, 3 })
                quads->push_back(base + index);
        }

        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        geometry->setName("Sky Stars");
        geometry->setVertexArray(vertices);
        geometry->setTexCoordArray(0, coordinates, osg::Array::BIND_PER_VERTEX);
        geometry->setColorArray(colours, osg::Array::BIND_PER_VERTEX);
        geometry->addPrimitiveSet(quads);
        geometry->getOrCreateStateSet()->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        return geometry;
    }

    osg::ref_ptr<osg::Image> createStarImage(int size)
    {
        return createWhiteImage(
            size, [](float distance) { return std::pow(1.f - smoothstep(0.f, 1.f, distance), 2.f); });
    }

    osg::ref_ptr<osg::Image> createTransparentImage()
    {
        return createWhiteImage(1, [](float) { return 0.f; });
    }

    osg::ref_ptr<osg::Image> createSunImage(int size)
    {
        return createWhiteImage(size, [](float distance) {
            // A disc of about a ninth of the width of the picture, and a glow of the same colour around it.
            const float disc = 1.f - smoothstep(0.10f, 0.13f, distance);
            const float glow = 0.4f * std::pow(1.f - smoothstep(0.f, 1.f, distance), 4.f);
            return std::max(disc, glow);
        });
    }

    osg::ref_ptr<osg::Image> createSunFlashImage(int size)
    {
        return createWhiteImage(size, [](float distance) {
            const float falloff = 1.f - smoothstep(0.f, 1.f, distance);
            return 0.6f * falloff * falloff * falloff;
        });
    }
}
