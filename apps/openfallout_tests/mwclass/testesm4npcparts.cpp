#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadhair.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadrace.hpp>

#include "apps/openfallout/mwclass/esm4npc.hpp"

namespace OFClass
{
    namespace
    {
        using Models = std::vector<std::string>;

        // A race with the parts that Fallout 3 and New Vegas give it: the body (upper body, left and right hand, and
        // the texture of the upper body, which has no model) and the head (head, ears, mouth, upper and lower teeth,
        // tongue, left and right eye), male and female
        ESM4::Race humanRace()
        {
            ESM4::Race race{};
            race.mBodyPartsMale = { { "male\\upperbody.nif", "" }, { "male\\lefthand.nif", "" },
                { "male\\righthand.nif", "" }, { "", "male\\skin.dds" } };
            race.mBodyPartsFemale = { { "female\\upperbody.nif", "" }, { "female\\lefthand.nif", "" },
                { "female\\righthand.nif", "" }, { "", "female\\skin.dds" } };
            race.mHeadParts = { { "male\\head.nif", "" }, { "", "" }, { "male\\mouth.nif", "" }, { "", "" }, { "", "" },
                { "", "" }, { "eyes\\left.nif", "" }, { "eyes\\right.nif", "" } };
            race.mHeadPartsFemale = { { "female\\head.nif", "" }, { "", "" }, { "female\\mouth.nif", "" }, { "", "" },
                { "", "" }, { "", "" }, { "eyes\\left.nif", "" }, { "eyes\\right.nif", "" } };
            return race;
        }

        ESM4::Hair hairOf(const char* model)
        {
            ESM4::Hair hair;
            hair.mModel = model;
            return hair;
        }

        ESM4::HeadPart headPartOf(const char* model)
        {
            ESM4::HeadPart part;
            part.mModel = model;
            return part;
        }

        ESM4::Armor armorOf(std::uint32_t slots, const char* maleModel, const char* femaleModel = "")
        {
            ESM4::Armor armor;
            armor.mArmorFlags = slots;
            armor.mModelMale = maleModel;
            armor.mModelFemale = femaleModel;
            return armor;
        }

        TEST(OFClassNpcPartsTest, aCharacterWithNothingOnHasTheBodyAndTheHeadOfItsRace)
        {
            const ESM4::Race race = humanRace();
            EXPECT_EQ(falloutNpcModels(race, false, nullptr, {}, {}),
                (Models{ "male\\upperbody.nif", "male\\lefthand.nif", "male\\righthand.nif", "male\\head.nif",
                    "male\\mouth.nif", "eyes\\left.nif", "eyes\\right.nif" }));
        }

        TEST(OFClassNpcPartsTest, aWomanHasThePartsOfTheFemaleRace)
        {
            const ESM4::Race race = humanRace();
            EXPECT_EQ(falloutNpcModels(race, true, nullptr, {}, {}),
                (Models{ "female\\upperbody.nif", "female\\lefthand.nif", "female\\righthand.nif", "female\\head.nif",
                    "female\\mouth.nif", "eyes\\left.nif", "eyes\\right.nif" }));
        }

        TEST(OFClassNpcPartsTest, theHairAndTheHeadPartsOfTheCharacterComeAfterTheHeadOfTheRace)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Hair hair = hairOf("hair\\bob.nif");
            const ESM4::HeadPart beard = headPartOf("hair\\beard.nif");
            const Models models = falloutNpcModels(race, false, &hair, { &beard }, {});
            ASSERT_EQ(models.size(), 9u);
            EXPECT_EQ(models[7], "hair\\beard.nif");
            EXPECT_EQ(models[8], "hair\\bob.nif");
        }

        TEST(OFClassNpcPartsTest, aPartThatHasNoModelIsLeftOut)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Hair bald = hairOf("");
            const ESM4::HeadPart nothing = headPartOf("");
            EXPECT_EQ(falloutNpcModels(race, false, &bald, { nullptr, &nothing }, {}),
                falloutNpcModels(race, false, nullptr, {}, {}));
        }

        TEST(OFClassNpcPartsTest, aSuitTakesThePlaceOfTheUpperBodyOfTheRace)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Armor suit = armorOf(ESM4::Armor::FO3_UpperBody, "suit\\jumpsuit.nif");
            const Models models = falloutNpcModels(race, false, nullptr, {}, { &suit });
            EXPECT_EQ(models,
                (Models{ "male\\lefthand.nif", "male\\righthand.nif", "male\\head.nif", "male\\mouth.nif",
                    "eyes\\left.nif", "eyes\\right.nif", "suit\\jumpsuit.nif" }));
        }

        TEST(OFClassNpcPartsTest, aGloveTakesThePlaceOfTheHandItCoversOnly)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Armor glove = armorOf(ESM4::Armor::FO3_RightHand, "suit\\glove.nif");
            const Models models = falloutNpcModels(race, false, nullptr, {}, { &glove });
            EXPECT_EQ(std::count(models.begin(), models.end(), "male\\righthand.nif"), 0);
            EXPECT_EQ(std::count(models.begin(), models.end(), "male\\lefthand.nif"), 1);
            EXPECT_EQ(std::count(models.begin(), models.end(), "male\\upperbody.nif"), 1);
            EXPECT_EQ(models.back(), "suit\\glove.nif");
        }

        TEST(OFClassNpcPartsTest, aWomanWearsTheFemaleModelOfAPieceAndTheMaleOneWhenItHasNoOtherModel)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Armor dress = armorOf(ESM4::Armor::FO3_UpperBody, "suit\\male.nif", "suit\\female.nif");
            const ESM4::Armor hat = armorOf(ESM4::Armor::FO3_Hat, "suit\\hat.nif");
            EXPECT_EQ(falloutNpcModels(race, true, nullptr, {}, { &dress, &hat }).back(), "suit\\hat.nif");
            EXPECT_EQ(falloutNpcModels(race, true, nullptr, {}, { &dress }).back(), "suit\\female.nif");
            EXPECT_EQ(falloutNpcModels(race, false, nullptr, {}, { &dress }).back(), "suit\\male.nif");
        }

        TEST(OFClassNpcPartsTest, aPieceThatCoversWhatAnotherCoversIsNotWorn)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Armor first = armorOf(ESM4::Armor::FO3_UpperBody, "suit\\first.nif");
            const ESM4::Armor second
                = armorOf(ESM4::Armor::FO3_UpperBody | ESM4::Armor::FO3_LeftHand, "suit\\second.nif");
            const ESM4::Armor hat = armorOf(ESM4::Armor::FO3_Hat, "suit\\hat.nif");
            const Models models = falloutNpcModels(race, false, nullptr, {}, { &first, &second, &hat });
            EXPECT_EQ(std::count(models.begin(), models.end(), "suit\\first.nif"), 1);
            EXPECT_EQ(std::count(models.begin(), models.end(), "suit\\second.nif"), 0);
            EXPECT_EQ(std::count(models.begin(), models.end(), "suit\\hat.nif"), 1);
            // the second was not worn, so the hand that it would have covered is the one of the race
            EXPECT_EQ(std::count(models.begin(), models.end(), "male\\lefthand.nif"), 1);
        }

        TEST(OFClassNpcPartsTest, aPieceThatCoversNothingOrHasNoModelIsNotWornAndBlocksNothing)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Armor noSlots = armorOf(0, "suit\\nothing.nif");
            const ESM4::Armor noModel = armorOf(ESM4::Armor::FO3_UpperBody, "");
            const ESM4::Armor suit = armorOf(ESM4::Armor::FO3_UpperBody, "suit\\jumpsuit.nif");
            const Models models = falloutNpcModels(race, false, nullptr, {}, { nullptr, &noSlots, &noModel, &suit });
            EXPECT_EQ(models.back(), "suit\\jumpsuit.nif");
            EXPECT_EQ(std::count(models.begin(), models.end(), "suit\\nothing.nif"), 0);
            EXPECT_EQ(std::count(models.begin(), models.end(), "male\\upperbody.nif"), 0);
        }

        TEST(OFClassNpcPartsTest, aHelmetThatCoversTheHeadTakesTheFaceAndItsPartsAway)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Hair hair = hairOf("hair\\bob.nif");
            const ESM4::HeadPart beard = headPartOf("hair\\beard.nif");
            const ESM4::Armor helmet = armorOf(ESM4::Armor::FO3_Head, "suit\\helmet.nif");
            // the hair is the hair of a slot of its own: the helmet does not hide it unless it covers it too
            EXPECT_EQ(falloutNpcModels(race, false, &hair, { &beard }, { &helmet }),
                (Models{ "male\\upperbody.nif", "male\\lefthand.nif", "male\\righthand.nif", "hair\\bob.nif",
                    "suit\\helmet.nif" }));
        }

        TEST(OFClassNpcPartsTest, aHatThatCoversTheHairTakesTheHairAway)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Hair hair = hairOf("hair\\bob.nif");
            const ESM4::Armor hat = armorOf(ESM4::Armor::FO3_Hat | ESM4::Armor::FO3_Hair, "suit\\hat.nif");
            const Models models = falloutNpcModels(race, false, &hair, {}, { &hat });
            EXPECT_EQ(std::count(models.begin(), models.end(), "hair\\bob.nif"), 0);
            EXPECT_EQ(std::count(models.begin(), models.end(), "male\\head.nif"), 1);
            EXPECT_EQ(models.back(), "suit\\hat.nif");
        }

        TEST(OFClassNpcPartsTest, aRaceWithNoPartsGivesOnlyWhatTheCharacterWears)
        {
            const ESM4::Race race{};
            const ESM4::Armor suit = armorOf(ESM4::Armor::FO3_UpperBody, "suit\\jumpsuit.nif");
            EXPECT_TRUE(falloutNpcModels(race, false, nullptr, {}, {}).empty());
            EXPECT_EQ(falloutNpcModels(race, true, nullptr, {}, { &suit }), (Models{ "suit\\jumpsuit.nif" }));
        }

        TEST(OFClassNpcPartsTest, aPictureOfARecordIsAPathUnderTextures)
        {
            EXPECT_EQ(falloutTexturePath("Characters\\Male\\HeadHuman.dds"), "textures/Characters/Male/HeadHuman.dds");
            EXPECT_EQ(falloutTexturePath("Characters/Hair/Bob.dds"), "textures/Characters/Hair/Bob.dds");
            EXPECT_EQ(falloutTexturePath("textures\\characters\\bob.dds"), "textures/characters/bob.dds");
            EXPECT_EQ(falloutTexturePath(""), "");
        }

        TEST(OFClassNpcPartsTest, thePartsKnowWhatTheyAreAndThePictureThatTheRecordsGiveThem)
        {
            ESM4::Race race = humanRace();
            race.mBodyPartsMale[0].texture = "Characters\\Male\\UpperBodyMale.dds";
            race.mBodyPartsMale[1].texture = "Characters\\Male\\HandMale.dds";
            race.mHeadParts[0].texture = "Characters\\Male\\HeadHumanOld.dds";
            race.mHeadParts[2].texture = "Characters\\Mouth\\MouthHuman.dds";
            ESM4::Hair hair = hairOf("hair\\bob.nif");
            hair.mIcon = "Characters\\Hair\\Bob.dds";
            const ESM4::HeadPart beard = headPartOf("hair\\beard.nif");
            const ESM4::Armor suit = armorOf(ESM4::Armor::FO3_UpperBody, "suit\\jumpsuit.nif");

            const FalloutNpcParts parts = falloutNpcParts(race, false, &hair, { &beard }, { &suit });
            ASSERT_EQ(parts.mParts.size(), 9u);
            const auto expect = [&](std::size_t i, const char* model, const char* texture, FalloutPartKind kind) {
                EXPECT_EQ(parts.mParts[i].mModel, model) << i;
                EXPECT_EQ(parts.mParts[i].mTexture, texture) << i;
                EXPECT_EQ(parts.mParts[i].mKind, kind) << i;
            };
            // the suit takes the place of the upper body of the race
            expect(0, "male\\lefthand.nif", "textures/Characters/Male/HandMale.dds", FalloutPartKind::Hand);
            expect(1, "male\\righthand.nif", "", FalloutPartKind::Hand);
            expect(2, "male\\head.nif", "textures/Characters/Male/HeadHumanOld.dds", FalloutPartKind::Head);
            expect(3, "male\\mouth.nif", "textures/Characters/Mouth/MouthHuman.dds", FalloutPartKind::Face);
            expect(4, "eyes\\left.nif", "", FalloutPartKind::Eye);
            expect(5, "eyes\\right.nif", "", FalloutPartKind::Eye);
            expect(6, "hair\\beard.nif", "", FalloutPartKind::HeadPart);
            expect(7, "hair\\bob.nif", "textures/Characters/Hair/Bob.dds", FalloutPartKind::Hair);
            expect(8, "suit\\jumpsuit.nif", "", FalloutPartKind::Worn);
        }

        TEST(OFClassNpcPartsTest, theUpperBodyOfTheRaceIsTheBodyAndTheTextureOfTheBodyAloneIsNotAPart)
        {
            const ESM4::Race race = humanRace();
            const FalloutNpcParts parts = falloutNpcParts(race, false, nullptr, {}, {});
            ASSERT_FALSE(parts.mParts.empty());
            EXPECT_EQ(parts.mParts.front().mKind, FalloutPartKind::Body);
            EXPECT_EQ(std::count_if(parts.mParts.begin(), parts.mParts.end(),
                          [](const FalloutNpcPart& part) { return part.mModel.empty(); }),
                0);
        }

        TEST(OFClassNpcPartsTest, aHatOrAHeadbandPutsTheHairUnderIt)
        {
            const ESM4::Race race = humanRace();
            const ESM4::Hair hair = hairOf("hair\\bob.nif");
            const ESM4::Armor cap = armorOf(ESM4::Armor::FO3_Hat | ESM4::Armor::FO3_Headband, "suit\\cap.nif");
            const ESM4::Armor band = armorOf(ESM4::Armor::FO3_Headband, "suit\\band.nif");
            const ESM4::Armor glasses = armorOf(ESM4::Armor::FO3_EyeGlasses, "suit\\glasses.nif");
            const ESM4::Armor helmet = armorOf(ESM4::Armor::FO3_Hat | ESM4::Armor::FO3_Hair, "suit\\helmet.nif");

            EXPECT_FALSE(falloutNpcParts(race, false, &hair, {}, {}).mHatOn);
            EXPECT_FALSE(falloutNpcParts(race, false, &hair, {}, { &glasses }).mHatOn);
            // a cap that leaves the hair shows the hair that fits under it
            const FalloutNpcParts underCap = falloutNpcParts(race, false, &hair, {}, { &cap });
            EXPECT_TRUE(underCap.mHatOn);
            EXPECT_EQ(std::count_if(underCap.mParts.begin(), underCap.mParts.end(),
                          [](const FalloutNpcPart& part) { return part.mKind == FalloutPartKind::Hair; }),
                1);
            EXPECT_TRUE(falloutNpcParts(race, false, &hair, {}, { &band }).mHatOn);
            // a helmet that covers the hair takes it away
            const FalloutNpcParts underHelmet = falloutNpcParts(race, false, &hair, {}, { &helmet });
            EXPECT_TRUE(underHelmet.mHatOn);
            EXPECT_EQ(std::count_if(underHelmet.mParts.begin(), underHelmet.mParts.end(),
                          [](const FalloutNpcPart& part) { return part.mKind == FalloutPartKind::Hair; }),
                0);
        }

        // The head parts of the test: the records by form ID, the way the store has them
        struct HeadPartStore
        {
            std::map<ESM::FormId, ESM4::HeadPart> mParts;
            std::vector<ESM::FormId> mAsked;

            ESM4::HeadPart& add(std::uint32_t id, const char* model, std::vector<std::uint32_t> extra = {})
            {
                ESM4::HeadPart& part = mParts[ESM::FormId::fromUint32(id)];
                part.mModel = model;
                for (const std::uint32_t extraId : extra)
                    part.mExtraParts.push_back(ESM::FormId::fromUint32(extraId));
                return part;
            }

            std::function<const ESM4::HeadPart*(ESM::FormId)> finder()
            {
                return [this](ESM::FormId id) -> const ESM4::HeadPart* {
                    mAsked.push_back(id);
                    const auto found = mParts.find(id);
                    return found == mParts.end() ? nullptr : &found->second;
                };
            }
        };

        std::vector<std::string> modelsOf(const std::vector<const ESM4::HeadPart*>& parts)
        {
            std::vector<std::string> models;
            for (const ESM4::HeadPart* part : parts)
                models.push_back(part->mModel.getOriginal());
            return models;
        }

        TEST(OFClassNpcPartsTest, aHeadPartThatNamesOthersIsFollowedByThemInOrder)
        {
            HeadPartStore store;
            store.add(0x801, "", { 0x802, 0x803 });
            store.add(0x802, "beard\\left.nif");
            store.add(0x803, "beard\\right.nif", { 0x804 });
            store.add(0x804, "beard\\chin.nif");
            store.add(0x805, "brow.nif");
            const auto parts
                = expandHeadParts({ ESM::FormId::fromUint32(0x801), ESM::FormId::fromUint32(0x805) }, store.finder());
            EXPECT_EQ(
                modelsOf(parts), (Models{ "", "beard\\left.nif", "beard\\right.nif", "beard\\chin.nif", "brow.nif" }));

            // the parent has no model, but the parts it names are worn
            const ESM4::Race race{};
            EXPECT_EQ(falloutNpcModels(race, false, nullptr, parts, {}),
                (Models{ "beard\\left.nif", "beard\\right.nif", "beard\\chin.nif", "brow.nif" }));
        }

        TEST(OFClassNpcPartsTest, headPartsThatNameEachOtherOrThemselvesAreListedOnce)
        {
            HeadPartStore store;
            store.add(0x801, "a.nif", { 0x802, 0x801 });
            store.add(0x802, "b.nif", { 0x801 });
            const auto parts
                = expandHeadParts({ ESM::FormId::fromUint32(0x801), ESM::FormId::fromUint32(0x802) }, store.finder());
            EXPECT_EQ(modelsOf(parts), (Models{ "a.nif", "b.nif" }));
        }

        TEST(OFClassNpcPartsTest, aHeadPartThatIsNotThereOrIsNullLeavesOutItsOwnExtraPartsOnly)
        {
            HeadPartStore store;
            store.add(0x802, "b.nif");
            const auto parts = expandHeadParts(
                { ESM::FormId::fromUint32(0x801), ESM::FormId(), ESM::FormId::fromUint32(0x802) }, store.finder());
            EXPECT_EQ(modelsOf(parts), (Models{ "b.nif" }));
            // the unset form ID is not looked up
            EXPECT_EQ(store.mAsked.size(), 2u);
        }
    }
}
