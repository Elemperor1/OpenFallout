#include "loadwatr.hpp"

#include "reader.hpp"
#include "recordreader.hpp"

#include <cmath>
#include <cstring>

namespace ESM4
{
    namespace
    {
        // Where the members of the settings are, in bytes: 16 bytes that are not known, then the sun power, the
        // reflectivity, the fresnel amount, 4 unused bytes, the near and far distance of the fog above the water, and
        // the colours (red, green, blue and an unused byte each).
        constexpr std::size_t sSunPower = 16;
        constexpr std::size_t sReflectivity = 20;
        constexpr std::size_t sFresnel = 24;
        constexpr std::size_t sShallowColour = 40;
        constexpr std::size_t sDeepColour = 44;
        constexpr std::size_t sReflectionColour = 48;
        constexpr std::size_t sAppearanceSize = 52;

        float readFloat(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            float value;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }

        Water::Colour readColour(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            return { bytes[offset], bytes[offset + 1], bytes[offset + 2] };
        }
    }

    std::optional<Water::Appearance> Water::appearance() const
    {
        // The settings of a record that has a DNAM are in it, the others have them before the damage in DATA
        const std::vector<std::uint8_t>& bytes = mVisualData.empty() ? mData : mVisualData;
        if (bytes.size() < sAppearanceSize)
            return std::nullopt;

        Appearance result;
        result.mShallow = readColour(bytes, sShallowColour);
        result.mDeep = readColour(bytes, sDeepColour);
        result.mReflection = readColour(bytes, sReflectionColour);
        result.mSunPower = readFloat(bytes, sSunPower);
        result.mReflectivity = readFloat(bytes, sReflectivity);
        result.mFresnel = readFloat(bytes, sFresnel);
        if (!std::isfinite(result.mSunPower) || !std::isfinite(result.mReflectivity) || !std::isfinite(result.mFresnel))
            return std::nullopt;
        return result;
    }

    void Water::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "WATR");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("FULL"):
                    in.string(mFullName);
                    break;
                case ESM::fourCC("NNAM"):
                    in.string(mNoiseTexture);
                    break;
                case ESM::fourCC("ANAM"):
                    in.value(mOpacity);
                    break;
                case ESM::fourCC("FNAM"):
                    in.value(mWaterFlags);
                    break;
                case ESM::fourCC("MNAM"):
                    in.bytes(mMnam);
                    break;
                case ESM::fourCC("SNAM"):
                    in.formId(mSound);
                    break;
                case ESM::fourCC("XNAM"):
                    in.formId(mActorEffect);
                    break;
                case ESM::fourCC("DATA"):
                    in.bytes(mData, { 2, 186 });
                    break;
                case ESM::fourCC("DNAM"):
                    in.bytesBetween(mVisualData, 184, 196, 4);
                    break;
                case ESM::fourCC("GNAM"):
                    in.value(mRelatedWaters, &RelatedWaters::mDaytime, &RelatedWaters::mNighttime,
                        &RelatedWaters::mUnderwater);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
