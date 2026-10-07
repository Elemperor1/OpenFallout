#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
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
    }
}
