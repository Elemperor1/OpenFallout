#include "loadexpl.hpp"

#include <cstring>

#include "reader.hpp"
#include "recordreader.hpp"

namespace ESM4
{
    namespace
    {
        template <class T>
        T field(const std::array<std::uint8_t, 52>& bytes, std::size_t offset)
        {
            T value{};
            if (offset + sizeof(T) <= bytes.size())
                std::memcpy(&value, bytes.data() + offset, sizeof(T));
            return value;
        }

        ESM::FormId formId(const std::array<std::uint8_t, 52>& bytes, std::size_t offset)
        {
            return ESM::FormId::fromUint32(field<std::uint32_t>(bytes, offset));
        }
    }

    Explosion::GameData Explosion::gameData() const
    {
        GameData data;
        data.mForce = field<float>(mData, 0);
        data.mDamage = field<float>(mData, 4);
        data.mRadius = field<float>(mData, 8);
        data.mLight = formId(mData, 12);
        data.mSound1 = formId(mData, 16);
        data.mFlags = field<std::uint32_t>(mData, 20);
        data.mImageSpaceRadius = field<float>(mData, 24);
        data.mImpactDataSet = formId(mData, 28);
        data.mSound2 = formId(mData, 32);
        data.mRadiationLevel = field<float>(mData, 36);
        data.mRadiationDissipationTime = field<float>(mData, 40);
        data.mRadiationRadius = field<float>(mData, 44);
        data.mSoundLevel = field<std::uint32_t>(mData, 48);
        return data;
    }

    void Explosion::load(Reader& reader)
    {
        mId = reader.getFormIdFromHeader();
        mFlags = reader.hdr().record.flags;

        RecordReader in(reader, "EXPL");
        while (in.next())
        {
            switch (in.type())
            {
                case ESM::fourCC("EDID"):
                    in.string(mEditorId);
                    break;
                case ESM::fourCC("OBND"):
                    in.value(mBounds);
                    break;
                case ESM::fourCC("FULL"):
                    in.string(mFullName);
                    break;
                case ESM::fourCC("MODL"):
                    in.path(mModel);
                    break;
                case ESM::fourCC("MODT"):
                    in.bytes(mModelTextures);
                    break;
                case ESM::fourCC("EITM"):
                    in.formId(mObjectEffect);
                    break;
                case ESM::fourCC("MNAM"):
                    in.formId(mImageSpaceModifier);
                    break;
                case ESM::fourCC("INAM"):
                    in.formId(mPlacedImpactObject);
                    break;
                case ESM::fourCC("DATA"):
                    in.value(mData);
                    // The light, the two sounds and the impact data set.
                    in.adjustFormIds(mData, { 12, 16, 28, 32 });
                    break;
                case ESM::fourCC("MODB"):
                    in.value(mBoundRadius);
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
