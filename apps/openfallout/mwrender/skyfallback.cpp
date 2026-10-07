#include "skyfallback.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>

#include <osg/Math>
#include <osg/Vec3f>

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
        // How many times the texture repeats from one side of the dome to the other
        constexpr float repeats = 3.f;

        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec2Array> coordinates = new osg::Vec2Array;
        osg::ref_ptr<osg::Vec4Array> colours = new osg::Vec4Array;

        const auto add = [&](const osg::Vec3f& direction, float alpha) {
            vertices->push_back(direction * radius);
            // Seen from above, a point of the dome is at its distance from the top in the plane, so that the clouds
            // thin out towards the horizon like a layer of them would.
            coordinates->push_back(
                osg::Vec2f(0.5f + direction.x() * repeats / 2.f, 0.5f + direction.y() * repeats / 2.f));
            colours->push_back(osg::Vec4f(0.f, 0.f, 0.f, alpha));
        };

        add(osg::Vec3f(0.f, 0.f, 1.f), 1.f);
        for (int ring = 0; ring < rings; ++ring)
        {
            const float elevation = osg::DegreesToRadians(elevations[ring]);
            for (int i = 0; i < segments; ++i)
            {
                const float angle = 2.f * osg::PIf * i / segments;
                add(osg::Vec3f(std::cos(elevation) * std::cos(angle), std::cos(elevation) * std::sin(angle),
                        std::sin(elevation)),
                    alphas[ring]);
            }
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
