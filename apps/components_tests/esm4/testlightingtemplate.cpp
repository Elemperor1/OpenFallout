#include <components/esm4/loadlgtm.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace ESM4Test;

    // The first nine fields of the lighting struct, 36 bytes, which is all that Oblivion writes.
    std::string lightingData(std::uint32_t seed)
    {
        std::string data;
        append<std::uint32_t>(data, seed + 1); // ambient
        append<std::uint32_t>(data, seed + 2); // directional
        append<std::uint32_t>(data, seed + 3); // fog colour
        append<float>(data, static_cast<float>(seed + 4)); // fog near
        append<float>(data, static_cast<float>(seed + 5)); // fog far
        append<std::int32_t>(data, static_cast<std::int32_t>(seed + 6)); // rotation XY
        append<std::int32_t>(data, static_cast<std::int32_t>(seed + 7)); // rotation Z
        append<float>(data, static_cast<float>(seed + 8)); // fog direction fade
        append<float>(data, static_cast<float>(seed + 9)); // fog clip distance
        return data;
    }

    // Fallout 3 and New Vegas add the power of the fog: 40 bytes.
    std::string falloutLightingData(std::uint32_t seed)
    {
        std::string data = lightingData(seed);
        append<float>(data, 1.5f);
        return data;
    }

    TEST(ESM4LightingTemplateTest, readsTheFalloutLightingStruct)
    {
        const std::vector<ESM4::LightingTemplate> records = loadRecords<ESM4::LightingTemplate>(
            "LGTM", record("LGTM", 5, zString("EDID", "Template") + subRecord("DATA", falloutLightingData(100))));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 5u);
        EXPECT_EQ(records[0].mEditorId, "Template");
        EXPECT_EQ(records[0].mLighting.ambient, 101u);
        EXPECT_EQ(records[0].mLighting.directional, 102u);
        EXPECT_EQ(records[0].mLighting.fogColor, 103u);
        EXPECT_EQ(records[0].mLighting.fogNear, 104.f);
        EXPECT_EQ(records[0].mLighting.fogFar, 105.f);
        EXPECT_EQ(records[0].mLighting.rotationXY, 106);
        EXPECT_EQ(records[0].mLighting.rotationZ, 107);
        EXPECT_EQ(records[0].mLighting.fogDirFade, 108.f);
        EXPECT_EQ(records[0].mLighting.fogClipDist, 109.f);
        EXPECT_EQ(records[0].mLighting.fogPower, 1.5f);
    }

    TEST(ESM4LightingTemplateTest, readsTheShorterOblivionStructAndTheRecordAfterIt)
    {
        const std::vector<ESM4::LightingTemplate> records = loadRecords<ESM4::LightingTemplate>("LGTM",
            record("LGTM", 5, subRecord("DATA", lightingData(100)))
                + record("LGTM", 6, subRecord("DATA", falloutLightingData(200))));

        ASSERT_EQ(records.size(), 2u);
        EXPECT_EQ(records[0].mLighting.fogFar, 105.f);
        EXPECT_EQ(records[0].mLighting.fogClipDist, 109.f);
        EXPECT_EQ(records[0].mLighting.fogPower, 1.f);
        EXPECT_EQ(records[1].mId.toUint32(), 6u);
        EXPECT_EQ(records[1].mLighting.fogFar, 205.f);
        EXPECT_EQ(records[1].mLighting.fogPower, 1.5f);
    }
}
