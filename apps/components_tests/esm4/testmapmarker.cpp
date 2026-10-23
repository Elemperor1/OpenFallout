#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadwrld.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace ESM4Test;

    constexpr std::uint32_t zone = 0x00035AAA;

    ESM4::Reference loadMarker(const std::string& flags)
    {
        const std::string data = valueSubRecord("NAME", std::uint32_t(0x10)) + subRecord("XMRK", "") + flags
            + zString("FULL", "Megaton") + valueSubRecord<std::uint16_t>("TNAM", 0x0203);
        const std::vector<ESM4::Reference> records = loadRecords<ESM4::Reference>("REFR", record("REFR", 0x2001, data));
        EXPECT_EQ(records.size(), 1u);
        return records.empty() ? ESM4::Reference() : records[0];
    }

    TEST(ESM4MapMarkerTest, readsTheFlagsOfAMarker)
    {
        const ESM4::Reference marker = loadMarker(valueSubRecord<std::uint8_t>("FNAM", 0x03));
        EXPECT_TRUE(marker.mIsMapMarker);
        EXPECT_EQ(marker.mMapMarkerFlags, 0x03);
        EXPECT_EQ(marker.mMapMarker, 0x0203);
    }

    TEST(ESM4MapMarkerTest, aMarkerWithoutFlagsIsHiddenAndCannotBeTravelledTo)
    {
        const ESM4::Reference marker = loadMarker({});
        EXPECT_TRUE(marker.mIsMapMarker);
        EXPECT_EQ(marker.mMapMarkerFlags, 0);
    }

    TEST(ESM4MapMarkerTest, flagsOfAnotherSizeAreLeftAlone)
    {
        const ESM4::Reference marker = loadMarker(valueSubRecord<std::uint32_t>("FNAM", 0x01020304));
        EXPECT_EQ(marker.mMapMarkerFlags, 0);
    }

    TEST(ESM4MapMarkerTest, aCellAndAWorldspaceKeepTheirEncounterZone)
    {
        const std::vector<ESM4::Cell> cells = loadRecords<ESM4::Cell>(
            "CELL", record("CELL", 0x3001, zString("EDID", "Cell") + valueSubRecord<std::uint32_t>("XEZN", zone)));
        ASSERT_EQ(cells.size(), 1u);
        EXPECT_EQ(cells[0].mEncounterZone.toUint32(), zone);
        const std::vector<ESM4::World> worlds = loadRecords<ESM4::World>(
            "WRLD", record("WRLD", 0x3002, zString("EDID", "World") + valueSubRecord<std::uint32_t>("XEZN", zone)));
        ASSERT_EQ(worlds.size(), 1u);
        EXPECT_EQ(worlds[0].mEncounterZone.toUint32(), zone);
    }
}
