#ifndef OPENFALLOUT_MWRENDER_FOGMANAGER_H
#define OPENFALLOUT_MWRENDER_FOGMANAGER_H

#include <utility>

#include <osg/Vec4f>

namespace OFWorld
{
    class Cell;
}

namespace OFRender
{
    /// The fog range of a cell or a weather, in game units, made to end inside the view distance. Nothing is drawn
    /// beyond the view distance, so a fog that ends further away would show the edge of the world as a line; a range
    /// that ends beyond it is scaled down to end there, and one that ends inside is kept. The result is continuous in
    /// the end of the range, so that a weather that changes it does not make the fog jump.
    std::pair<float, float> fitFogRange(float fogNear, float fogFar, float viewDistance);

    class FogManager
    {
    public:
        FogManager();

        void configure(float viewDistance, const OFWorld::Cell& cell);
        void configure(float viewDistance, float fogDepth, float underwaterFog, float dlFactor, float dlOffset,
            const osg::Vec4f& color);
        /// Fog that starts and ends at given distances in game units, and ends at the view distance when it would end
        /// beyond it (see fitFogRange). The underwater fog is the share of the view distance that is not fogged under
        /// water, as for the fog of a density.
        void configureRange(
            float viewDistance, float fogNear, float fogFar, float underwaterFog, const osg::Vec4f& color);

        osg::Vec4f getFogColor(bool isUnderwater) const;
        float getFogStart(bool isUnderwater) const;
        float getFogEnd(bool isUnderwater) const;

    private:
        float mLandFogStart;
        float mLandFogEnd;
        float mUnderwaterFogStart;
        float mUnderwaterFogEnd;
        osg::Vec4f mFogColor;
        osg::Vec4f mUnderwaterColor;
        float mUnderwaterWeight;
        float mUnderwaterIndoorFog;
    };
}

#endif
