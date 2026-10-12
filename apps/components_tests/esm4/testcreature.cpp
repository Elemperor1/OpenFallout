#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadlvlc.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    // Loads the one record of a plugin that is a Fallout 3 file, by its record only: the group headers of Fallout have
    // another size than the synthetic plugins have.
    template <class Record>
    Record loadFromAFalloutFile(std::string_view type, const std::string& data)
    {
        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string plugin
            = versionedRecord("TES4", 0, subRecord("HEDR", hedr), 15) + versionedRecord(type, 1, data, 15);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        EXPECT_TRUE(reader.isFalloutFile());
        reader.getRecordData();
        Record result;
        result.load(reader);
        return result;
    }

    // ACBS of Fallout 3 and New Vegas, 24 bytes
    std::string actorConfig(std::uint32_t flags, std::int16_t level, std::uint16_t templateFlags)
    {
        std::string data;
        append(data, flags);
        append<std::uint16_t>(data, 0); // fatigue
        append<std::uint16_t>(data, 0); // barter gold
        append(data, level);
        append<std::uint16_t>(data, 1); // calc min
        append<std::uint16_t>(data, 3); // calc max
        append<std::uint16_t>(data, 100); // speed multiplier
        append<float>(data, 0.f); // karma
        append<std::int16_t>(data, 0); // disposition base
        append(data, templateFlags);
        return data;
    }

    std::string formIdData(std::uint32_t id)
    {
        std::string data;
        append(data, id);
        return data;
    }

    TEST(ESM4CreatureTest, readsTheModelsAndAnimationsOfACreatureOfFallout)
    {
        const std::string data = zString("EDID", "Gecko") + zString("FULL", "Gecko")
            + zString("MODL", "Creatures\\Gecko\\Skeleton.nif")
            + subRecord("NIFZ", std::string("GeckoBody.nif\0GeckoEyes.nif\0", 28))
            + subRecord("KFFZ", std::string("Idle.kf\0", 8)) + subRecord("ACBS", actorConfig(0x1, 2, 0));
        const ESM4::Creature creature = loadFromAFalloutFile<ESM4::Creature>("CREA", data);

        EXPECT_TRUE(creature.mIsFONV);
        EXPECT_EQ(creature.mModel.getOriginal(), "Creatures\\Gecko\\Skeleton.nif");
        EXPECT_THAT(creature.mNif, ElementsAre("GeckoBody.nif", "GeckoEyes.nif"));
        EXPECT_THAT(creature.mKf, ElementsAre("Idle.kf"));
        EXPECT_EQ(creature.mBaseConfig.fo3.levelOrMult, 2);
        EXPECT_FALSE(creature.takesFromTemplate(ESM4::Creature::Template_UseModel));
        EXPECT_FALSE(creature.mHasBounds);
    }

    TEST(ESM4CreatureTest, readsTheVoiceTypeAndTheKindOfACreatureOfFallout)
    {
        std::string stats(17, '\0');
        stats[0] = 3; // the kind of creature
        stats[4] = 50; // health
        const std::string data = zString("EDID", "Beast") + subRecord("ACBS", actorConfig(0x1, 2, 0))
            + subRecord("VTCK", formIdData(0x00ABCDEF)) + subRecord("DATA", stats);

        const ESM4::Creature creature = loadFromAFalloutFile<ESM4::Creature>("CREA", data);

        EXPECT_EQ(creature.mVoice.toUint32(), 0x00ABCDEFu);
        EXPECT_EQ(creature.mCreatureType, 3);
        EXPECT_EQ(creature.mFalloutStats.mHealth, 50);
    }

    TEST(ESM4CreatureTest, doesNotReadAVoiceTypeThatIsNotOneForm)
    {
        std::string voice;
        append<std::uint32_t>(voice, 0x00ABCDEF);
        append<std::uint32_t>(voice, 0x00ABCDF0);
        const std::string data = zString("EDID", "Beast") + subRecord("ACBS", actorConfig(0x1, 2, 0))
            + subRecord("VTCK", voice) + zString("FULL", "Beast Name");

        const ESM4::Creature creature = loadFromAFalloutFile<ESM4::Creature>("CREA", data);

        EXPECT_TRUE(creature.mVoice.isZeroOrUnset());
        EXPECT_EQ(creature.mFullName, "Beast Name");
    }

    TEST(ESM4CreatureTest, readsTheTemplateAndWhatARecordTakesFromIt)
    {
        const std::uint16_t flags = ESM4::Creature::Template_UseModel | ESM4::Creature::Template_UseBaseData;
        const std::string data = zString("EDID", "Gecko") + subRecord("ACBS", actorConfig(0x1, 2, flags))
            + subRecord("TPLT", formIdData(0x00010005));
        const ESM4::Creature creature = loadFromAFalloutFile<ESM4::Creature>("CREA", data);

        EXPECT_EQ(creature.mBaseTemplate, ESM::FormId::fromUint32(0x00010005));
        EXPECT_TRUE(creature.takesFromTemplate(ESM4::Creature::Template_UseModel));
        EXPECT_TRUE(creature.takesFromTemplate(ESM4::Creature::Template_UseBaseData));
        EXPECT_FALSE(creature.takesFromTemplate(ESM4::Creature::Template_UseInventory));
    }

    TEST(ESM4CreatureTest, keepsEveryFactionOfACreature)
    {
        // SNAM: the faction, its rank and three bytes
        const auto faction = [](std::uint32_t id, std::int8_t rank) {
            std::string data = formIdData(id);
            append<std::int8_t>(data, rank);
            data.append(3, '\0');
            return subRecord("SNAM", data);
        };
        const std::string data = zString("EDID", "Gecko") + faction(0x00010001, 0) + faction(0x00010002, 3);
        const ESM4::Creature creature = loadFromAFalloutFile<ESM4::Creature>("CREA", data);

        ASSERT_EQ(creature.mFactions.size(), 2u);
        EXPECT_EQ(creature.mFactions[0].faction, 0x00010001u);
        EXPECT_EQ(creature.mFactions[0].rank, 0);
        EXPECT_EQ(creature.mFactions[1].faction, 0x00010002u);
        EXPECT_EQ(creature.mFactions[1].rank, 3);
    }

    TEST(ESM4CreatureTest, readsTheBoxOfACreature)
    {
        std::string box;
        append<std::int16_t>(box, -30);
        append<std::int16_t>(box, -20);
        append<std::int16_t>(box, 0);
        append<std::int16_t>(box, 30);
        append<std::int16_t>(box, 20);
        append<std::int16_t>(box, 50);
        const std::string data = zString("EDID", "Gecko") + subRecord("OBND", box);
        const ESM4::Creature creature = loadFromAFalloutFile<ESM4::Creature>("CREA", data);

        ASSERT_TRUE(creature.mHasBounds);
        EXPECT_EQ(creature.mBounds.mX1, -30);
        EXPECT_EQ(creature.mBounds.mY2, 20);
        EXPECT_EQ(creature.mBounds.mZ2, 50);
    }

    TEST(ESM4CreatureTest, leavesABoxOfAnotherSizeAlone)
    {
        const std::string data = zString("EDID", "Gecko") + subRecord("OBND", bytePattern(8, 1));
        EXPECT_FALSE(loadFromAFalloutFile<ESM4::Creature>("CREA", data).mHasBounds);
    }

    std::string levelledEntry(std::int16_t level, std::uint32_t item, std::int16_t count)
    {
        std::string data;
        append(data, level);
        append<std::uint16_t>(data, 0);
        append(data, item);
        append(data, count);
        append<std::uint16_t>(data, 0);
        return data;
    }

    TEST(ESM4LevelledCreatureTest, readsTheFlagsOfAListSoThatTheyAreKnownToBeThere)
    {
        // the chance of none is 30, and the list calculates from all levels (bit 0 of LVLF)
        const std::string data = zString("EDID", "LvlGecko") + valueSubRecord("LVLD", std::uint8_t(30))
            + valueSubRecord("LVLF", std::uint8_t(0x01)) + subRecord("LVLO", levelledEntry(1, 0x00010001, 1))
            + subRecord("LVLO", levelledEntry(5, 0x00010002, 2));
        const ESM4::LevelledCreature list = loadFromAFalloutFile<ESM4::LevelledCreature>("LVLC", data);

        EXPECT_TRUE(list.mHasLvlCreaFlags);
        EXPECT_TRUE(list.calcAllLvlLessThanPlayer());
        EXPECT_FALSE(list.calcEachItemInCount());
        EXPECT_EQ(list.chanceNone(), 30);
        ASSERT_EQ(list.mLvlObject.size(), 2u);
        EXPECT_EQ(list.mLvlObject[1].level, 5);
        EXPECT_EQ(list.mLvlObject[1].count, 2);
    }

    TEST(ESM4LevelledCreatureTest, aListWithoutFlagsCalculatesFromNoLevelsAndHasTheChanceItSays)
    {
        const std::string data = zString("EDID", "LvlGecko") + valueSubRecord("LVLD", std::uint8_t(30));
        const ESM4::LevelledCreature list = loadFromAFalloutFile<ESM4::LevelledCreature>("LVLC", data);

        EXPECT_FALSE(list.calcAllLvlLessThanPlayer());
        EXPECT_EQ(list.mChanceNone, 30);
    }

    TEST(ESM4CreatureTest, readsTheHealthAndAttributesOfACreatureOfFallout)
    {
        // DATA: type, combat, magic and stealth skills, health, unused, damage, then strength to luck
        std::string stats;
        for (const std::uint8_t value : { 4, 30, 0, 40 })
            append<std::uint8_t>(stats, value);
        append<std::uint16_t>(stats, 70);
        append<std::uint16_t>(stats, 0);
        append<std::uint16_t>(stats, 12);
        for (std::uint8_t value = 1; value <= 7; ++value)
            append<std::uint8_t>(stats, value + 2);
        const std::string data = zString("EDID", "Gecko") + subRecord("DATA", stats);
        const ESM4::Creature creature = loadFromAFalloutFile<ESM4::Creature>("CREA", data);

        EXPECT_TRUE(creature.mFalloutStats.mKnown);
        EXPECT_EQ(creature.mFalloutStats.mHealth, 70);
        EXPECT_THAT(creature.mFalloutStats.mAttributes, ElementsAre(3, 4, 5, 6, 7, 8, 9));
    }
}
