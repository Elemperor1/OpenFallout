#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/wornarmorcensus.hpp>

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

    constexpr std::uint32_t upperBody = 0x4;
    constexpr std::uint32_t leftHand = 0x8;
    constexpr std::uint32_t hat = 0x400;

    constexpr std::uint16_t useInventory = 0x0100;
    constexpr std::uint16_t useTraits = 0x0001;
    constexpr std::uint32_t female = 0x1;

    constexpr std::uint32_t suit = 0x1001;
    constexpr std::uint32_t jacket = 0x1002;
    constexpr std::uint32_t cap = 0x1003;
    constexpr std::uint32_t unnamedPiece = 0x1004;
    constexpr std::uint32_t outfits = 0x1010;
    constexpr std::uint32_t noRecord = 0x1099;

    constexpr std::uint16_t newVegasHeaderVersion = 134; // 1.34

    // A group in the layout of Fallout 3 and the games after it, which has a header of 24 bytes.
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

    // The TES4 record of a plugin of New Vegas, with the names of its masters
    std::string versionedHeader(const std::vector<std::string>& masters = {})
    {
        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        std::string data = subRecord("HEDR", hedr);
        for (const std::string& master : masters)
            data += zString("MAST", master) + valueSubRecord<std::uint64_t>("DATA", 0);
        return versionedRecord("TES4", 0, data, 15);
    }

    std::string rec(std::string_view type, std::uint32_t id, const std::string& data)
    {
        return versionedRecord(type, id, data, 15);
    }

    std::string deleted(std::string_view type, std::uint32_t id)
    {
        std::string result = rec(type, id, "");
        // the flags are the second word after the type and the size
        const std::uint32_t flags = ESM4::Rec_Deleted;
        result.replace(8, sizeof(flags), reinterpret_cast<const char*>(&flags), sizeof(flags));
        return result;
    }

    // BMDT of Fallout 3 and New Vegas: the parts of the body, then flags
    std::string armour(std::uint32_t id, std::uint32_t parts, const char* male, const char* femaleModel = "")
    {
        std::string bmdt;
        append(bmdt, parts);
        append<std::uint32_t>(bmdt, 0);
        std::string data = zString("EDID", "Armour") + subRecord("BMDT", bmdt);
        if (*male != '\0')
            data += zString("MODL", male);
        if (*femaleModel != '\0')
            data += zString("MOD3", femaleModel);
        return rec("ARMO", id, data);
    }

    std::string entry(int level, std::uint32_t item)
    {
        std::string data;
        append<std::int16_t>(data, static_cast<std::int16_t>(level));
        append<std::uint16_t>(data, 0);
        append(data, item);
        append<std::int16_t>(data, 1);
        append<std::uint16_t>(data, 0);
        return subRecord("LVLO", data);
    }

    std::string list(std::uint32_t id, const std::string& entries)
    {
        return rec("LVLI", id,
            zString("EDID", "List") + valueSubRecord<std::int8_t>("LVLD", 0) + valueSubRecord<std::uint8_t>("LVLF", 0)
                + entries);
    }

    std::string item(std::uint32_t id)
    {
        std::string data;
        append(data, id);
        append<std::uint32_t>(data, 1);
        return subRecord("CNTO", data);
    }

    // The base configuration of Fallout 3 and New Vegas, 24 bytes
    std::string configuration(std::uint32_t flags, int level, std::uint16_t templateFlags)
    {
        std::string data;
        append(data, flags);
        append<std::uint16_t>(data, 50); // fatigue
        append<std::uint16_t>(data, 0); // barter gold
        append<std::int16_t>(data, static_cast<std::int16_t>(level));
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 100); // speed multiplier
        append<float>(data, 0.f); // karma
        append<std::int16_t>(data, 50); // disposition
        append(data, templateFlags);
        return subRecord("ACBS", data);
    }

    std::string character(std::uint32_t id, std::uint32_t flags, std::uint16_t templateFlags, const std::string& items,
        std::uint32_t templateId = 0)
    {
        std::string data = zString("EDID", "Character") + configuration(flags, 5, templateFlags) + items;
        if (templateId != 0)
            data += valueSubRecord<std::uint32_t>("TPLT", templateId);
        return rec("NPC_", id, data);
    }

    std::string basePlugin()
    {
        const std::string armours = armour(suit, upperBody | leftHand, "suit.nif", "suit_f.nif")
            + armour(jacket, upperBody, "jacket.nif") + armour(cap, hat, "cap.nif") + armour(unnamedPiece, hat, "");

        // at level 5 the entries of level 1 and 3 may be chosen, and only the higher one is; level 9 is too high
        const std::string lists = list(outfits, entry(1, suit) + entry(3, jacket) + entry(9, cap));

        const std::string characters
            // a list that gives the jacket
            = character(0x2001, 0, 0, item(outfits))
            // takes everything from the first
            + character(0x2002, 0, useInventory | useTraits, "", 0x2001)
            // a suit and a jacket that both cover the upper body, and a cap
            + character(0x2003, 0, 0, item(suit) + item(jacket) + item(cap))
            // a woman in a jacket, which has no female model
            + character(0x2004, female, 0, item(jacket))
            // wears a piece with no model, and a name that is no record
            + character(0x2005, 0, 0, item(unnamedPiece) + item(noRecord))
            // lists nothing
            + character(0x2006, 0, 0, "")
            // deleted after it was listed
            + character(0x2007, 0, 0, item(suit)) + deleted("NPC_", 0x2007);

        return versionedHeader() + versionedGroup("ARMO", armours) + versionedGroup("LVLI", lists)
            + versionedGroup("NPC_", characters);
    }

    void collect(ESM4::WornArmorCensus& census, const std::string& plugin, const std::string& fileName = "base.esm",
        std::uint32_t modIndex = 0, const std::map<std::string, int>& loaded = {})
    {
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), fileName, nullptr, nullptr);
        reader.setModIndex(modIndex);
        reader.updateModIndices(loaded);
        census.collect(reader);
    }

    TEST(ESM4WornArmorCensusTest, countsWhatTheCharactersWearWithTheCodeOfTheGame)
    {
        ESM4::WornArmorCensus census;
        collect(census, basePlugin());
        const ESM4::WornArmorCensus::Summary summary = census.summarize();

        // 0x2007 is deleted
        EXPECT_EQ(summary.mCharacters, 6u);
        EXPECT_EQ(summary.mWithoutTraits, 0u);
        EXPECT_EQ(summary.mWomen, 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());

        // the jacket from the list (twice, as the second takes it from the first), the suit with the cap, the jacket,
        // and none for the character that lists a piece with no model and the one that lists nothing
        EXPECT_EQ(summary.mPieces[0], 2u);
        EXPECT_EQ(summary.mPieces[1], 3u);
        EXPECT_EQ(summary.mPieces[2], 1u);
        EXPECT_EQ(summary.mCoverUpperBody, 4u);
        EXPECT_EQ(summary.mCoverHands, 1u);
        EXPECT_EQ(summary.mWithArmorButNoneShows, 1u);
        EXPECT_EQ(summary.mPiecesOverlapping, 1u);
        EXPECT_EQ(summary.mPiecesWithoutModel, 1u);
        EXPECT_EQ(summary.mWomenWearingMaleModels, 1u);

        EXPECT_EQ(summary.mTrace.mInventoryFromTemplate, 1u);
        EXPECT_EQ(summary.mTrace.mListsEntered, 1u + 1u);
        EXPECT_EQ(summary.mTrace.mArmorFromLists, 2u);
        EXPECT_EQ(summary.mTrace.mOtherItems, 1u);
    }

    TEST(ESM4WornArmorCensusTest, triesOtherLevelsOfThePlayer)
    {
        ESM4::WornArmorCensus census;
        collect(census, basePlugin());
        const ESM4::WornArmorCensus::Summary summary = census.summarize();

        // at level 1 the list gives the suit, at every level the characters with their own pieces are dressed; the
        // three characters with a list are 0x2001, 0x2002 (its template) and nobody else
        for (const std::size_t dressed : summary.mDressedByLevel)
            EXPECT_EQ(dressed, 4u);
    }

    TEST(ESM4WornArmorCensusTest, aLaterFileReplacesAndDeletesRecords)
    {
        ESM4::WornArmorCensus census;
        collect(census, basePlugin());

        // the jacket now covers the head (the cap's slot is its own) and a character is deleted
        const std::string patch = versionedHeader({ "base.esm" })
            + versionedGroup("ARMO", armour(jacket, hat, "jacket.nif"))
            + versionedGroup("NPC_", deleted("NPC_", 0x2003));
        collect(census, patch, "patch.esp", 1, { { "base.esm", 0 } });
        const ESM4::WornArmorCensus::Summary summary = census.summarize();

        EXPECT_EQ(summary.mCharacters, 5u);
        // the jacket covers a hat and no longer the upper body, so the woman's jacket and the list's jacket are alone
        EXPECT_EQ(summary.mCoverUpperBody, 0u);
        EXPECT_EQ(summary.mPiecesOverlapping, 0u);
    }

    TEST(ESM4WornArmorCensusTest, writesTheCounts)
    {
        ESM4::WornArmorCensus census;
        collect(census, basePlugin());
        std::ostringstream out;
        census.write(out);

        EXPECT_THAT(out.str(), HasSubstr("Characters (NPC_ records): 6\n"));
        EXPECT_THAT(out.str(), HasSubstr("Player level assumed: 5\n"));
        EXPECT_THAT(out.str(), HasSubstr("  taken from a template: 1\n"));
        EXPECT_THAT(out.str(), HasSubstr("  the upper body: 4\n"));
        EXPECT_THAT(out.str(), HasSubstr("level 30:"));
        EXPECT_THAT(out.str(), Not(HasSubstr("Reading stopped early")));
    }
}
