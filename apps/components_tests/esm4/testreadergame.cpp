#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    using namespace ESM4Test;

    // The Reader of a plugin with one record of the given form version, positioned at that record.
    // `headerVersion` is the version in the HEDR of the plugin.
    bool isFalloutRecord(float headerVersion, std::uint16_t formVersion)
    {
        std::string hedr;
        append<float>(hedr, headerVersion);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string plugin = versionedRecord("TES4", 0, subRecord("HEDR", hedr), formVersion)
            + versionedRecord("WTHR", 1, "", formVersion);

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.typeId, ESM4::REC_WTHR);
        return reader.isFalloutRecord();
    }

    TEST(ESM4ReaderGameTest, recognizesFallout3AndNewVegas)
    {
        EXPECT_TRUE(isFalloutRecord(0.94f, 15)); // Fallout 3
        EXPECT_TRUE(isFalloutRecord(1.32f, 15)); // New Vegas and its first downloads
        EXPECT_TRUE(isFalloutRecord(1.33f, 15));
        EXPECT_TRUE(isFalloutRecord(1.34f, 15));
    }

    TEST(ESM4ReaderGameTest, doesNotRecognizeTheOtherGames)
    {
        EXPECT_FALSE(isFalloutRecord(0.94f, 43)); // Skyrim LE has the header version of Fallout 3
        EXPECT_FALSE(isFalloutRecord(1.7f, 44)); // Skyrim SE
        EXPECT_FALSE(isFalloutRecord(0.95f, 131)); // Fallout 4
    }

    TEST(ESM4ReaderGameTest, doesNotRecognizeOblivion)
    {
        const std::string plugin = header() + record("WTHR", 1, "");
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_FALSE(reader.isFalloutRecord());
    }
}
