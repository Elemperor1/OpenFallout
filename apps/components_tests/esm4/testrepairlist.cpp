#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadweap.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace
{
    using namespace ESM4Test;

    // The list of the things that mend a thing (REPL) is read from the record, and the subrecords that the loader skips
    // before and after it (here MODT with a payload that is not four bytes and OBND, which is twelve) are skipped whole
    TEST(ESM4RepairListTest, armorReadsItsRepairListAmongTheSkippedSubrecords)
    {
        const std::string data = zString("EDID", "TestArmor") + subRecord("OBND", bytePattern(12, 1))
            + subRecord("MODT", bytePattern(7, 20)) + valueSubRecord<std::uint32_t>("REPL", 0x00ABCDEF)
            + subRecord("MO2T", bytePattern(9, 40)) + zString("ICON", "Interface/icon.dds");
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEditorId, "TestArmor");
        EXPECT_EQ(records[0].mRepairList.toUint32(), 0x00ABCDEFu);
        EXPECT_EQ(records[0].mIconMale, "Interface/icon.dds");
    }

    TEST(ESM4RepairListTest, armorWithoutTheSubrecordHasNoList)
    {
        const std::string data
            = zString("EDID", "TestArmor") + subRecord("MODT", bytePattern(7, 20)) + zString("ICON", "a.dds");
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_TRUE(records[0].mRepairList.isZeroOrUnset());
        EXPECT_EQ(records[0].mIconMale, "a.dds");
    }

    // What an armor is worn as (ETYP) and its general flags (BMDT, whose class bits say heavy or medium) are what the
    // Jury Rigging rule compares
    TEST(ESM4RepairListTest, armorReadsItsEquipmentTypeAndGeneralFlags)
    {
        std::string bmdt;
        for (std::uint32_t value : { 0x00000004u, 0x00000088u })
            bmdt.append(reinterpret_cast<const char*>(&value), sizeof(value));
        const std::string data = zString("EDID", "TestArmor") + subRecord("BMDT", bmdt)
            + valueSubRecord<std::uint32_t>("ETYP", 8) + zString("ICON", "a.dds");
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEquipmentType, 8);
        EXPECT_NE(records[0].mGeneralFlags & ESM4::Armor::FO3_HeavyArmor, 0u);
        EXPECT_NE(records[0].mGeneralFlags & 0x08, 0u);
        EXPECT_EQ(records[0].mIconMale, "a.dds");
    }

    // The executable's record starts with the type 0 and the repair rule compares the two integers, so two pieces that
    // both lack the field are of the same type (FalloutNV.exe 0x5129F0 and 0x47BB50, Mac round 12BB G3)
    TEST(ESM4RepairListTest, armorWithoutEtypHasTheEquipmentTypeZero)
    {
        const std::string data = zString("EDID", "TestArmor") + zString("ICON", "a.dds");
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEquipmentType, 0);
    }

    TEST(ESM4RepairListTest, weaponReadsItsRepairList)
    {
        const std::string data = zString("EDID", "TestWeapon") + valueSubRecord<std::uint32_t>("REPL", 0x00123456);
        const std::vector<ESM4::Weapon> records = loadRecords<ESM4::Weapon>("WEAP", record("WEAP", 6, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mRepairList.toUint32(), 0x00123456u);
    }

    // New Vegas armor names the sounds that its wearer's animation text keys play (SNAM: the sound, its chance, three
    // bytes of nothing, the type) and an audio template (TNAM) to take them from when it has none
    TEST(ESM4RepairListTest, armorReadsItsWornSoundsAndItsAudioTemplate)
    {
        const auto worn = [](std::uint32_t sound, std::uint8_t chance, std::uint32_t type) {
            std::string snam;
            snam.append(reinterpret_cast<const char*>(&sound), sizeof(sound));
            snam.push_back(static_cast<char>(chance));
            snam.append("\x01\x02\x03", 3);
            snam.append(reinterpret_cast<const char*>(&type), sizeof(type));
            return subRecord("SNAM", snam);
        };
        const std::string data = zString("EDID", "AudioTemplate") + worn(0x00AB0001, 100, 22)
            + worn(0x00AB0002, 70, 21) + valueSubRecord<std::uint32_t>("TNAM", 0x00AB0100) + zString("ICON", "a.dds");
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(records[0].mSounds.size(), 2u);
        EXPECT_EQ(records[0].mSounds[0].mSound.toUint32(), 0x00AB0001u);
        EXPECT_EQ(records[0].mSounds[0].mChance, 100);
        EXPECT_EQ(records[0].mSounds[0].mType, 22u);
        EXPECT_EQ(records[0].mSounds[1].mChance, 70);
        EXPECT_EQ(records[0].mSounds[1].mType, 21u);
        EXPECT_EQ(records[0].mTemplate.toUint32(), 0x00AB0100u);
        EXPECT_EQ(records[0].mIconMale, "a.dds");
    }

    TEST(ESM4RepairListTest, armorWithoutWornSoundsHasNoneAndNoTemplate)
    {
        const std::string data = zString("EDID", "TestArmor") + zString("ICON", "a.dds");
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_TRUE(records[0].mSounds.empty());
        EXPECT_TRUE(records[0].mTemplate.isZeroOrUnset());
    }

    // A sound subrecord of another size is skipped whole
    TEST(ESM4RepairListTest, armorSkipsAWornSoundOfAnotherSize)
    {
        const std::string data
            = zString("EDID", "TestArmor") + subRecord("SNAM", bytePattern(8, 3)) + zString("ICON", "a.dds");
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_TRUE(records[0].mSounds.empty());
        EXPECT_EQ(records[0].mIconMale, "a.dds");
    }

    // The sounds an armor is worn with (SNAM, twelve bytes) and the armor whose sounds it takes when it has none (TNAM)
    // are read from those subrecords alone: a skipped subrecord of the same size (the hashes of a model, the bounds,
    // the race, a count of keywords) is not a sound or a template
    TEST(ESM4ArmorSoundTest, onlySnamMakesAWornSoundAndOnlyTnamTheTemplate)
    {
        // a sound: the form, the chance, three bytes of nothing and the type
        std::string soundData;
        const std::uint32_t form = 0x00001234;
        soundData.append(reinterpret_cast<const char*>(&form), 4);
        soundData.push_back(static_cast<char>(75));
        soundData.append(3, '\0');
        const std::uint32_t type = 2;
        soundData.append(reinterpret_cast<const char*>(&type), 4);
        const std::string data = zString("EDID", "SoundArmor") + subRecord("MODT", bytePattern(12, 1))
            + subRecord("MO2T", bytePattern(12, 40)) + subRecord("OBND", bytePattern(12, 60))
            + valueSubRecord<std::uint32_t>("RNAM", 0x00000099) + valueSubRecord<std::uint32_t>("KSIZ", 3)
            + subRecord("SNAM", soundData) + valueSubRecord<std::uint32_t>("TNAM", 0x00004321);
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(records[0].mSounds.size(), 1u);
        EXPECT_EQ(records[0].mSounds[0].mSound.toUint32(), 0x1234u);
        EXPECT_EQ(records[0].mSounds[0].mChance, 75);
        EXPECT_EQ(records[0].mSounds[0].mType, 2u);
        EXPECT_EQ(records[0].mTemplate.toUint32(), 0x4321u);
    }

    TEST(ESM4ArmorSoundTest, anArmorWithOnlyModelDataAndBoundsHasNoSoundAndNoTemplate)
    {
        const std::string data = zString("EDID", "Plain") + subRecord("MODT", bytePattern(12, 1))
            + subRecord("OBND", bytePattern(12, 60)) + valueSubRecord<std::uint32_t>("RNAM", 0x00000099)
            + valueSubRecord<std::uint32_t>("KSIZ", 3);
        const std::vector<ESM4::Armor> records = loadRecords<ESM4::Armor>("ARMO", record("ARMO", 5, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_TRUE(records[0].mSounds.empty());
        EXPECT_TRUE(records[0].mTemplate.isZeroOrUnset());
    }
}
