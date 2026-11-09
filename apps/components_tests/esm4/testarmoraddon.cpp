#include <components/esm4/loadarma.hpp>
#include <components/esm4/reader.hpp>

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

    ESM4::ArmorAddon load(const std::string& data)
    {
        const std::vector<ESM4::ArmorAddon> result
            = loadRecords<ESM4::ArmorAddon>("ARMA", record("ARMA", 0x801, data));
        EXPECT_EQ(result.size(), 1u);
        return result.empty() ? ESM4::ArmorAddon{} : result[0];
    }

    TEST(ESM4ArmorAddonTest, readsTheModelsTexturesAndRaceOfTheAddon)
    {
        const std::string data = zString("EDID", "AddonEdid") + zString("MOD2", "Meshes\\Male.nif")
            + zString("MOD3", "Meshes\\Female.nif") + zString("MOD4", "Meshes\\MaleFirstPerson.nif")
            + zString("MOD5", "Meshes\\FemaleFirstPerson.nif") + valueSubRecord<std::uint32_t>("NAM0", 0x00000811)
            + valueSubRecord<std::uint32_t>("NAM1", 0x00000812) + valueSubRecord<std::uint32_t>("RNAM", 0x00000813);

        const ESM4::ArmorAddon addon = load(data);

        EXPECT_EQ(addon.mId.toUint32(), 0x801u);
        EXPECT_EQ(addon.mEditorId, "AddonEdid");
        EXPECT_EQ(addon.mModelMale.getOriginal(), "Meshes\\Male.nif");
        EXPECT_EQ(addon.mModelFemale.getOriginal(), "Meshes\\Female.nif");
        EXPECT_EQ(addon.mTextureMale.toUint32(), 0x811u);
        EXPECT_EQ(addon.mTextureFemale.toUint32(), 0x812u);
        EXPECT_EQ(addon.mRacePrimary.toUint32(), 0x813u);
    }

    TEST(ESM4ArmorAddonTest, readsTheBodyTemplateOfOblivionAndFallout)
    {
        std::string body;
        append<std::uint32_t>(body, 0x00000204); // body parts
        append<std::uint8_t>(body, 5); // flags
        body += std::string("\x01\x02\x03", 3);
        append<std::uint32_t>(body, 1); // heavy

        const ESM4::ArmorAddon addon = load(subRecord("BODT", body));

        EXPECT_EQ(addon.mBodyTemplate.bodyPart, 0x204u);
        EXPECT_EQ(addon.mBodyTemplate.flags, 5);
        EXPECT_EQ(addon.mBodyTemplate.unknown1, 1);
        EXPECT_EQ(addon.mBodyTemplate.unknown2, 2);
        EXPECT_EQ(addon.mBodyTemplate.unknown3, 3);
        EXPECT_EQ(addon.mBodyTemplate.type, 1u);
    }

    TEST(ESM4ArmorAddonTest, readsTheBodyTemplateOfSkyrimWithTheTypeOnlyWhenThereIsOne)
    {
        std::string parts;
        append<std::uint32_t>(parts, 0x00000030);
        std::string withType = parts;
        append<std::uint32_t>(withType, 2);

        const ESM4::ArmorAddon bare = load(subRecord("BOD2", parts));
        const ESM4::ArmorAddon typed = load(subRecord("BOD2", withType));

        EXPECT_EQ(bare.mBodyTemplate.bodyPart, 0x30u);
        EXPECT_EQ(bare.mBodyTemplate.flags, 0);
        EXPECT_EQ(bare.mBodyTemplate.type, 0u);
        EXPECT_EQ(typed.mBodyTemplate.bodyPart, 0x30u);
        EXPECT_EQ(typed.mBodyTemplate.type, 2u);
    }

    TEST(ESM4ArmorAddonTest, readsThePrioritiesAndTheDetectionOfTheAddon)
    {
        std::string data;
        append<std::uint8_t>(data, 3); // male priority
        append<std::uint8_t>(data, 4); // female priority
        append<std::uint8_t>(data, 5); // male weight slider
        append<std::uint8_t>(data, 6); // female weight slider
        append<std::uint16_t>(data, 0);
        append<std::uint8_t>(data, 7); // detection sound
        append<std::uint8_t>(data, 0);
        append<float>(data, 1.5f); // weapon adjust

        const ESM4::ArmorAddon addon = load(subRecord("DNAM", data));

        EXPECT_EQ(addon.mMalePriority, 3);
        EXPECT_EQ(addon.mFemalePriority, 4);
        EXPECT_EQ(addon.mWeightSliderMale, 5);
        EXPECT_EQ(addon.mWeightSliderFemale, 6);
        EXPECT_EQ(addon.mDetectionSoundValue, 7);
        EXPECT_EQ(addon.mWeaponAdjust, 1.5f);
        // data of another size is left alone
        const ESM4::ArmorAddon other = load(subRecord("DNAM", bytePattern(5, 1)) + zString("EDID", "Next"));
        EXPECT_EQ(other.mMalePriority, 0);
        EXPECT_EQ(other.mEditorId, "Next");
    }

    TEST(ESM4ArmorAddonTest, takesTheRacesOfSkyrimFromTheModelSubrecordAndSkipsTheModelOfFallout)
    {
        std::string race;
        append<std::uint32_t>(race, 0x00000820);

        // Skyrim LE has the header version of Fallout 3, and so does this
        const ESM4::ArmorAddon skyrim = loadVersionedRecord<ESM4::ArmorAddon>("ARMA", 0.94f, 43,
            subRecord("MODL", race) + subRecord("MODL", race) + zString("MODL", "Meshes\\Fallout.nif"));
        const ESM4::ArmorAddon oblivion = load(subRecord("MODL", race));

        ASSERT_EQ(skyrim.mRaces.size(), 2u);
        EXPECT_EQ(skyrim.mRaces[0].toUint32(), 0x820u);
        EXPECT_TRUE(oblivion.mRaces.empty());
    }

    TEST(ESM4ArmorAddonTest, skipsTheSubrecordsItDoesNotUse)
    {
        std::string skipped;
        for (const char* type : { "MO2T", "MO2S", "MO2C", "MO2F", "MO3T", "MO3S", "MO3C", "MO3F", "MOSD", "MO4T",
                 "MO4S", "MO4C", "MO4F", "MO5T", "MO5S", "MO5C", "MO5F", "NAM2", "NAM3", "SNDD", "BMDT", "DATA",
                 "ETYP", "FULL", "ICO2", "ICON", "MODT", "MODS", "MODD", "OBND", "BSMB", "BSMP", "BSMS", "ONAM" })
            skipped += subRecord(type, bytePattern(4, 1));

        EXPECT_EQ(load(skipped + zString("EDID", "Last")).mEditorId, "Last");
    }

    TEST(ESM4ArmorAddonTest, doesNotReadASubrecordItDoesNotKnow)
    {
        try
        {
            load(zString("EDID", "Addon") + subRecord("XXYZ", ""));
            FAIL() << "No exception";
        }
        catch (const std::runtime_error& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("Unknown subrecord XXYZ"));
        }
    }
}
