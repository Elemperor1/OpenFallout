#ifndef OPENFALLOUT_MWRENDER_WATERLOOK_H
#define OPENFALLOUT_MWRENDER_WATERLOOK_H

#include <optional>

#include <osg/Vec3f>

namespace ESM4
{
    struct Water;
}

namespace OFRender
{
    /// What the shader of the water needs to know of a kind of water to draw it: the colours it is over the ground and
    /// far from it, how much of the ground it hides, and how much of the sky and the land around it shows in it.
    struct WaterLook
    {
        /// The colour over the ground under shallow water, and over the ground under deep water and where there is none
        /// to see, both before the light of the sun and sky is taken into account
        osg::Vec3f mShallowColour;
        osg::Vec3f mDeepColour;
        /// How much of the colour of the water (shallow to deep) is over the ground that shows through it, from 0 (the
        /// ground as it is) to 1 (the colour only)
        float mOpacity = 0.f;
        /// How much of the reflection of the sky and the land is taken where the water reflects, from 0 to 1
        float mReflectivity = 1.f;

        /// The look of the water of Morrowind, with no colour over the ground and the whole of the reflection. The
        /// deep colour is the one the shader has always had for deep water, see WATER_COLOR in fog.glsl.
        static WaterLook standard();

        /// The one colour of the water that is drawn without the shader, which has no depth to take the colour from:
        /// halfway between the colour of shallow and of deep water.
        osg::Vec3f simpleColour() const { return (mShallowColour + mDeepColour) * 0.5f; }

        bool operator==(const WaterLook& other) const = default;
    };

    /// The look of a kind of water of Fallout 3 or New Vegas: the colours of its settings, its opacity (a percentage in
    /// the record) and its reflectivity, each limited to the range that the shader works with. Nothing when the record
    /// has no settings to take them from.
    std::optional<WaterLook> makeWaterLook(const ESM4::Water& water);
}

#endif
