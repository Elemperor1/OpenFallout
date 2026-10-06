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

    // Whether the Reader of a plugin with the header version, the form version of its TES4 record and one record of
    // another form version takes it for a Fallout file, once it is positioned at that record.
    bool isFalloutFile(float headerVersion, std::uint16_t fileFormVersion, std::uint16_t recordFormVersion)
    {
        std::string hedr;
        append<float>(hedr, headerVersion);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string plugin = versionedRecord("TES4", 0, subRecord("HEDR", hedr), fileFormVersion)
            + versionedRecord("WTHR", 1, "", recordFormVersion);

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_EQ(reader.hdr().record.typeId, ESM4::REC_WTHR);
        return reader.isFalloutFile();
    }

    TEST(ESM4ReaderGameTest, recognizesFallout3AndNewVegas)
    {
        EXPECT_TRUE(isFalloutFile(0.94f, 15, 15)); // Fallout 3
        EXPECT_TRUE(isFalloutFile(1.32f, 15, 15)); // New Vegas and its first downloads
        EXPECT_TRUE(isFalloutFile(1.33f, 15, 15));
        EXPECT_TRUE(isFalloutFile(1.34f, 15, 15));
    }

    TEST(ESM4ReaderGameTest, doesNotRecognizeTheOtherGames)
    {
        EXPECT_FALSE(isFalloutFile(0.94f, 43, 43)); // Skyrim LE has the header version of Fallout 3
        EXPECT_FALSE(isFalloutFile(1.7f, 44, 44)); // Skyrim SE
        EXPECT_FALSE(isFalloutFile(0.95f, 131, 131)); // Fallout 4
    }

    TEST(ESM4ReaderGameTest, tellsSkyrimFromFallout3ByTheFileNotByTheRecord)
    {
        // Skyrim.esm has records with form versions from 14 on.
        for (const std::uint16_t recordFormVersion : { 14, 15, 16, 39 })
        {
            EXPECT_FALSE(isFalloutFile(0.94f, 43, recordFormVersion)) << recordFormVersion;
            EXPECT_TRUE(isFalloutFile(0.94f, 15, recordFormVersion)) << recordFormVersion;
        }
    }

    TEST(ESM4ReaderGameTest, doesNotRecognizeOblivion)
    {
        const std::string plugin = header() + record("WTHR", 1, "");
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_FALSE(reader.isFalloutFile());
    }
}
