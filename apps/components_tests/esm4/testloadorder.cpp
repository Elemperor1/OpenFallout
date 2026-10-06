#include <components/esm4/loadcdck.hpp>
#include <components/esm4/loadchal.hpp>
#include <components/esm4/loaddehy.hpp>
#include <components/esm4/loadhung.hpp>
#include <components/esm4/loadmesg.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadrads.hpp>
#include <components/esm4/loadslpd.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

// The form IDs that the loaders read are adjusted to the load order of their plugin, and a null one stays null. The
// plugins here have no masters and the load order index 3.
namespace
{
    using namespace testing;
    using namespace ESM4Test;

    constexpr std::uint32_t sLoadOrderIndex = 3;

    template <class T>
    std::string objectBytes(const T& value)
    {
        return std::string(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    template <class T>
    std::vector<T> loadAsThirdPlugin(const char* type, const std::string& data)
    {
        return loadRecords<T>(type, record(type, 1, data), 0, nullptr, sLoadOrderIndex);
    }

    ESM::FormId adjusted(std::uint32_t id)
    {
        return ESM::FormId{ id, static_cast<std::int32_t>(sLoadOrderIndex) };
    }

    TEST(ESM4LoadOrderTest, adjustsTheFormIdsOfAMagicEffect)
    {
        ESM4::MagicEffect::Data data;
        data.mAssociatedItem = 0x101;
        data.mLight = 0x102;
        data.mEffectShader = 0x103;
        data.mObjectDisplayShader = 0x104;
        data.mEffectSound = 0x105;
        data.mBoltSound = 0x106;
        data.mHitSound = 0x107;
        data.mAreaSound = 0; // null
        data.mResistValue = 0x108; // an actor value, not a form ID
        data.mActorValue = 0x109;

        const std::vector<ESM4::MagicEffect> records
            = loadAsThirdPlugin<ESM4::MagicEffect>("MGEF", subRecord("DATA", objectBytes(data)));

        ASSERT_EQ(records.size(), 1u);
        const ESM4::MagicEffect::Data& result = records[0].mData;
        EXPECT_EQ(result.mAssociatedItem, 0x03000101u);
        EXPECT_EQ(result.mLight, 0x03000102u);
        EXPECT_EQ(result.mEffectShader, 0x03000103u);
        EXPECT_EQ(result.mObjectDisplayShader, 0x03000104u);
        EXPECT_EQ(result.mEffectSound, 0x03000105u);
        EXPECT_EQ(result.mBoltSound, 0x03000106u);
        EXPECT_EQ(result.mHitSound, 0x03000107u);
        EXPECT_EQ(result.mAreaSound, 0u);
        EXPECT_EQ(result.mResistValue, 0x108);
        EXPECT_EQ(result.mActorValue, 0x109);
    }

    TEST(ESM4LoadOrderTest, adjustsTheActorEffectOfTheStagesOfTheSurvivalNeeds)
    {
        ESM4::RadiationStage::Data radiation;
        radiation.mTriggerThreshold = 5;
        radiation.mActorEffect = 0x201;
        ESM4::DehydrationStage::Data dehydration;
        dehydration.mActorEffect = 0x202;
        ESM4::HungerStage::Data hunger;
        hunger.mActorEffect = 0x203;
        ESM4::SleepDeprivationStage::Data sleep;
        sleep.mActorEffect = 0;

        const auto rads = loadAsThirdPlugin<ESM4::RadiationStage>("RADS", subRecord("DATA", objectBytes(radiation)));
        const auto dehy
            = loadAsThirdPlugin<ESM4::DehydrationStage>("DEHY", subRecord("DATA", objectBytes(dehydration)));
        const auto hung = loadAsThirdPlugin<ESM4::HungerStage>("HUNG", subRecord("DATA", objectBytes(hunger)));
        const auto slpd = loadAsThirdPlugin<ESM4::SleepDeprivationStage>("SLPD", subRecord("DATA", objectBytes(sleep)));

        ASSERT_EQ(rads.size(), 1u);
        ASSERT_EQ(dehy.size(), 1u);
        ASSERT_EQ(hung.size(), 1u);
        ASSERT_EQ(slpd.size(), 1u);
        EXPECT_EQ(rads[0].mData.mTriggerThreshold, 5u);
        EXPECT_EQ(rads[0].mData.mActorEffect, 0x03000201u);
        EXPECT_EQ(dehy[0].mData.mActorEffect, 0x03000202u);
        EXPECT_EQ(hung[0].mData.mActorEffect, 0x03000203u);
        EXPECT_EQ(slpd[0].mData.mActorEffect, 0u);
    }

    TEST(ESM4LoadOrderTest, adjustsTheFormIdsOfAMessage)
    {
        const std::vector<ESM4::Message> records = loadAsThirdPlugin<ESM4::Message>("MESG",
            valueSubRecord<std::uint32_t>("INAM", 0x00000301)
                + subRecord("CTDA", conditionData(0x40, 1.f, 14, 0x00000302)) + zString("ITXT", "Button")
                + subRecord("CTDA", conditionData(0x40, 1.f, 14, 0x00000303)));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mIcon, adjusted(0x301));
        ASSERT_EQ(records[0].mConditions.size(), 1u);
        EXPECT_EQ(records[0].mConditions[0].reference, 0x03000302u);
        ASSERT_EQ(records[0].mButtons.size(), 1u);
        ASSERT_EQ(records[0].mButtons[0].mConditions.size(), 1u);
        EXPECT_EQ(records[0].mButtons[0].mConditions[0].reference, 0x03000303u);
    }

    TEST(ESM4LoadOrderTest, adjustsTheFormIdsOfADeckAndAChallenge)
    {
        const std::vector<ESM4::CaravanDeck> decks = loadAsThirdPlugin<ESM4::CaravanDeck>(
            "CDCK", valueSubRecord<std::uint32_t>("CARD", 0x00000401) + valueSubRecord<std::uint32_t>("CARD", 0));
        const std::vector<ESM4::Challenge> challenges = loadAsThirdPlugin<ESM4::Challenge>("CHAL",
            valueSubRecord<std::uint32_t>("SCRI", 0x00000402) + valueSubRecord<std::uint32_t>("SNAM", 0x00000403)
                + valueSubRecord<std::uint32_t>("XNAM", 0));

        ASSERT_EQ(decks.size(), 1u);
        ASSERT_EQ(decks[0].mCards.size(), 2u);
        EXPECT_EQ(decks[0].mCards[0], adjusted(0x401));
        EXPECT_EQ(decks[0].mCards[1].toUint32(), 0u);
        ASSERT_EQ(challenges.size(), 1u);
        EXPECT_EQ(challenges[0].mScript, adjusted(0x402));
        EXPECT_EQ(challenges[0].mSnam, adjusted(0x403));
        EXPECT_EQ(challenges[0].mXnam.toUint32(), 0u);
    }
}
