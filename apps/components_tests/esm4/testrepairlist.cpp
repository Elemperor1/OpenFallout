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

    TEST(ESM4RepairListTest, weaponReadsItsRepairList)
    {
        const std::string data = zString("EDID", "TestWeapon") + valueSubRecord<std::uint32_t>("REPL", 0x00123456);
        const std::vector<ESM4::Weapon> records = loadRecords<ESM4::Weapon>("WEAP", record("WEAP", 6, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mRepairList.toUint32(), 0x00123456u);
    }
}
