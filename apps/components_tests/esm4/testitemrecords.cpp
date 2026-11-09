#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadaloc.hpp>
#include <components/esm4/loadappa.hpp>
#include <components/esm4/loadaspc.hpp>
#include <components/esm4/loadbptd.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadflor.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadidlm.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/esm4/loadltex.hpp>
#include <components/esm4/loadlvln.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadpgrd.hpp>
#include <components/esm4/loadroad.hpp>
#include <components/esm4/loadscrl.hpp>
#include <components/esm4/loadsgst.hpp>
#include <components/esm4/loadslgm.hpp>
#include <components/esm4/loadsndr.hpp>
#include <components/esm4/loadtxst.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>

namespace
{
    using namespace ESM4Test;

    template <class T>
    T load(std::string_view group, const std::string& data, std::uint32_t id = 0x801)
    {
        const std::vector<T> result = loadRecords<T>(group, record(group, id, data));
        EXPECT_EQ(result.size(), 1u);
        return result.empty() ? T{} : result[0];
    }

    // The loaders refuse a subrecord that the record does not have, so that a file that is not understood is not
    // read wrongly
    template <class T>
    void expectUnknownSubrecordRefused(std::string_view group)
    {
        SCOPED_TRACE(std::string(group));
        EXPECT_THROW(load<T>(group, zString("ZZZZ", "Unknown")), std::runtime_error);
    }

    std::string formIdData(std::uint32_t id)
    {
        std::string result;
        append(result, id);
        return result;
    }

    template <class T>
    std::string raw(const T& value)
    {
        return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    TEST(ESM4ItemRecordsTest, anActivatorHasItsModelSoundsAndTheRadioOfNewVegas)
    {
        const ESM4::Activator acti = load<ESM4::Activator>("ACTI",
            zString("EDID", "Activator") + zString("FULL", "Switch") + zString("MODL", "Meshes\\Switch.nif")
                + valueSubRecord<std::uint32_t>("SCRI", 0x811) + valueSubRecord<std::uint32_t>("SNAM", 0x812)
                + valueSubRecord<std::uint32_t>("VNAM", 0x813) + valueSubRecord<float>("MODB", 3.5f)
                + valueSubRecord<std::uint32_t>("INAM", 0x814) + valueSubRecord<std::uint32_t>("RNAM", 0x815)
                + zString("XATO", "Pull") + subRecord("DEST", "abcd") + subRecord("OBND", "abcdefghijkl")
                + zString("FNAM", "skipped"));

        EXPECT_EQ(acti.mId.toUint32(), 0x801u);
        EXPECT_EQ(acti.mEditorId, "Activator");
        EXPECT_EQ(acti.mFullName, "Switch");
        EXPECT_EQ(acti.mModel.getOriginal(), "Meshes\\Switch.nif");
        EXPECT_EQ(acti.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(acti.mLoopingSound.toUint32(), 0x812u);
        EXPECT_EQ(acti.mActivationSound.toUint32(), 0x813u);
        EXPECT_EQ(acti.mBoundRadius, 3.5f);
        EXPECT_EQ(acti.mRadioTemplate.toUint32(), 0x814u);
        EXPECT_EQ(acti.mRadioStation.toUint32(), 0x815u);
        EXPECT_EQ(acti.mActivationPrompt, "Pull");
        expectUnknownSubrecordRefused<ESM4::Activator>("ACTI");
    }

    TEST(ESM4ItemRecordsTest, aLightHasTheDataOfTheSizeOfItsGame)
    {
        std::string older;
        append<std::int32_t>(older, 30);
        append<std::uint32_t>(older, 256);
        append<std::uint32_t>(older, 0xFF8040);
        append<std::int32_t>(older, 0x08);
        append<std::uint32_t>(older, 12); // value
        append<float>(older, 1.5f); // weight
        ASSERT_EQ(older.size(), 24u);
        const ESM4::Light oblivion = load<ESM4::Light>("LIGH",
            zString("EDID", "Torch") + zString("FULL", "Torch") + zString("MODL", "Meshes\\Torch.nif")
                + zString("ICON", "icons/torch.dds") + valueSubRecord<std::uint32_t>("SCRI", 0x811)
                + valueSubRecord<std::uint32_t>("SNAM", 0x812) + valueSubRecord<float>("MODB", 2.f)
                + valueSubRecord<float>("FNAM", 0.5f) + subRecord("DATA", older));

        EXPECT_EQ(oblivion.mEditorId, "Torch");
        EXPECT_EQ(oblivion.mFullName, "Torch");
        EXPECT_EQ(oblivion.mModel.getOriginal(), "Meshes\\Torch.nif");
        EXPECT_EQ(oblivion.mIcon, "icons/torch.dds");
        EXPECT_EQ(oblivion.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(oblivion.mSound.toUint32(), 0x812u);
        EXPECT_EQ(oblivion.mBoundRadius, 2.f);
        EXPECT_EQ(oblivion.mFade, 0.5f);
        EXPECT_EQ(oblivion.mData.time, 30);
        EXPECT_EQ(oblivion.mData.radius, 256u);
        EXPECT_EQ(oblivion.mData.colour, 0xFF8040u);
        EXPECT_EQ(oblivion.mData.flags, 0x08);
        EXPECT_EQ(oblivion.mData.value, 12u);
        EXPECT_EQ(oblivion.mData.weight, 1.5f);
        // What the record that has no more is given
        EXPECT_EQ(oblivion.mData.falloff, 1.f);
        EXPECT_EQ(oblivion.mData.FOV, 90.f);

        std::string fallout;
        append<std::int32_t>(fallout, 30);
        append<std::uint32_t>(fallout, 256);
        append<std::uint32_t>(fallout, 0xFF8040);
        append<std::int32_t>(fallout, 0x08);
        append<float>(fallout, 2.5f); // falloff
        append<float>(fallout, 45.f); // FOV
        append<std::uint32_t>(fallout, 12);
        append<float>(fallout, 1.5f);
        ASSERT_EQ(fallout.size(), 32u);
        const ESM4::Light fo3 = load<ESM4::Light>("LIGH", subRecord("DATA", fallout));
        EXPECT_EQ(fo3.mData.falloff, 2.5f);
        EXPECT_EQ(fo3.mData.FOV, 45.f);
        EXPECT_EQ(fo3.mData.value, 12u);
        EXPECT_EQ(fo3.mData.weight, 1.5f);
        EXPECT_EQ(fo3.mData.nearClip, 0.f);

        std::string skyrim = fallout.substr(0, 24);
        append<float>(skyrim, 0.25f); // near clip
        append<float>(skyrim, 0.5f); // frequency
        append<float>(skyrim, 0.75f); // intensity amplitude
        append<float>(skyrim, 1.f); // movement amplitude
        append<std::uint32_t>(skyrim, 99);
        append<float>(skyrim, 4.f);
        ASSERT_EQ(skyrim.size(), 48u);
        const ESM4::Light tes5 = load<ESM4::Light>("LIGH", subRecord("DATA", skyrim));
        EXPECT_EQ(tes5.mData.nearClip, 0.25f);
        EXPECT_EQ(tes5.mData.frequency, 0.5f);
        EXPECT_EQ(tes5.mData.intensityAmplitude, 0.75f);
        EXPECT_EQ(tes5.mData.movementAmplitude, 1.f);
        EXPECT_EQ(tes5.mData.value, 99u);
        EXPECT_EQ(tes5.mData.weight, 4.f);

        std::string fallout4 = skyrim.substr(0, 40);
        append<float>(fallout4, 5.f); // constant
        append<float>(fallout4, 6.f); // scalar
        append<float>(fallout4, 7.f); // exponent
        append<float>(fallout4, 8.f); // god rays near clip
        append<std::uint32_t>(fallout4, 77);
        append<float>(fallout4, 9.f);
        ASSERT_EQ(fallout4.size(), 64u);
        const ESM4::Light fo4 = load<ESM4::Light>("LIGH", subRecord("DATA", fallout4));
        EXPECT_EQ(fo4.mData.constant, 5.f);
        EXPECT_EQ(fo4.mData.scalar, 6.f);
        EXPECT_EQ(fo4.mData.exponent, 7.f);
        EXPECT_EQ(fo4.mData.godRaysNearClip, 8.f);
        EXPECT_EQ(fo4.mData.value, 77u);
        EXPECT_EQ(fo4.mData.weight, 9.f);

        // Data of another size is not read
        const ESM4::Light odd = load<ESM4::Light>("LIGH", subRecord("DATA", std::string(30, 'x')));
        EXPECT_EQ(odd.mData.radius, 0u);
        expectUnknownSubrecordRefused<ESM4::Light>("LIGH");
    }

    TEST(ESM4ItemRecordsTest, aHeadPartHasItsFilesPerTypeAndItsRaces)
    {
        const ESM4::HeadPart part = load<ESM4::HeadPart>("HDPT",
            zString("EDID", "Hair") + zString("FULL", "Hair") + zString("MODL", "Meshes\\Hair.nif")
                + valueSubRecord<std::uint8_t>("DATA", 3) + valueSubRecord<std::uint32_t>("PNAM", 3)
                + valueSubRecord<std::uint64_t>("XALG", 0x1122334455667788ull)
                + valueSubRecord<std::uint32_t>("HNAM", 0x811) + valueSubRecord<std::uint32_t>("HNAM", 0x812)
                + valueSubRecord<std::uint32_t>("NAM0", 0) + zString("NAM1", "Hair.tri")
                + valueSubRecord<std::uint32_t>("NAM0", 2) + zString("NAM1", "HairChargen.tri")
                + valueSubRecord<std::uint32_t>("TNAM", 0x813) + valueSubRecord<std::uint32_t>("CNAM", 0x814)
                + valueSubRecord<std::uint32_t>("RNAM", 0x815) + valueSubRecord<std::uint32_t>("RNAM", 0x816)
                + subRecord("MODT", "xxxx") + subRecord("CTDA", "xxxx"));

        EXPECT_EQ(part.mEditorId, "Hair");
        EXPECT_EQ(part.mFullName, "Hair");
        EXPECT_EQ(part.mModel.getOriginal(), "Meshes\\Hair.nif");
        EXPECT_EQ(part.mData, 3);
        EXPECT_EQ(part.mType, ESM4::HeadPart::Type_Hair);
        EXPECT_EQ(part.mExtraFlags2, 0x1122334455667788ull);
        ASSERT_EQ(part.mExtraParts.size(), 2u);
        EXPECT_EQ(part.mExtraParts[1].toUint32(), 0x812u);
        EXPECT_EQ(part.mTriFile[0], "Hair.tri");
        EXPECT_TRUE(part.mTriFile[1].empty());
        EXPECT_EQ(part.mTriFile[2], "HairChargen.tri");
        EXPECT_EQ(part.mBaseTexture.toUint32(), 0x813u);
        EXPECT_EQ(part.mColor.toUint32(), 0x814u);
        ASSERT_EQ(part.mValidRaces.size(), 2u);
        EXPECT_EQ(part.mValidRaces[0].toUint32(), 0x815u);
        expectUnknownSubrecordRefused<ESM4::HeadPart>("HDPT");

        // A file does not say what it is for, unless the subrecord before says
        EXPECT_THROW(load<ESM4::HeadPart>("HDPT", zString("NAM1", "Hair.tri")), std::runtime_error);
        EXPECT_THROW(load<ESM4::HeadPart>("HDPT", valueSubRecord<std::uint32_t>("NAM0", 3) + zString("NAM1", "a.tri")),
            std::runtime_error);
    }

    TEST(ESM4ItemRecordsTest, aMediaLocationControllerHasItsSetsAndTheFlagsInTwoParts)
    {
        std::string flags;
        append<std::uint8_t>(flags, 0xA5);
        append<std::uint8_t>(flags, 0xFF);
        append<std::uint16_t>(flags, 0x1234);
        const ESM4::MediaLocationController controller = load<ESM4::MediaLocationController>("ALOC",
            zString("EDID", "Controller") + zString("FULL", "Controller") + valueSubRecord<std::uint32_t>("GNAM", 0x811)
                + valueSubRecord<std::uint32_t>("LNAM", 0x812) + valueSubRecord<std::uint32_t>("YNAM", 0x813)
                + valueSubRecord<std::uint32_t>("HNAM", 0x814) + valueSubRecord<std::uint32_t>("XNAM", 0x815)
                + valueSubRecord<std::uint32_t>("ZNAM", 0x816) + valueSubRecord<std::uint32_t>("RNAM", 0x817)
                + subRecord("NAM1", flags) + valueSubRecord<float>("NAM4", 1.5f) + valueSubRecord<float>("NAM7", 2.5f)
                + valueSubRecord<std::uint32_t>("NAM5", 600) + valueSubRecord<std::uint32_t>("NAM6", 1800)
                + valueSubRecord<std::uint32_t>("NAM2", 0) + valueSubRecord<std::uint32_t>("NAM3", 0)
                + valueSubRecord<std::uint32_t>("FNAM", 0));

        EXPECT_EQ(controller.mEditorId, "Controller");
        EXPECT_EQ(controller.mFullName, "Controller");
        ASSERT_EQ(controller.mBattleSets.size(), 1u);
        EXPECT_EQ(controller.mBattleSets[0].toUint32(), 0x811u);
        EXPECT_EQ(controller.mLocationSets[0].toUint32(), 0x812u);
        EXPECT_EQ(controller.mEnemySets[0].toUint32(), 0x813u);
        EXPECT_EQ(controller.mNeutralSets[0].toUint32(), 0x814u);
        EXPECT_EQ(controller.mFriendSets[0].toUint32(), 0x815u);
        EXPECT_EQ(controller.mAllySets[0].toUint32(), 0x816u);
        EXPECT_EQ(controller.mConditionalFaction.toUint32(), 0x817u);
        // The first byte is two numbers of four bits, the second one overwritten by the one that is read
        EXPECT_EQ(controller.mMediaFlags.loopingOptions, 0x0A);
        EXPECT_EQ(controller.mMediaFlags.factionNotFound, 0x05);
        EXPECT_EQ(controller.mLocationDelay, 1.5f);
        EXPECT_EQ(controller.mRetriggerDelay, 2.5f);
        EXPECT_EQ(controller.mDayStart, 600u);
        EXPECT_EQ(controller.mNightStart, 1800u);
        expectUnknownSubrecordRefused<ESM4::MediaLocationController>("ALOC");
    }

    TEST(ESM4ItemRecordsTest, aBodyPartDataHasItsPartsInTheOrderTheyEnd)
    {
        ESM4::BodyPartData::BPND bpnd{};
        bpnd.damageMult = 2.f;
        bpnd.flags = 3;
        bpnd.partType = 4;
        bpnd.healthPercent = 50;
        bpnd.actorValue = 9;
        bpnd.toHitChance = 75;
        bpnd.explDebrisCount = 5;
        bpnd.explDebris = 0x821;
        bpnd.sevDebrisCount = 6;
        bpnd.limbReplacementScale = 1.25f;
        const ESM4::BodyPartData data = load<ESM4::BodyPartData>("BPTD",
            zString("EDID", "Body") + zString("FULL", "Body") + zString("MODL", "Meshes\\Body.nif")
                + zString("BPTN", "Head") + zString("BPNN", "Bip01 Head") + zString("BPNT", "VATSHead")
                + zString("BPNI", "Bip01 Neck") + subRecord("BPND", raw(bpnd)) + zString("NAM1", "Limb.nif")
                + zString("NAM4", "Bone") + subRecord("NAM5", "xxxx")
                + zString("BPTN", "Arm")
                // A part with data of the wrong size has none
                + subRecord("BPND", "short") + zString("NAM4", "Other") + subRecord("RAGA", "xxxx"));

        EXPECT_EQ(data.mEditorId, "Body");
        EXPECT_EQ(data.mFullName, "Body");
        EXPECT_EQ(data.mModel.getOriginal(), "Meshes\\Body.nif");
        ASSERT_EQ(data.mBodyParts.size(), 2u);
        const auto& head = data.mBodyParts[0];
        EXPECT_EQ(head.mPartName, "Head");
        EXPECT_EQ(head.mNodeName, "Bip01 Head");
        EXPECT_EQ(head.mVATSTarget, "VATSHead");
        EXPECT_EQ(head.mIKStartNode, "Bip01 Neck");
        EXPECT_EQ(head.mLimbReplacementModel, "Limb.nif");
        EXPECT_EQ(head.mGoreEffectsTarget, "Bone");
        EXPECT_EQ(head.mData.damageMult, 2.f);
        EXPECT_EQ(head.mData.flags, 3);
        EXPECT_EQ(head.mData.partType, 4);
        EXPECT_EQ(head.mData.healthPercent, 50);
        EXPECT_EQ(head.mData.actorValue, 9);
        EXPECT_EQ(head.mData.toHitChance, 75);
        EXPECT_EQ(head.mData.explDebrisCount, 5);
        EXPECT_EQ(head.mData.explDebris, 0x821u);
        EXPECT_EQ(head.mData.sevDebrisCount, 6);
        EXPECT_EQ(head.mData.limbReplacementScale, 1.25f);
        const auto& arm = data.mBodyParts[1];
        EXPECT_EQ(arm.mPartName, "Arm");
        EXPECT_TRUE(arm.mNodeName.empty());
        EXPECT_EQ(arm.mData.damageMult, 0.f);
        EXPECT_EQ(arm.mGoreEffectsTarget, "Other");
        expectUnknownSubrecordRefused<ESM4::BodyPartData>("BPTD");
    }

    TEST(ESM4ItemRecordsTest, anItemModHasItsTextsSoundsAndData)
    {
        std::string itemData;
        append<std::uint32_t>(itemData, 120);
        append<float>(itemData, 0.5f);
        const ESM4::ItemMod mod = load<ESM4::ItemMod>("IMOD",
            zString("EDID", "Mod") + zString("FULL", "Scope") + zString("MODL", "Meshes\\Scope.nif")
                + valueSubRecord<float>("MODB", 1.5f) + zString("DESC", "A scope") + zString("ICON", "icons/scope.dds")
                + zString("MICO", "icons/scope_small.dds") + valueSubRecord<std::uint32_t>("SCRI", 0x811)
                + valueSubRecord<std::uint32_t>("YNAM", 0x812) + valueSubRecord<std::uint32_t>("ZNAM", 0x813)
                + subRecord("DATA", itemData) + subRecord("OBND", std::string(12, 'x')) + subRecord("DEST", "xxxx"));

        EXPECT_EQ(mod.mEditorId, "Mod");
        EXPECT_EQ(mod.mFullName, "Scope");
        EXPECT_EQ(mod.mModel.getOriginal(), "Meshes\\Scope.nif");
        EXPECT_EQ(mod.mBoundRadius, 1.5f);
        EXPECT_EQ(mod.mText, "A scope");
        EXPECT_EQ(mod.mIcon, "icons/scope.dds");
        EXPECT_EQ(mod.mMiniIcon, "icons/scope_small.dds");
        EXPECT_EQ(mod.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(mod.mPickUpSound.toUint32(), 0x812u);
        EXPECT_EQ(mod.mDropSound.toUint32(), 0x813u);
        EXPECT_EQ(mod.mData.mValue, 120u);
        EXPECT_EQ(mod.mData.mWeight, 0.5f);
        expectUnknownSubrecordRefused<ESM4::ItemMod>("IMOD");
    }

    TEST(ESM4ItemRecordsTest, aSigilStoneHasItsNameAndTheNamesOfItsScriptEffects)
    {
        std::string stone;
        append<std::uint8_t>(stone, 4);
        append<std::uint32_t>(stone, 200);
        append<float>(stone, 2.f);
        ESM4::ScriptEffect effect{};
        effect.formId = 0x831;
        effect.school = 2;
        effect.flags = 1;
        const ESM4::SigilStone sigil = load<ESM4::SigilStone>("SGST",
            zString("EDID", "Sigil") + zString("FULL", "Sigil Stone") + zString("FULL", "Effect One")
                + zString("FULL", "Effect Two") + subRecord("DATA", stone) + zString("MODL", "Meshes\\Sigil.nif")
                + zString("ICON", "icons/sigil.dds") + valueSubRecord<std::uint32_t>("SCRI", 0x811)
                + valueSubRecord<float>("MODB", 1.f) + subRecord("SCIT", raw(effect)) + subRecord("EFID", "abcd")
                + subRecord("EFIT", "abcd"));

        EXPECT_EQ(sigil.mFullName, "Sigil Stone");
        ASSERT_EQ(sigil.mScriptEffect.size(), 2u);
        EXPECT_EQ(sigil.mScriptEffect[0], "Effect One");
        EXPECT_EQ(sigil.mScriptEffect[1], "Effect Two");
        EXPECT_EQ(sigil.mData.uses, 4);
        EXPECT_EQ(sigil.mData.value, 200u);
        EXPECT_EQ(sigil.mData.weight, 2.f);
        EXPECT_EQ(sigil.mModel.getOriginal(), "Meshes\\Sigil.nif");
        EXPECT_EQ(sigil.mIcon, "icons/sigil.dds");
        EXPECT_EQ(sigil.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(sigil.mBoundRadius, 1.f);
        EXPECT_EQ(sigil.mEffect.formId, 0x831u);
        EXPECT_EQ(sigil.mEffect.school, 2);
        expectUnknownSubrecordRefused<ESM4::SigilStone>("SGST");
    }

    TEST(ESM4ItemRecordsTest, aGameSettingHasTheTypeThatItsNameStartsWith)
    {
        const auto setting = [](std::string_view name, const std::string& data) {
            return load<ESM4::GameSetting>("GMST", zString("EDID", name) + subRecord("DATA", data));
        };

        const ESM4::GameSetting boolean = setting("bEnabled", formIdData(1));
        EXPECT_EQ(boolean.mEditorId, "bEnabled");
        EXPECT_EQ(std::get<bool>(boolean.mData), true);
        EXPECT_EQ(std::get<bool>(setting("bDisabled", formIdData(0)).mData), false);

        std::string number;
        append<std::int32_t>(number, -7);
        EXPECT_EQ(std::get<std::int32_t>(setting("iCount", number).mData), -7);

        std::string fraction;
        append<float>(fraction, 0.75f);
        EXPECT_EQ(std::get<float>(setting("fScale", fraction).mData), 0.75f);

        std::string unsignedNumber;
        append<std::uint32_t>(unsignedNumber, 0xFFFFFFF0u);
        EXPECT_EQ(std::get<std::uint32_t>(setting("uColour", unsignedNumber).mData), 0xFFFFFFF0u);

        EXPECT_EQ(std::get<std::string>(setting("sName", std::string("Text\0", 5)).mData), "Text");

        EXPECT_THROW(setting("xUnknown", formIdData(1)), std::runtime_error);
        EXPECT_THROW(
            load<ESM4::GameSetting>("GMST", zString("EDID", "bX") + zString("FULL", "Name")), std::runtime_error);
    }

    TEST(ESM4ItemRecordsTest, anApparatusHasTheDataOfItsGame)
    {
        std::string data;
        append<std::uint8_t>(data, 2);
        append<std::uint32_t>(data, 90);
        append<float>(data, 3.f);
        append<float>(data, 1.25f);
        const ESM4::Apparatus oblivion = load<ESM4::Apparatus>("APPA",
            zString("EDID", "Retort") + zString("FULL", "Retort") + zString("MODL", "Meshes\\Retort.nif")
                + zString("ICON", "icons/retort.dds") + valueSubRecord<std::uint32_t>("SCRI", 0x811)
                + valueSubRecord<float>("MODB", 1.f) + zString("DESC", "Brews") + subRecord("DATA", data)
                + subRecord("QUAL", "abcd") + subRecord("OBND", "abcd"));

        EXPECT_EQ(oblivion.mFullName, "Retort");
        EXPECT_EQ(oblivion.mModel.getOriginal(), "Meshes\\Retort.nif");
        EXPECT_EQ(oblivion.mIcon, "icons/retort.dds");
        EXPECT_EQ(oblivion.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(oblivion.mBoundRadius, 1.f);
        EXPECT_EQ(oblivion.mText, "Brews");
        EXPECT_EQ(oblivion.mData.type, 2);
        EXPECT_EQ(oblivion.mData.value, 90u);
        EXPECT_EQ(oblivion.mData.weight, 3.f);
        EXPECT_EQ(oblivion.mData.quality, 1.25f);
        expectUnknownSubrecordRefused<ESM4::Apparatus>("APPA");

        // The records of Fallout 3 have the value and the weight only
        std::string falloutData;
        append<std::uint32_t>(falloutData, 90);
        append<float>(falloutData, 3.f);
        const ESM4::Apparatus fallout = loadVersionedRecord<ESM4::Apparatus>(
            "APPA", 0.94f, 15, zString("EDID", "Retort") + subRecord("DATA", falloutData));
        EXPECT_EQ(fallout.mData.value, 90u);
        EXPECT_EQ(fallout.mData.weight, 3.f);
        EXPECT_EQ(fallout.mData.quality, 0.f);
    }

    TEST(ESM4ItemRecordsTest, aContainerHasItsItemsAndSounds)
    {
        std::string flags;
        append<std::uint8_t>(flags, 3);
        append<float>(flags, 10.f);
        std::string first;
        append<std::uint32_t>(first, 0x841);
        append<std::uint32_t>(first, 5);
        std::string second;
        append<std::uint32_t>(second, 0x842);
        append<std::uint32_t>(second, 1);
        const ESM4::Container container = load<ESM4::Container>("CONT",
            zString("EDID", "Chest") + zString("FULL", "Chest") + zString("MODL", "Meshes\\Chest.nif")
                + valueSubRecord<float>("MODB", 4.f) + subRecord("DATA", flags) + subRecord("CNTO", first)
                + subRecord("CNTO", second) + valueSubRecord<std::uint32_t>("SNAM", 0x811)
                + valueSubRecord<std::uint32_t>("QNAM", 0x812) + valueSubRecord<std::uint32_t>("SCRI", 0x813)
                + subRecord("COCT", "abcd") + subRecord("RNAM", "abcd"));

        EXPECT_EQ(container.mFullName, "Chest");
        EXPECT_EQ(container.mModel.getOriginal(), "Meshes\\Chest.nif");
        EXPECT_EQ(container.mBoundRadius, 4.f);
        EXPECT_EQ(container.mDataFlags, 3);
        EXPECT_EQ(container.mWeight, 10.f);
        ASSERT_EQ(container.mInventory.size(), 2u);
        EXPECT_EQ(container.mInventory[0].item, 0x841u);
        EXPECT_EQ(container.mInventory[0].count, 5u);
        EXPECT_EQ(container.mInventory[1].item, 0x842u);
        EXPECT_EQ(container.mOpenSound.toUint32(), 0x811u);
        EXPECT_EQ(container.mCloseSound.toUint32(), 0x812u);
        EXPECT_EQ(container.mScriptId.toUint32(), 0x813u);
        expectUnknownSubrecordRefused<ESM4::Container>("CONT");
    }

    TEST(ESM4ItemRecordsTest, aDoorHasItsSoundsFlagsAndTeleport)
    {
        const ESM4::Door door = load<ESM4::Door>("DOOR",
            zString("EDID", "Door") + zString("FULL", "Door") + zString("MODL", "Meshes\\Door.nif")
                + valueSubRecord<std::uint32_t>("SCRI", 0x811) + valueSubRecord<std::uint32_t>("SNAM", 0x812)
                + valueSubRecord<std::uint32_t>("ANAM", 0x813) + valueSubRecord<std::uint32_t>("BNAM", 0x814)
                + valueSubRecord<std::uint8_t>("FNAM", ESM4::Door::Flag_AutomaticDoor | ESM4::Door::Flag_Hidden)
                + valueSubRecord<std::uint32_t>("TNAM", 0x815) + valueSubRecord<float>("MODB", 6.f)
                + subRecord("OBND", "abcd") + subRecord("KSIZ", "abcd"));

        EXPECT_EQ(door.mFullName, "Door");
        EXPECT_EQ(door.mModel.getOriginal(), "Meshes\\Door.nif");
        EXPECT_EQ(door.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(door.mOpenSound.toUint32(), 0x812u);
        EXPECT_EQ(door.mCloseSound.toUint32(), 0x813u);
        EXPECT_EQ(door.mLoopSound.toUint32(), 0x814u);
        EXPECT_EQ(door.mDoorFlags, 0x06);
        EXPECT_EQ(door.mRandomTeleport.toUint32(), 0x815u);
        EXPECT_EQ(door.mBoundRadius, 6.f);
        expectUnknownSubrecordRefused<ESM4::Door>("DOOR");
    }

    TEST(ESM4ItemRecordsTest, aKeyAndAMiscellaneousItemHaveTheirIconsSoundsAndData)
    {
        std::string data;
        append<std::uint32_t>(data, 15);
        append<float>(data, 0.25f);

        const ESM4::Key key = load<ESM4::Key>("KEYM",
            zString("EDID", "Key") + zString("FULL", "Key") + zString("MODL", "Meshes\\Key.nif")
                + zString("ICON", "icons/key.dds") + zString("MICO", "icons/key_small.dds") + subRecord("DATA", data)
                + valueSubRecord<std::uint32_t>("SCRI", 0x811) + valueSubRecord<float>("MODB", 1.f)
                + valueSubRecord<std::uint32_t>("YNAM", 0x812) + valueSubRecord<std::uint32_t>("ZNAM", 0x813)
                + subRecord("KSIZ", "abcd"));
        EXPECT_EQ(key.mFullName, "Key");
        EXPECT_EQ(key.mModel.getOriginal(), "Meshes\\Key.nif");
        EXPECT_EQ(key.mIcon, "icons/key.dds");
        EXPECT_EQ(key.mMiniIcon, "icons/key_small.dds");
        EXPECT_EQ(key.mData.value, 15u);
        EXPECT_EQ(key.mData.weight, 0.25f);
        EXPECT_EQ(key.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(key.mBoundRadius, 1.f);
        EXPECT_EQ(key.mPickUpSound.toUint32(), 0x812u);
        EXPECT_EQ(key.mDropSound.toUint32(), 0x813u);
        expectUnknownSubrecordRefused<ESM4::Key>("KEYM");

        const ESM4::MiscItem misc = load<ESM4::MiscItem>("MISC",
            zString("EDID", "Misc") + zString("FULL", "Junk") + zString("MODL", "Meshes\\Junk.nif")
                + zString("ICON", "icons/junk.dds") + zString("MICO", "icons/junk_small.dds")
                + valueSubRecord<std::uint32_t>("SCRI", 0x821) + subRecord("DATA", data)
                + valueSubRecord<float>("MODB", 2.f) + valueSubRecord<std::uint32_t>("YNAM", 0x822)
                + valueSubRecord<std::uint32_t>("ZNAM", 0x823) + subRecord("RNAM", "abcd"));
        EXPECT_EQ(misc.mFullName, "Junk");
        EXPECT_EQ(misc.mIcon, "icons/junk.dds");
        EXPECT_EQ(misc.mMiniIcon, "icons/junk_small.dds");
        EXPECT_EQ(misc.mScriptId.toUint32(), 0x821u);
        EXPECT_EQ(misc.mData.value, 15u);
        EXPECT_EQ(misc.mData.weight, 0.25f);
        EXPECT_EQ(misc.mBoundRadius, 2.f);
        EXPECT_EQ(misc.mPickUpSound.toUint32(), 0x822u);
        EXPECT_EQ(misc.mDropSound.toUint32(), 0x823u);
        expectUnknownSubrecordRefused<ESM4::MiscItem>("MISC");
    }

    TEST(ESM4ItemRecordsTest, aLandTextureHasTheHavokDataThatItsSizeSays)
    {
        // Three bytes: the material, friction and restitution
        const ESM4::LandTexture full = load<ESM4::LandTexture>("LTEX",
            zString("EDID", "Sand") + subRecord("HNAM", std::string("\x05\x0A\x0B", 3))
                + zString("ICON", "Landscape\\Sand.dds") + valueSubRecord<std::uint8_t>("SNAM", 40)
                + valueSubRecord<std::uint32_t>("GNAM", 0x811) + valueSubRecord<std::uint32_t>("GNAM", 0x812)
                + valueSubRecord<std::uint32_t>("TNAM", 0x813) + valueSubRecord<std::uint32_t>("MNAM", 0x814)
                + valueSubRecord<std::uint32_t>("INAM", 1));
        EXPECT_EQ(full.mEditorId, "Sand");
        EXPECT_EQ(full.mHavokMaterial, 5);
        EXPECT_EQ(full.mHavokFriction, 10);
        EXPECT_EQ(full.mHavokRestitution, 11);
        EXPECT_EQ(full.mTextureFile, "Landscape\\Sand.dds");
        EXPECT_EQ(full.mTextureSpecular, 40);
        ASSERT_EQ(full.mGrass.size(), 2u);
        EXPECT_EQ(full.mGrass[1].toUint32(), 0x812u);
        EXPECT_EQ(full.mTexture.toUint32(), 0x813u);
        EXPECT_EQ(full.mMaterial.toUint32(), 0x814u);
        EXPECT_EQ(full.mMaterialFlags, ESM4::LandTexture::Flag_IsSnow);

        const ESM4::LandTexture friction
            = load<ESM4::LandTexture>("LTEX", subRecord("HNAM", std::string("\x0A\x0B", 2)));
        EXPECT_EQ(friction.mHavokFriction, 10);
        EXPECT_EQ(friction.mHavokRestitution, 11);

        // Other sizes are not read
        const ESM4::LandTexture other = load<ESM4::LandTexture>("LTEX", subRecord("HNAM", std::string("\x0A", 1)));
        EXPECT_EQ(other.mHavokFriction, 0);
        expectUnknownSubrecordRefused<ESM4::LandTexture>("LTEX");
    }

    TEST(ESM4ItemRecordsTest, anIdleMarkerHasItsAnimationsWhereTheGameStoresThem)
    {
        const std::string animations = formIdData(0x851) + formIdData(0x852) + formIdData(0x853);
        const ESM4::IdleMarker oblivion = load<ESM4::IdleMarker>("IDLM",
            zString("EDID", "Marker") + zString("MODL", "Meshes\\Marker.nif") + valueSubRecord<std::uint8_t>("IDLF", 5)
                + valueSubRecord<std::uint8_t>("IDLC", 3) + valueSubRecord<float>("IDLT", 4.5f)
                + subRecord("IDLA", animations) + subRecord("OBND", "abcd") + subRecord("KSIZ", "abcd"));

        EXPECT_EQ(oblivion.mModel.getOriginal(), "Meshes\\Marker.nif");
        EXPECT_EQ(oblivion.mIdleFlags, 5);
        EXPECT_EQ(oblivion.mIdleCount, 3);
        EXPECT_EQ(oblivion.mIdleTimer, 4.5f);
        ASSERT_EQ(oblivion.mIdleAnim.size(), 3u);
        EXPECT_EQ(oblivion.mIdleAnim[2].toUint32(), 0x853u);
        expectUnknownSubrecordRefused<ESM4::IdleMarker>("IDLM");

        // Fallout 3 and New Vegas have a list that the loader does not read, and a count that is not always a byte
        const ESM4::IdleMarker fallout = loadVersionedRecord<ESM4::IdleMarker>(
            "IDLM", 0.94f, 15, valueSubRecord<std::uint8_t>("IDLC", 3) + subRecord("IDLA", animations));
        EXPECT_EQ(fallout.mIdleCount, 3);
        EXPECT_TRUE(fallout.mIdleAnim.empty());
        const ESM4::IdleMarker newVegas = loadVersionedRecord<ESM4::IdleMarker>(
            "IDLM", 1.34f, 15, subRecord("IDLC", formIdData(2)) + subRecord("IDLA", animations));
        EXPECT_EQ(newVegas.mIdleCount, 0);
        EXPECT_TRUE(newVegas.mIdleAnim.empty());
    }

    TEST(ESM4ItemRecordsTest, anIngredientHasTheNamesOfItsScriptEffectsAndTheDataOfItsSize)
    {
        std::string enchantment;
        append<std::uint32_t>(enchantment, 40);
        append<std::uint32_t>(enchantment, 2);
        std::string data;
        append<std::uint32_t>(data, 6);
        append<float>(data, 0.125f);
        ESM4::ScriptEffect effect{};
        effect.formId = 0x861;
        const ESM4::Ingredient ingredient = load<ESM4::Ingredient>("INGR",
            zString("EDID", "Herb") + zString("FULL", "Herb") + zString("FULL", "Effect") + subRecord("DATA", data)
                + zString("ICON", "icons/herb.dds") + zString("MODL", "Meshes\\Herb.nif")
                + valueSubRecord<std::uint32_t>("SCRI", 0x811) + subRecord("ENIT", enchantment)
                + valueSubRecord<float>("MODB", 1.f) + subRecord("SCIT", raw(effect)) + subRecord("EFID", "abcd")
                + subRecord("EFIT", "abcd") + subRecord("ETYP", "abcd"));

        EXPECT_EQ(ingredient.mFullName, "Herb");
        ASSERT_EQ(ingredient.mScriptEffect.size(), 1u);
        EXPECT_EQ(ingredient.mScriptEffect[0], "Effect");
        EXPECT_EQ(ingredient.mData.value, 6u);
        EXPECT_EQ(ingredient.mData.weight, 0.125f);
        EXPECT_EQ(ingredient.mIcon, "icons/herb.dds");
        EXPECT_EQ(ingredient.mModel.getOriginal(), "Meshes\\Herb.nif");
        EXPECT_EQ(ingredient.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(ingredient.mEnchantment.value, 40u);
        EXPECT_EQ(ingredient.mEnchantment.flags, 2u);
        EXPECT_EQ(ingredient.mBoundRadius, 1.f);
        EXPECT_EQ(ingredient.mEffect.formId, 0x861u);
        expectUnknownSubrecordRefused<ESM4::Ingredient>("INGR");

        // Fallout 3 has the weight only
        std::string weight;
        append<float>(weight, 0.5f);
        const ESM4::Ingredient fallout = load<ESM4::Ingredient>("INGR", subRecord("DATA", weight));
        EXPECT_EQ(fallout.mData.weight, 0.5f);
    }

    TEST(ESM4ItemRecordsTest, aPotionHasItsTextsDataAndTheSoundsOfItsItem)
    {
        std::string weight;
        append<float>(weight, 0.5f);
        std::string item;
        append<std::int32_t>(item, 25);
        append<std::uint32_t>(item, 1);
        append<std::uint32_t>(item, 0x871);
        append<float>(item, 0.5f);
        append<std::uint32_t>(item, 0x872);
        ESM4::ScriptEffect effect{};
        effect.formId = 0x873;
        const ESM4::Potion potion = load<ESM4::Potion>("ALCH",
            zString("EDID", "Potion") + zString("FULL", "Potion") + zString("MODL", "Meshes\\Potion.nif")
                + zString("ICON", "icons/potion.dds") + zString("MICO", "icons/potion_small.dds")
                + subRecord("DATA", weight) + valueSubRecord<std::uint32_t>("SCRI", 0x811)
                + valueSubRecord<float>("MODB", 3.f) + subRecord("SCIT", raw(effect)) + subRecord("ENIT", item)
                + valueSubRecord<std::uint32_t>("YNAM", 0x812) + valueSubRecord<std::uint32_t>("ZNAM", 0x813)
                + zString("DESC", "Drink") + subRecord("ETYP", "abcd"));

        EXPECT_EQ(potion.mFullName, "Potion");
        EXPECT_EQ(potion.mModel.getOriginal(), "Meshes\\Potion.nif");
        EXPECT_EQ(potion.mIcon, "icons/potion.dds");
        EXPECT_EQ(potion.mMiniIcon, "icons/potion_small.dds");
        EXPECT_EQ(potion.mData.weight, 0.5f);
        EXPECT_EQ(potion.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(potion.mBoundRadius, 3.f);
        EXPECT_EQ(potion.mEffect.formId, 0x873u);
        EXPECT_EQ(potion.mItem.value, 25);
        EXPECT_EQ(potion.mItem.flags, 1u);
        EXPECT_EQ(potion.mItem.withdrawl, 0x871u);
        EXPECT_EQ(potion.mItem.chanceAddition, 0.5f);
        EXPECT_EQ(potion.mItem.sound, 0x872u);
        EXPECT_EQ(potion.mPickUpSound.toUint32(), 0x812u);
        EXPECT_EQ(potion.mDropSound.toUint32(), 0x813u);
        expectUnknownSubrecordRefused<ESM4::Potion>("ALCH");

        // The records of Oblivion have the value and the flags only
        std::string older;
        append<std::int32_t>(older, 30);
        append<std::uint32_t>(older, 2);
        const ESM4::Potion oblivion = load<ESM4::Potion>("ALCH", subRecord("ENIT", older));
        EXPECT_EQ(oblivion.mItem.value, 30);
        EXPECT_EQ(oblivion.mItem.flags, 2u);
    }

    TEST(ESM4ItemRecordsTest, aFloraAndAFurnitureHaveTheirOwnFields)
    {
        const ESM4::Flora flora = load<ESM4::Flora>("FLOR",
            zString("EDID", "Bush") + zString("FULL", "Bush") + zString("MODL", "Meshes\\Bush.nif")
                + valueSubRecord<std::uint32_t>("SCRI", 0x811) + valueSubRecord<std::uint32_t>("PFIG", 0x812)
                + subRecord("PFPC", std::string("\x01\x02\x03\x04", 4)) + valueSubRecord<std::uint32_t>("SNAM", 0x813)
                + valueSubRecord<float>("MODB", 2.f) + subRecord("FNAM", "abcd"));
        EXPECT_EQ(flora.mFullName, "Bush");
        EXPECT_EQ(flora.mModel.getOriginal(), "Meshes\\Bush.nif");
        EXPECT_EQ(flora.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(flora.mIngredient.toUint32(), 0x812u);
        EXPECT_EQ(flora.mPercentHarvest.spring, 1);
        EXPECT_EQ(flora.mPercentHarvest.summer, 2);
        EXPECT_EQ(flora.mPercentHarvest.autumn, 3);
        EXPECT_EQ(flora.mPercentHarvest.winter, 4);
        EXPECT_EQ(flora.mSound.toUint32(), 0x813u);
        EXPECT_EQ(flora.mBoundRadius, 2.f);
        expectUnknownSubrecordRefused<ESM4::Flora>("FLOR");

        const ESM4::Furniture furniture = load<ESM4::Furniture>("FURN",
            zString("EDID", "Bed") + zString("FULL", "Bed") + zString("MODL", "Meshes\\Bed.nif")
                + valueSubRecord<std::uint32_t>("SCRI", 0x821) + valueSubRecord<std::uint32_t>("MNAM", 0x30)
                + valueSubRecord<float>("MODB", 4.f) + subRecord("WBDT", "ab") + subRecord("XMRK", "abcd"));
        EXPECT_EQ(furniture.mFullName, "Bed");
        EXPECT_EQ(furniture.mModel.getOriginal(), "Meshes\\Bed.nif");
        EXPECT_EQ(furniture.mScriptId.toUint32(), 0x821u);
        EXPECT_EQ(furniture.mActiveMarkerFlags, 0x30u);
        EXPECT_EQ(furniture.mBoundRadius, 4.f);
        expectUnknownSubrecordRefused<ESM4::Furniture>("FURN");
        // The name of a furniture that the record gives twice is the first
        const ESM4::Furniture twice
            = load<ESM4::Furniture>("FURN", zString("FULL", "First") + zString("FULL", "Second"));
        EXPECT_EQ(twice.mFullName, "First");
    }

    TEST(ESM4ItemRecordsTest, aLevelledNpcHasItsEntriesOfTheSizeThatTheGameWrites)
    {
        std::string full;
        append<std::int16_t>(full, 3);
        append<std::uint16_t>(full, 0);
        append<std::uint32_t>(full, 0x881);
        append<std::int16_t>(full, 2);
        append<std::uint16_t>(full, 0);
        std::string shorter;
        append<std::int16_t>(shorter, 7);
        append<std::uint32_t>(shorter, 0x882);
        append<std::int16_t>(shorter, 1);
        const ESM4::LevelledNpc list = load<ESM4::LevelledNpc>("LVLN",
            zString("EDID", "Crowd") + zString("MODL", "Meshes\\Crowd.nif") + valueSubRecord<std::uint8_t>("LLCT", 2)
                + valueSubRecord<std::int8_t>("LVLD", 25) + valueSubRecord<std::uint8_t>("LVLF", 3)
                + subRecord("LVLO", full) + subRecord("LVLO", shorter) + subRecord("OBND", "abcd"));

        EXPECT_EQ(list.mModel.getOriginal(), "Meshes\\Crowd.nif");
        EXPECT_EQ(list.mListCount, 2);
        EXPECT_EQ(list.chanceNone(), 25);
        EXPECT_TRUE(list.calcAllLvlLessThanPlayer());
        EXPECT_TRUE(list.calcEachItemInCount());
        // The entry of 8 bytes is read and not added: the records that have one are an oddity of Oblivion
        ASSERT_EQ(list.mLvlObject.size(), 1u);
        EXPECT_EQ(list.mLvlObject[0].level, 3);
        EXPECT_EQ(list.mLvlObject[0].item, 0x881u);
        EXPECT_EQ(list.mLvlObject[0].count, 2);
        expectUnknownSubrecordRefused<ESM4::LevelledNpc>("LVLN");
        EXPECT_THROW(load<ESM4::LevelledNpc>("LVLN", subRecord("LVLO", "abcdefghi")), std::runtime_error);
    }

    TEST(ESM4ItemRecordsTest, aSoulGemAClothingAndAScrollHaveTheirData)
    {
        std::string data;
        append<std::uint32_t>(data, 55);
        append<float>(data, 1.5f);

        const ESM4::SoulGem gem = load<ESM4::SoulGem>("SLGM",
            zString("EDID", "Gem") + zString("FULL", "Gem") + zString("MODL", "Meshes\\Gem.nif")
                + zString("ICON", "icons/gem.dds") + subRecord("DATA", data)
                + valueSubRecord<std::uint32_t>("SCRI", 0x811) + valueSubRecord<std::uint8_t>("SOUL", 3)
                + valueSubRecord<std::uint8_t>("SLCP", 4) + valueSubRecord<float>("MODB", 1.f)
                + subRecord("KSIZ", "abcd"));
        EXPECT_EQ(gem.mFullName, "Gem");
        EXPECT_EQ(gem.mIcon, "icons/gem.dds");
        EXPECT_EQ(gem.mData.value, 55u);
        EXPECT_EQ(gem.mData.weight, 1.5f);
        EXPECT_EQ(gem.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(gem.mSoul, 3);
        EXPECT_EQ(gem.mSoulCapacity, 4);
        EXPECT_EQ(gem.mBoundRadius, 1.f);
        expectUnknownSubrecordRefused<ESM4::SoulGem>("SLGM");

        const ESM4::Clothing clothing = load<ESM4::Clothing>("CLOT",
            zString("EDID", "Shirt") + zString("FULL", "Shirt") + subRecord("DATA", data)
                + valueSubRecord<std::uint32_t>("BMDT", 0x0003) + valueSubRecord<std::uint32_t>("SCRI", 0x821)
                + valueSubRecord<std::uint32_t>("ENAM", 0x822) + valueSubRecord<std::uint16_t>("ANAM", 150)
                + valueSubRecord<float>("MODB", 2.f) + zString("MODL", "Meshes\\ShirtM.nif")
                + zString("MOD2", "Meshes\\ShirtMW.nif") + zString("MOD3", "Meshes\\ShirtF.nif")
                + zString("MOD4", "Meshes\\ShirtFW.nif") + zString("ICON", "icons/shirt.dds")
                + zString("ICO2", "icons/shirtf.dds") + subRecord("MODT", "abcd") + subRecord("MO2B", "abcd")
                + subRecord("MO3T", "abcd"));
        EXPECT_EQ(clothing.mFullName, "Shirt");
        EXPECT_EQ(clothing.mData.value, 55u);
        EXPECT_EQ(clothing.mData.weight, 1.5f);
        EXPECT_EQ(clothing.mClothingFlags, 3u);
        EXPECT_EQ(clothing.mScriptId.toUint32(), 0x821u);
        EXPECT_EQ(clothing.mEnchantment.toUint32(), 0x822u);
        EXPECT_EQ(clothing.mEnchantmentPoints, 150);
        EXPECT_EQ(clothing.mBoundRadius, 2.f);
        EXPECT_EQ(clothing.mModelMale.getOriginal(), "Meshes\\ShirtM.nif");
        EXPECT_EQ(clothing.mModelMaleWorld.getOriginal(), "Meshes\\ShirtMW.nif");
        EXPECT_EQ(clothing.mModelFemale.getOriginal(), "Meshes\\ShirtF.nif");
        EXPECT_EQ(clothing.mModelFemaleWorld.getOriginal(), "Meshes\\ShirtFW.nif");
        EXPECT_EQ(clothing.mIconMale, "icons/shirt.dds");
        EXPECT_EQ(clothing.mIconFemale, "icons/shirtf.dds");
        expectUnknownSubrecordRefused<ESM4::Clothing>("CLOT");

        const ESM4::Scroll scroll = load<ESM4::Scroll>("SCRL",
            zString("EDID", "Scroll") + zString("FULL", "Scroll") + zString("DESC", "Reads") + subRecord("DATA", data)
                + zString("MODL", "Meshes\\Scroll.nif") + valueSubRecord<std::uint32_t>("YNAM", 0x831)
                + valueSubRecord<std::uint32_t>("ZNAM", 0x832) + subRecord("OBND", "abcd") + subRecord("EFID", "abcd"));
        EXPECT_EQ(scroll.mFullName, "Scroll");
        EXPECT_EQ(scroll.mText, "Reads");
        EXPECT_EQ(scroll.mData.value, 55u);
        EXPECT_EQ(scroll.mData.weight, 1.5f);
        EXPECT_EQ(scroll.mModel.getOriginal(), "Meshes\\Scroll.nif");
        EXPECT_EQ(scroll.mPickUpSound.toUint32(), 0x831u);
        EXPECT_EQ(scroll.mDropSound.toUint32(), 0x832u);
        expectUnknownSubrecordRefused<ESM4::Scroll>("SCRL");
    }

    TEST(ESM4ItemRecordsTest, anAcousticSpaceHasItsLoopsAndWhetherItIsAnInterior)
    {
        const ESM4::AcousticSpace space = load<ESM4::AcousticSpace>("ASPC",
            zString("EDID", "Room") + valueSubRecord<std::uint32_t>("ANAM", 4)
                + valueSubRecord<std::uint32_t>("SNAM", 0x811) + valueSubRecord<std::uint32_t>("SNAM", 0x812)
                + valueSubRecord<std::uint32_t>("RDAT", 0x813) + valueSubRecord<std::uint32_t>("INAM", 1)
                + subRecord("WNAM", "abcd") + subRecord("BNAM", "abcd") + subRecord("OBND", "abcd"));

        EXPECT_EQ(space.mEditorId, "Room");
        EXPECT_EQ(space.mEnvironmentType, 4u);
        ASSERT_EQ(space.mAmbientLoopSounds.size(), 2u);
        EXPECT_EQ(space.mAmbientLoopSounds[1].toUint32(), 0x812u);
        EXPECT_EQ(space.mSoundRegion.toUint32(), 0x813u);
        EXPECT_EQ(space.mIsInterior, 1u);
        // New Vegas has the same in a byte
        const ESM4::AcousticSpace byte = load<ESM4::AcousticSpace>("ASPC", valueSubRecord<std::uint8_t>("XTRI", 1));
        EXPECT_EQ(byte.mIsInterior, 1u);
        expectUnknownSubrecordRefused<ESM4::AcousticSpace>("ASPC");
    }

    TEST(ESM4ItemRecordsTest, aTextureSetHasAPathForEachOfItsMapsAndItsFlags)
    {
        const ESM4::TextureSet set = load<ESM4::TextureSet>("TXST",
            zString("EDID", "Set") + zString("FLTR", "Filter") + zString("TX00", "diffuse.dds")
                + zString("TX01", "normal.dds") + zString("TX02", "envmask.dds") + zString("TX03", "tone.dds")
                + zString("TX04", "detail.dds") + zString("TX05", "env.dds") + zString("TX06", "layer.dds")
                + zString("TX07", "smooth.dds") + zString("TX08", "spec.dds") + zString("TX09", "lighting.dds")
                + zString("TX10", "flow.dds") + valueSubRecord<std::uint16_t>("DNAM", 3) + zString("MNAM", "mat.bgsm")
                + subRecord("DODT", std::string(36, 'x')) + subRecord("OBND", "abcd"));

        EXPECT_EQ(set.mEditorId, "Set");
        EXPECT_EQ(set.mFilter, "Filter");
        EXPECT_EQ(set.mDiffuse, "diffuse.dds");
        EXPECT_EQ(set.mNormalMap, "normal.dds");
        EXPECT_EQ(set.mEnvMask, "envmask.dds");
        EXPECT_EQ(set.mToneMap, "tone.dds");
        EXPECT_EQ(set.mDetailMap, "detail.dds");
        EXPECT_EQ(set.mEnvMap, "env.dds");
        EXPECT_EQ(set.mMultiLayer, "layer.dds");
        EXPECT_EQ(set.mLighting, "lighting.dds");
        EXPECT_EQ(set.mFlow, "flow.dds");
        EXPECT_EQ(set.mDataFlags, 3);
        EXPECT_EQ(set.mMaterial, "mat.bgsm");
        expectUnknownSubrecordRefused<ESM4::TextureSet>("TXST");
    }

    TEST(ESM4ItemRecordsTest, aSoundReferenceHasItsFilesConditionAndTheDataThatItsSizeSays)
    {
        std::string loop;
        append<std::uint16_t>(loop, 0x0101);
        append<std::uint8_t>(loop, 2);
        append<std::uint8_t>(loop, 3);
        std::string data;
        append<std::int8_t>(data, -5);
        append<std::uint8_t>(data, 6);
        append<std::uint8_t>(data, 128);
        append<std::uint8_t>(data, 7);
        append<std::uint16_t>(data, 350);
        // A condition of Skyrim: the 20 bytes that the loader reads, then the run on, the reference and one more
        std::string condition;
        append<std::uint32_t>(condition, 0);
        append<float>(condition, 1.f);
        append<std::uint32_t>(condition, 0x2A);
        append<std::uint32_t>(condition, 0);
        append<std::uint32_t>(condition, 0);
        append<std::uint32_t>(condition, 2);
        append<std::uint32_t>(condition, 0x891);
        append<std::uint32_t>(condition, 0);

        const ESM4::SoundReference sound = load<ESM4::SoundReference>("SNDR",
            zString("EDID", "Sound") + subRecord("CTDA", condition) + valueSubRecord<std::uint32_t>("GNAM", 0x811)
                + valueSubRecord<std::uint32_t>("SNAM", 0x812) + valueSubRecord<std::uint32_t>("ONAM", 0x813)
                + zString("ANAM", "sound.wav") + subRecord("LNAM", loop) + subRecord("BNAM", data)
                + subRecord("CNAM", "abcd") + subRecord("FNAM", "abcd"));

        EXPECT_EQ(sound.mEditorId, "Sound");
        EXPECT_EQ(sound.mSoundCategory.toUint32(), 0x811u);
        EXPECT_EQ(sound.mSoundId.toUint32(), 0x812u);
        EXPECT_EQ(sound.mOutputModel.toUint32(), 0x813u);
        EXPECT_EQ(sound.mSoundFile, "sound.wav");
        EXPECT_EQ(sound.mLoopInfo.flags, 0x0101);
        EXPECT_EQ(sound.mLoopInfo.rumble, 3);
        EXPECT_EQ(sound.mData.frequencyAdjustment, -5);
        EXPECT_EQ(sound.mData.priority, 128);
        EXPECT_EQ(sound.mData.staticAttenuation, 350);
        EXPECT_EQ(sound.mTargetCondition.functionIndex, 0x2Au);
        EXPECT_EQ(sound.mTargetCondition.runOn, 2u);
        EXPECT_EQ(sound.mTargetCondition.reference, 0x891u);

        // The data of another size is a descriptor to start from, or is not read
        const ESM4::SoundReference base = load<ESM4::SoundReference>("SNDR", subRecord("BNAM", formIdData(0x8A1)));
        EXPECT_EQ(base.mBaseDescriptor.toUint32(), 0x8A1u);
        const ESM4::SoundReference other = load<ESM4::SoundReference>("SNDR", subRecord("BNAM", "abc"));
        EXPECT_TRUE(other.mBaseDescriptor.isZeroOrUnset());
        expectUnknownSubrecordRefused<ESM4::SoundReference>("SNDR");
    }

    TEST(ESM4ItemRecordsTest, aPathgridHasItsNodesLinksAndTheNodesOfThePlacesAroundIt)
    {
        const auto node = [](float x, float y, float z, std::uint8_t links) {
            std::string result;
            append(result, x);
            append(result, y);
            append(result, z);
            append(result, links);
            append<std::uint8_t>(result, 0);
            append<std::uint16_t>(result, 0);
            return result;
        };
        std::string links;
        append<std::int16_t>(links, 1); // the first node goes to the second
        append<std::int16_t>(links, 0); // and the second to the first
        append<std::int16_t>(links, -1); // and to nothing
        std::string foreign;
        append<std::int32_t>(foreign, 1);
        append<float>(foreign, 10.f);
        append<float>(foreign, 20.f);
        append<float>(foreign, 30.f);
        std::string object = formIdData(0x8B1);
        append<std::int32_t>(object, 0);
        append<std::int32_t>(object, 1);

        const ESM4::Pathgrid grid = load<ESM4::Pathgrid>("PGRD",
            valueSubRecord<std::int16_t>("DATA", 2) + subRecord("PGRP", node(1.f, 2.f, 4.f, 1) + node(5.f, 6.f, 7.f, 2))
                + subRecord("PGRR", links) + subRecord("PGRI", foreign) + subRecord("PGRL", object)
                + subRecord("PGAG", "abcd"));

        EXPECT_EQ(grid.mData, 2);
        ASSERT_EQ(grid.mNodes.size(), 2u);
        EXPECT_EQ(grid.mNodes[0].x, 1.f);
        EXPECT_EQ(grid.mNodes[1].z, 7.f);
        // The priority is repurposed: the nodes at an even height have none
        EXPECT_EQ(grid.mNodes[0].priority, 0);
        EXPECT_EQ(grid.mNodes[1].priority, 1);
        ASSERT_EQ(grid.mLinks.size(), 2u);
        EXPECT_EQ(grid.mLinks[0].startNode, 0);
        EXPECT_EQ(grid.mLinks[0].endNode, 1);
        EXPECT_EQ(grid.mLinks[1].startNode, 1);
        EXPECT_EQ(grid.mLinks[1].endNode, 0);
        ASSERT_EQ(grid.mForeign.size(), 1u);
        EXPECT_EQ(grid.mForeign[0].localNode, 1);
        EXPECT_EQ(grid.mForeign[0].y, 20.f);
        ASSERT_EQ(grid.mObjects.size(), 1u);
        EXPECT_EQ(grid.mObjects[0].object.toUint32(), 0x8B1u);
        ASSERT_EQ(grid.mObjects[0].linkedNodes.size(), 2u);
        EXPECT_EQ(grid.mObjects[0].linkedNodes[1], 1);
        EXPECT_FALSE(grid.mEditorId.empty());
        expectUnknownSubrecordRefused<ESM4::Pathgrid>("PGRD");

        // The nodes that the record says it has are the ones in it
        EXPECT_THROW(load<ESM4::Pathgrid>(
                         "PGRD", valueSubRecord<std::int16_t>("DATA", 3) + subRecord("PGRP", node(1.f, 2.f, 4.f, 0))),
            std::runtime_error);
    }

    TEST(ESM4ItemRecordsTest, aRoadHasItsNodesAndTheLinksThatArePointsOfThem)
    {
        const auto node = [](float x, float y, float z, std::uint8_t links) {
            std::string result;
            append(result, x);
            append(result, y);
            append(result, z);
            append(result, links);
            append<std::uint8_t>(result, 0);
            append<std::uint16_t>(result, 0);
            return result;
        };
        const auto point = [](float x, float y, float z) {
            std::string result;
            append(result, x);
            append(result, y);
            append(result, z);
            return result;
        };
        const std::string nodes = node(1.f, 2.f, 3.f, 1) + node(4.f, 5.f, 6.f, 1);

        const ESM4::Road road = load<ESM4::Road>(
            "ROAD", subRecord("PGRP", nodes) + subRecord("PGRR", point(4.f, 5.f, 6.f) + point(1.f, 2.f, 3.f)));

        ASSERT_EQ(road.mNodes.size(), 2u);
        EXPECT_EQ(road.mNodes[1].y, 5.f);
        ASSERT_EQ(road.mLinks.size(), 2u);
        EXPECT_EQ(road.mLinks[0].startNode, 0);
        EXPECT_EQ(road.mLinks[0].endNode, 1);
        EXPECT_EQ(road.mLinks[1].startNode, 1);
        EXPECT_EQ(road.mLinks[1].endNode, 0);
        EXPECT_FALSE(road.mEditorId.empty());

        // A link to a place that is not one of the nodes cannot be followed
        EXPECT_THROW(load<ESM4::Road>("ROAD",
                         subRecord("PGRP", nodes) + subRecord("PGRR", point(9.f, 9.f, 9.f) + point(1.f, 2.f, 3.f))),
            std::runtime_error);
        expectUnknownSubrecordRefused<ESM4::Road>("ROAD");
    }
}
