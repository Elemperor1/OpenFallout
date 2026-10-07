#ifndef OPENFALLOUT_MWRENDER_SKYFALLBACK_H
#define OPENFALLOUT_MWRENDER_SKYFALLBACK_H

#include <osg/Geometry>
#include <osg/Image>
#include <osg/ref_ptr>

namespace OFRender
{
    /// The files that the sky of Morrowind is made of (the dome, the clouds, the stars, the sun and the moons) are
    /// not in the data of other games. The sky manager builds these stand-ins in code when the game does not supply
    /// the file, so that its own file of the same name is used whenever there is one.

    /// How much of the colour of the sky a point of the dome shows, and how much of the fog colour (which is the clear
    /// colour of the screen) shows through, at an angle above the horizon in radians: 0 at the horizon, 1 straight up.
    float atmosphereAlpha(float elevation);

    /// A dome of the given radius around the origin with the vertices of the sky colour at the alpha of
    /// atmosphereAlpha, as the sky shader expects them: the colour is the one of the weather, taken from a uniform, and
    /// only the alpha of each vertex counts.
    osg::ref_ptr<osg::Geometry> createAtmosphereDome(float radius);

    /// The disc of the sun, white with an alpha that is 1 in the disc and falls off in a glow around it, on a square
    /// image. The colour of the weather multiplies it.
    osg::ref_ptr<osg::Image> createSunImage(int size = 128);

    /// The glow that the sun makes when the camera looks at it, white, a soft fall off from the centre to the edge of a
    /// square image.
    osg::ref_ptr<osg::Image> createSunFlashImage(int size = 128);
}

#endif
