#include <components/esm4/loadammo.hpp>
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

    ESM4::Ammunition load(const std::string& data)
    {
        const std::vector<ESM4::Ammunition> result
            = loadRecords<ESM4::Ammunition>("AMMO", record("AMMO", 0x801, data));
        EXPECT_EQ(result.size(), 1u);
        return result.empty() ? ESM4::Ammunition{} : result[0];
    }

    TEST(ESM4AmmunitionTest, readsTheTextsModelsAndFormsOfTheItem)
    {
        std::string effects;
        const std::string data = zString("EDID", "AmmoEdid") + zString("FULL", "Full name") + zString("ONAM", "Short")
            + zString("QNAM", "Abbrev") + zString("DESC", "Description") + zString("ICON", "icons/ammo.dds")
            + zString("MICO", "icons/ammo_small.dds") + zString("MODL", "Meshes\\Ammo.nif")
            + valueSubRecord<float>("MODB", 2.5f) + valueSubRecord<std::uint16_t>("ANAM", 40)
            + valueSubRecord<std::uint32_t>("ENAM", 0x00000811) + valueSubRecord<std::uint32_t>("YNAM", 0x00000812)
            + valueSubRecord<std::uint32_t>("ZNAM", 0x00000813) + valueSubRecord<std::uint32_t>("SCRI", 0x00000814)
            + valueSubRecord<std::uint32_t>("RCIL", 0x00000815) + valueSubRecord<std::uint32_t>("RCIL", 0x00000816);

        const ESM4::Ammunition ammo = load(data);

        EXPECT_EQ(ammo.mId.toUint32(), 0x801u);
        EXPECT_EQ(ammo.mEditorId, "AmmoEdid");
        EXPECT_EQ(ammo.mFullName, "Full name");
        EXPECT_EQ(ammo.mShortName, "Short");
        EXPECT_EQ(ammo.mAbbrev, "Abbrev");
        EXPECT_EQ(ammo.mText, "Description");
        EXPECT_EQ(ammo.mIcon, "icons/ammo.dds");
        EXPECT_EQ(ammo.mMiniIcon, "icons/ammo_small.dds");
        EXPECT_EQ(ammo.mModel.getOriginal(), "Meshes\\Ammo.nif");
        EXPECT_EQ(ammo.mBoundRadius, 2.5f);
        EXPECT_EQ(ammo.mEnchantmentPoints, 40);
        EXPECT_EQ(ammo.mEnchantment.toUint32(), 0x811u);
        EXPECT_EQ(ammo.mPickUpSound.toUint32(), 0x812u);
        EXPECT_EQ(ammo.mDropSound.toUint32(), 0x813u);
        EXPECT_EQ(ammo.mScript.toUint32(), 0x814u);
        ASSERT_EQ(ammo.mAmmoEffects.size(), 2u);
        EXPECT_EQ(ammo.mAmmoEffects[0].toUint32(), 0x815u);
        EXPECT_EQ(ammo.mAmmoEffects[1].toUint32(), 0x816u);
    }

    TEST(ESM4AmmunitionTest, readsTheDataOfOblivionWithTheDamageAsAnInteger)
    {
        std::string data;
        append<float>(data, 1500.f); // speed
        append<std::uint32_t>(data, 0x00000101); // flags: only the first byte is a flag
        append<std::uint32_t>(data, 25); // value
        append<float>(data, 0.5f); // weight
        append<std::uint16_t>(data, 12); // damage

        const ESM4::Ammunition ammo = load(subRecord("DATA", data));

        EXPECT_EQ(ammo.mData.mSpeed, 1500.f);
        EXPECT_EQ(ammo.mData.mFlags, 1u);
        EXPECT_EQ(ammo.mData.mValue, 25u);
        EXPECT_EQ(ammo.mData.mWeight, 0.5f);
        EXPECT_EQ(ammo.mData.mDamage, 12.f);
        EXPECT_EQ(ammo.mData.mClipRounds, 0);
    }

    TEST(ESM4AmmunitionTest, readsTheDataOfFalloutWithTheNumberOfRoundsInAClip)
    {
        std::string data;
        append<float>(data, 1200.f); // speed
        append<std::uint32_t>(data, 0xAABBCC02); // flags, then the bytes that Fallout does not use
        append<std::uint32_t>(data, 7); // value
        append<std::uint8_t>(data, 30); // rounds in a clip

        const ESM4::Ammunition ammo = load(subRecord("DATA", data));

        EXPECT_EQ(ammo.mData.mSpeed, 1200.f);
        EXPECT_EQ(ammo.mData.mFlags, 2u);
        EXPECT_EQ(ammo.mData.mValue, 7u);
        EXPECT_EQ(ammo.mData.mClipRounds, 30);
        EXPECT_EQ(ammo.mData.mWeight, 0.f);
    }

    TEST(ESM4AmmunitionTest, readsTheDataOfSkyrimWithTheProjectileFirst)
    {
        std::string data;
        append<std::uint32_t>(data, 0x00000820); // projectile
        append<std::uint32_t>(data, 3); // flags
        append<float>(data, 9.f); // damage
        append<std::uint32_t>(data, 11); // value

        const ESM4::Ammunition ammo = load(subRecord("DATA", data));

        EXPECT_EQ(ammo.mData.mProjectile.toUint32(), 0x820u);
        EXPECT_EQ(ammo.mData.mFlags, 3u);
        EXPECT_EQ(ammo.mData.mDamage, 9.f);
        EXPECT_EQ(ammo.mData.mValue, 11u);
        EXPECT_EQ(ammo.mData.mWeight, 0.f);

        append<float>(data, 0.25f); // weight, which Skyrim Special Edition adds
        const ESM4::Ammunition special = load(subRecord("DATA", data));

        EXPECT_EQ(special.mData.mDamage, 9.f);
        EXPECT_EQ(special.mData.mValue, 11u);
        EXPECT_EQ(special.mData.mWeight, 0.25f);
    }

    TEST(ESM4AmmunitionTest, readsTheDataOfFallout4WithJustTheValueAndTheWeight)
    {
        std::string data;
        append<std::uint32_t>(data, 13);
        append<float>(data, 0.125f);

        const ESM4::Ammunition ammo = load(subRecord("DATA", data));

        EXPECT_EQ(ammo.mData.mValue, 13u);
        EXPECT_EQ(ammo.mData.mWeight, 0.125f);
    }

    TEST(ESM4AmmunitionTest, skipsDataOfAnUnknownSize)
    {
        const ESM4::Ammunition ammo
            = load(subRecord("DATA", bytePattern(5, 1)) + zString("EDID", "AfterTheData"));

        EXPECT_EQ(ammo.mData.mValue, 0u);
        EXPECT_EQ(ammo.mEditorId, "AfterTheData");
    }

    TEST(ESM4AmmunitionTest, readsTheSecondDataSubrecordOfNewVegas)
    {
        std::string data;
        append<std::uint32_t>(data, 4); // projectiles per shot
        append<std::uint32_t>(data, 0x00000830); // projectile
        append<float>(data, 0.75f); // weight
        append<std::uint32_t>(data, 0x00000831); // ammo that is used up
        append<float>(data, 50.f); // percentage that is used up

        const ESM4::Ammunition ammo = load(subRecord("DAT2", data));

        EXPECT_EQ(ammo.mData.mProjPerShot, 4u);
        EXPECT_EQ(ammo.mData.mProjectile.toUint32(), 0x830u);
        EXPECT_EQ(ammo.mData.mWeight, 0.75f);
        EXPECT_EQ(ammo.mData.mConsumedAmmo.toUint32(), 0x831u);
        EXPECT_EQ(ammo.mData.mConsumedPercentage, 50.f);
        // another size is left alone
        EXPECT_EQ(load(subRecord("DAT2", bytePattern(8, 1)) + zString("EDID", "Next")).mEditorId, "Next");
        EXPECT_EQ(load(subRecord("DAT2", bytePattern(8, 1))).mData.mProjPerShot, 0u);
    }

    TEST(ESM4AmmunitionTest, readsTheDamageSubrecordOfFallout4AndKeepsOnlyTheFirstByteOfItsFlags)
    {
        std::string data;
        append<std::uint32_t>(data, 0x00000840); // projectile
        append<std::uint32_t>(data, 0x12345603); // flags
        append<float>(data, 6.f); // damage
        append<std::uint32_t>(data, 99); // health

        const ESM4::Ammunition ammo = load(subRecord("DNAM", data));

        EXPECT_EQ(ammo.mData.mProjectile.toUint32(), 0x840u);
        EXPECT_EQ(ammo.mData.mFlags, 3u);
        EXPECT_EQ(ammo.mData.mDamage, 6.f);
        EXPECT_EQ(ammo.mData.mHealth, 99u);
    }

    TEST(ESM4AmmunitionTest, skipsTheSubrecordsItDoesNotUse)
    {
        std::string skipped;
        for (const char* type : { "MODT", "MODC", "MODS", "MODF", "OBND", "KSIZ", "KWDA", "DAMC", "DEST", "DMDC",
                 "DMDL", "DMDT", "DMDS", "DSTA", "DSTD", "DSTF", "PTRN", "NAM1", "NAM2" })
            skipped += subRecord(type, bytePattern(4, 1));

        const ESM4::Ammunition ammo = load(skipped + zString("EDID", "Last"));

        EXPECT_EQ(ammo.mEditorId, "Last");
    }

    TEST(ESM4AmmunitionTest, doesNotReadASubrecordItDoesNotKnow)
    {
        try
        {
            load(zString("EDID", "Ammo") + subRecord("XXYZ", ""));
            FAIL() << "No exception";
        }
        catch (const std::runtime_error& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("Unknown subrecord XXYZ"));
        }
    }
}
