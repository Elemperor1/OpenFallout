#include "skyfallback.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include <osg/Math>

namespace OFRender
{
    namespace
    {
        // Degrees above the horizon of the rings of the dome. The alpha rises fast near the horizon, so there are more
        // rings there.
        constexpr std::array<float, 10> sRingElevations = { 0.f, 5.f, 10.f, 15.f, 20.f, 30.f, 40.f, 50.f, 60.f, 75.f };

        constexpr int sSegments = 32;

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
