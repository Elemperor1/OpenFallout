#include <components/esm4/loadrace.hpp>
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

    TEST(ESM4RaceTest, readsTheVoiceTypesOfTheMaleAndTheFemale)
    {
        std::string voices;
        append<std::uint32_t>(voices, 0x000A0001);
        append<std::uint32_t>(voices, 0x000A0002);

        const std::vector<ESM4::Race> races
            = loadRecords<ESM4::Race>("RACE", record("RACE", 1, zString("EDID", "Human") + subRecord("VTCK", voices)));

        ASSERT_EQ(races.size(), 1u);
        EXPECT_EQ(races[0].mDefaultVoice[0].toUint32(), 0x000A0001u);
        EXPECT_EQ(races[0].mDefaultVoice[1].toUint32(), 0x000A0002u);
    }

    TEST(ESM4RaceTest, doesNotReadVoiceTypesOfASizeThatIsNotTwoForms)
    {
        std::string voices;
        append<std::uint32_t>(voices, 0x000A0001); // one form, as a short record has
        std::string others;
        append<std::uint32_t>(others, 0x000A0003);
        append<std::uint32_t>(others, 0x000A0004);
        append<std::uint32_t>(others, 0x000A0005);

        for (const std::string& data : { voices, others })
        {
            const std::vector<ESM4::Race> races = loadRecords<ESM4::Race>("RACE",
                record("RACE", 1, zString("EDID", "Human") + subRecord("VTCK", data) + zString("FULL", "Human")));

            ASSERT_EQ(races.size(), 1u);
            EXPECT_TRUE(races[0].mDefaultVoice[0].isZeroOrUnset());
            EXPECT_TRUE(races[0].mDefaultVoice[1].isZeroOrUnset());
            EXPECT_EQ(races[0].mEditorId, "Human");
            EXPECT_EQ(races[0].mFullName, "Human");
        }
    }

    TEST(ESM4RaceTest, hasNoFlagsWhenTheRecordHasNoData)
    {
        const std::vector<ESM4::Race> races
            = loadRecords<ESM4::Race>("RACE", record("RACE", 1, zString("EDID", "Minimal")));

        ASSERT_EQ(races.size(), 1u);
        EXPECT_EQ(races[0].mRaceFlags, 0u);
    }

    TEST(ESM4RaceTest, hasNoVoiceTypesWhenTheRecordGivesNone)
    {
        const std::vector<ESM4::Race> races
            = loadRecords<ESM4::Race>("RACE", record("RACE", 1, zString("EDID", "Human")));

        ASSERT_EQ(races.size(), 1u);
        EXPECT_TRUE(races[0].mDefaultVoice[0].isZeroOrUnset());
        EXPECT_TRUE(races[0].mDefaultVoice[1].isZeroOrUnset());
    }
    // DATA of Oblivion, Fallout 3 and New Vegas: eight skills with their bonus, the heights and weights of the male and
    // the female and the flags
    std::string raceData(std::uint8_t firstSkill, float height)
    {
        std::string data;
        for (std::uint8_t i = 0; i < 8; ++i)
        {
            append<std::uint8_t>(data, firstSkill + i);
            append<std::uint8_t>(data, 5 + i);
        }
        append<float>(data, height);
        append<float>(data, height + 0.1f);
        append<float>(data, 90.f);
        append<float>(data, 80.f);
        append<std::uint32_t>(data, 0x21);
        return subRecord("DATA", data);
    }

    std::string indexedPart(std::uint32_t index, std::string_view mesh, std::string_view texture)
    {
        std::string result = valueSubRecord("INDX", index) + zString("MODL", mesh);
        if (!texture.empty())
            result += zString("ICON", texture);
        return result;
    }

    std::string floatsOf(std::string_view type, std::size_t count, float first)
    {
        std::string data;
        for (std::size_t i = 0; i < count; ++i)
            append<float>(data, first + static_cast<float>(i));
        return subRecord(type, data);
    }

    std::string formList(std::string_view type, const std::vector<std::uint32_t>& forms)
    {
        std::string data;
        for (const std::uint32_t form : forms)
            append(data, form);
        return subRecord(type, data);
    }

    std::string raceLoadFailure(float headerVersion, std::uint16_t formVersion, const std::string& data)
    {
        try
        {
            loadVersionedRecord<ESM4::Race>("RACE", headerVersion, formVersion, data);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4RaceTest, readsTheTextsAndTheSkillsOfARaceOfFallout)
    {
        const std::string data = zString("EDID", "Caucasian") + zString("FULL", "Caucasian")
            + zString("DESC", "A long description") + valueSubRecord<std::uint32_t>("SPLO", 0x00000811)
            + valueSubRecord<std::uint32_t>("SPLO", 0x00000812) + raceData(0x20, 1.05f);

        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 0.94f, 15, data, 0x13);

        EXPECT_EQ(race.mId.toUint32(), 0x13u);
        EXPECT_EQ(race.mEditorId, "Caucasian");
        EXPECT_EQ(race.mFullName, "Caucasian");
        EXPECT_EQ(race.mDesc, "A long description");
        ASSERT_EQ(race.mBonusSpells.size(), 2u);
        EXPECT_EQ(race.mBonusSpells[1].toUint32(), 0x812u);
        ASSERT_EQ(race.mSkillBonus.size(), 8u);
        EXPECT_EQ(race.mSkillBonus.at(static_cast<ESM4::Race::SkillIndex>(0x20)), 5);
        EXPECT_EQ(race.mSkillBonus.at(static_cast<ESM4::Race::SkillIndex>(0x27)), 12);
        EXPECT_EQ(race.mHeightMale, 1.05f);
        EXPECT_EQ(race.mHeightFemale, 1.05f + 0.1f);
        EXPECT_EQ(race.mWeightMale, 90.f);
        EXPECT_EQ(race.mWeightFemale, 80.f);
        EXPECT_EQ(race.mRaceFlags, 0x21u);
    }

    TEST(ESM4RaceTest, skipsADescriptionOfOneByteThatFallout3HasInPlaceOfOne)
    {
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>(
            "RACE", 0.94f, 15, subRecord("DESC", std::string(1, '\0')) + zString("FULL", "Name"));

        EXPECT_TRUE(race.mDesc.empty());
        EXPECT_EQ(race.mFullName, "Name");
    }

    TEST(ESM4RaceTest, readsTheHairAndTheVoicesThatTheRaceGivesItsCharacters)
    {
        std::string defaults;
        append<std::uint32_t>(defaults, 0x00000821);
        append<std::uint32_t>(defaults, 0x00000822);

        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 1.34f, 15,
            subRecord("DNAM", defaults) + subRecord("CNAM", std::string(1, '\x07')) + valueSubRecord<float>("PNAM", 5.f)
                + valueSubRecord<float>("UNAM", 3.f) + subRecord("ATTR", bytePattern(2, 1))
                + formList("HNAM", { 0x00000831, 0x00000832, 0x00000833 })
                + formList("ENAM", { 0x00000841, 0x00000842 }));

        EXPECT_EQ(race.mDefaultHair[0].toUint32(), 0x821u);
        EXPECT_EQ(race.mDefaultHair[1].toUint32(), 0x822u);
        EXPECT_EQ(race.mFaceGenMainClamp, 5.f);
        EXPECT_EQ(race.mFaceGenFaceClamp, 3.f);
        ASSERT_EQ(race.mHairChoices.size(), 3u);
        EXPECT_EQ(race.mHairChoices[2].toUint32(), 0x833u);
        ASSERT_EQ(race.mEyeChoices.size(), 2u);
        EXPECT_EQ(race.mEyeChoices[0].toUint32(), 0x841u);
        EXPECT_EQ(race.mAttribMale.strength, 0);
    }

    TEST(ESM4RaceTest, doesNotTakeAListOfHairThatIsNotWholeForms)
    {
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>(
            "RACE", 0.94f, 15, subRecord("HNAM", bytePattern(6, 1)) + zString("EDID", "Next"));

        EXPECT_TRUE(race.mHairChoices.empty());
        EXPECT_EQ(race.mEditorId, "Next");
    }

    TEST(ESM4RaceTest, readsTheMeshesAndTexturesOfTheHeadAndBodyOfTheMaleAndTheFemaleOfFallout)
    {
        const std::string head = subRecord("NAM0", "") + subRecord("MNAM", "")
            + indexedPart(0, "head_m.nif", "head_m.dds") + indexedPart(1, "mouth_m.nif", "")
            + subRecord("MODB", bytePattern(4, 0)) + subRecord("FNAM", "") + indexedPart(0, "head_f.nif", "head_f.dds")
            + indexedPart(7, "eye_f.nif", "eye_f.dds");
        const std::string body = subRecord("NAM1", "") + subRecord("MNAM", "")
            + indexedPart(0, "upper_m.nif", "upper_m.dds") + indexedPart(3, "feet_m.nif", "feet_m.dds")
            + subRecord("FNAM", "") + indexedPart(2, "hands_f.nif", "hands_f.dds");

        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 0.94f, 15, raceData(0, 1.f) + head + body);

        ASSERT_EQ(race.mHeadParts.size(), 8u);
        ASSERT_EQ(race.mHeadPartsFemale.size(), 8u);
        EXPECT_EQ(race.mHeadParts[0].mesh, "head_m.nif");
        EXPECT_EQ(race.mHeadParts[0].texture, "head_m.dds");
        EXPECT_EQ(race.mHeadParts[1].mesh, "mouth_m.nif");
        EXPECT_TRUE(race.mHeadParts[1].texture.empty());
        EXPECT_EQ(race.mHeadPartsFemale[0].mesh, "head_f.nif");
        EXPECT_EQ(race.mHeadPartsFemale[7].texture, "eye_f.dds");
        ASSERT_EQ(race.mBodyPartsMale.size(), 4u);
        ASSERT_EQ(race.mBodyPartsFemale.size(), 4u);
        EXPECT_EQ(race.mBodyPartsMale[0].mesh, "upper_m.nif");
        EXPECT_EQ(race.mBodyPartsMale[3].texture, "feet_m.dds");
        EXPECT_EQ(race.mBodyPartsFemale[2].mesh, "hands_f.nif");
        EXPECT_EQ(race.mBodyPartsFemale[2].texture, "hands_f.dds");
    }

    TEST(ESM4RaceTest, readsTheSamePartsOfARaceOfNewVegas)
    {
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 1.34f, 15,
            raceData(0, 1.f) + subRecord("NAM0", "") + subRecord("MNAM", "")
                + indexedPart(0, "head_m.nif", "head_m.dds") + subRecord("NAM1", "") + subRecord("FNAM", "")
                + indexedPart(1, "lower_f.nif", "lower_f.dds"));

        ASSERT_EQ(race.mHeadParts.size(), 8u);
        EXPECT_EQ(race.mHeadParts[0].mesh, "head_m.nif");
        ASSERT_EQ(race.mBodyPartsFemale.size(), 4u);
        EXPECT_EQ(race.mBodyPartsFemale[1].mesh, "lower_f.nif");
    }

    TEST(ESM4RaceTest, skipsAMeshThatNoIndexCameBefore)
    {
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 0.94f, 15,
            raceData(0, 1.f) + subRecord("NAM0", "") + zString("MODL", "orphan.nif") + zString("ICON", "orphan.dds"));

        ASSERT_EQ(race.mHeadParts.size(), 8u);
        EXPECT_TRUE(race.mHeadParts[0].mesh.empty());
        EXPECT_TRUE(race.mHeadParts[0].texture.empty());
    }

    TEST(ESM4RaceTest, doesNotTakeAPartThatIsNotOneOfTheEightHeadPartsOfFallout)
    {
        EXPECT_THAT(
            raceLoadFailure(0.94f, 15,
                raceData(0, 1.f) + subRecord("NAM0", "") + subRecord("MNAM", "") + indexedPart(8, "head.nif", "")),
            HasSubstr("head parts index is out of range: 8 >= 8"));
        EXPECT_THAT(
            raceLoadFailure(0.94f, 15,
                raceData(0, 1.f) + subRecord("NAM0", "") + subRecord("FNAM", "") + indexedPart(9, "head.nif", "")),
            HasSubstr("head parts female index is out of range: 9 >= 8"));
        EXPECT_THAT(
            raceLoadFailure(0.94f, 15,
                raceData(0, 1.f) + subRecord("NAM1", "") + subRecord("MNAM", "") + indexedPart(4, "body.nif", "")),
            HasSubstr("body parts male index is out of range: 4 >= 4"));
        EXPECT_THAT(raceLoadFailure(0.94f, 15,
                        raceData(0, 1.f) + subRecord("NAM1", "") + subRecord("FNAM", "") + valueSubRecord("INDX", 5u)
                            + zString("ICON", "body.dds")),
            HasSubstr("body parts female index is out of range: 5 >= 4"));
    }

    TEST(ESM4RaceTest, readsTheFaceGenCoefficientsOfTheMaleAndTheFemale)
    {
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 0.94f, 15,
            raceData(0, 1.f) + subRecord("MNAM", "") + floatsOf("FGGS", 50, 1.f) + floatsOf("FGGA", 30, 100.f)
                + floatsOf("FGTS", 50, 200.f) + subRecord("FNAM", "") + floatsOf("FGGS", 50, 300.f)
                + floatsOf("FGGA", 30, 400.f) + floatsOf("FGTS", 50, 500.f) + subRecord("SNAM", bytePattern(2, 1)));

        ASSERT_EQ(race.mSymShapeModeCoefficients.size(), 50u);
        EXPECT_EQ(race.mSymShapeModeCoefficients[0], 1.f);
        EXPECT_EQ(race.mSymShapeModeCoefficients[49], 50.f);
        ASSERT_EQ(race.mAsymShapeModeCoefficients.size(), 30u);
        EXPECT_EQ(race.mAsymShapeModeCoefficients[29], 129.f);
        ASSERT_EQ(race.mSymTextureModeCoefficients.size(), 50u);
        EXPECT_EQ(race.mSymTextureModeCoefficients[1], 201.f);
        ASSERT_EQ(race.mSymShapeModeCoeffFemale.size(), 50u);
        EXPECT_EQ(race.mSymShapeModeCoeffFemale[0], 300.f);
        ASSERT_EQ(race.mAsymShapeModeCoeffFemale.size(), 30u);
        EXPECT_EQ(race.mAsymShapeModeCoeffFemale[0], 400.f);
        ASSERT_EQ(race.mSymTextureModeCoeffFemale.size(), 50u);
        EXPECT_EQ(race.mSymTextureModeCoeffFemale[49], 549.f);
    }

    TEST(ESM4RaceTest, readsTheDispositionTowardsOtherRaces)
    {
        std::string disposition;
        append<std::uint32_t>(disposition, 0x00000851);
        append<std::int32_t>(disposition, -30);
        std::string other;
        append<std::uint32_t>(other, 0x00000852);
        append<std::int32_t>(other, 15);

        const ESM4::Race race = loadVersionedRecord<ESM4::Race>(
            "RACE", 0.94f, 15, subRecord("XNAM", disposition) + subRecord("XNAM", other));

        ASSERT_EQ(race.mDisposition.size(), 2u);
        EXPECT_EQ(race.mDisposition.at(ESM::FormId::fromUint32(0x851)), -30);
        EXPECT_EQ(race.mDisposition.at(ESM::FormId::fromUint32(0x852)), 15);
    }

    TEST(ESM4RaceTest, readsARaceOfOblivionWithItsAttributesAndTwoRacesItIsMixedFrom)
    {
        std::string attributes;
        for (std::uint8_t i = 0; i < 16; ++i)
            append<std::uint8_t>(attributes, 30 + i);
        std::string mixed;
        append<std::uint32_t>(mixed, 0x00000861);
        append<std::uint32_t>(mixed, 0x00000862);

        std::vector<ESM4::Race> races = loadRecords<ESM4::Race>("RACE",
            record("RACE", 1,
                raceData(0x0C, 1.f) + subRecord("ATTR", attributes) + subRecord("VNAM", mixed) + subRecord("NAM0", "")
                    + indexedPart(0, "head.nif", "head.dds") + indexedPart(3, "mouth.nif", "")
                    + indexedPart(8, "eye.nif", "") + subRecord("NAM1", "") + zString("MODL", "tail.nif")
                    + zString("ICON", "tail.dds") + subRecord("MNAM", "") + indexedPart(0, "body.nif", "body.dds")));

        ASSERT_EQ(races.size(), 1u);
        const ESM4::Race& race = races[0];
        EXPECT_EQ(race.mAttribMale.strength, 30);
        EXPECT_EQ(race.mAttribMale.luck, 37);
        EXPECT_EQ(race.mAttribFemale.strength, 38);
        EXPECT_EQ(race.mAttribFemale.luck, 45);
        EXPECT_EQ(race.mVNAM[0].toUint32(), 0x861u);
        EXPECT_EQ(race.mVNAM[1].toUint32(), 0x862u);
        // the head parts do not depend on the sex, there are nine, and the tail follows the start of the body parts
        // without an index
        ASSERT_EQ(race.mHeadParts.size(), 9u);
        EXPECT_EQ(race.mHeadParts[0].mesh, "head.nif");
        EXPECT_EQ(race.mHeadParts[3].mesh, "mouth.nif");
        EXPECT_EQ(race.mHeadParts[8].mesh, "eye.nif");
        ASSERT_EQ(race.mBodyPartsMale.size(), 5u);
        EXPECT_EQ(race.mBodyPartsMale[0].mesh, "body.nif");
        EXPECT_EQ(race.mBodyPartsMale[0].texture, "body.dds");
        ASSERT_EQ(race.mBodyPartsFemale.size(), 5u);
        EXPECT_EQ(race.mBodyPartsFemale[4].mesh, "tail.nif");
        EXPECT_EQ(race.mBodyPartsFemale[4].texture, "tail.dds");
    }

    TEST(ESM4RaceTest, readsTheFaceGenCoefficientsOfARaceOfOblivionWithoutASex)
    {
        const std::vector<ESM4::Race> races = loadRecords<ESM4::Race>("RACE",
            record("RACE", 1, floatsOf("FGGS", 50, 1.f) + floatsOf("FGGA", 30, 2.f) + floatsOf("FGTS", 50, 3.f)));

        ASSERT_EQ(races.size(), 1u);
        EXPECT_EQ(races[0].mSymShapeModeCoefficients.size(), 50u);
        EXPECT_EQ(races[0].mAsymShapeModeCoefficients.size(), 30u);
        EXPECT_EQ(races[0].mSymTextureModeCoefficients.size(), 50u);
        EXPECT_TRUE(races[0].mSymShapeModeCoeffFemale.empty());
    }

    TEST(ESM4RaceTest, readsARaceOfSkyrimWithItsBodyTemplateModelsAndKeywords)
    {
        std::string data;
        for (std::uint8_t i = 0; i < 7; ++i)
        {
            append<std::uint8_t>(data, 0x20 + i);
            append<std::uint8_t>(data, 10 + i);
        }
        append<std::uint16_t>(data, 0);
        append<float>(data, 1.1f);
        append<float>(data, 1.2f);
        append<float>(data, 1.3f);
        append<float>(data, 1.4f);
        append<std::uint32_t>(data, 0x07);
        data += bytePattern(128 - data.size(), 1);

        std::string body;
        append<std::uint32_t>(body, 0x00000004);
        std::string bodyWithType = body;
        append<std::uint32_t>(bodyWithType, 1);

        const std::string head = subRecord("NAM0", "") + subRecord("MNAM", "") + valueSubRecord("INDX", 1u)
            + valueSubRecord<std::uint32_t>("HEAD", 0x00000871) + subRecord("FNAM", "") + valueSubRecord("INDX", 6u)
            + valueSubRecord<std::uint32_t>("HEAD", 0x00000872) + subRecord("NAM1", "") + subRecord("NAM3", "")
            + zString("MODL", "model.hkx") + zString("ICON", "icon.dds");
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 0.94f, 43,
            subRecord("DATA", data) + subRecord("MNAM", "") + zString("ANAM", "male_skeleton.nif")
                + subRecord("FNAM", "") + zString("ANAM", "female_skeleton.nif")
                + valueSubRecord<std::uint32_t>("WNAM", 0x00000873) + subRecord("BOD2", bodyWithType)
                + valueSubRecord<std::uint32_t>("KSIZ", 2) + formList("KWDA", { 0x00000881, 0x00000882 })
                + zString("NAME", "Body") + head + subRecord("VNAM", body) + subRecord("VNAM", bytePattern(2, 1)));

        EXPECT_EQ(race.mSkillBonus.size(), 7u);
        EXPECT_EQ(race.mSkillBonus.at(static_cast<ESM4::Race::SkillIndex>(0x23)), 13);
        EXPECT_EQ(race.mHeightMale, 1.1f);
        EXPECT_EQ(race.mWeightFemale, 1.4f);
        EXPECT_EQ(race.mRaceFlags, 7u);
        EXPECT_EQ(race.mModelMale.getOriginal(), "male_skeleton.nif");
        EXPECT_EQ(race.mModelFemale.getOriginal(), "female_skeleton.nif");
        EXPECT_EQ(race.mSkin.toUint32(), 0x873u);
        EXPECT_EQ(race.mBodyTemplate.bodyPart, 4u);
        EXPECT_EQ(race.mBodyTemplate.type, 1u);
        EXPECT_EQ(race.mNumKeywords, 2u);
        ASSERT_EQ(race.mHeadPartIdsMale.size(), 5u);
        ASSERT_EQ(race.mHeadPartIdsFemale.size(), 7u);
        EXPECT_EQ(race.mHeadPartIdsMale[1].toUint32(), 0x871u);
        EXPECT_EQ(race.mHeadPartIdsFemale[6].toUint32(), 0x872u);
    }

    TEST(ESM4RaceTest, readsTheLongerDataOfSkyrimSpecialEditionAndTheBodyTemplateWithoutAType)
    {
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 1.7f, 44,
            subRecord("DATA", bytePattern(164, 1)) + valueSubRecord<std::uint32_t>("BOD2", 0x0000000C)
                + subRecord("BOD2", bytePattern(5, 1)) + subRecord("VNAM", bytePattern(2, 1)));

        EXPECT_EQ(race.mSkillBonus.size(), 7u);
        EXPECT_EQ(race.mBodyTemplate.bodyPart, 0xCu);
        EXPECT_EQ(race.mBodyTemplate.type, 0u);
    }

    TEST(ESM4RaceTest, skipsDataOfASizeNoGameHas)
    {
        const ESM4::Race race = loadVersionedRecord<ESM4::Race>(
            "RACE", 0.94f, 15, subRecord("DATA", bytePattern(10, 1)) + zString("EDID", "Next"));

        EXPECT_TRUE(race.mSkillBonus.empty());
        EXPECT_EQ(race.mEditorId, "Next");
    }

    TEST(ESM4RaceTest, readsTheBodyTemplateOfOblivionAndFallout)
    {
        std::string body;
        append<std::uint32_t>(body, 0x00000204);
        append<std::uint8_t>(body, 5);
        body += std::string("\x01\x02\x03", 3);
        append<std::uint32_t>(body, 2);

        const ESM4::Race race = loadVersionedRecord<ESM4::Race>("RACE", 0.94f, 15, subRecord("BODT", body));

        EXPECT_EQ(race.mBodyTemplate.bodyPart, 0x204u);
        EXPECT_EQ(race.mBodyTemplate.flags, 5);
        EXPECT_EQ(race.mBodyTemplate.unknown3, 3);
        EXPECT_EQ(race.mBodyTemplate.type, 2u);
    }

    TEST(ESM4RaceTest, skipsTheSubrecordsItDoesNotUse)
    {
        std::string skipped;
        for (const char* type : { "MODB", "MTNM", "ATKD", "ATKE", "GNAM", "NAM4", "NAM5", "LNAM", "QNAM", "HCLF",
                 "UNES", "TINC", "TIND", "TINI", "TINL", "TINP", "TINT", "TINV", "TIRS", "PHWT", "AHCF", "AHCM", "MPAI",
                 "MPAV", "DFTF", "DFTM", "FLMV", "FTSF", "FTSM", "MTYP", "NAM7", "NAM8", "PHTN", "RNAM", "RNMV", "RPRF",
                 "RPRM", "SNMV", "SPCT", "SPED", "SWMV", "WKMV", "SPMV", "ATKR", "CTDA", "CIS1", "CIS2", "MODT", "MODC",
                 "MODS", "MODF", "YNAM", "NAM2", "MODD", "ONAM", "APPR", "ATKS", "ATKT", "ATKW", "BMMP", "BSMB", "BSMP",
                 "BSMS", "FMRI", "FMRN", "HLTX", "MLSI", "MPGN", "MPGS", "MPPC", "MPPF", "MPPI", "MPPK", "MPPM", "MPPN",
                 "MPPT", "MSID", "MSM0", "MSM1", "NNAM", "NTOP", "PRPS", "PTOP", "QSTI", "RBPC", "SADD", "SAKD", "SAPT",
                 "SGNM", "SRAC", "SRAF", "STCP", "STKD", "TETI", "TTEB", "TTEC", "TTED", "TTEF", "TTET", "TTGE", "TTGP",
                 "UNWP", "WMAP", "ZNAM" })
            skipped += subRecord(type, bytePattern(4, 1));

        EXPECT_EQ(
            loadVersionedRecord<ESM4::Race>("RACE", 0.94f, 15, skipped + zString("EDID", "Last")).mEditorId, "Last");
    }

    TEST(ESM4RaceTest, doesNotReadASubrecordItDoesNotKnow)
    {
        EXPECT_THAT(raceLoadFailure(0.94f, 15, subRecord("XXYZ", "")), HasSubstr("Unknown subrecord XXYZ"));
    }
}
