#include <components/esm4/loaddobj.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    /// The objects of DATA: the form number 0x800 plus the number of the slot, as many as are asked for
    std::string objects(std::size_t count)
    {
        std::string data;
        for (std::size_t i = 0; i < count; ++i)
            append<std::uint32_t>(data, static_cast<std::uint32_t>(0x800 + i));
        return data;
    }

    TEST(ESM4DefaultObjectTest, reads_the_objects_of_fallout_3)
    {
        const auto records = loadRecords<ESM4::DefaultObj>(
            "DOBJ", record("DOBJ", 0x801, zString("EDID", "DefaultObjectManager") + subRecord("DATA", objects(21))));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEditorId, "DefaultObjectManager");
        EXPECT_EQ(records[0].mData.stimpack.toUint32(), 0x800u);
        EXPECT_EQ(records[0].mData.radAway.toUint32(), 0x803u);
        EXPECT_EQ(records[0].mData.drugWearsOffImageSpace.toUint32(), 0x814u);
        EXPECT_TRUE(records[0].mData.doctorsBag.isZeroOrUnset());
    }

    TEST(ESM4DefaultObjectTest, reads_the_objects_of_new_vegas_up_to_the_doctors_bag_and_beyond)
    {
        const auto records = loadRecords<ESM4::DefaultObj>(
            "DOBJ", record("DOBJ", 0x801, zString("EDID", "DefaultObjectManager") + subRecord("DATA", objects(34))));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mData.doctorsBag.toUint32(), 0x815u);
        EXPECT_EQ(records[0].mData.cateyeMobileEffectNYI.toUint32(), 0x821u);
    }

    TEST(ESM4DefaultObjectTest, reads_a_record_with_fewer_or_more_objects_as_far_as_it_goes)
    {
        const auto fewer = loadRecords<ESM4::DefaultObj>(
            "DOBJ", record("DOBJ", 0x801, subRecord("DATA", objects(3)) + zString("EDID", "X")));
        ASSERT_EQ(fewer.size(), 1u);
        EXPECT_EQ(fewer[0].mData.radX.toUint32(), 0x802u);
        EXPECT_TRUE(fewer[0].mData.radAway.isZeroOrUnset());
        EXPECT_EQ(fewer[0].mEditorId, "X");

        const auto more = loadRecords<ESM4::DefaultObj>(
            "DOBJ", record("DOBJ", 0x801, subRecord("DATA", objects(40)) + zString("EDID", "Y")));
        ASSERT_EQ(more.size(), 1u);
        EXPECT_EQ(more[0].mData.cateyeMobileEffectNYI.toUint32(), 0x821u);
        EXPECT_EQ(more[0].mEditorId, "Y");
    }

    TEST(ESM4DefaultObjectTest, skips_a_subrecord_it_does_not_know)
    {
        const auto records = loadRecords<ESM4::DefaultObj>(
            "DOBJ", record("DOBJ", 0x801, subRecord("XXXZ", bytePattern(6, 3)) + subRecord("DATA", objects(2))));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mData.superStimpack.toUint32(), 0x801u);
    }
}
