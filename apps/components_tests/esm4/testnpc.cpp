#include <components/esm4/loadnpc.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string coefficients(std::string_view type, std::size_t count)
    {
        std::string data;
        for (std::size_t i = 0; i < count; ++i)
            append(data, static_cast<float>(i) + 0.5f);
        return subRecord(type, data);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Npc>("NPC_", record("NPC_", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4NpcTest, readsFaceGenerationCoefficients)
    {
        const std::string data
            = zString("EDID", "Npc") + coefficients("FGGS", 50) + coefficients("FGGA", 30) + coefficients("FGTS", 50);

        const std::vector<ESM4::Npc> npcs = loadRecords<ESM4::Npc>("NPC_", record("NPC_", 1, data));

        ASSERT_EQ(npcs.size(), 1u);
        EXPECT_EQ(npcs[0].mEditorId, "Npc");
        ASSERT_EQ(npcs[0].mSymShapeModeCoefficients.size(), 50u);
        ASSERT_EQ(npcs[0].mAsymShapeModeCoefficients.size(), 30u);
        ASSERT_EQ(npcs[0].mSymTextureModeCoefficients.size(), 50u);
        EXPECT_EQ(npcs[0].mSymShapeModeCoefficients[49], 49.5f);
        EXPECT_EQ(npcs[0].mAsymShapeModeCoefficients[0], 0.5f);
        EXPECT_EQ(npcs[0].mSymTextureModeCoefficients[1], 1.5f);
    }

    TEST(ESM4NpcTest, readsFaceGenerationCoefficientsThatHaveNoData)
    {
        // These records are in the Tale of Two Wastelands plugins. The sub-records that follow must not be eaten.
        const std::string data = zString("EDID", "Npc") + subRecord("FGGS", "") + subRecord("FGGA", "")
            + subRecord("FGTS", "") + valueSubRecord<float>("LNAM", 2.f) + zString("FULL", "Name");

        const std::vector<ESM4::Npc> npcs = loadRecords<ESM4::Npc>("NPC_", record("NPC_", 1, data));

        ASSERT_EQ(npcs.size(), 1u);
        EXPECT_THAT(npcs[0].mSymShapeModeCoefficients, IsEmpty());
        EXPECT_THAT(npcs[0].mAsymShapeModeCoefficients, IsEmpty());
        EXPECT_THAT(npcs[0].mSymTextureModeCoefficients, IsEmpty());
        EXPECT_EQ(npcs[0].mHairLength, 2.f);
        EXPECT_EQ(npcs[0].mFullName, "Name");
    }

    TEST(ESM4NpcTest, rejectsFaceGenerationCoefficientsOfAnotherSize)
    {
        EXPECT_EQ(loadFailure(coefficients("FGGS", 49)), "ESM4::NPC_::load - FGGS has an unexpected size");
        EXPECT_EQ(loadFailure(coefficients("FGGA", 31)), "ESM4::NPC_::load - FGGA has an unexpected size");
        EXPECT_EQ(loadFailure(coefficients("FGTS", 51)), "ESM4::NPC_::load - FGTS has an unexpected size");
    }
}
