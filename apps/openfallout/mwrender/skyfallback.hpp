#ifndef OPENFALLOUT_MWRENDER_SKYFALLBACK_H
#define OPENFALLOUT_MWRENDER_SKYFALLBACK_H

#include <string_view>

#include <osg/Geometry>
#include <osg/Group>
#include <osg/Image>
#include <osg/ref_ptr>

namespace OFRender
{
    /// The files that the sky of Morrowind is made of (the dome, the clouds, the stars, the sun and the moons) are
    /// not in the data of other games. The sky manager builds these stand-ins in code when the game does not supply
    /// the file, so that its own file of the same name is used whenever there is one.

    /// The radius of the generated sky for a viewing distance: the distance at which the dome of the sky is meant to
    /// be, brought in to nine tenths of the far plane of the camera if that is nearer, because anything beyond the far
    /// plane is cut off and the sky would be gone. The sky is only seen from the inside, so a nearer dome looks the
    /// same.
    float generatedSkyRadius(float viewDistance);

    /// How much of the colour of the sky a point of the dome shows, and how much of the fog colour (which is the clear
    /// colour of the screen) shows through, at an angle above the horizon in radians: 0 at the horizon, 1 straight up.
    float atmosphereAlpha(float elevation);

    /// A dome of the given radius around the origin with the vertices of the sky colour at the alpha of
    /// atmosphereAlpha, as the sky shader expects them: the colour is the one of the weather, taken from a uniform, and
    /// only the alpha of each vertex counts.
    osg::ref_ptr<osg::Geometry> createAtmosphereDome(float radius);

    /// The disc that the clouds are drawn on, over the viewer, with a vertex for the middle and four rings of 16 around
    /// it, the farthest one at the radius from the origin: the layout of the cloud mesh of Morrowind, so that what the
    /// sky manager does to that mesh works on this one. The rings are where the lines of sight at 70, 45, 25 and 5
    /// degrees above the horizon meet the disc. The alpha of the vertices (0 in the farthest ring, a quarter in the
    /// one before, 1 in the rest) fades the clouds out towards the horizon, and the texture coordinates lay a texture
    /// that repeats on the disc as it is, so that it is seen in perspective, the clouds closer together the nearer the
    /// horizon.
    osg::ref_ptr<osg::Geometry> createCloudDome(float radius);

    /// The band that a strip of cloud for the horizon is drawn on: a ring of the radius around the origin from the
    /// horizon up to about 22 degrees, with the first half of the texture (a strip in the games is two bands stacked
    /// in one image, each four times as wide as it is tall) repeated four times round the sky. It does not drift.
    osg::ref_ptr<osg::Geometry> createHorizonBand(float radius);

    /// Whether the texture of the clouds of a weather is a strip for the horizon (the games call such a file
    /// *Horizon*) and not a layer of clouds over the whole sky.
    bool isHorizonCloudTexture(std::string_view path);

    /// The group of the two shapes that the clouds of a generated sky are drawn on, the disc of createCloudDome as the
    /// first child and the band of createHorizonBand as the second, with only the disc shown.
    osg::ref_ptr<osg::Group> createCloudLayer(float radius);

    /// Shows the shape that the texture needs in a layer of createCloudLayer, which is the band for a strip for the
    /// horizon and the disc for any other texture, or none.
    void selectCloudShape(osg::Group& layer, std::string_view texture);

    /// Stars as small squares of the given number around the origin at the radius, at random places above and a little
    /// below the horizon, each facing the origin. The same stars every time. A star has a brightness for the alpha of
    /// its vertices, and the texture coordinates map the image of createStarImage onto it.
    osg::ref_ptr<osg::Geometry> createStarField(float radius, int count);

    /// The disc of the sun, white with an alpha that is 1 in the disc and falls off in a glow around it, on a square
    /// image. The colour of the weather multiplies it.
    osg::ref_ptr<osg::Image> createSunImage(int size = 128);

    /// The picture of a star: a white dot that fades out to the edge of a square image.
    osg::ref_ptr<osg::Image> createStarImage(int size = 16);

    /// A single pixel that shows nothing, for a texture that is missing and must not show.
    osg::ref_ptr<osg::Image> createTransparentImage();

    /// The glow that the sun makes when the camera looks at it, white, a soft fall off from the centre to the edge of a
    /// square image.
    osg::ref_ptr<osg::Image> createSunFlashImage(int size = 128);
}

#endif
