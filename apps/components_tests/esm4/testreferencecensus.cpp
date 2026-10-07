#include <components/esm4/common.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/referencecensus.hpp>

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

    constexpr std::uint32_t stat = 0x1001;
    constexpr std::uint32_t npc = 0x1002;
    constexpr std::uint32_t note = 0x1003;
    constexpr std::uint32_t tree = 0x1004;

    // The header of a plugin that has one master.
    std::string headerWithMaster(const std::string& master)
    {
        std::string hedr;
        append<float>(hedr, 0.8f);
        append<std::int32_t>(hedr, 0);
        append<std::uint32_t>(hedr, 0x800);
        return record(
            "TES4", 0, subRecord("HEDR", hedr) + zString("MAST", master) + valueSubRecord<std::uint64_t>("DATA", 0));
    }

    std::string name(std::uint32_t base)
    {
        return valueSubRecord("NAME", base);
    }

    std::string basePlugin()
    {
        // A static, a character, a note and a tree, and the references that place them. The note has no model, the
        // tree has one that is not a nif.
        const std::string records
            = record("STAT", stat, zString("EDID", "Stone") + zString("MODL", "meshes\\Stone.NIF"))
            + record("NPC_", npc, zString("EDID", "Guard") + zString("MODL", "meshes\\skeleton.nif"))
            + record("NOTE", note, zString("EDID", "Letter"))
            + record("TREE", tree, zString("MODL", "trees\\shrub.spt")) + record("REFR", 0x2001, name(stat))
            + record("REFR", 0x2002, name(stat), ESM4::Rec_Disabled) + record("ACHR", 0x2003, name(npc))
            + record("REFR", 0x2004, name(note)) + record("REFR", 0x2005, name(0x1009)) // no record has this form ID
            + record("REFR", 0x2006, zString("EDID", "NoBase")) + record("REFR", 0x2007, name(stat), ESM4::Rec_Deleted)
            + compressedRecord("REFR", 0x2008, name(stat)) + record("REFR", 0x2009, name(tree));
        return header() + topGroup("STAT", records);
    }

    // A plugin that deletes a reference of the base plugin, changes another one and places a new one. Its form IDs
    // start with 00 for the base plugin and with 01 for the plugin itself.
    std::string patchPlugin()
    {
        const std::string records = record("REFR", 0x00002001, "", ESM4::Rec_Deleted)
            + record("REFR", 0x00002002, name(stat)) + record("REFR", 0x01000001, name(0x00001003));
        return headerWithMaster("base.esm") + topGroup("REFR", records);
    }

    void collect(ESM4::ReferenceCensus& census, const std::string& plugin, const std::string& fileName,
        std::uint32_t modIndex, const std::map<std::string, int>& loaded)
    {
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), fileName, nullptr, nullptr);
        reader.setModIndex(modIndex);
        reader.updateModIndices(loaded);
        census.collect(reader);
    }

    using Counts = std::map<std::string, std::map<std::string, ESM4::ReferenceCensus::Count>>;

    std::size_t total(const Counts& counts, const std::string& base, const std::string& reference)
    {
        const auto baseIt = counts.find(base);
        if (baseIt == counts.end())
            return 0;
        const auto it = baseIt->second.find(reference);
        return it == baseIt->second.end() ? 0 : it->second.mTotal;
    }

    TEST(ESM4ReferenceCensusTest, countsReferencesByTheRecordTypeOfTheirBaseObject)
    {
        ESM4::ReferenceCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});

        const Counts counts = census.getCounts();
        // 0x2001, 0x2002 and the compressed 0x2008; 0x2007 is deleted
        EXPECT_EQ(total(counts, "STAT", "REFR"), 3u);
        EXPECT_EQ(counts.at("STAT").at("REFR").mDisabled, 1u);
        EXPECT_EQ(total(counts, "NPC_", "ACHR"), 1u);
        EXPECT_EQ(total(counts, "NOTE", "REFR"), 1u);
        EXPECT_EQ(total(counts, ESM4::ReferenceCensus::unknownBase, "REFR"), 1u);
        EXPECT_EQ(total(counts, ESM4::ReferenceCensus::noBase, "REFR"), 1u);
        EXPECT_EQ(total(counts, "TREE", "REFR"), 1u);
        EXPECT_EQ(counts.size(), 6u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4ReferenceCensusTest, countsReferencesByTheFileExtensionOfTheModelOfTheirBaseObject)
    {
        ESM4::ReferenceCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});

        // The extension is in lower case. A reference with no base object, or one that no record has, has no model to
        // count.
        EXPECT_THAT(census.getModels(),
            UnorderedElementsAre(Pair("STAT", UnorderedElementsAre(Pair("nif", 3u))),
                Pair("NPC_", UnorderedElementsAre(Pair("nif", 1u))),
                Pair("NOTE", UnorderedElementsAre(Pair(ESM4::ReferenceCensus::noModel, 1u))),
                Pair("TREE", UnorderedElementsAre(Pair("spt", 1u)))));
    }

    TEST(ESM4ReferenceCensusTest, listsTheBaseObjectsThatNoRecordWasFoundForCommonestFirst)
    {
        const std::string plugin = header()
            + topGroup("REFR",
                record("REFR", 0x2001, name(0x1009)) + record("REFR", 0x2002, name(0x100a))
                    + record("REFR", 0x2003, name(0x100a)) + record("REFR", 0x2004, name(stat))
                    + record("STAT", stat, ""));
        ESM4::ReferenceCensus census;
        collect(census, plugin, "base.esm", 0, {});

        const auto unknown = census.getUnknownBases(10);
        ASSERT_EQ(unknown.size(), 2u);
        EXPECT_EQ(unknown[0].first.mIndex, 0x100au);
        EXPECT_EQ(unknown[0].second, 2u);
        EXPECT_EQ(unknown[1].first.mIndex, 0x1009u);
        EXPECT_EQ(census.getUnknownBases(1).size(), 1u);
    }

    TEST(ESM4ReferenceCensusTest, aModelOfAnOverridingRecordReplacesTheOneItHad)
    {
        // The patch gives the static of the base plugin a model that is not a nif, and a second record leaves the
        // model out.
        ESM4::ReferenceCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        const std::string patch = headerWithMaster("base.esm")
            + topGroup("STAT",
                record("STAT", 0x00001001, zString("MODL", "meshes\\stone.kf")) + record("NPC_", 0x00001002, ""));
        collect(census, patch, "patch.esp", 1, { { "base.esm", 0 } });

        const auto models = census.getModels();
        EXPECT_THAT(models.at("STAT"), UnorderedElementsAre(Pair("kf", 3u)));
        EXPECT_THAT(models.at("NPC_"), UnorderedElementsAre(Pair("nif", 1u)));
    }

    TEST(ESM4ReferenceCensusTest, aReferenceThatALaterFileOverridesOrDeletesCountsAsThatFileHasIt)
    {
        ESM4::ReferenceCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        collect(census, patchPlugin(), "patch.esp", 1, { { "base.esm", 0 } });

        const Counts counts = census.getCounts();
        // The base plugin has 3 references to the static. One is deleted by the patch, and 0x2002 is not disabled any
        // more.
        EXPECT_EQ(total(counts, "STAT", "REFR"), 2u);
        EXPECT_EQ(counts.at("STAT").at("REFR").mDisabled, 0u);
        // The reference the patch places to the note of the base plugin
        EXPECT_EQ(total(counts, "NOTE", "REFR"), 2u);
        EXPECT_EQ(census.getDeleted(), 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4ReferenceCensusTest, anOverrideThatNamesNoBaseKeepsItAndOneThatNamesNullReplacesIt)
    {
        ESM4::ReferenceCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        // 0x2001 has no NAME in the patch, 0x2002 has a NAME of zero.
        const std::string patch = headerWithMaster("base.esm")
            + topGroup(
                "REFR", record("REFR", 0x00002001, zString("EDID", "Same")) + record("REFR", 0x00002002, name(0)));
        collect(census, patch, "patch.esp", 1, { { "base.esm", 0 } });

        const Counts counts = census.getCounts();
        EXPECT_EQ(total(counts, "STAT", "REFR"), 2u);
        EXPECT_EQ(total(counts, ESM4::ReferenceCensus::noBase, "REFR"), 2u);
    }

    TEST(ESM4ReferenceCensusTest, findsABaseObjectThatIsWrittenAfterTheReferenceToIt)
    {
        const std::string plugin = header()
            + topGroup("REFR", record("REFR", 0x2001, name(stat)) + record("STAT", stat, zString("EDID", "Stone")));
        ESM4::ReferenceCensus census;
        collect(census, plugin, "base.esm", 0, {});

        EXPECT_EQ(total(census.getCounts(), "STAT", "REFR"), 1u);
    }

    TEST(ESM4ReferenceCensusTest, countsAReferenceToABaseObjectOfAMasterThatWasNotGivenAsUnknown)
    {
        // The base plugin is not read, so its records are not known; the patch only changes the master list index.
        ESM4::ReferenceCensus census;
        collect(census, patchPlugin(), "patch.esp", 1, { { "base.esm", 0 } });

        const Counts counts = census.getCounts();
        EXPECT_EQ(total(counts, ESM4::ReferenceCensus::unknownBase, "REFR"), 2u);
        EXPECT_EQ(census.getDeleted(), 0u);
    }

    TEST(ESM4ReferenceCensusTest, writesATableWithAColumnForEachTypeOfReference)
    {
        ESM4::ReferenceCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});

        std::ostringstream stream;
        census.write(stream);
        const std::string text = stream.str();

        EXPECT_THAT(text, HasSubstr("Base"));
        EXPECT_THAT(text, MatchesRegex("(.|\n)*STAT +0 +3 +3 +1\n(.|\n)*"));
        EXPECT_THAT(text, MatchesRegex("(.|\n)*NPC_ +1 +0 +1 +0\n(.|\n)*"));
        EXPECT_THAT(text, MatchesRegex("(.|\n)*TREE +0 +0 +1\n(.|\n)*"));
        EXPECT_THAT(text, HasSubstr("0x1009: 1\n"));
        EXPECT_THAT(text, HasSubstr("0 references of earlier files are deleted by later ones"));
    }

    TEST(ESM4ReferenceCensusTest, aReferenceThatCannotBeReadDoesNotStopTheFile)
    {
        // The NAME promises 4 bytes and the record has 2 of them.
        std::string overrunning("NAME");
        append<std::uint16_t>(overrunning, 4);
        overrunning += "ab";
        const std::string plugin = header()
            + topGroup("STAT",
                record("STAT", stat, "") + record("REFR", 0x2001, overrunning) + record("REFR", 0x2002, name(stat)));
        ESM4::ReferenceCensus census;
        collect(census, plugin, "base.esm", 0, {});

        // The reference that cannot be read is left out, it is not counted as one with no base object.
        const Counts counts = census.getCounts();
        EXPECT_EQ(total(counts, "STAT", "REFR"), 1u);
        EXPECT_EQ(counts.size(), 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4ReferenceCensusTest, aReferenceThatEndsInsideASubRecordHeaderCannotBeRead)
    {
        // A base object and then 3 bytes, which are not the 6 of a header.
        const std::string plugin = header()
            + topGroup("STAT",
                record("STAT", stat, "") + record("REFR", 0x2001, name(stat) + "abc")
                    + record("REFR", 0x2002, name(stat)));
        ESM4::ReferenceCensus census;
        collect(census, plugin, "base.esm", 0, {});

        const Counts counts = census.getCounts();
        EXPECT_EQ(total(counts, "STAT", "REFR"), 1u);
        EXPECT_EQ(counts.size(), 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4ReferenceCensusTest, anOverrideThatCannotBeReadLeavesTheReferenceOut)
    {
        // The patch overrides the reference 0x2001 of the base plugin, whose NAME promises 4 bytes and has 2
        std::string overrunning("NAME");
        append<std::uint16_t>(overrunning, 4);
        overrunning += "ab";
        const std::string patch
            = headerWithMaster("base.esm") + topGroup("REFR", record("REFR", 0x00002001, overrunning));
        ESM4::ReferenceCensus census;
        collect(census, basePlugin(), "base.esm", 0, {});
        ASSERT_EQ(total(census.getCounts(), "STAT", "REFR"), 3u);
        collect(census, patch, "patch.esp", 1, { { "base.esm", 0 } });

        EXPECT_EQ(total(census.getCounts(), "STAT", "REFR"), 2u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4ReferenceCensusTest, aReferenceWhoseBaseObjectIsNotAFormIdCannotBeRead)
    {
        // A NAME of 2 bytes and one of 8, which fit the record but are not a form ID
        std::string short_("NAME");
        append<std::uint16_t>(short_, 2);
        short_ += "ab";
        std::string long_("NAME");
        append<std::uint16_t>(long_, 8);
        long_ += "abcdefgh";
        const std::string plugin = header()
            + topGroup("STAT",
                record("STAT", stat, "") + record("REFR", 0x2001, short_) + record("REFR", 0x2002, long_)
                    + record("REFR", 0x2003, name(stat)));
        ESM4::ReferenceCensus census;
        collect(census, plugin, "base.esm", 0, {});

        const Counts counts = census.getCounts();
        EXPECT_EQ(total(counts, "STAT", "REFR"), 1u);
        EXPECT_EQ(counts.size(), 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4ReferenceCensusTest, countsPlacedHazardsAndTheOtherRecordsThatPlaceAnObject)
    {
        const std::string plugin = header()
            + topGroup("STAT",
                record("STAT", stat, zString("MODL", "meshes\\Stone.nif")) + record("PHZD", 0x2001, name(stat))
                    + record("PMIS", 0x2002, name(stat)) + record("PBEA", 0x2003, name(stat))
                    + record("PGRE", 0x2004, name(stat)) + record("ACRE", 0x2005, name(stat)));
        ESM4::ReferenceCensus census;
        collect(census, plugin, "base.esm", 0, {});

        const Counts counts = census.getCounts();
        for (const char* reference : { "PHZD", "PMIS", "PBEA", "PGRE", "ACRE" })
            EXPECT_EQ(total(counts, "STAT", reference), 1u) << reference;
        EXPECT_EQ(counts.size(), 1u);
    }
}
