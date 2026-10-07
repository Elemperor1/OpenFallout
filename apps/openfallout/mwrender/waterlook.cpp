#include "waterlook.hpp"

#include <algorithm>

#include <components/esm4/loadwatr.hpp>

namespace OFRender
{
    namespace
    {
        osg::Vec3f toColour(const ESM4::Water::Colour& colour)
        {
            return osg::Vec3f(colour.mRed, colour.mGreen, colour.mBlue) / 255.f;
        }
    }

    WaterLook WaterLook::standard()
    {
        return { .mShallowColour = osg::Vec3f(0.090195f, 0.115685f, 0.12745f),
            .mDeepColour = osg::Vec3f(0.090195f, 0.115685f, 0.12745f),
            .mOpacity = 0.f,
            .mReflectivity = 1.f };
    }

    std::optional<WaterLook> makeWaterLook(const ESM4::Water& water)
    {
        const std::optional<ESM4::Water::Appearance> appearance = water.appearance();
        if (!appearance)
            return std::nullopt;

        return WaterLook{ .mShallowColour = toColour(appearance->mShallow),
            .mDeepColour = toColour(appearance->mDeep),
            .mOpacity = std::clamp(water.mOpacity / 100.f, 0.f, 1.f),
            .mReflectivity = std::clamp(appearance->mReflectivity, 0.f, 1.f) };
    }
}
