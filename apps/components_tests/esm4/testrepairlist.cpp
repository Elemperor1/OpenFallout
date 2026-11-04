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
}
