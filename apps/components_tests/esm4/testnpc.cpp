#include <components/esm4/loadnpc.hpp>
#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
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

    // The one NPC_ record of a plugin with the header version and the form version of its TES4 record, with the
    // 24 byte record headers of Fallout 3 and the games after it
    ESM4::Npc loadVersioned(float headerVersion, std::uint16_t formVersion, const std::string& data)
    {
        std::string hedr;
        append<float>(hedr, headerVersion);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string plugin = versionedRecord("TES4", 0, subRecord("HEDR", hedr), formVersion)
            + versionedRecord("NPC_", 0x801, data, formVersion);

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        reader.getRecordData();
        ESM4::Npc npc;
        npc.load(reader);
        return npc;
    }

    // ACBS of Fallout 3 and New Vegas: flags, fatigue, barter gold, level, calc min and max, speed multiplier, karma,
    // disposition base and template flags. The karma of 1000 has 0x447A in its upper half, where Skyrim has its
    // template flags.
    std::string falloutBaseConfig(std::uint16_t templateFlags)
    {
        std::string data;
        append<std::uint32_t>(data, 0);
        for (const std::uint16_t value : { 0, 0, 1, 1, 1, 100 })
            append<std::uint16_t>(data, value);
        append<float>(data, 1000.f);
        append<std::int16_t>(data, 0);
        append<std::uint16_t>(data, templateFlags);
        return subRecord("ACBS", data);
    }

    TEST(ESM4NpcTest, knowsTheContentFileItWasReadFromApartFromTheOneThatMadeIt)
    {
        // the plugin with the load order index 5 changes a character of its master, which has the index 2
        std::string hedr;
        append<float>(hedr, 0.8f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string plugin
            = record("TES4", 0,
                  subRecord("HEDR", hedr) + zString("MAST", "base.esm") + valueSubRecord<std::uint64_t>("DATA", 0))
            + topGroup("NPC_",
                record("NPC_", 0x801, zString("EDID", "Npc")) + record("NPC_", 0x01000802, zString("EDID", "Own")));

        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "patch.esp", nullptr, nullptr);
        reader.setModIndex(5);
        reader.updateModIndices({ { "base.esm", 2 } });
        std::vector<ESM4::Npc> npcs;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                npcs.emplace_back().load(r);
                return true;
            },
            [](ESM4::Reader&) {});

        ASSERT_EQ(npcs.size(), 2u);
        EXPECT_EQ(npcs[0].mId.mContentFile, 2);
        EXPECT_EQ(npcs[0].mSourceFile, 5);
        // a character of the plugin itself has the plugin as both
        EXPECT_EQ(npcs[1].mId.mContentFile, 5);
        EXPECT_EQ(npcs[1].mSourceFile, 5);
    }

    TEST(ESM4NpcTest, takesACharacterOfFallout3ForOneOfFalloutAndReadsItsTemplateFlagsWhereFalloutHasThem)
    {
        const std::string data = zString("EDID", "Npc") + falloutBaseConfig(ESM4::Npc::Template_UseModel)
            + valueSubRecord<float>("WNAM", 1.5f);

        // The header version of Fallout 3 is the one of Skyrim LE, the form version of its files is 15
        const ESM4::Npc fallout3 = loadVersioned(0.94f, 15, data);
        EXPECT_TRUE(fallout3.mIsFONV);
        EXPECT_EQ(fallout3.mBaseConfig.fo3.templateFlags, ESM4::Npc::Template_UseModel);
        EXPECT_EQ(fallout3.mFootWeight, 1.5f);
        EXPECT_TRUE(fallout3.mWornArmor.isZeroOrUnset());

        const ESM4::Npc newVegas = loadVersioned(1.34f, 15, data);
        EXPECT_TRUE(newVegas.mIsFONV);
        EXPECT_EQ(newVegas.mBaseConfig.fo3.templateFlags, ESM4::Npc::Template_UseModel);
        EXPECT_EQ(newVegas.mFootWeight, 1.5f);
    }

    TEST(ESM4NpcTest, doesNotTakeACharacterOfSkyrimForOneOfFallout)
    {
        // WNAM of Skyrim is the armour that the character wears
        const std::string data = zString("EDID", "Npc") + valueSubRecord<std::uint32_t>("WNAM", 0x801);

        const ESM4::Npc skyrim = loadVersioned(0.94f, 43, data);
        EXPECT_FALSE(skyrim.mIsFONV);
        EXPECT_FALSE(skyrim.mWornArmor.isZeroOrUnset());
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
