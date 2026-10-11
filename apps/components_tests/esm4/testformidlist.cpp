#include <components/esm4/loadflst.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    TEST(ESM4FormIdListTest, keeps_every_entry_in_order)
    {
        const auto list = loadRecords<ESM4::FormIdList>("FLST",
            record("FLST", 0x801,
                zString("EDID", "OFList") + valueSubRecord<std::uint32_t>("LNAM", 0x00000810)
                    + valueSubRecord<std::uint32_t>("LNAM", 0x00000811)
                    + valueSubRecord<std::uint32_t>("LNAM", 0x00000810)));

        ASSERT_EQ(list.size(), 1u);
        EXPECT_EQ(list[0].mEditorId, "OFList");
        ASSERT_EQ(list[0].mObjects.size(), 3u);
        EXPECT_EQ(list[0].mObjects[0].toUint32(), 0x810u);
        EXPECT_EQ(list[0].mObjects[1].toUint32(), 0x811u);
        EXPECT_EQ(list[0].mObjects[2].toUint32(), 0x810u);
    }

    TEST(ESM4FormIdListTest, skips_a_subrecord_it_does_not_know)
    {
        const auto list = loadRecords<ESM4::FormIdList>("FLST",
            record("FLST", 0x801,
                zString("EDID", "OFList") + subRecord("DATA", bytePattern(6, 3))
                    + valueSubRecord<std::uint32_t>("LNAM", 0x00000810)));

        ASSERT_EQ(list.size(), 1u);
        ASSERT_EQ(list[0].mObjects.size(), 1u);
        EXPECT_EQ(list[0].mObjects[0].toUint32(), 0x810u);
    }
}
