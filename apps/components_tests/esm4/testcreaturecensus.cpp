#include <components/esm4/common.hpp>
#include <components/esm4/creaturecensus.hpp>
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

    constexpr std::uint32_t radscorpion = 0x1001;
    constexpr std::uint32_t deathclaw = 0x1002;
    constexpr std::uint32_t robot = 0x1003;
    constexpr std::uint32_t variant = 0x1004;
    constexpr std::uint32_t noModel = 0x1005;
    constexpr std::uint32_t broken = 0x1006;
    constexpr std::uint32_t removed = 0x1007;

    constexpr std::uint16_t useModel = 0x0040;

    // DATA of Fallout 3 and New Vegas: 17 bytes that start with the type of creature
    std::string typeData(std::uint8_t type, std::size_t size = 17)
    {
        std::string data(size, '\0');
        data[0] = static_cast<char>(type);
        return subRecord("DATA", data);
    }

    // The base configuration of Fallout 3 and New Vegas, 24 bytes, with the flags of the template at the end
    std::string configuration(std::uint16_t templateFlags)
    {
        std::string data(22, '\0');
        append(data, templateFlags);
        return subRecord("ACBS", data);
    }

    // A list of zero-terminated names in one sub-record
    std::string names(std::string_view code, std::initializer_list<std::string_view> list)
    {
        std::string data;
        for (const std::string_view name : list)
        {
            data.append(name);
            data.push_back('\0');
        }
        return subRecord(code, data);
    }

    std::string basePlugin()
    {
        const std::string creatures
            // a skeleton, models of the body and animations, all in a folder
            = record("CREA", radscorpion,
                  zString("MODL", "Creatures\\RadScorpion\\Skeleton.nif") + typeData(2) + configuration(0)
                      + names("NIFZ", { "Creatures\\RadScorpion\\Body.nif", "Creatures\\RadScorpion\\Claws.nif" })
                      + names("KFFZ", { "Creatures\\RadScorpion\\Attack.kf" }) + zString("PNAM", "parts"))
            // a model that is not a skeleton, names with no folder
            + record("CREA", deathclaw,
                zString("MODL", "deathclaw.nif") + typeData(1) + configuration(0) + names("NIFZ", { "head.nif" }))
            // no files in the lists
            + record("CREA", robot, zString("MODL", "creatures\\robot\\skeleton.nif") + typeData(6) + configuration(0))
            // takes the model of its template
            + record("CREA", variant,
                typeData(1) + configuration(useModel) + valueSubRecord<std::uint32_t>("TPLT", radscorpion))
            // no model, and a DATA that is not the 17 bytes of Fallout
            + record("CREA", noModel, typeData(0, 4) + names("KFFZ", {}))
            // a sub-record that says it has more data than the record has
            + record("CREA", broken, std::string("DATA") + std::string("\x32\x00", 2) + std::string(8, '\0'))
            // deleted after it was listed
            + record("CREA", removed, zString("MODL", "gone.nif") + typeData(3))
            + record("CREA", removed, "", ESM4::Rec_Deleted);

        return header() + topGroup("CREA", creatures);
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

    void collect(ESM4::CreatureCensus& census, const std::string& plugin, const std::string& fileName = "base.esm",
        std::uint32_t modIndex = 0, const std::map<std::string, int>& loaded = {})
    {
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), fileName, nullptr, nullptr);
        reader.setModIndex(modIndex);
        reader.updateModIndices(loaded);
        census.collect(reader);
    }

    TEST(ESM4CreatureCensusTest, countsTheCreaturesByTypeTemplateAndBodyParts)
    {
        ESM4::CreatureCensus census;
        collect(census, basePlugin());
        const ESM4::CreatureCensus::Summary summary = census.summarize();

        // the broken record and the deleted one are left out
        EXPECT_EQ(summary.mCreatures, 5u);
        // the type is the first byte of a DATA of 17 bytes; the short one has none
        EXPECT_THAT(summary.mTypes, UnorderedElementsAre(Pair(2, 1u), Pair(1, 2u), Pair(6, 1u), Pair(-1, 1u)));
        EXPECT_EQ(summary.mWithTemplate, 1u);
        EXPECT_EQ(summary.mUseModelFromTemplate, 1u);
        EXPECT_EQ(summary.mWithBodyParts, 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4CreatureCensusTest, countsTheModelsAndTheFileLists)
    {
        ESM4::CreatureCensus census;
        collect(census, basePlugin());
        const ESM4::CreatureCensus::Summary summary = census.summarize();

        // a skeleton in two records, in any case; one other model; two records with no model
        EXPECT_EQ(summary.mSkeleton.mCount, 2u);
        EXPECT_EQ(summary.mOtherModel.mCount, 1u);
        EXPECT_THAT(summary.mOtherModel.mExamples, ElementsAre("deathclaw.nif"));
        EXPECT_EQ(summary.mNoModel.mCount, 2u);
        EXPECT_EQ(summary.mModelWithFolder.mCount, 2u);
        EXPECT_EQ(summary.mModelWithoutFolder.mCount, 1u);

        // two body files, one, and none in the other three; an empty list counts as none
        EXPECT_EQ(summary.mBodyFiles[2], 1u);
        EXPECT_EQ(summary.mBodyFiles[1], 1u);
        EXPECT_EQ(summary.mBodyFiles[0], 3u);
        EXPECT_EQ(summary.mBodyFileWithFolder.mCount, 2u);
        EXPECT_EQ(summary.mBodyFileWithoutFolder.mCount, 1u);

        EXPECT_EQ(summary.mAnimationFiles[1], 1u);
        EXPECT_EQ(summary.mAnimationFiles[0], 4u);
        EXPECT_EQ(summary.mAnimationFileWithFolder.mCount, 1u);
        EXPECT_THAT(summary.mAnimationFileWithFolder.mExamples, ElementsAre("Creatures\\RadScorpion\\Attack.kf"));
        EXPECT_EQ(summary.mAnimationFileWithoutFolder.mCount, 0u);
    }

    TEST(ESM4CreatureCensusTest, aLaterFileReplacesAndDeletesRecords)
    {
        ESM4::CreatureCensus census;
        collect(census, basePlugin());

        // the scorpion now has two animations and no body part data
        const std::string patch = headerWithMaster("base.esm")
            + topGroup("CREA",
                record("CREA", radscorpion,
                    zString("MODL", "creatures\\radscorpion\\skeleton.nif") + typeData(2) + configuration(0)
                        + names("KFFZ", { "a.kf", "b.kf" }))
                    + record("CREA", deathclaw, "", ESM4::Rec_Deleted));
        collect(census, patch, "patch.esp", 1, { { "base.esm", 0 } });
        const ESM4::CreatureCensus::Summary summary = census.summarize();

        // the deathclaw is deleted
        EXPECT_EQ(summary.mCreatures, 4u);
        EXPECT_EQ(summary.mWithBodyParts, 0u);
        EXPECT_EQ(summary.mAnimationFiles[2], 1u);
        EXPECT_EQ(summary.mOtherModel.mCount, 0u);
    }

    TEST(ESM4CreatureCensusTest, namesTheCreatureTypes)
    {
        EXPECT_EQ(ESM4::CreatureCensus::creatureTypeName(0), "Animal");
        EXPECT_EQ(ESM4::CreatureCensus::creatureTypeName(6), "Robot");
        EXPECT_EQ(ESM4::CreatureCensus::creatureTypeName(-1), "none");
        EXPECT_EQ(ESM4::CreatureCensus::creatureTypeName(40), "type 40");
    }

    TEST(ESM4CreatureCensusTest, writesTheCounts)
    {
        ESM4::CreatureCensus census;
        collect(census, basePlugin());
        std::ostringstream out;
        census.write(out);

        EXPECT_THAT(out.str(), HasSubstr("Creatures (CREA records): 5\n"));
        EXPECT_THAT(out.str(), HasSubstr("MutatedInsect"));
        EXPECT_THAT(out.str(), HasSubstr("  a file called skeleton.nif: 2   e.g. "));
        EXPECT_THAT(out.str(), HasSubstr("deathclaw.nif"));
        EXPECT_THAT(out.str(), Not(HasSubstr("Reading stopped early")));
    }
}
