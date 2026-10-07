#include "loadwthr.hpp"

#include <components/misc/strings/algorithm.hpp>

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    namespace
    {
        bool hasClouds(std::string_view texture)
        {
            // The names have either kind of slash and any case ("sky\\alpha.dds", "Sky\\Alpha.dds", "sky/Alpha.dds")
            const std::size_t slash = texture.find_last_of("\\/");
            const std::string_view file = slash == std::string_view::npos ? texture : texture.substr(slash + 1);
            return !file.empty() && !Misc::StringUtils::ciEqual(file, "alpha.dds");
        }
    }

    std::string_view Weather::cloudTexture() const
    {
        for (const std::string& texture : mCloudTextures)
            if (hasClouds(texture))
                return texture;
        return {};
    }

    std::optional<Weather::Colour> Weather::colour(ColourType type, TimeOfDay time) const
    {
        const std::size_t times = colourTimeCount();
        if (static_cast<std::size_t>(time) >= times)
            return std::nullopt;
        // For each type in turn, the colours of its times of day, each of them red, green, blue and an unused byte
        const std::size_t offset = (static_cast<std::size_t>(type) * times + static_cast<std::size_t>(time)) * 4;
        return Colour{ mColours[offset], mColours[offset + 1], mColours[offset + 2] };
    }

    void Weather::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "WTHR");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("\000IAD"):
                    in.formId(mImageSpaceModifiers[0]);
                    break;
                case ESM::fourCC("\001IAD"):
                    in.formId(mImageSpaceModifiers[1]);
                    break;
                case ESM::fourCC("\002IAD"):
                    in.formId(mImageSpaceModifiers[2]);
                    break;
                case ESM::fourCC("\003IAD"):
                    in.formId(mImageSpaceModifiers[3]);
                    break;
                case ESM::fourCC("\004IAD"):
                    in.formId(mImageSpaceModifiers[4]);
                    break;
                case ESM::fourCC("\005IAD"):
                    in.formId(mImageSpaceModifiers[5]);
                    break;
                case ESM::fourCC("DNAM"):
                    in.string(mCloudTextures[0]);
                    break;
                case ESM::fourCC("CNAM"):
                    in.string(mCloudTextures[1]);
                    break;
                case ESM::fourCC("ANAM"):
                    in.string(mCloudTextures[2]);
                    break;
                case ESM::fourCC("BNAM"):
                    in.string(mCloudTextures[3]);
                    break;
                case ESM::fourCC("LNAM"):
                    in.value(mLnam);
                    break;
                case ESM::fourCC("ONAM"):
                    in.value(mCloudSpeeds);
                    break;
                case ESM::fourCC("PNAM"):
                    in.bytes(mCloudColours, { 64, 96 });
                    break;
                case ESM::fourCC("NAM0"):
                    in.bytes(mColours, { 160, 240 });
                    break;
                case ESM::fourCC("FNAM"):
                    in.value(mFog);
                    break;
                case ESM::fourCC("INAM"):
                    in.value(mInam);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData);
                    break;
                case ESM::fourCC("SNAM"):
                    in.value(mSounds.emplace_back(), &WeatherSound::mSound);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("MODB"):
                    in.value(mBoundRadius);
                    break;
                case ESM::fourCC("MODT"):
                    in.bytes(mModelTextures);
                    break;
                case ESM::fourCC("MODS"):
                    in.alternateTextures(mModelAlternateTextures);
                    break;
                case ESM::fourCC("MODD"):
                    in.value(mModelFlags);
                    break;
                default:
                    in.unknown();
            }
        }
        in.finish();
    }
}
