#include <components/esm4/loadcell.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    using namespace ESM4Test;

    /// A group in the layout of Fallout 3 and the games after it, which has a header of 24 bytes.
    std::string versionedGroup(std::string_view label, std::string_view children)
    {
        std::string result("GRUP");
        append<std::uint32_t>(result, static_cast<std::uint32_t>(24 + children.size()));
        result.append(label);
        append<std::int32_t>(result, 0); // top level group
        append<std::uint16_t>(result, 0); // stamp
        append<std::uint16_t>(result, 0);
        append<std::uint32_t>(result, 0);
        result.append(children);
        return result;
    }

    /// The cell of a plugin that has the header version and the form version of a game, with the flags of a cell that
    /// has water and the height of water in its XCLW sub-record.
    ESM4::Cell loadCell(float headerVersion, std::uint16_t formVersion, float waterHeight)
    {
        std::string hedr;
        append<float>(hedr, headerVersion);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string cell = zString("EDID", "OFTestCell") + valueSubRecord<std::uint8_t>("DATA", 0x02)
            + valueSubRecord<float>("XCLW", waterHeight);
        const std::string plugin = versionedRecord("TES4", 0, subRecord("HEDR", hedr), formVersion)
            + versionedGroup("CELL", versionedRecord("CELL", 1, cell, formVersion));

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        reader.setModIndex(0);
        ESM4::Cell result;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                result.load(r);
                return true;
            },
            [](ESM4::Reader&) {});
        return result;
    }

    TEST(ESM4CellWaterTest, fallout3KeepsTheHeightOfWater)
    {
        const ESM4::Cell cell = loadCell(0.94f, 15, 2600.f);
        EXPECT_TRUE(cell.mCellFlags & ESM4::CELL_HasWater);
        EXPECT_EQ(cell.mWaterHeight, 2600.f);
        EXPECT_TRUE(cell.hasWaterHeight());
        EXPECT_TRUE(cell.mExteriorWaterIsFlagged);
    }

    TEST(ESM4CellWaterTest, newVegasKeepsTheHeightOfWater)
    {
        const ESM4::Cell cell = loadCell(1.34f, 15, -4200.f);
        EXPECT_EQ(cell.mWaterHeight, -4200.f);
        EXPECT_TRUE(cell.mExteriorWaterIsFlagged);
    }

    TEST(ESM4CellWaterTest, newVegasKeepsTheLargestFloatThatSaysThereIsNoHeight)
    {
        const ESM4::Cell cell = loadCell(1.34f, 15, std::numeric_limits<float>::max());
        EXPECT_EQ(cell.mWaterHeight, std::numeric_limits<float>::max());
        EXPECT_FALSE(cell.hasWaterHeight());
    }

    TEST(ESM4CellWaterTest, skyrimHasNoHeightOfWater)
    {
        // The water records of Skyrim are broken, and Skyrim LE has the header version of Fallout 3
        EXPECT_FALSE(loadCell(0.94f, 43, 2600.f).hasWaterHeight());
        EXPECT_FALSE(loadCell(1.7f, 44, 2600.f).hasWaterHeight());
    }

    TEST(ESM4CellWaterTest, theFlagForWaterIsForInteriorCellsOutsideFallout)
    {
        EXPECT_FALSE(loadCell(0.94f, 43, 2600.f).mExteriorWaterIsFlagged);
        EXPECT_FALSE(loadCell(1.7f, 44, 2600.f).mExteriorWaterIsFlagged);
    }
}
