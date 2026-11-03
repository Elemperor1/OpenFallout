#include <components/falloutscript/barter.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScript::Barter;

    // The values that the plugins of both games give the four factors
    Settings pluginSettings()
    {
        Settings settings;
        settings.mBuyBase = 1.55;
        settings.mBuyMult = -0.45;
        settings.mSellBase = 0.45;
        settings.mSellMult = 0.45;
        return settings;
    }

    TEST(FalloutScriptBarterTest, takesTheFactorsFromTheBarterSkill)
    {
        const Factors none = factors(pluginSettings(), 0);
        EXPECT_NEAR(none.mBuy, 1.55, 1e-9);
        EXPECT_NEAR(none.mSell, 0.45, 1e-9);
        const Factors best = factors(pluginSettings(), 100);
        EXPECT_NEAR(best.mBuy, 1.10, 1e-9);
        EXPECT_NEAR(best.mSell, 0.90, 1e-9);
        const Factors half = factors(pluginSettings(), 50);
        EXPECT_NEAR(half.mBuy, 1.325, 1e-9);
        EXPECT_NEAR(half.mSell, 0.675, 1e-9);
    }

    TEST(FalloutScriptBarterTest, hasTheDefaultsOfTheExecutablesWhenTheSettingsAreMissing)
    {
        const Factors none = factors(Settings{}, 0);
        EXPECT_NEAR(none.mBuy, 1.5, 1e-9);
        EXPECT_NEAR(none.mSell, 0.5, 1e-9);
        const Factors best = factors(Settings{}, 100);
        EXPECT_NEAR(best.mBuy, 1.0, 1e-9);
        EXPECT_NEAR(best.mSell, 1.0, 1e-9);
    }

    TEST(FalloutScriptBarterTest, roundsUpToWholeNumbersAboveOneAndToTenthsBelow)
    {
        EXPECT_NEAR(roundUp(12.01, 1.0), 13.0, 1e-9);
        EXPECT_NEAR(roundUp(12.0, 1.0), 12.0, 1e-9);
        EXPECT_NEAR(roundUp(0.31, 0.1), 0.4, 1e-9);
        EXPECT_NEAR(roundUp(0.3, 0.1), 0.3, 1e-9);
        // A hair over a whole number is a float's doing and is not above it
        EXPECT_NEAR(roundUp(12.0000001, 1.0), 12.0, 1e-9);
        EXPECT_NEAR(roundUp(5.0, 0.0), 5.0, 1e-9);
    }

    TEST(FalloutScriptBarterTest, takesTheWorthOfAThingWithNoConditionAsItsValue)
    {
        const Settings settings;
        EXPECT_NEAR(baseValue(100, 10, settings), 100.0, 1e-9);
        EXPECT_NEAR(baseValue(1000, 10, settings), 1000.0, 1e-9);
        EXPECT_NEAR(baseValue(10, 10, settings), 10.0, 1e-9);
        EXPECT_NEAR(baseValue(3, 10, settings), 3.0, 1e-9);
        EXPECT_NEAR(baseValue(2, 10, settings), 2.0, 1e-9);
        EXPECT_NEAR(baseValue(1, 10, settings), 1.0, 1e-9);
        EXPECT_NEAR(baseValue(0, 10, settings), 0.0, 1e-9);
    }

    TEST(FalloutScriptBarterTest, takesLessForAThingThatIsWornDown)
    {
        const Settings settings;
        // Half the condition is 0.0316 * 100 * 5^1.5 = 35.3...
        EXPECT_NEAR(baseValue(100, 5, settings), 36.0, 1e-9);
        EXPECT_NEAR(baseValue(100, 0, settings), 0.0, 1e-9);
    }

    TEST(FalloutScriptBarterTest, pricesWhatTheBuyerPaysAboveTheBaseValue)
    {
        const Factors half = factors(pluginSettings(), 50);
        EXPECT_NEAR(unitPrice(100, true, half, 0, nullptr), 133.0, 1e-9);
        // Under 1 the price goes by tenths
        EXPECT_NEAR(unitPrice(1, true, half, 0, nullptr), 1.4, 1e-9);
        EXPECT_NEAR(unitPrice(0.5, true, half, 0, nullptr), 0.7, 1e-9);
    }

    TEST(FalloutScriptBarterTest, pricesWhatTheSellerGetsBelowTheBaseValue)
    {
        const Factors half = factors(pluginSettings(), 50);
        EXPECT_NEAR(unitPrice(100, false, half, 0, nullptr), 68.0, 1e-9);
        EXPECT_NEAR(unitPrice(1, false, half, 0, nullptr), 0.7, 1e-9);
    }

    TEST(FalloutScriptBarterTest, neverAsksLessThanTheBaseValueNorPaysMore)
    {
        EXPECT_NEAR(unitPrice(100, true, Factors{ 0.5, 0.5 }, 0, nullptr), 100.0, 1e-9);
        EXPECT_NEAR(unitPrice(100, false, Factors{ 1.5, 1.5 }, 0, nullptr), 100.0, 1e-9);
    }

    TEST(FalloutScriptBarterTest, givesAPositivePercentToThePlayerOnBothSides)
    {
        const Factors doubled{ 2.0, 0.5 };
        EXPECT_NEAR(unitPrice(100, true, doubled, 0, nullptr), 200.0, 1e-9);
        EXPECT_NEAR(unitPrice(100, true, doubled, 20, nullptr), 160.0, 1e-9);
        EXPECT_NEAR(unitPrice(100, true, doubled, -20, nullptr), 240.0, 1e-9);
        EXPECT_NEAR(unitPrice(100, false, doubled, 0, nullptr), 50.0, 1e-9);
        EXPECT_NEAR(unitPrice(100, false, doubled, 20, nullptr), 60.0, 1e-9);
        EXPECT_NEAR(unitPrice(100, false, doubled, -20, nullptr), 40.0, 1e-9);
    }

    TEST(FalloutScriptBarterTest, appliesThePerksBeforeTheRounding)
    {
        const Factors doubled{ 2.0, 0.5 };
        const auto tenPercentOff = [](double value) { return value * 0.9; };
        EXPECT_NEAR(unitPrice(100, true, doubled, 0, tenPercentOff), 180.0, 1e-9);
        EXPECT_NEAR(unitPrice(3, true, doubled, 0, tenPercentOff), 6.0, 1e-9);
    }

    TEST(FalloutScriptBarterTest, countsWhatIsSoldAsPlusAndWhatIsBoughtAsMinus)
    {
        EXPECT_NEAR(balance({}), 0.0, 1e-9);
        EXPECT_NEAR(balance({ { 10, 2, false } }), 20.0, 1e-9);
        EXPECT_NEAR(balance({ { 15, 1, true } }), -15.0, 1e-9);
        EXPECT_NEAR(balance({ { 10, 2, false }, { 15, 1, true } }), 5.0, 1e-9);
        EXPECT_NEAR(balance({ { 0.7, 3, false }, { 1.4, 1, true } }), 0.7, 1e-9);
    }

    TEST(FalloutScriptBarterTest, roundsTheCapsThatMoveAndCutsAPayoutAtWhatTheVendorHas)
    {
        EXPECT_EQ(amount(5.4, 100), 5);
        EXPECT_EQ(amount(5.5, 100), 6);
        EXPECT_EQ(amount(500, 120), 120);
        EXPECT_EQ(amount(500, 0), 0);
        // What the player pays is not cut
        EXPECT_EQ(amount(-500, 120), 500);
        EXPECT_EQ(amount(-5.6, 0), 6);
        EXPECT_EQ(amount(-0.4, 0), 0);
    }

    TEST(FalloutScriptBarterTest, enablesTheAcceptButtonOnlyForADealThatCanBeMade)
    {
        EXPECT_FALSE(canAccept(true, 0, 1000));
        EXPECT_FALSE(canAccept(true, 50, 1000));
        EXPECT_TRUE(canAccept(false, 0, 0));
        EXPECT_TRUE(canAccept(false, 100, 0));
        EXPECT_FALSE(canAccept(false, -5, 4));
        EXPECT_TRUE(canAccept(false, -5, 5));
    }
}
