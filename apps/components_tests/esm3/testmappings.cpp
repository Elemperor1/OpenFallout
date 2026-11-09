#include <components/esm3/mappings.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <set>
#include <stdexcept>

// How the parts that the armor and the clothing of Morrowind are made of find their bones and their body parts.
namespace
{
    using namespace testing;
    using namespace ESM;

    TEST(Esm3MappingsTest, everyPartOfAnArmorNamesItsBone)
    {
        for (int i = 0; i < PRT_Count; ++i)
            EXPECT_FALSE(getBoneName(static_cast<PartReferenceType>(i)).empty()) << i;
    }

    TEST(Esm3MappingsTest, theBonesComeInPairsLeftAndRight)
    {
        EXPECT_EQ(getBoneName(PRT_RHand), "right hand");
        EXPECT_EQ(getBoneName(PRT_LHand), "left hand");
        EXPECT_EQ(getBoneName(PRT_RWrist), "right wrist");
        EXPECT_EQ(getBoneName(PRT_LWrist), "left wrist");
        EXPECT_EQ(getBoneName(PRT_RForearm), "right forearm");
        EXPECT_EQ(getBoneName(PRT_LForearm), "left forearm");
        EXPECT_EQ(getBoneName(PRT_RUpperarm), "right upper arm");
        EXPECT_EQ(getBoneName(PRT_LUpperarm), "left upper arm");
        EXPECT_EQ(getBoneName(PRT_RFoot), "right foot");
        EXPECT_EQ(getBoneName(PRT_LFoot), "left foot");
        EXPECT_EQ(getBoneName(PRT_RAnkle), "right ankle");
        EXPECT_EQ(getBoneName(PRT_LAnkle), "left ankle");
        EXPECT_EQ(getBoneName(PRT_RKnee), "right knee");
        EXPECT_EQ(getBoneName(PRT_LKnee), "left knee");
        EXPECT_EQ(getBoneName(PRT_RLeg), "right upper leg");
        EXPECT_EQ(getBoneName(PRT_LLeg), "left upper leg");
        EXPECT_EQ(getBoneName(PRT_RPauldron), "right clavicle");
        EXPECT_EQ(getBoneName(PRT_LPauldron), "left clavicle");
    }

    TEST(Esm3MappingsTest, someBonesAreSharedAndSomeAreSpecial)
    {
        EXPECT_EQ(getBoneName(PRT_Head), "head");
        EXPECT_EQ(getBoneName(PRT_Hair), "head");
        EXPECT_EQ(getBoneName(PRT_Neck), "neck");
        EXPECT_EQ(getBoneName(PRT_Cuirass), "chest");
        EXPECT_EQ(getBoneName(PRT_Groin), "groin");
        EXPECT_EQ(getBoneName(PRT_Skirt), "groin");
        EXPECT_EQ(getBoneName(PRT_Shield), "shield bone");
        EXPECT_EQ(getBoneName(PRT_Weapon), "weapon bone");
        EXPECT_EQ(getBoneName(PRT_Tail), "tail");
    }

    TEST(Esm3MappingsTest, aPartThatDoesNotExistHasNoBone)
    {
        EXPECT_THAT([] { getBoneName(PRT_Count); }, Throws<std::runtime_error>());
        EXPECT_THAT([] { getMeshFilter(static_cast<PartReferenceType>(99)); }, Throws<std::runtime_error>());
    }

    TEST(Esm3MappingsTest, theMeshFilterIsTheBoneExceptForTheHair)
    {
        EXPECT_EQ(getMeshFilter(PRT_Hair), "hair");
        EXPECT_EQ(getMeshFilter(PRT_Head), "head");
        EXPECT_EQ(getMeshFilter(PRT_LKnee), "left knee");
        EXPECT_EQ(getMeshFilter(PRT_Weapon), "weapon bone");
    }

    TEST(Esm3MappingsTest, leftAndRightPartsUseTheSameBodyPart)
    {
        EXPECT_EQ(getMeshPart(PRT_Head), BodyPart::MP_Head);
        EXPECT_EQ(getMeshPart(PRT_Hair), BodyPart::MP_Hair);
        EXPECT_EQ(getMeshPart(PRT_Neck), BodyPart::MP_Neck);
        EXPECT_EQ(getMeshPart(PRT_Cuirass), BodyPart::MP_Chest);
        EXPECT_EQ(getMeshPart(PRT_Groin), BodyPart::MP_Groin);
        EXPECT_EQ(getMeshPart(PRT_Tail), BodyPart::MP_Tail);

        const std::pair<PartReferenceType, PartReferenceType> pairs[] = {
            { PRT_RHand, PRT_LHand },
            { PRT_RWrist, PRT_LWrist },
            { PRT_RForearm, PRT_LForearm },
            { PRT_RUpperarm, PRT_LUpperarm },
            { PRT_RFoot, PRT_LFoot },
            { PRT_RAnkle, PRT_LAnkle },
            { PRT_RKnee, PRT_LKnee },
            { PRT_RLeg, PRT_LLeg },
        };
        for (const auto& [right, left] : pairs)
            EXPECT_EQ(getMeshPart(right), getMeshPart(left)) << getBoneName(right);

        EXPECT_EQ(getMeshPart(PRT_RHand), BodyPart::MP_Hand);
        EXPECT_EQ(getMeshPart(PRT_RWrist), BodyPart::MP_Wrist);
        EXPECT_EQ(getMeshPart(PRT_RForearm), BodyPart::MP_Forearm);
        EXPECT_EQ(getMeshPart(PRT_RUpperarm), BodyPart::MP_Upperarm);
        EXPECT_EQ(getMeshPart(PRT_RFoot), BodyPart::MP_Foot);
        EXPECT_EQ(getMeshPart(PRT_RAnkle), BodyPart::MP_Ankle);
        EXPECT_EQ(getMeshPart(PRT_RKnee), BodyPart::MP_Knee);
        EXPECT_EQ(getMeshPart(PRT_RLeg), BodyPart::MP_Upperleg);
    }

    TEST(Esm3MappingsTest, partsThatAreWornOnTheOutsideHaveNoBodyPart)
    {
        for (const PartReferenceType type : { PRT_Skirt, PRT_Shield, PRT_RPauldron, PRT_LPauldron, PRT_Weapon })
            EXPECT_THAT([type] { getMeshPart(type); },
                ThrowsMessage<std::runtime_error>(HasSubstr("not associated with a mesh part")))
                << type;
    }
}
