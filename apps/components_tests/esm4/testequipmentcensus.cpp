#include <components/esm4/common.hpp>
#include <components/esm4/equipmentcensus.hpp>
#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    // The parts of the body of Fallout 3 and New Vegas
    constexpr std::uint32_t head = 0x1;
    constexpr std::uint32_t upperBody = 0x4;
    constexpr std::uint32_t leftHand = 0x8;
    constexpr std::uint32_t hat = 0x400;
    constexpr std::size_t upperBodyIndex = 2;
    constexpr std::size_t leftHandIndex = 3;
    constexpr std::size_t headIndex = 0;
    constexpr std::size_t hatIndex = 10;

    constexpr std::uint32_t powerArmour = 0x20;
    constexpr std::uint32_t notPlayable = 0x40;
    constexpr std::uint32_t useInventory = 0x0100;

    constexpr std::uint32_t suit = 0x1001;
    constexpr std::uint32_t jacket = 0x1002;
    constexpr std::uint32_t cap = 0x1003;
    constexpr std::uint32_t nothing = 0x1004;
    constexpr std::uint32_t oldArmour = 0x1005;
    constexpr std::uint32_t list = 0x1010;
    constexpr std::uint32_t otherList = 0x1011;
    constexpr std::uint32_t weapon = 0x1020;
    constexpr std::uint32_t noRecord = 0x1099;

    std::string bipedData(std::uint32_t parts, std::uint32_t flags)
    {
        std::string data;
        append(data, parts);
        append(data, flags);
        return subRecord("BMDT", data);
    }

    std::string item(std::uint32_t id)
    {
        std::string data;
        append(data, id);
        append<std::uint32_t>(data, 1);
        return subRecord("CNTO", data);
    }

    std::string entry(std::uint32_t id)
    {
        std::string data;
        append<std::int16_t>(data, 1);
        append<std::uint16_t>(data, 0);
        append(data, id);
        append<std::int16_t>(data, 1);
        append<std::uint16_t>(data, 0);
        return subRecord("LVLO", data);
    }

    // The base configuration of Fallout 3 and New Vegas, 24 bytes, with the flags of the template at the end
    std::string configuration(std::uint16_t templateFlags)
    {
        std::string data;
        append<std::uint32_t>(data, 0); // flags
        append<std::uint16_t>(data, 50); // fatigue
        append<std::uint16_t>(data, 0); // barter gold
        append<std::int16_t>(data, 1); // level
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 100); // speed multiplier
        append<float>(data, 0.f); // karma
        append<std::int16_t>(data, 50); // disposition
        append(data, templateFlags);
        return subRecord("ACBS", data);
    }

    std::string armour(std::uint32_t id, std::uint32_t parts, std::uint32_t flags, bool male, bool female)
    {
        std::string data = zString("EDID", "Armour") + bipedData(parts, flags);
        if (male)
            data += zString("MODL", "meshes\\armor\\male.nif");
        if (female)
            data += zString("MOD3", "meshes\\armor\\female.nif");
        return record("ARMO", id, data);
    }

    std::string basePlugin()
    {
        const std::string armours = armour(suit, upperBody | leftHand, powerArmour, true, true)
            + armour(jacket, upperBody, notPlayable, true, false) + armour(cap, hat, 0, true, false)
            + armour(nothing, 0, 0, false, false)
            // the BMDT of Oblivion has 4 bytes, and no part of the body that this census knows
            + record("ARMO", oldArmour,
                zString("EDID", "Old") + subRecord("BMDT", std::string(4, '\x04')) + zString("MODL", "old.nif"));

        const std::string lists = record("LVLI", list, entry(suit) + entry(otherList) + entry(weapon) + entry(noRecord))
            + record("LVLI", otherList, "");

        const std::string weapons = record("WEAP", weapon, zString("EDID", "Gun"));

        const std::string characters
            // lists a suit and a jacket that both cover the upper body, a cap, a gun and a levelled list
            = record("NPC_", 0x2001,
                  configuration(0) + item(suit) + item(jacket) + item(cap) + item(weapon) + item(list) + item(suit))
            // takes its items from the template
            + record("NPC_", 0x2002, configuration(useInventory) + item(suit))
            // lists nothing
            + record("NPC_", 0x2003, configuration(0))
            // a piece that covers nothing and a cap
            + record("NPC_", 0x2004, configuration(0) + item(nothing) + item(cap))
            // deleted after it was listed, so it is not a character
            + record("NPC_", 0x2005, configuration(0) + item(suit)) + record("NPC_", 0x2005, "", ESM4::Rec_Deleted);

        return header() + topGroup("ARMO", armours) + topGroup("LVLI", lists) + topGroup("WEAP", weapons)
            + topGroup("NPC_", characters);
    }

    std::string headerWithMaster(const std::string& master)
    {
        std::string hedr;
        append<float>(hedr, 0.8f);
        append<std::int32_t>(hedr, 0);
        append<std::uint32_t>(hedr, 0x800);
        return record(
            "TES4", 0, subRecord("HEDR", hedr) + zString("MAST", master) + valueSubRecord<std::uint64_t>("DATA", 0));
    }

    // Changes the cap of the base plugin to cover the head, deletes the jacket and the character with a cap and
    // a piece that covers nothing. The form IDs of the base plugin start with 00.
    std::string patchPlugin()
    {
        const std::string armours = armour(cap, head, 0, true, true) + record("ARMO", jacket, "", ESM4::Rec_Deleted);
        const std::string characters = record("NPC_", 0x2004, "", ESM4::Rec_Deleted);
        return headerWithMaster("base.esm") + topGroup("ARMO", armours) + topGroup("NPC_", characters);
    }

    void collect(ESM4::EquipmentCensus& census, const std::string& plugin, const std::string& fileName,
        std::uint32_t modIndex, const std::map<std::string, int>& loaded)
    {
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), fileName, nullptr, nullptr);
        reader.setModIndex(modIndex);
        reader.updateModIndices(loaded);
        census.collect(reader);
    }

    TEST(ESM4EquipmentCensusTest, countsWhatTheArmourCoversAndHasModelsFor)
    {
        ESM4::EquipmentCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        const ESM4::EquipmentCensus::Summary summary = census.summarize();

        EXPECT_EQ(summary.mArmour, 5u);
        EXPECT_EQ(summary.mNoBodyPart, 2u); // nothing, and the piece with the BMDT of Oblivion
        EXPECT_EQ(summary.mNoMaleModel, 1u);
        EXPECT_EQ(summary.mNoFemaleModel, 4u);
        EXPECT_EQ(summary.mNonPlayable, 1u);
        EXPECT_EQ(summary.mPowerArmour, 1u);
        EXPECT_EQ(summary.mCoveringBodyPart[upperBodyIndex], 2u);
        EXPECT_EQ(summary.mCoveringBodyPart[leftHandIndex], 1u);
        EXPECT_EQ(summary.mCoveringBodyPart[hatIndex], 1u);
        EXPECT_EQ(summary.mCoveringBodyPart[headIndex], 0u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4EquipmentCensusTest, countsThePiecesThatCharactersListAndWhichOfThemShareAPartOfTheBody)
    {
        ESM4::EquipmentCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        const ESM4::EquipmentCensus::Summary summary = census.summarize();

        // 0x2005 is deleted
        EXPECT_EQ(summary.mCharacters, 4u);
        EXPECT_EQ(summary.mInventoryFromTemplate, 1u);
        EXPECT_EQ(summary.mNoItems, 1u);
        EXPECT_EQ(summary.mWithArmour, 2u);
        EXPECT_EQ(summary.mWithLevelledList, 1u);
        // the first lists three pieces (the suit twice counts once), the other two
        EXPECT_EQ(summary.mPieces[3], 1u);
        EXPECT_EQ(summary.mPieces[2], 1u);
        EXPECT_EQ(summary.mPieces[0], 0u);
        EXPECT_EQ(summary.mWithOverlap, 1u);
        EXPECT_EQ(summary.mOverlapPart[upperBodyIndex], 1u);
        EXPECT_EQ(summary.mOverlapPart[leftHandIndex], 0u);
        EXPECT_EQ(summary.mListedPieces, 5u);
        EXPECT_EQ(summary.mListedNoBodyPart, 1u);
        EXPECT_EQ(summary.mListedNoModel, 1u);
        EXPECT_EQ(summary.mListedNonPlayable, 1u);
    }

    TEST(ESM4EquipmentCensusTest, countsWhatLevelledListsName)
    {
        ESM4::EquipmentCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        const ESM4::EquipmentCensus::Summary summary = census.summarize();

        EXPECT_EQ(summary.mLists, 2u);
        EXPECT_EQ(summary.mEntries.mArmour, 1u);
        EXPECT_EQ(summary.mEntries.mLists, 1u);
        EXPECT_EQ(summary.mEntries.mOther, 1u);
        EXPECT_EQ(summary.mEntries.mUnknown, 1u);
    }

    TEST(ESM4EquipmentCensusTest, countsARecordAsALaterFileHasIt)
    {
        ESM4::EquipmentCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        collect(census, patchPlugin(), "patch.esp", 1, { { "base.esm", 0 } });
        const ESM4::EquipmentCensus::Summary summary = census.summarize();

        // the jacket is deleted and the cap covers the head now
        EXPECT_EQ(summary.mArmour, 4u);
        EXPECT_EQ(summary.mNonPlayable, 0u);
        EXPECT_EQ(summary.mCoveringBodyPart[hatIndex], 0u);
        EXPECT_EQ(summary.mCoveringBodyPart[headIndex], 1u);
        EXPECT_EQ(summary.mCoveringBodyPart[upperBodyIndex], 1u);
        EXPECT_EQ(summary.mNoFemaleModel, 2u); // the cap has one now

        // the character with the cap and the piece that covers nothing is deleted, and the suit and the
        // levelled list are all that the first one lists of the pieces left
        EXPECT_EQ(summary.mCharacters, 3u);
        EXPECT_EQ(summary.mPieces[2], 1u); // suit and cap
        EXPECT_EQ(summary.mWithOverlap, 0u);
        EXPECT_EQ(summary.mListedPieces, 2u);
        EXPECT_EQ(summary.mListedNonPlayable, 0u);
    }

    TEST(ESM4EquipmentCensusTest, leavesOutARecordItCannotRead)
    {
        // a sub-record that says it has more data than the record has
        std::string cut = "BMDT";
        append<std::uint16_t>(cut, 50);
        cut.append(8, '\0');
        const std::string plugin
            = header() + topGroup("ARMO", record("ARMO", suit, cut) + armour(cap, hat, 0, true, false));

        ESM4::EquipmentCensus census;
        collect(census, plugin, "base.esm", 0, {});
        EXPECT_EQ(census.summarize().mArmour, 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4EquipmentCensusTest, writesTheCounts)
    {
        ESM4::EquipmentCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        std::ostringstream out;
        census.write(out);

        EXPECT_THAT(out.str(), HasSubstr("Armour records: 5\n"));
        EXPECT_THAT(out.str(), HasSubstr("  UpperBody"));
        EXPECT_THAT(out.str(), HasSubstr("Characters (NPC_ records): 4\n"));
        EXPECT_THAT(out.str(), HasSubstr("  with two pieces that cover the same part of the body: 1\n"));
        EXPECT_THAT(out.str(), HasSubstr("  3: 1\n"));
        EXPECT_THAT(out.str(), HasSubstr("Levelled lists (LVLI records): 2\n"));
        EXPECT_THAT(out.str(), Not(HasSubstr("Reading stopped early")));
    }
}
