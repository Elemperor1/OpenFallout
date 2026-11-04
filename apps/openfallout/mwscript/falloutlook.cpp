#include "falloutscripts.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadeyes.hpp>
#include <components/esm4/loadhair.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>

#include "../mwbase/environment.hpp"
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

        /// The flags of a hair, a head part and eyes: the first is that the player can have it. A hair and eyes also
        /// say which sex cannot have them (the second: not a man's, the third: not a woman's), as the executable tests
        /// them
        /// (+0x48 of a hair and +0x30 of eyes: bit 1 for a man and bit 2 for a woman; Mac round 12BB C2).
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

        /// The sex a hair or eyes are for: the flags say which sex cannot have them (the second bit: not a man's, the
        /// third: not a woman's; FalloutNV.exe 0x5FDF60, 0x5FDF80, 0x5FC530 and 0x5FC590, Mac round 12BB C2)
        bool sexFits(std::uint8_t flags, bool female)
        {
            return (flags & (female ? notFemaleFlag : notMaleFlag)) == 0;
        }

        bool hairFits(const ESM4::Hair& hair, bool female)
        {
            return (hair.mData.flags & playableFlag) != 0 && sexFits(hair.mData.flags, female);
        }

        bool eyesFit(const ESM4::Eyes& eyes, bool female)
        {
            return (eyes.mData.flags & playableFlag) != 0 && sexFits(eyes.mData.flags, female);
        }

        bool listed(const std::vector<ESM::FormId>& list, FormId id)
        {
            return std::any_of(
                list.begin(), list.end(), [id](const ESM::FormId& entry) { return numberOf(entry) == id; });
        }

        /// The records of a kind as the executable's own lists have them: the newest first, because every new form is
        /// put at the head of its list (FalloutNV.exe 0x4603B0 and 0x5AE3D0, Mac round 12BB C1 and C2). The rows of the
        /// hair, eyes and facial hair lists are in this order, not sorted.
        template <class T>
        std::vector<const T*> newestFirst(const OFWorld::ESMStore& store)
        {
            std::vector<const T*> all;
            for (const T& record : store.get<T>())
                all.push_back(&record);
            std::reverse(all.begin(), all.end());
            return all;
        }

        /// The names as the executable's sort has them: byte by byte, capitals before small letters (strcmp, 0x603220)
        bool lessByName(const std::string& left, FormId leftId, const std::string& right, FormId rightId)
        {
            const int order = left.compare(right);
            return order != 0 ? order < 0 : leftId < rightId;
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
        if (std::optional<FaceLook> picked = mChargen.look())
        {
            // A race of a plugin that is not loaded now reads as none: the record's own race stands then
            if (picked->mRace == 0)
                picked->mRace = recordLook().mRace;
            return *picked;
        }
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

        // The hairs the race has and the sex can wear, and the eyes the same way, in the order of the data (12BB C2)
        for (const ESM4::Hair* hair : newestFirst<ESM4::Hair>(mStore))
        {
            const FormId number = numberOf(hair->mId);
            if (!hairFits(*hair, look.mFemale) || !listed(race->mHairChoices, number))
                continue;
            catalog.mHairs.push_back({ number, nameOf(hair->mFullName, hair->mEditorId), number == look.mHair });
        }
        for (const ESM4::Eyes* eyes : newestFirst<ESM4::Eyes>(mStore))
        {
            const FormId number = numberOf(eyes->mId);
            if (!eyesFit(*eyes, look.mFemale) || !listed(race->mEyeChoices, number))
                continue;
            catalog.mEyes.push_back({ number, nameOf(eyes->mFullName, eyes->mEditorId), number == look.mEyes });
        }
        if (!look.mFemale)
        {
            // Every head part the player can have, whatever the race (there is no other test, 0x7AD312). The row that
            // is lit is the one the character has first in its list, which is the last of the record's parts (they are
            // put at the head of the list one after the other, 0x602D4C).
            const FormId first = look.mHeadParts.empty() ? 0 : look.mHeadParts.back();
            for (const ESM4::HeadPart* part : newestFirst<ESM4::HeadPart>(mStore))
            {
                if ((part->mData & playableFlag) == 0)
                    continue;
                const FormId number = numberOf(part->mId);
                catalog.mFacialHair.push_back({ number, nameOf(part->mFullName, part->mEditorId), number == first });
            }
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
            // A hair that the new race or sex cannot have is replaced by the race's own for the sex, and only when the
            // race has none by the first of its hairs that fits (0x7B1CA0)
            const auto hairIsFit = [&](FormId id) {
                if (id == 0 || !listed(race->mHairChoices, id))
                    return false;
                const ESM4::Hair* hair = store.get<ESM4::Hair>().search(refId(id));
                return hair != nullptr && sexFits(hair->mData.flags, look.mFemale);
            };
            if (!hairIsFit(look.mHair))
            {
                look.mHair = numberOf(race->mDefaultHair[look.mFemale ? 1 : 0]);
                if (look.mHair == 0)
                {
                    for (const ESM::FormId& id : race->mHairChoices)
                    {
                        const ESM4::Hair* hair = store.get<ESM4::Hair>().search(id);
                        if (hair != nullptr && hairFits(*hair, look.mFemale))
                        {
                            look.mHair = numberOf(id);
                            break;
                        }
                    }
                }
            }
            const ESM4::Eyes* current = look.mEyes == 0 ? nullptr : store.get<ESM4::Eyes>().search(refId(look.mEyes));
            const bool eyesAreFit = current != nullptr && listed(race->mEyeChoices, look.mEyes)
                && sexFits(current->mData.flags, look.mFemale);
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
        // The row that is lit is the part first in the character's list (the last of the record's); a click on it takes
        // it off, and a click on another takes the whole list away and puts that part on
        const bool worn = !look.mHeadParts.empty() && look.mHeadParts.back() == part;
        look.mHeadParts.clear();
        if (!worn && part != 0)
            look.mHeadParts.push_back(part);
        mChargen.setLook(look);
    }

    void FalloutScripts::lookRandomize()
    {
        FaceLook look = playerLook();
        const double variation = gameSetting("fNPCGeneticVariation", 0.5);
        FalloutScript::Gauss gauss([this]() {
            return std::uniform_real_distribution<double>(0.0, 1.0)(mLookRandom);
        });
        mChargen.setLook(FalloutScript::randomFace(look, variation, gauss));
    }

    bool FalloutScripts::lookPreview(LookPreview& preview)
    {
        const FaceLook look = playerLook();
        ESM::FormId playerBase;
        baseOf(mPlayer, playerBase);
        const ESM4::Npc* base = nullptr;
        // A character of the data that has the sex and the race: its skeleton and body are what the games draw
        const std::vector<const ESM4::Npc*> presets = presetsOf(mStore, look.mRace, look.mFemale, playerBase);
        if (!presets.empty())
            base = presets.front();
        else if (!playerBase.isZeroOrUnset())
            base = mStore.get<ESM4::Npc>().search(playerBase);
        if (base == nullptr)
            return false;
        preview.mTemplate = ESM::RefId(base->mId);
        preview.mRecord = *base;
        ESM4::Npc& record = preview.mRecord;
        record.mRace = ESM::FormId::fromUint32(look.mRace);
        // The record takes no traits from a template: it says what the player looks like
        record.mBaseTemplate = ESM::FormId();
        std::uint32_t& flags = record.mIsFONV ? record.mBaseConfig.fo3.flags : record.mBaseConfig.tes5.flags;
        const std::uint32_t female = record.mIsFONV ? static_cast<std::uint32_t>(ESM4::Npc::FO3_Female)
                                                    : static_cast<std::uint32_t>(ESM4::Npc::TES5_Female);
        flags = look.mFemale ? (flags | female) : (flags & ~female);
        record.mHair = ESM::FormId::fromUint32(look.mHair);
        record.mHairLength = look.mHairLength;
        record.mEyes = ESM::FormId::fromUint32(look.mEyes);
        record.mHairColour.red = look.mHairColour[0];
        record.mHairColour.green = look.mHairColour[1];
        record.mHairColour.blue = look.mHairColour[2];
        record.mHeadParts.clear();
        for (const FormId part : look.mHeadParts)
            record.mHeadParts.push_back(ESM::FormId::fromUint32(part));
        record.mSymShapeModeCoefficients = look.mShape;
        record.mAsymShapeModeCoefficients = look.mAsymmetric;
        record.mSymTextureModeCoefficients = look.mTone;
        return true;
    }

    const FalloutScript::FaceControlFile& FalloutScripts::faceControls()
    {
        const std::lock_guard lock(mFaceControlsMutex);
        if (!mFaceControls)
        {
            mFaceControls = FalloutScript::FaceControlFile();
            std::string bytes;
            if (const Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem())
            {
                Files::IStreamPtr stream = resources->getVFS()->find(VFS::Path::Normalized("facegen/si.ctl"));
                if (stream && stream->peek() != std::istream::traits_type::eof())
                {
                    std::ostringstream content;
                    content << stream->rdbuf();
                    bytes = content.str();
                }
            }
            if (std::optional<FalloutScript::FaceControlFile> file = FalloutScript::readFaceControlFile(bytes))
            {
                mFaceControls = std::move(*file);
                Log(Debug::Info) << "Face controls: " << mFaceControls->mControls.count(FaceKind::Shape) << " shape, "
                                 << mFaceControls->mControls.count(FaceKind::Tone) << " tone";
            }
            else
                Log(Debug::Warning)
                    << "Face controls: facegen/si.ctl is missing or is not the layout of the games' file";
        }
        return *mFaceControls;
    }

    int FalloutScripts::lookControlCount(FaceKind kind)
    {
        return faceControls().mControls.count(kind);
    }

    namespace
    {
        /// The face numbers of a character on the screen: those of its race for its sex and its own on top (the record
        /// keeps only the difference), which is what the sliders read and move (FalloutNV.exe 0x603AD0, 0x7AF770)
        struct FaceNumbers
        {
            std::vector<float> mShape;
            std::vector<float> mTone;
        };

        std::vector<float> sum(const std::vector<float>& race, const std::vector<float>& own, std::size_t count)
        {
            std::vector<float> result = sized(race, count);
            const std::vector<float> added = sized(own, count);
            for (std::size_t i = 0; i < count; ++i)
                result[i] = result[i] + added[i];
            return result;
        }

        /// What the numbers of the race are taken off again
        std::vector<float> difference(
            const std::vector<float>& absolute, const std::vector<float>& race, std::size_t count)
        {
            std::vector<float> result = sized(absolute, count);
            const std::vector<float> taken = sized(race, count);
            for (std::size_t i = 0; i < count; ++i)
                result[i] = result[i] - taken[i];
            return result;
        }

        /// The numbers of the face of the race for the sex of the look (none when the race is not there)
        const std::vector<float>& raceNumbers(const OFWorld::ESMStore& store, const FaceLook& look, FaceKind kind)
        {
            static const std::vector<float> none;
            const ESM4::Race* race = store.get<ESM4::Race>().search(refId(look.mRace));
            if (race == nullptr)
                return none;
            if (kind == FaceKind::Shape)
                return look.mFemale ? race->mSymShapeModeCoeffFemale : race->mSymShapeModeCoefficients;
            return look.mFemale ? race->mSymTextureModeCoeffFemale : race->mSymTextureModeCoefficients;
        }

        std::pair<int, int> rangeOf(FaceKind kind, int control)
        {
            for (const FalloutScript::FaceSlider& slider : FalloutScript::faceSliders())
            {
                if (slider.mKind == kind && slider.mControl == control)
                    return { slider.mMinimum, slider.mMaximum };
            }
            return { FalloutScript::faceSliderDefaultMinimum, FalloutScript::faceSliderDefaultMaximum };
        }
    }

    int FalloutScripts::lookSlider(FaceKind kind, int control)
    {
        const FaceLook look = playerLook();
        const bool shape = kind == FaceKind::Shape;
        const std::vector<float> absolute = sum(raceNumbers(mStore, look, kind), shape ? look.mShape : look.mTone,
            shape ? FaceLook::shapeCount : FaceLook::toneCount);
        // The number a slider shows is the control times ten, cut towards zero, and kept to the slider's range
        const auto [minimum, maximum] = rangeOf(kind, control);
        return std::clamp(FalloutScript::faceSliderInteger(faceControls().mControls.value(kind, control, absolute)),
            minimum, maximum);
    }

    void FalloutScripts::lookSetSlider(FaceKind kind, int control, int value)
    {
        const auto [minimum, maximum] = rangeOf(kind, control);
        value = std::clamp(value, minimum, maximum);
        FaceLook look = playerLook();
        const bool shape = kind == FaceKind::Shape;
        const std::vector<float>& base = raceNumbers(mStore, look, kind);
        const std::size_t count = shape ? FaceLook::shapeCount : FaceLook::toneCount;
        // The slider is set in the numbers of the whole face, and what the record keeps is what is left of them when
        // the race's are taken off (0x7AF770)
        std::vector<float> absolute = sum(base, shape ? look.mShape : look.mTone, count);
        if (!faceControls().mControls.move(kind, control, FalloutScript::faceSliderValue(value), absolute))
            return;
        (shape ? look.mShape : look.mTone) = difference(absolute, base, count);
        mChargen.setLook(look);
    }

    bool FalloutScripts::lookHasAge()
    {
        return faceControls().mAge.loaded();
    }

    bool FalloutScripts::lookSetAge(int slider)
    {
        const FalloutScript::FaceAge& age = faceControls().mAge;
        if (!age.loaded())
            return false;
        FaceLook look = playerLook();
        const std::vector<float>& baseShape = raceNumbers(mStore, look, FaceKind::Shape);
        const std::vector<float>& baseTone = raceNumbers(mStore, look, FaceKind::Tone);
        std::vector<float> shape = sum(baseShape, look.mShape, FaceLook::shapeCount);
        std::vector<float> tone = sum(baseTone, look.mTone, FaceLook::toneCount);
        if (!age.setAge(static_cast<float>(FalloutScript::faceAgeYears(std::clamp(slider, 1, 10))), shape, tone))
            return false;
        look.mShape = difference(shape, baseShape, FaceLook::shapeCount);
        look.mTone = difference(tone, baseTone, FaceLook::toneCount);
        mChargen.setLook(look);
        return true;
    }
}
