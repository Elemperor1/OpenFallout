#include <components/esm4/loadfact.hpp>

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

    /// Build an XNAM relation in the 8 byte layout of older plugins, which stops after the modifier.
    std::string relation(std::uint32_t target, std::int32_t modifier)
    {
        std::string data;
        append(data, target);
        append(data, modifier);
        return subRecord("XNAM", data);
    }

    /// Build an XNAM relation in the 12 byte layout of Fallout 3 and New Vegas, with the group combat reaction.
    std::string relation(std::uint32_t target, std::int32_t modifier, std::uint32_t reaction)
    {
        std::string data;
        append(data, target);
        append(data, modifier);
        append(data, reaction);
        return subRecord("XNAM", data);
    }

    /// Build a DATA sub-record with the two flag bytes and, if the size is four, the two unused ones.
    std::string flags(std::uint8_t first, std::uint8_t second)
    {
        std::string data;
        append(data, first);
        append(data, second);
        append<std::uint16_t>(data, 0);
        return subRecord("DATA", data);
    }

    /// Load the one faction in the plugin and return the message of the exception that is thrown, if any.
    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    /// Verify a faction keeps its relations, flags and ranks in file order.
    TEST(ESM4FactionTest, readsRelationsFlagsAndRanks)
    {
        const std::string data = zString("EDID", "TestFaction") + zString("FULL", "Test Faction")
            + relation(0x000a0001, 10, 2) + relation(0x000a0002, -20, 0) + flags(0x05, 0x03)
            + valueSubRecord<float>("CNAM", 1.5f) + valueSubRecord<std::int32_t>("RNAM", 0)
            + zString("MNAM", "Initiate") + zString("FNAM", "Initiatrix") + zString("INAM", "icon.dds")
            + valueSubRecord<std::int32_t>("RNAM", 1) + zString("MNAM", "Knight");

        const std::vector<ESM4::Faction> factions = loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data));

        ASSERT_EQ(factions.size(), 1u);
        const ESM4::Faction& faction = factions.front();
        EXPECT_EQ(faction.mEditorId, "TestFaction");
        EXPECT_EQ(faction.mFullName, "Test Faction");
        ASSERT_EQ(faction.mRelations.size(), 2u);
        EXPECT_EQ(faction.mRelations[0].mTarget.toUint32(), 0x000a0001u);
        EXPECT_EQ(faction.mRelations[0].mModifier, 10);
        EXPECT_EQ(faction.mRelations[0].mGroupCombatReaction, ESM4::Faction::Reaction_Ally);
        EXPECT_EQ(faction.mRelations[1].mTarget.toUint32(), 0x000a0002u);
        EXPECT_EQ(faction.mRelations[1].mModifier, -20);
        EXPECT_EQ(faction.mFactionFlags, ESM4::Faction::Flag_HiddenFromPlayer | ESM4::Faction::Flag_SpecialCombat);
        EXPECT_EQ(faction.mFactionFlags2, ESM4::Faction::Flag_TrackCrime | ESM4::Faction::Flag_AllowSell);
        EXPECT_EQ(faction.mCrimeGoldMultiplier, 1.5f);
        ASSERT_EQ(faction.mRanks.size(), 2u);
        EXPECT_EQ(faction.mRanks[0].mIndex, 0);
        EXPECT_EQ(faction.mRanks[0].mMaleTitle, "Initiate");
        EXPECT_EQ(faction.mRanks[0].mFemaleTitle, "Initiatrix");
        EXPECT_EQ(faction.mRanks[0].mInsignia, "icon.dds");
        EXPECT_EQ(faction.mRanks[1].mIndex, 1);
        EXPECT_EQ(faction.mRanks[1].mMaleTitle, "Knight");
        EXPECT_EQ(faction.mRanks[1].mFemaleTitle, "");
        EXPECT_EQ(faction.mRanks[1].mInsignia, "");
        EXPECT_TRUE(faction.mReputation.isZeroOrUnset());
    }

    /// Verify relations keep the group combat reaction, or have none in the 8 byte layout, and the faction keeps its
    /// reputation.
    TEST(ESM4FactionTest, readsBothRelationLayoutsAndTheReputation)
    {
        const std::string data = zString("EDID", "VegasFaction") + relation(0x000a0001, 0, 3) + relation(0x000a0002, 1)
            + relation(0x000a0003, -1, 1) + valueSubRecord<std::uint32_t>("WMI1", 0x000a0009);

        const std::vector<ESM4::Faction> factions = loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data));

        ASSERT_EQ(factions.size(), 1u);
        const ESM4::Faction& faction = factions.front();
        ASSERT_EQ(faction.mRelations.size(), 3u);
        EXPECT_EQ(faction.mRelations[0].mGroupCombatReaction, ESM4::Faction::Reaction_Friend);
        EXPECT_EQ(faction.mRelations[1].mGroupCombatReaction, ESM4::Faction::Reaction_Neutral);
        EXPECT_EQ(faction.mRelations[1].mModifier, 1);
        EXPECT_EQ(faction.mRelations[2].mGroupCombatReaction, ESM4::Faction::Reaction_Enemy);
        EXPECT_EQ(faction.mRelations[2].mModifier, -1);
        EXPECT_EQ(faction.mReputation.toUint32(), 0x000a0009u);
    }

    /// Verify DATA with one or two bytes leaves the flags it does not hold at zero, and a faction can have none.
    TEST(ESM4FactionTest, readsFlagsOfEverySupportedSize)
    {
        const std::string one = zString("EDID", "One") + subRecord("DATA", std::string("\x02", 1));
        const std::string two = zString("EDID", "Two") + subRecord("DATA", std::string("\x04\x01", 2));
        const std::string none = zString("EDID", "None");

        const std::vector<ESM4::Faction> factions = loadRecords<ESM4::Faction>(
            "FACT", record("FACT", 1, one) + record("FACT", 2, two) + record("FACT", 3, none));

        ASSERT_EQ(factions.size(), 3u);
        EXPECT_EQ(factions[0].mFactionFlags, ESM4::Faction::Flag_Evil);
        EXPECT_EQ(factions[0].mFactionFlags2, 0);
        EXPECT_EQ(factions[1].mFactionFlags, ESM4::Faction::Flag_SpecialCombat);
        EXPECT_EQ(factions[1].mFactionFlags2, ESM4::Faction::Flag_TrackCrime);
        EXPECT_EQ(factions[2].mFactionFlags, 0);
        EXPECT_EQ(factions[2].mFactionFlags2, 0);
        EXPECT_TRUE(factions[2].mRelations.empty());
        EXPECT_TRUE(factions[2].mRanks.empty());
    }

    /// Verify an empty rank title, which has no bytes at all, is read as an empty string.
    TEST(ESM4FactionTest, readsRankTitlesWithoutBytes)
    {
        const std::string data = zString("EDID", "Titles") + valueSubRecord<std::int32_t>("RNAM", 4)
            + subRecord("MNAM", "") + zString("FNAM", "") + zString("INAM", "x");

        const std::vector<ESM4::Faction> factions = loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data));

        ASSERT_EQ(factions.size(), 1u);
        ASSERT_EQ(factions.front().mRanks.size(), 1u);
        EXPECT_EQ(factions.front().mRanks.front().mMaleTitle, "");
        EXPECT_EQ(factions.front().mRanks.front().mFemaleTitle, "");
        EXPECT_EQ(factions.front().mRanks.front().mInsignia, "x");
    }

    /// Verify a compressed faction is read like any other.
    TEST(ESM4FactionTest, readsACompressedFaction)
    {
        const std::string data = zString("EDID", "Packed") + relation(0x000a0001, 5) + flags(1, 0)
            + valueSubRecord<std::int32_t>("RNAM", 0) + zString("MNAM", "Last");

        const std::vector<ESM4::Faction> factions
            = loadRecords<ESM4::Faction>("FACT", compressedRecord("FACT", 1, data));

        ASSERT_EQ(factions.size(), 1u);
        EXPECT_EQ(factions.front().mEditorId, "Packed");
        ASSERT_EQ(factions.front().mRanks.size(), 1u);
        EXPECT_EQ(factions.front().mRanks.front().mMaleTitle, "Last");
    }

    /// Verify sizes that no version of the record has are refused, not read as something else.
    TEST(ESM4FactionTest, rejectsSizesNoVersionHas)
    {
        const std::string edid = zString("EDID", "Bad");
        const std::vector<std::pair<std::string, std::string>> cases = {
            { "XNAM", subRecord("XNAM", std::string(10, '\0')) },
            { "XNAM", subRecord("XNAM", std::string(4, '\0')) },
            { "DATA", subRecord("DATA", "") },
            { "DATA", subRecord("DATA", std::string(5, '\0')) },
            { "CNAM", subRecord("CNAM", std::string(2, '\0')) },
            { "RNAM", subRecord("RNAM", std::string(2, '\0')) },
            { "WMI1", subRecord("WMI1", std::string(8, '\0')) },
        };

        for (const auto& [type, subrecord] : cases)
            EXPECT_THAT(loadFailure(edid + subrecord), HasSubstr(type + " has an unexpected size")) << type;
    }

    /// Verify a rank title with no rank before it is refused.
    TEST(ESM4FactionTest, rejectsARankTitleWithoutARank)
    {
        for (const char* type : { "MNAM", "FNAM", "INAM" })
            EXPECT_THAT(loadFailure(zString("EDID", "Bad") + zString(type, "Title")), HasSubstr("before RNAM")) << type;
    }

    /// Verify a sub-record this loader does not know is reported by name, as the other loaders do.
    TEST(ESM4FactionTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_THAT(
            loadFailure(zString("EDID", "Bad") + subRecord("ZZZZ", "1234")), HasSubstr("Unknown subrecord ZZZZ"));
    }

    /// Verify a sub-record that promises more bytes than its record holds is refused, not read from the next record.
    TEST(ESM4FactionTest, rejectsASubrecordThatCrossesItsRecord)
    {
        // The sub-record header promises twelve bytes, the record holds four, and another record follows.
        std::string crossing = "XNAM";
        append<std::uint16_t>(crossing, 12);
        append<std::uint32_t>(crossing, 0x000a0001);
        const std::string first = zString("EDID", "Cross") + crossing;
        const std::string second = zString("EDID", "Next") + relation(0x000a0002, 1, 1);

        try
        {
            loadRecords<ESM4::Faction>("FACT", record("FACT", 1, first) + record("FACT", 2, second));
            FAIL() << "the XNAM was read across the record boundary";
        }
        catch (const std::exception& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("longer than its record"));
        }
    }

    /// Verify bytes that are too few for another sub-record header at the end of a record are refused, not ignored.
    TEST(ESM4FactionTest, rejectsBytesLeftOverAfterTheLastSubrecord)
    {
        const std::string edid = zString("EDID", "Left");

        for (const std::size_t extra : { 1u, 3u, 5u })
            EXPECT_THAT(loadFailure(edid + std::string(extra, '\x01')), HasSubstr("unread bytes")) << extra;
    }

    /// Verify a file that ends inside the header of the next sub-record is refused, not read as a complete record.
    TEST(ESM4FactionTest, rejectsAFileThatEndsInsideASubrecordHeader)
    {
        // The record promises the whole header of an XNAM and the file ends after three of its six bytes.
        std::string header = "XNAM";
        append<std::uint16_t>(header, 12);
        const std::string data = zString("EDID", "Cut") + header;

        try
        {
            loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data), 3);
            FAIL() << "the cut header was accepted";
        }
        catch (const std::exception& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("unread bytes"));
        }
    }

    /// Verify records that end exactly at their last sub-record are not mistaken for records that end too early: a
    /// plain one, a compressed one and one that ends with a sub-record that has no bytes.
    TEST(ESM4FactionTest, acceptsRecordsThatEndExactlyAtTheirLastSubrecord)
    {
        const std::string data = zString("EDID", "Exact") + subRecord("DATA", "\x01") + relation(0x000a0001, 1, 0);
        const std::string emptyTitle
            = zString("EDID", "Empty") + valueSubRecord<std::int32_t>("RNAM", 0) + subRecord("MNAM", "");

        EXPECT_EQ(loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data)).size(), 1u);
        EXPECT_EQ(loadRecords<ESM4::Faction>("FACT", compressedRecord("FACT", 1, data)).size(), 1u);
        EXPECT_EQ(loadRecords<ESM4::Faction>("FACT", record("FACT", 1, emptyTitle)).size(), 1u);
    }

    /// Verify a compressed record whose last sub-record has 9 bytes or less is read in full. The size of a compressed
    /// record is 4 bytes below the size of its data, which once hid such a sub-record from the reader.
    TEST(ESM4FactionTest, readsAShortLastSubrecordOfACompressedRecord)
    {
        const std::string data = zString("EDID", "Short") + relation(0x000a0001, 1, 0) + subRecord("DATA", "\x01");

        const std::vector<ESM4::Faction> factions
            = loadRecords<ESM4::Faction>("FACT", compressedRecord("FACT", 1, data));

        ASSERT_EQ(factions.size(), 1u);
        EXPECT_EQ(factions.front().mFactionFlags, 1);
        EXPECT_EQ(factions.front().mRelations.size(), 1u);
    }

    /// Verify a file that ends one byte early, in the terminator of a string, is refused, not read as complete.
    TEST(ESM4FactionTest, rejectsAStringWhoseTerminatorTheFileEndsBefore)
    {
        const std::string edid = zString("EDID", "Cut");
        const std::vector<std::pair<std::string, std::string>> cases = {
            { "EDID", zString("EDID", "Name") },
            { "FULL", edid + zString("FULL", "Name") },
            { "MNAM", edid + valueSubRecord<std::int32_t>("RNAM", 1) + zString("MNAM", "Title") },
        };

        for (const auto& [type, data] : cases)
        {
            try
            {
                loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data), 1);
                ADD_FAILURE() << type << " cut before its terminator was accepted";
            }
            catch (const std::exception& e)
            {
                EXPECT_THAT(e.what(), HasSubstr("shorter than its size")) << type;
            }
        }
    }

    /// Verify a file that ends inside a field is refused, not read as zeros.
    TEST(ESM4FactionTest, rejectsFieldsThatTheFileEndsInside)
    {
        const std::string edid = zString("EDID", "Cut");
        const std::vector<std::pair<std::string, std::string>> cases = {
            { "FULL", edid + zString("FULL", "Name") },
            { "XNAM", edid + relation(0x000a0001, 1) },
            { "XNAM", edid + relation(0x000a0001, 1, 2) },
            { "DATA", edid + flags(1, 1) },
            { "CNAM", edid + valueSubRecord<float>("CNAM", 1.f) },
            { "RNAM", edid + valueSubRecord<std::int32_t>("RNAM", 1) },
            { "WMI1", edid + valueSubRecord<std::uint32_t>("WMI1", 1) },
            { "MNAM", edid + valueSubRecord<std::int32_t>("RNAM", 1) + zString("MNAM", "Title") },
        };

        for (const auto& [type, data] : cases)
        {
            try
            {
                loadRecords<ESM4::Faction>("FACT", record("FACT", 1, data), 2);
                ADD_FAILURE() << type << " cut by two bytes was accepted";
            }
            catch (const std::exception& e)
            {
                EXPECT_THAT(e.what(), HasSubstr("shorter than its size")) << type;
            }
        }
    }
}
