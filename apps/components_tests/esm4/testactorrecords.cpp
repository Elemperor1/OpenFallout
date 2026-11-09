#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadweap.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

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

    template <class T>
    std::string raw(const T& value)
    {
        return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    std::string formId(std::uint32_t id)
    {
        std::string result;
        append(result, id);
        return result;
    }

    std::string item(std::uint32_t id, std::uint32_t count)
    {
        std::string result;
        append(result, id);
        append(result, count);
        return result;
    }

    std::string faction(std::uint32_t id, std::int8_t rank)
    {
        std::string result;
        append(result, id);
        append(result, rank);
        result.append(3, '\0');
        return result;
    }

    std::string floats(std::size_t count, float first)
    {
        std::string result;
        for (std::size_t i = 0; i < count; ++i)
            append(result, first + static_cast<float>(i));
        return result;
    }

    TEST(ESM4ActorRecordsTest, aCharacterOfOblivionHasItsListsAndTheLayoutOfItsOwnGame)
    {
        // The skills (21 bytes), the health and the attributes
        std::string stats = bytePattern(21, 10);
        append<std::uint32_t>(stats, 150);
        stats += bytePattern(8, 40);
        ASSERT_EQ(stats.size(), 33u);
        std::string ai;
        append<std::uint8_t>(ai, 10);
        append<std::uint8_t>(ai, 20);
        append<std::uint8_t>(ai, 30);
        append<std::uint8_t>(ai, 40);
        append<std::uint32_t>(ai, 0x00010002);
        append<std::uint8_t>(ai, 5);
        append<std::uint8_t>(ai, 6);
        append<std::uint16_t>(ai, 0);
        ASSERT_EQ(ai.size(), 12u);
        std::string config;
        append<std::uint32_t>(config, 0x00000001); // flags
        append<std::uint16_t>(config, 7); // base spell
        append<std::uint16_t>(config, 8); // fatigue
        append<std::uint16_t>(config, 9); // barter gold
        append<std::int16_t>(config, 4); // level
        append<std::uint16_t>(config, 1);
        append<std::uint16_t>(config, 2);
        ASSERT_EQ(config.size(), 16u);

        const ESM4::Npc npc = load<ESM4::Npc>("NPC_",
            zString("EDID", "Guard") + zString("FULL", "Guard") + zString("MODL", "Meshes\\Guard.nif")
                + subRecord("CNTO", item(0x811, 2)) + subRecord("CNTO", item(0x812, 1))
                + subRecord("SPLO", formId(0x821)) + subRecord("SPLO", formId(0x822)) + subRecord("PKID", formId(0x831))
                + subRecord("PKID", formId(0x832)) + subRecord("SNAM", faction(0x841, 3))
                + subRecord("SNAM", faction(0x842, -1)) + subRecord("RNAM", formId(0x851))
                + subRecord("CNAM", formId(0x852)) + subRecord("HNAM", formId(0x853)) + subRecord("ENAM", formId(0x854))
                + subRecord("INAM", formId(0x855)) + subRecord("SCRI", formId(0x856)) + subRecord("AIDT", ai)
                + subRecord("ACBS", config) + subRecord("DATA", stats) + subRecord("ZNAM", formId(0x861))
                + subRecord("CSCR", formId(0x862)) + subRecord("CSDI", formId(0x863))
                + valueSubRecord<std::uint8_t>("CSDC", 25) + valueSubRecord<float>("MODB", 7.5f)
                + subRecord("KFFZ", std::string("Idle.kf\0Walk.kf\0", 16)) + valueSubRecord<float>("LNAM", 0.5f)
                + subRecord("HCLR", std::string("\x01\x02\x03\x04", 4)) + subRecord("TPLT", formId(0x871))
                + valueSubRecord<std::int16_t>("FNAM", 12) + subRecord("PNAM", formId(0x881))
                + subRecord("PNAM", formId(0x882)) + subRecord("HCLF", formId(0x891)) + subRecord("BCLF", formId(0x892))
                + valueSubRecord<std::uint32_t>("COCT", 3) + subRecord("DOFT", formId(0x8A1))
                + subRecord("SOFT", formId(0x8A2)) + subRecord("DPLT", formId(0x8A3)));

        EXPECT_TRUE(npc.mIsTES4);
        EXPECT_FALSE(npc.mIsFONV);
        EXPECT_EQ(npc.mEditorId, "Guard");
        EXPECT_EQ(npc.mFullName, "Guard");
        EXPECT_EQ(npc.mModel.getOriginal(), "Meshes\\Guard.nif");
        ASSERT_EQ(npc.mInventory.size(), 2u);
        EXPECT_EQ(npc.mInventory[0].item, 0x811u);
        EXPECT_EQ(npc.mInventory[0].count, 2u);
        ASSERT_EQ(npc.mSpell.size(), 2u);
        EXPECT_EQ(npc.mSpell[1].toUint32(), 0x822u);
        ASSERT_EQ(npc.mAIPackages.size(), 2u);
        EXPECT_EQ(npc.mAIPackages[1].toUint32(), 0x832u);
        ASSERT_EQ(npc.mFactions.size(), 2u);
        EXPECT_EQ(npc.mFactions[0].faction, 0x841u);
        EXPECT_EQ(npc.mFactions[0].rank, 3);
        EXPECT_EQ(npc.mFaction.faction, 0x842u);
        EXPECT_EQ(npc.mFaction.rank, -1);
        EXPECT_EQ(npc.mRace.toUint32(), 0x851u);
        EXPECT_EQ(npc.mClass.toUint32(), 0x852u);
        EXPECT_EQ(npc.mHair.toUint32(), 0x853u);
        EXPECT_EQ(npc.mEyes.toUint32(), 0x854u);
        EXPECT_EQ(npc.mDeathItem.toUint32(), 0x855u);
        EXPECT_EQ(npc.mScriptId.toUint32(), 0x856u);
        EXPECT_EQ(npc.mAIData.aggression, 10);
        EXPECT_EQ(npc.mAIData.confidence, 20);
        EXPECT_EQ(npc.mAIData.aiFlags, 0x00010002u);
        EXPECT_EQ(npc.mAIData.trainSkill, 5);
        EXPECT_EQ(npc.mBaseConfig.tes4.flags, 1u);
        EXPECT_EQ(npc.mBaseConfig.tes4.fatigue, 8);
        EXPECT_EQ(npc.mBaseConfig.tes4.levelOrOffset, 4);
        EXPECT_EQ(npc.mData.skills.armorer, 10);
        EXPECT_EQ(npc.mData.health, 150u);
        EXPECT_EQ(npc.mData.attribs.strength, 40);
        EXPECT_EQ(npc.mData.attribs.luck, 47);
        EXPECT_EQ(npc.mCombatStyle.toUint32(), 0x861u);
        EXPECT_EQ(npc.mSoundBase.toUint32(), 0x862u);
        EXPECT_EQ(npc.mSound.toUint32(), 0x863u);
        EXPECT_EQ(npc.mSoundChance, 25);
        EXPECT_EQ(npc.mBoundRadius, 7.5f);
        ASSERT_EQ(npc.mKf.size(), 2u);
        EXPECT_EQ(npc.mKf[0], "Idle.kf");
        EXPECT_EQ(npc.mKf[1], "Walk.kf");
        EXPECT_EQ(npc.mHairLength, 0.5f);
        EXPECT_EQ(npc.mHairColour.red, 1);
        EXPECT_EQ(npc.mHairColour.green, 2);
        EXPECT_EQ(npc.mHairColour.blue, 3);
        EXPECT_EQ(npc.mHairColour.custom, 4);
        EXPECT_EQ(npc.mBaseTemplate.toUint32(), 0x871u);
        EXPECT_EQ(npc.mFgRace, 12);
        ASSERT_EQ(npc.mHeadParts.size(), 2u);
        EXPECT_EQ(npc.mHeadParts[1].toUint32(), 0x882u);
        EXPECT_EQ(npc.mHairColourId.toUint32(), 0x891u);
        EXPECT_EQ(npc.mBeardColourId.toUint32(), 0x892u);
        EXPECT_EQ(npc.mDefaultOutfit.toUint32(), 0x8A1u);
        EXPECT_EQ(npc.mSleepOutfit.toUint32(), 0x8A2u);
        EXPECT_EQ(npc.mDefaultPkg.toUint32(), 0x8A3u);
    }

    TEST(ESM4ActorRecordsTest, aCharacterTakesTheFacegenCoefficientsInTheSizesThatThereAre)
    {
        const ESM4::Npc npc = load<ESM4::Npc>("NPC_",
            subRecord("FGGS", floats(50, 1.f)) + subRecord("FGGA", floats(30, 100.f))
                + subRecord("FGTS", floats(50, 200.f)));

        ASSERT_EQ(npc.mSymShapeModeCoefficients.size(), 50u);
        EXPECT_EQ(npc.mSymShapeModeCoefficients[49], 50.f);
        ASSERT_EQ(npc.mAsymShapeModeCoefficients.size(), 30u);
        EXPECT_EQ(npc.mAsymShapeModeCoefficients[0], 100.f);
        ASSERT_EQ(npc.mSymTextureModeCoefficients.size(), 50u);
        EXPECT_EQ(npc.mSymTextureModeCoefficients[1], 201.f);

        // An empty one is none, and one of another size is not understood
        const ESM4::Npc none = load<ESM4::Npc>("NPC_", subRecord("FGGS", ""));
        EXPECT_TRUE(none.mSymShapeModeCoefficients.empty());
        EXPECT_THROW(load<ESM4::Npc>("NPC_", subRecord("FGGA", floats(29, 1.f))), std::runtime_error);
    }

    TEST(ESM4ActorRecordsTest, aCharacterOfAnotherSizeOfSubrecordKeepsWhatItWasGiven)
    {
        // Layouts that the loader does not know are skipped
        const ESM4::Npc npc = load<ESM4::Npc>("NPC_",
            subRecord("AIDT", "abcdefgh") + subRecord("ACBS", "abcdefghijklmnop12") + subRecord("DATA", "")
                + subRecord("DATA", "abc") + subRecord("VTCK", "abcdefgh") + subRecord("SNAM", faction(0x841, 2)));

        EXPECT_EQ(npc.mAIData.aggression, 0);
        EXPECT_FALSE(npc.mFalloutStats.mKnown);
        EXPECT_TRUE(npc.mVoice.isZeroOrUnset());
        ASSERT_EQ(npc.mFactions.size(), 1u);

        // Five bytes are a faction without the padding: the rank of the Fallout 4
        std::string small;
        append<std::uint32_t>(small, 0x851);
        append<std::int8_t>(small, 4);
        const ESM4::Npc fallout4 = load<ESM4::Npc>("NPC_", subRecord("SNAM", small));
        ASSERT_EQ(fallout4.mFactions.size(), 1u);
        EXPECT_EQ(fallout4.mFactions[0].faction, 0x851u);
        EXPECT_EQ(fallout4.mFactions[0].rank, 4);

        EXPECT_THROW(load<ESM4::Npc>("NPC_", zString("ZZZZ", "Unknown")), std::runtime_error);
    }

    TEST(ESM4ActorRecordsTest, aCharacterTakesFromItsTemplateWhatItsFlagsSay)
    {
        // The flags of Oblivion's and Skyrim's layouts are in other places, and Fallout's are with the karma
        std::string fallout;
        append<std::uint32_t>(fallout, 0);
        append<std::uint16_t>(fallout, 0);
        append<std::uint16_t>(fallout, 0);
        append<std::int16_t>(fallout, 1);
        append<std::uint16_t>(fallout, 0);
        append<std::uint16_t>(fallout, 0);
        append<std::uint16_t>(fallout, 100);
        append<float>(fallout, 0.f);
        append<std::int16_t>(fallout, 0);
        append<std::uint16_t>(fallout, 0x0102);
        ASSERT_EQ(fallout.size(), 24u);
        const ESM4::Npc npc = loadVersionedRecord<ESM4::Npc>("NPC_", 1.34f, 15, subRecord("ACBS", fallout));

        EXPECT_TRUE(npc.mIsFONV);
        EXPECT_FALSE(npc.mIsTES4);
        EXPECT_TRUE(npc.takesFromTemplate(0x0002));
        EXPECT_TRUE(npc.takesFromTemplate(0x0100));
        EXPECT_FALSE(npc.takesFromTemplate(0x0004));
        EXPECT_EQ(npc.mBaseConfig.fo3.speedMultiplier, 100);

        // The layout of Skyrim is of 24 bytes too, but the flags are four bytes before
        std::string skyrimConfig(24, '\0');
        const std::uint16_t skyrimFlags = 0x0100;
        std::memcpy(skyrimConfig.data() + 18, &skyrimFlags, sizeof(skyrimFlags));
        const ESM4::Npc skyrim = loadVersionedRecord<ESM4::Npc>("NPC_", 1.7f, 44, subRecord("ACBS", skyrimConfig));
        EXPECT_FALSE(skyrim.mIsFONV);
        EXPECT_TRUE(skyrim.takesFromTemplate(0x0100));
        EXPECT_FALSE(skyrim.takesFromTemplate(0x0002));

        // The layout of Fallout 4 is 20 bytes, with the flags in another place
        std::string fo4(20, '\0');
        const std::uint16_t templateFlags = 0x0010;
        std::memcpy(fo4.data() + 14, &templateFlags, sizeof(templateFlags));
        const ESM4::Npc newest = loadVersionedRecord<ESM4::Npc>("NPC_", 0.95f, 131, subRecord("ACBS", fo4));
        EXPECT_TRUE(newest.mIsFO4);
        EXPECT_TRUE(newest.takesFromTemplate(0x0010));
        EXPECT_FALSE(newest.takesFromTemplate(0x0001));
    }

    TEST(ESM4ActorRecordsTest, aCreatureOfOblivionHasTheDataOfItsLayoutAndItsAnimationFiles)
    {
        std::string config;
        append<std::uint32_t>(config, 0x00000002);
        append<std::uint16_t>(config, 3);
        append<std::uint16_t>(config, 4);
        append<std::uint16_t>(config, 5);
        append<std::int16_t>(config, 6);
        append<std::uint16_t>(config, 7);
        append<std::uint16_t>(config, 8);
        ASSERT_EQ(config.size(), 16u);
        ESM4::Creature::Data data{};
        data.combat = 40;
        data.magic = 50;
        data.stealth = 60;
        data.soul = 3;
        data.health = 200;
        data.damage = 15;
        data.attribs.strength = 70;
        std::string ai;
        append<std::uint32_t>(ai, 0x01020304);
        append<std::uint32_t>(ai, 0x05060708);
        append<std::uint32_t>(ai, 0x090A0B0C);

        const ESM4::Creature creature = load<ESM4::Creature>("CREA",
            zString("EDID", "Rat") + zString("FULL", "Rat") + zString("MODL", "Creatures\\Rat\\Rat.nif")
                + subRecord("CNTO", item(0x811, 3)) + subRecord("SPLO", formId(0x821))
                + subRecord("PKID", formId(0x831)) + subRecord("SNAM", faction(0x841, 1))
                + subRecord("INAM", formId(0x851)) + subRecord("SCRI", formId(0x852)) + subRecord("AIDT", ai)
                + subRecord("ACBS", config) + subRecord("DATA", raw(data)) + subRecord("ZNAM", formId(0x861))
                + subRecord("CSCR", formId(0x862)) + subRecord("CSDI", formId(0x863))
                + valueSubRecord<std::uint8_t>("CSDC", 30) + valueSubRecord<float>("BNAM", 1.5f)
                + valueSubRecord<float>("TNAM", 2.5f) + valueSubRecord<float>("WNAM", 3.5f)
                + valueSubRecord<float>("MODB", 4.5f) + zString("NAM0", "Spray.nif") + zString("NAM1", "Decal.nif")
                + subRecord("NIFZ", std::string("Body.nif\0Head.nif\0", 18))
                + subRecord("KFFZ", std::string("Idle.kf\0", 8)) + valueSubRecord<std::uint32_t>("NIFT", 5)
                + valueSubRecord<std::uint32_t>("NIFT", 0) + subRecord("NIFT", "abc") + subRecord("MODT", "abcd"));

        EXPECT_EQ(creature.mEditorId, "Rat");
        EXPECT_EQ(creature.mModel.getOriginal(), "Creatures\\Rat\\Rat.nif");
        ASSERT_EQ(creature.mInventory.size(), 1u);
        EXPECT_EQ(creature.mInventory[0].count, 3u);
        ASSERT_EQ(creature.mSpell.size(), 1u);
        ASSERT_EQ(creature.mAIPackages.size(), 1u);
        ASSERT_EQ(creature.mFactions.size(), 1u);
        EXPECT_EQ(creature.mDeathItem.toUint32(), 0x851u);
        EXPECT_EQ(creature.mScriptId.toUint32(), 0x852u);
        // 12 bytes are the layout of Oblivion
        EXPECT_EQ(creature.mAIData.aggression, 0x04);
        EXPECT_EQ(creature.mBaseConfig.tes4.flags, 2u);
        EXPECT_EQ(creature.mBaseConfig.tes4.levelOrOffset, 6);
        EXPECT_EQ(creature.mData.combat, 40);
        EXPECT_EQ(creature.mData.health, 200);
        EXPECT_EQ(creature.mData.damage, 15);
        EXPECT_EQ(creature.mData.attribs.strength, 70);
        EXPECT_EQ(creature.mCombatStyle.toUint32(), 0x861u);
        EXPECT_EQ(creature.mSoundBase.toUint32(), 0x862u);
        EXPECT_EQ(creature.mSound.toUint32(), 0x863u);
        EXPECT_EQ(creature.mSoundChance, 30);
        EXPECT_EQ(creature.mBaseScale, 1.5f);
        EXPECT_EQ(creature.mTurningSpeed, 2.5f);
        EXPECT_EQ(creature.mFootWeight, 3.5f);
        EXPECT_EQ(creature.mBoundRadius, 4.5f);
        EXPECT_EQ(creature.mBloodSpray, "Spray.nif");
        EXPECT_EQ(creature.mBloodDecal, "Decal.nif");
        ASSERT_EQ(creature.mNif.size(), 2u);
        EXPECT_EQ(creature.mNif[1], "Head.nif");
        ASSERT_EQ(creature.mKf.size(), 1u);
        EXPECT_EQ(creature.mKf[0], "Idle.kf");
        EXPECT_THROW(load<ESM4::Creature>("CREA", zString("ZZZZ", "Unknown")), std::runtime_error);
    }

    TEST(ESM4ActorRecordsTest, aCreatureOfFallout3HasItsStatsInTheOtherLayout)
    {
        std::string stats(17, '\0');
        stats[0] = 4; // type
        stats[1] = 11; // combat
        stats[2] = 12; // magic
        stats[3] = 13; // stealth
        const std::uint16_t health = 300;
        std::memcpy(stats.data() + 4, &health, sizeof(health));
        const std::uint16_t damage = 25;
        std::memcpy(stats.data() + 8, &damage, sizeof(damage));
        for (int i = 0; i < 7; ++i)
            stats[10 + i] = static_cast<char>(20 + i);
        std::string ai(20, '\0');
        ai[0] = 2; // aggression
        std::string config(24, '\0');
        const std::int16_t level = 9;
        std::memcpy(config.data() + 8, &level, sizeof(level));

        const ESM4::Creature creature = loadVersionedRecord<ESM4::Creature>("CREA", 0.94f, 15,
            subRecord("DATA", stats) + subRecord("AIDT", ai) + subRecord("ACBS", config)
                + subRecord("TPLT", formId(0x871)) + subRecord("PNAM", formId(0x881)) + subRecord("PNAM", formId(0x882))
                + subRecord("VTCK", formId(0x891)) + valueSubRecord<std::uint8_t>("RNAM", 90)
                + subRecord("EAMT", "abcd"));

        EXPECT_TRUE(creature.mIsFONV);
        EXPECT_EQ(creature.mCreatureType, 4);
        EXPECT_TRUE(creature.mFalloutStats.mKnown);
        EXPECT_EQ(creature.mFalloutStats.mCombatSkill, 11);
        EXPECT_EQ(creature.mFalloutStats.mMagicSkill, 12);
        EXPECT_EQ(creature.mFalloutStats.mStealthSkill, 13);
        EXPECT_EQ(creature.mFalloutStats.mHealth, 300);
        EXPECT_EQ(creature.mFalloutStats.mDamage, 25);
        EXPECT_EQ(creature.mFalloutStats.mAttributes[0], 20);
        EXPECT_EQ(creature.mFalloutStats.mAttributes[6], 26);
        EXPECT_EQ(creature.mFalloutAI.mAggression, 2);
        EXPECT_EQ(creature.mBaseConfig.fo3.levelOrMult, 9);
        EXPECT_EQ(creature.mBaseTemplate.toUint32(), 0x871u);
        ASSERT_EQ(creature.mBodyParts.size(), 2u);
        EXPECT_EQ(creature.mBodyParts[1].toUint32(), 0x882u);
        EXPECT_EQ(creature.mVoice.toUint32(), 0x891u);
        EXPECT_EQ(creature.mAttackReach, 90);

        // Values of other sizes are not read
        const ESM4::Creature other = loadVersionedRecord<ESM4::Creature>(
            "CREA", 0.94f, 15, subRecord("VTCK", "abcdefgh") + subRecord("RNAM", "abcd"));
        EXPECT_TRUE(other.mVoice.isZeroOrUnset());
        EXPECT_EQ(other.mAttackReach, 0);
    }

    TEST(ESM4ActorRecordsTest, anArmorHasTheDataThatItsSizeSaysAndTheFlagsOfItsGame)
    {
        // Oblivion: the armor rating, the value, the health and the weight
        std::string oblivionData;
        append<std::uint16_t>(oblivionData, 35);
        append<std::uint32_t>(oblivionData, 100);
        append<std::uint32_t>(oblivionData, 250);
        append<float>(oblivionData, 12.f);
        ASSERT_EQ(oblivionData.size(), 14u);
        std::string flags;
        append<std::uint32_t>(flags, 0x00050004);
        const ESM4::Armor oblivion = load<ESM4::Armor>("ARMO",
            zString("EDID", "Cuirass") + zString("FULL", "Cuirass") + subRecord("DATA", oblivionData)
                + subRecord("BMDT", flags) + zString("MODL", "Meshes\\CuirassM.nif")
                + zString("MOD2", "Meshes\\CuirassMW.nif") + zString("MOD3", "Meshes\\CuirassF.nif")
                + zString("MOD4", "Meshes\\CuirassFW.nif") + zString("ICON", "icons/cuirass.dds")
                + zString("MICO", "icons/cuirass_small.dds") + zString("ICO2", "icons/cuirassf.dds")
                + zString("MIC2", "icons/cuirassf_small.dds") + subRecord("SCRI", formId(0x811))
                + valueSubRecord<std::uint16_t>("ANAM", 120) + subRecord("ENAM", formId(0x812))
                + valueSubRecord<float>("MODB", 5.f) + zString("DESC", "Heavy") + subRecord("YNAM", formId(0x813))
                + subRecord("ZNAM", formId(0x814)));

        EXPECT_EQ(oblivion.mFullName, "Cuirass");
        EXPECT_EQ(oblivion.mData.armor, 35);
        EXPECT_EQ(oblivion.mData.value, 100u);
        EXPECT_EQ(oblivion.mData.health, 250u);
        EXPECT_EQ(oblivion.mData.weight, 12.f);
        EXPECT_EQ(oblivion.mArmorFlags, 0x00050004u);
        EXPECT_EQ(oblivion.mGeneralFlags, ESM4::Armor::TYPE_TES4 | 0x05u);
        EXPECT_EQ(oblivion.mModelMale.getOriginal(), "Meshes\\CuirassM.nif");
        EXPECT_EQ(oblivion.mModelMaleWorld.getOriginal(), "Meshes\\CuirassMW.nif");
        EXPECT_EQ(oblivion.mModelFemale.getOriginal(), "Meshes\\CuirassF.nif");
        EXPECT_EQ(oblivion.mModelFemaleWorld.getOriginal(), "Meshes\\CuirassFW.nif");
        EXPECT_EQ(oblivion.mIconMale, "icons/cuirass.dds");
        EXPECT_EQ(oblivion.mMiniIconMale, "icons/cuirass_small.dds");
        EXPECT_EQ(oblivion.mIconFemale, "icons/cuirassf.dds");
        EXPECT_EQ(oblivion.mMiniIconFemale, "icons/cuirassf_small.dds");
        EXPECT_EQ(oblivion.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(oblivion.mEnchantmentPoints, 120);
        EXPECT_EQ(oblivion.mEnchantment.toUint32(), 0x812u);
        EXPECT_EQ(oblivion.mBoundRadius, 5.f);
        EXPECT_EQ(oblivion.mText, "Heavy");
        EXPECT_EQ(oblivion.mPickUpSound.toUint32(), 0x813u);
        EXPECT_EQ(oblivion.mDropSound.toUint32(), 0x814u);

        // Fallout: the value, the health and the weight, and the flags of the body parts and the general ones
        std::string falloutData;
        append<std::uint32_t>(falloutData, 70);
        append<std::uint32_t>(falloutData, 300);
        append<float>(falloutData, 8.f);
        std::string falloutFlags;
        append<std::uint32_t>(falloutFlags, 0x00000004);
        append<std::uint32_t>(falloutFlags, 0xAABBCC03);
        const ESM4::Armor fallout
            = load<ESM4::Armor>("ARMO", subRecord("DATA", falloutData) + subRecord("BMDT", falloutFlags));
        EXPECT_EQ(fallout.mData.value, 70u);
        EXPECT_EQ(fallout.mData.health, 300u);
        EXPECT_EQ(fallout.mData.weight, 8.f);
        EXPECT_EQ(fallout.mArmorFlags, 4u);
        EXPECT_EQ(fallout.mGeneralFlags, ESM4::Armor::TYPE_FO3 | 0x03u);

        // Skyrim: the value and the weight
        std::string skyrimData;
        append<std::uint32_t>(skyrimData, 55);
        append<float>(skyrimData, 4.f);
        const ESM4::Armor skyrim = load<ESM4::Armor>("ARMO", subRecord("DATA", skyrimData));
        EXPECT_EQ(skyrim.mData.value, 55u);
        EXPECT_EQ(skyrim.mData.weight, 4.f);

        // Another size is not read
        const ESM4::Armor other = load<ESM4::Armor>("ARMO", subRecord("DATA", "abcde"));
        EXPECT_EQ(other.mData.value, 0u);
    }

    TEST(ESM4ActorRecordsTest, anArmorOfSkyrimHasItsBodyTemplateInTheLayoutThatItHas)
    {
        std::string longer;
        append<std::uint32_t>(longer, 0x00000030); // flags
        append<std::uint32_t>(longer, 0x00000002); // extra flags: the weight class goes to the general flags
        append<std::uint32_t>(longer, 1); // skill
        const ESM4::Armor bodt = load<ESM4::Armor>("ARMO", subRecord("BODT", longer));
        EXPECT_EQ(bodt.mArmorFlags, 0x30u);
        EXPECT_EQ(bodt.mGeneralFlags, ESM4::Armor::TYPE_TES5 | (2u << 3) | 1u);

        std::string shorter;
        append<std::uint32_t>(shorter, 0x00000040);
        append<std::uint32_t>(shorter, 2);
        const ESM4::Armor shortBodt = load<ESM4::Armor>("ARMO", subRecord("BODT", shorter));
        EXPECT_EQ(shortBodt.mArmorFlags, 0x40u);
        EXPECT_EQ(shortBodt.mGeneralFlags, ESM4::Armor::TYPE_TES5 | 2u);

        const ESM4::Armor bod2 = load<ESM4::Armor>("ARMO", subRecord("BOD2", shorter));
        EXPECT_EQ(bod2.mArmorFlags, 0x40u);
        EXPECT_EQ(bod2.mGeneralFlags, ESM4::Armor::TYPE_TES5 | 2u);

        // Fallout 4 has the flags only
        const ESM4::Armor fo4 = load<ESM4::Armor>("ARMO", subRecord("BOD2", formId(0x80)));
        EXPECT_EQ(fo4.mArmorFlags, 0x80u);
        const ESM4::Armor skipped = load<ESM4::Armor>("ARMO", subRecord("BOD2", "abc"));
        EXPECT_EQ(skipped.mArmorFlags, 0u);
    }

    TEST(ESM4ActorRecordsTest, anArmorHasItsAddOnsAsFormIdsInTheOrderOfTheirIndexes)
    {
        const auto indexed = [](std::uint16_t index) {
            std::string result;
            append(result, index);
            return result;
        };
        // Skyrim: one after the other
        const ESM4::Armor skyrim
            = load<ESM4::Armor>("ARMO", subRecord("MODL", formId(0x811)) + subRecord("MODL", formId(0x812)));
        ASSERT_EQ(skyrim.mAddOns.size(), 2u);
        EXPECT_EQ(skyrim.mAddOns[1].toUint32(), 0x812u);

        // Fallout 4: the index comes first, and the list has holes
        const ESM4::Armor fo4 = load<ESM4::Armor>("ARMO",
            subRecord("INDX", indexed(2)) + subRecord("MODL", formId(0x821)) + subRecord("INDX", indexed(0))
                + subRecord("MODL", formId(0x822)));
        ASSERT_EQ(fo4.mAddOns.size(), 3u);
        EXPECT_EQ(fo4.mAddOns[2].toUint32(), 0x821u);
        EXPECT_EQ(fo4.mAddOns[0].toUint32(), 0x822u);
        EXPECT_TRUE(fo4.mAddOns[1].isZeroOrUnset());
    }

    TEST(ESM4ActorRecordsTest, aWeaponHasTheDataThatItsSizeSays)
    {
        std::string oblivion;
        append<std::uint32_t>(oblivion, 2); // type
        append<float>(oblivion, 1.25f); // speed
        append<float>(oblivion, 1.5f); // reach
        append<std::uint32_t>(oblivion, 1); // flags
        append<std::uint32_t>(oblivion, 200); // value
        append<std::uint32_t>(oblivion, 400); // health
        append<float>(oblivion, 9.f); // weight
        append<std::uint16_t>(oblivion, 13); // damage
        const ESM4::Weapon sword = load<ESM4::Weapon>("WEAP",
            zString("EDID", "Sword") + zString("FULL", "Sword") + subRecord("DATA", oblivion)
                + zString("MODL", "Meshes\\Sword.nif") + zString("ICON", "icons/sword.dds")
                + zString("MICO", "icons/sword_small.dds") + subRecord("SCRI", formId(0x811))
                + valueSubRecord<std::uint16_t>("ANAM", 90) + subRecord("ENAM", formId(0x812))
                + valueSubRecord<float>("MODB", 3.f) + zString("DESC", "Sharp") + subRecord("YNAM", formId(0x813))
                + subRecord("ZNAM", formId(0x814)));

        EXPECT_EQ(sword.mFullName, "Sword");
        EXPECT_EQ(sword.mData.type, 2u);
        EXPECT_EQ(sword.mData.speed, 1.25f);
        EXPECT_EQ(sword.mData.reach, 1.5f);
        EXPECT_EQ(sword.mData.flags, 1u);
        EXPECT_EQ(sword.mData.value, 200u);
        EXPECT_EQ(sword.mData.health, 400u);
        EXPECT_EQ(sword.mData.weight, 9.f);
        EXPECT_EQ(sword.mData.damage, 13);
        EXPECT_EQ(sword.mModel.getOriginal(), "Meshes\\Sword.nif");
        EXPECT_EQ(sword.mIcon, "icons/sword.dds");
        EXPECT_EQ(sword.mMiniIcon, "icons/sword_small.dds");
        EXPECT_EQ(sword.mScriptId.toUint32(), 0x811u);
        EXPECT_EQ(sword.mEnchantmentPoints, 90);
        EXPECT_EQ(sword.mEnchantment.toUint32(), 0x812u);
        EXPECT_EQ(sword.mBoundRadius, 3.f);
        EXPECT_EQ(sword.mText, "Sharp");
        EXPECT_EQ(sword.mPickUpSound.toUint32(), 0x813u);
        EXPECT_EQ(sword.mDropSound.toUint32(), 0x814u);

        // Ten bytes are the value, the weight and the damage
        std::string shorter;
        append<std::uint32_t>(shorter, 60);
        append<float>(shorter, 2.f);
        append<std::uint16_t>(shorter, 5);
        const ESM4::Weapon staff = load<ESM4::Weapon>("WEAP", subRecord("DATA", shorter));
        EXPECT_EQ(staff.mData.value, 60u);
        EXPECT_EQ(staff.mData.weight, 2.f);
        EXPECT_EQ(staff.mData.damage, 5);

        // Fifteen are those of Fallout 3, with the health and the size of a clip
        std::string fallout;
        append<std::uint32_t>(fallout, 300);
        append<std::uint32_t>(fallout, 800);
        append<float>(fallout, 5.f);
        append<std::uint16_t>(fallout, 24);
        append<std::uint8_t>(fallout, 12);
        const ESM4::Weapon pistol = load<ESM4::Weapon>("WEAP", subRecord("DATA", fallout));
        EXPECT_EQ(pistol.mData.value, 300u);
        EXPECT_EQ(pistol.mData.health, 800u);
        EXPECT_EQ(pistol.mData.weight, 5.f);
        EXPECT_EQ(pistol.mData.damage, 24);
        EXPECT_EQ(pistol.mData.clipSize, 12);

        // And New Vegas has them whatever the size
        const ESM4::Weapon newVegas = loadVersionedRecord<ESM4::Weapon>("WEAP", 1.34f, 15, subRecord("DATA", fallout));
        EXPECT_EQ(newVegas.mData.value, 300u);
        EXPECT_EQ(newVegas.mData.clipSize, 12);
        EXPECT_THROW(load<ESM4::Weapon>("WEAP", zString("ZZZZ", "Unknown")), std::runtime_error);
    }

    TEST(ESM4ActorRecordsTest, aWeaponOfNewVegasHasItsItemModsInTheirSlots)
    {
        const ESM4::Weapon weapon = loadVersionedRecord<ESM4::Weapon>("WEAP", 1.34f, 15,
            subRecord("WMI1", formId(0x811)) + subRecord("WMI2", formId(0x812)) + subRecord("WMI3", formId(0x813)));

        EXPECT_EQ(weapon.mModForm[0].toUint32(), 0x811u);
        EXPECT_EQ(weapon.mModForm[1].toUint32(), 0x812u);
        EXPECT_EQ(weapon.mModForm[2].toUint32(), 0x813u);

        // A subrecord of another size is not an item mod
        const ESM4::Weapon other = loadVersionedRecord<ESM4::Weapon>("WEAP", 1.34f, 15, subRecord("WMI1", "abcdefgh"));
        EXPECT_TRUE(other.mModForm[0].isZeroOrUnset());
    }
}
