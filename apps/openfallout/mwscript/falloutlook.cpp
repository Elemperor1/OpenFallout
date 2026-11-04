#include "falloutscripts.hpp"

#include <algorithm>
#include <cctype>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadeyes.hpp>
#include <components/esm4/loadhair.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwclass/esm4base.hpp"
#include "../mwclass/esm4sources.hpp"
#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        using FalloutScript::FaceKind;
        using FalloutScript::FaceLook;
        using FalloutScript::FormId;

        /// The flags of a hair, a head part and eyes: the first is that the player can have it. A hair also says which
        /// sex cannot wear it (the second: not a man's, the third: not a woman's), as the games before it do (guess:
        /// the executable's own tests are +0x48 bit 1 and bit 2 of the form, 12AW S7; the census of 12BB settles it).
        constexpr std::uint8_t playableFlag = 0x01;
        constexpr std::uint8_t notMaleFlag = 0x02;
        constexpr std::uint8_t notFemaleFlag = 0x04;

        ESM::RefId refId(FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        std::vector<float> sized(const std::vector<float>& values, std::size_t count)
        {
            std::vector<float> result(count, 0.f);
            std::copy_n(values.begin(), std::min(values.size(), count), result.begin());
            return result;
        }

        std::string nameOf(const std::string& full, const std::string& editor)
        {
            return full.empty() ? editor : full;
        }

        bool hairFits(const ESM4::Hair& hair, bool female)
        {
            const std::uint8_t flags = hair.mData.flags;
            return (flags & playableFlag) != 0 && (flags & (female ? notFemaleFlag : notMaleFlag)) == 0;
        }

        bool lessByName(const std::string& left, FormId leftId, const std::string& right, FormId rightId)
        {
            const std::string a = Misc::StringUtils::lowerCase(left);
            const std::string b = Misc::StringUtils::lowerCase(right);
            return a != b ? a < b : leftId < rightId;
        }
    }

    FaceLook FalloutScripts::recordLook()
    {
        FaceLook look;
        ESM::FormId base;
        if (baseOf(mPlayer, base) && !base.isZeroOrUnset())
        {
            if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
            {
                const OFClass::StoreWornArmorSource source(mStore);
                const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(mPlayer), base);
                const auto chain = ESM4::templateChain(source, *npc, OFClass::ESM4Impl::sDefaultLevel, seed);
                const ESM4::Npc* owner = ESM4::templateOwner(chain, ESM4::Npc::Template_UseTraits);
                if (owner == nullptr)
                    owner = npc;
                look.mRace = numberOf(owner->mRace);
                look.mFemale = owner->mIsFONV ? (owner->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female) != 0
                                              : (owner->mBaseConfig.tes5.flags & ESM4::Npc::TES5_Female) != 0;
                look.mHair = numberOf(owner->mHair);
                look.mHairLength = owner->mHairLength;
                look.mEyes = numberOf(owner->mEyes);
                look.mHairColour = { owner->mHairColour.red, owner->mHairColour.green, owner->mHairColour.blue };
                for (const ESM::FormId& part : owner->mHeadParts)
                    if (const FormId number = numberOf(part))
                        look.mHeadParts.push_back(number);
                look.mShape = sized(owner->mSymShapeModeCoefficients, FaceLook::shapeCount);
                look.mAsymmetric = sized(owner->mAsymShapeModeCoefficients, FaceLook::asymmetricCount);
                look.mTone = sized(owner->mSymTextureModeCoefficients, FaceLook::toneCount);
            }
        }
        // What was picked on the way (a script's SexChange, the race of an earlier screen) is the player's too
        if (const std::optional<bool> female = mChargen.female())
            look.mFemale = *female;
        if (const FormId race = mChargen.race())
            look.mRace = race;
        return look;
    }

    FaceLook FalloutScripts::playerLook()
    {
        if (const std::optional<FaceLook> picked = mChargen.look())
            return *picked;
        return recordLook();
    }

    namespace
    {
        /// The characters that the games offer as faces to start from for a sex and race: those of the race and sex that
        /// the editor marked as presets (not the player's own record); when there are none, all of that race and sex
        /// (0x7AF610 and 0x603280). They are in the order of their names.
        std::vector<const ESM4::Npc*> presetsOf(const OFWorld::ESMStore& store, FormId race, bool female, ESM::FormId skip)
        {
            std::vector<const ESM4::Npc*> marked;
            std::vector<const ESM4::Npc*> all;
            for (const ESM4::Npc& npc : store.get<ESM4::Npc>())
            {
                if (!npc.mIsFONV || npc.mId == skip || numberOf(npc.mRace) != race)
                    continue;
                if (((npc.mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female) != 0) != female)
                    continue;
                all.push_back(&npc);
                if ((npc.mBaseConfig.fo3.flags & ESM4::Npc::FO3_PresetFace) != 0)
                    marked.push_back(&npc);
            }
            std::vector<const ESM4::Npc*>& list = marked.empty() ? all : marked;
            std::sort(list.begin(), list.end(), [](const ESM4::Npc* left, const ESM4::Npc* right) {
                return lessByName(left->mFullName, numberOf(left->mId), right->mFullName, numberOf(right->mId));
            });
            return list;
        }
    }

    FalloutScripts::LookCatalog FalloutScripts::lookCatalog()
    {
        LookCatalog catalog;
        const FaceLook look = playerLook();
        for (const ChargenRace& race : playableRaces())
            catalog.mRaces.push_back({ race.mId, race.mName, race.mId == look.mRace });
        const ESM4::Race* race = mStore.get<ESM4::Race>().search(refId(look.mRace));
        if (race == nullptr)
            return catalog;

        for (const ESM::FormId& id : race->mHairChoices)
        {
            const ESM4::Hair* hair = mStore.get<ESM4::Hair>().search(id);
            if (hair == nullptr || !hairFits(*hair, look.mFemale))
                continue;
            catalog.mHairs.push_back(
                { numberOf(hair->mId), nameOf(hair->mFullName, hair->mEditorId), numberOf(hair->mId) == look.mHair });
        }
        for (const ESM::FormId& id : race->mEyeChoices)
        {
            const ESM4::Eyes* eyes = mStore.get<ESM4::Eyes>().search(id);
            if (eyes == nullptr || (eyes->mData.flags & playableFlag) == 0)
                continue;
            catalog.mEyes.push_back(
                { numberOf(eyes->mId), nameOf(eyes->mFullName, eyes->mEditorId), numberOf(eyes->mId) == look.mEyes });
        }
        if (!look.mFemale)
        {
            for (const ESM4::HeadPart& part : mStore.get<ESM4::HeadPart>())
            {
                if ((part.mData & playableFlag) == 0)
                    continue;
                const FormId number = numberOf(part.mId);
                const bool worn = std::find(look.mHeadParts.begin(), look.mHeadParts.end(), number) != look.mHeadParts.end();
                catalog.mFacialHair.push_back({ number, nameOf(part.mFullName, part.mEditorId), worn });
            }
            std::sort(catalog.mFacialHair.begin(), catalog.mFacialHair.end(),
                [](const LookChoice& left, const LookChoice& right) {
                    return lessByName(left.mName, left.mId, right.mName, right.mId);
                });
        }

        ESM::FormId playerBase;
        baseOf(mPlayer, playerBase);
        for (const ESM4::Npc* npc : presetsOf(mStore, look.mRace, look.mFemale, playerBase))
            catalog.mPresets.push_back({ numberOf(npc->mId), nameOf(npc->mFullName, npc->mEditorId), false });
        return catalog;
    }

    namespace
    {
        void copyPreset(FaceLook& look, const ESM4::Npc& preset)
        {
            look.mHair = numberOf(preset.mHair);
            look.mHairLength = preset.mHairLength;
            look.mEyes = numberOf(preset.mEyes);
            look.mHairColour = { preset.mHairColour.red, preset.mHairColour.green, preset.mHairColour.blue };
            look.mHeadParts.clear();
            for (const ESM::FormId& part : preset.mHeadParts)
                if (const FormId number = numberOf(part))
                    look.mHeadParts.push_back(number);
            // The numbers of a preset are the difference from the race's face, as the record keeps them
            look.mShape = sized(preset.mSymShapeModeCoefficients, FaceLook::shapeCount);
            look.mAsymmetric = sized(preset.mAsymShapeModeCoefficients, FaceLook::asymmetricCount);
            look.mTone = sized(preset.mSymTextureModeCoefficients, FaceLook::toneCount);
        }
    }

    bool FalloutScripts::lookSetPreset(int number)
    {
        FaceLook look = playerLook();
        ESM::FormId playerBase;
        baseOf(mPlayer, playerBase);
        const std::vector<const ESM4::Npc*> presets = presetsOf(mStore, look.mRace, look.mFemale, playerBase);
        if (number < 1 || static_cast<std::size_t>(number) > presets.size())
            return false;
        copyPreset(look, *presets[static_cast<std::size_t>(number) - 1]);
        mChargen.setLook(look);
        return true;
    }

    namespace
    {
        /// The face of a new sex or race: the first preset of the kind, and a hair and eyes that the kind has (0x7B5180)
        void putOnFirstFace(FalloutScripts& host, const OFWorld::ESMStore& store, FaceLook& look, ESM::FormId playerBase)
        {
            look.mHeadParts.clear();
            const std::vector<const ESM4::Npc*> presets = presetsOf(store, look.mRace, look.mFemale, playerBase);
            if (!presets.empty())
                copyPreset(look, *presets.front());
            else
            {
                look.mShape.assign(FaceLook::shapeCount, 0.f);
                look.mAsymmetric.assign(FaceLook::asymmetricCount, 0.f);
                look.mTone.assign(FaceLook::toneCount, 0.f);
            }
            const ESM4::Race* race = store.get<ESM4::Race>().search(refId(look.mRace));
            if (race == nullptr)
                return;
            const auto hairIsFit = [&](FormId id) {
                if (id == 0)
                    return false;
                if (std::find_if(race->mHairChoices.begin(), race->mHairChoices.end(),
                        [id](const ESM::FormId& choice) { return numberOf(choice) == id; })
                    == race->mHairChoices.end())
                    return false;
                const ESM4::Hair* hair = store.get<ESM4::Hair>().search(refId(id));
                return hair != nullptr && hairFits(*hair, look.mFemale);
            };
            if (!hairIsFit(look.mHair))
            {
                look.mHair = 0;
                for (const ESM::FormId& id : race->mHairChoices)
                {
                    if (hairIsFit(numberOf(id)))
                    {
                        look.mHair = numberOf(id);
                        break;
                    }
                }
                if (look.mHair == 0)
                    look.mHair = numberOf(race->mDefaultHair[look.mFemale ? 1 : 0]);
            }
            const bool eyesAreFit = look.mEyes != 0
                && std::find_if(race->mEyeChoices.begin(), race->mEyeChoices.end(),
                       [&look](const ESM::FormId& choice) { return numberOf(choice) == look.mEyes; })
                    != race->mEyeChoices.end();
            if (!eyesAreFit)
                look.mEyes = race->mEyeChoices.empty() ? 0 : numberOf(race->mEyeChoices.front());
            (void)host;
        }
    }

    void FalloutScripts::lookSetSex(bool female)
    {
        FaceLook look = playerLook();
        if (look.mFemale == female && mChargen.look())
            return;
        look.mFemale = female;
        ESM::FormId playerBase;
        baseOf(mPlayer, playerBase);
        putOnFirstFace(*this, mStore, look, playerBase);
        mChargen.setLook(look);
    }

    void FalloutScripts::lookSetRace(FormId race)
    {
        FaceLook look = playerLook();
        if (look.mRace == race && mChargen.look())
            return;
        look.mRace = race;
        ESM::FormId playerBase;
        baseOf(mPlayer, playerBase);
        putOnFirstFace(*this, mStore, look, playerBase);
        mChargen.setLook(look);
    }

    void FalloutScripts::lookSetHair(FormId hair)
    {
        FaceLook look = playerLook();
        look.mHair = hair;
        mChargen.setLook(look);
    }

    void FalloutScripts::lookSetEyes(FormId eyes)
    {
        FaceLook look = playerLook();
        look.mEyes = eyes;
        mChargen.setLook(look);
    }

    void FalloutScripts::lookSetHairColour(int red, int green, int blue)
    {
        FaceLook look = playerLook();
        look.mHairColour = { static_cast<std::uint8_t>(std::clamp(red, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(green, 0, 255)), static_cast<std::uint8_t>(std::clamp(blue, 0, 255)) };
        mChargen.setLook(look);
    }

    void FalloutScripts::lookToggleFacialHair(FormId part)
    {
        FaceLook look = playerLook();
        const bool worn = std::find(look.mHeadParts.begin(), look.mHeadParts.end(), part) != look.mHeadParts.end();
        look.mHeadParts.clear();
        if (!worn && part != 0)
            look.mHeadParts.push_back(part);
        mChargen.setLook(look);
    }

    void FalloutScripts::lookRandomize()
    {
        FaceLook look = playerLook();
        double variation = 0.5;
        const std::map<std::string, double>& settings = gameSettingValues();
        if (const auto found = settings.find("fnpcgeneticvariation"); found != settings.end())
            variation = found->second;
        FalloutScript::Gauss gauss([this]() {
            return std::uniform_real_distribution<double>(0.0, 1.0)(mLookRandom);
        });
        mChargen.setLook(FalloutScript::randomFace(look, variation, gauss));
    }

    const FalloutScript::FaceControls& FalloutScripts::faceControls()
    {
        const std::lock_guard lock(mFaceControlsMutex);
        if (!mFaceControls)
        {
            // (the file's layout is read by the next round: until it is, the screen has no controls)
            mFaceControls = FalloutScript::FaceControls();
        }
        return *mFaceControls;
    }

    int FalloutScripts::lookControlCount(FaceKind kind)
    {
        return faceControls().count(kind);
    }

    int FalloutScripts::lookSlider(FaceKind kind, int control)
    {
        const FaceLook look = playerLook();
        const std::vector<float>& coefficients = kind == FaceKind::Shape ? look.mShape : look.mTone;
        return FalloutScript::faceSliderInteger(faceControls().value(kind, control, coefficients));
    }

    void FalloutScripts::lookSetSlider(FaceKind kind, int control, int value)
    {
        FaceLook look = playerLook();
        std::vector<float>& coefficients = kind == FaceKind::Shape ? look.mShape : look.mTone;
        if (!faceControls().move(kind, control, FalloutScript::faceSliderValue(value), coefficients))
            return;
        mChargen.setLook(look);
    }
}
