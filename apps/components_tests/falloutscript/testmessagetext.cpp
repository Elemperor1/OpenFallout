#include <components/falloutscript/messagetext.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
    using FalloutScript::formatMessage;

    TEST(FalloutScriptMessageTextTest, textWithoutTokensIsAsItIs)
    {
        EXPECT_EQ(formatMessage("Nothing to fill in.", {}), "Nothing to fill in.");
        EXPECT_EQ(formatMessage("Nothing to fill in.", { 1, 2 }), "Nothing to fill in.");
        EXPECT_EQ(formatMessage("", { 1 }), "");
    }

    TEST(FalloutScriptMessageTextTest, aTokenTakesTheValuesOneAfterTheOther)
    {
        EXPECT_EQ(formatMessage("%.0f of %.0f", { 3, 10 }), "3 of 10");
        EXPECT_EQ(formatMessage("%.2f", { 2.5 }), "2.50");
        EXPECT_EQ(formatMessage("%.1f%%", { 12.34 }), "12.3%");
        EXPECT_EQ(formatMessage("%g and %g", { 2.5, 100 }), "2.5 and 100");
        EXPECT_EQ(formatMessage("%f", { 1.5 }), "1.500000");
    }

    TEST(FalloutScriptMessageTextTest, anIntegerTokenCutsOffTheFraction)
    {
        EXPECT_EQ(formatMessage("%d", { 3.9 }), "3");
        EXPECT_EQ(formatMessage("%i", { -3.9 }), "-3");
        EXPECT_EQ(formatMessage("%u", { 7 }), "7");
        EXPECT_EQ(formatMessage("%03d", { 7 }), "007");
    }

    TEST(FalloutScriptMessageTextTest, aPercentSignIsWrittenTwice)
    {
        EXPECT_EQ(formatMessage("100%% sure", {}), "100% sure");
        EXPECT_EQ(formatMessage("%%d %d", { 4 }), "%d 4");
    }

    TEST(FalloutScriptMessageTextTest, aTokenWithNoValueLeftIsZero)
    {
        EXPECT_EQ(formatMessage("%.0f / %.0f", { 3 }), "3 / 0");
        EXPECT_EQ(formatMessage("%.0f", {}), "0");
        EXPECT_EQ(formatMessage("%.2f and %d", {}), "0.00 and 0");
    }

    TEST(FalloutScriptMessageTextTest, aTokenOfAnotherKindStaysAndTakesNoValue)
    {
        EXPECT_EQ(formatMessage("%s %.0f", { 5 }), "%s 5");
        EXPECT_EQ(formatMessage("%q", { 5 }), "%q");
        EXPECT_EQ(formatMessage("a 5%", { 5 }), "a 5%");
        EXPECT_EQ(formatMessage("a %.", { 5 }), "a %.");
        // text of a message that has a percent sign after a number, which the games read as a token
        EXPECT_EQ(formatMessage("up 5% Chance and %.0f", { 7 }), "up 5% Chance and 7");
    }

    TEST(FalloutScriptMessageTextTest, aHugeFieldIsCutToWhatAMessageCanHold)
    {
        const std::string text = formatMessage("%999999d", { 1 });
        EXPECT_EQ(text.size(), 99u);
        EXPECT_EQ(text.back(), '1');
    }

    TEST(FalloutScriptMessageTextTest, aValueThatIsNotANumberIsZero)
    {
        EXPECT_EQ(formatMessage("%d", { std::nan("") }), "0");
        EXPECT_EQ(formatMessage("%d", { 1e30 }), "9000000000000000");
    }

    TEST(FalloutScriptMessageTextTest, aNamedTokenTakesWhatTheResolverGives)
    {
        const FalloutScript::TokenResolver resolve = [](std::string_view name) -> std::optional<std::string> {
            if (name == "sUActnVats")
                return "V";
            if (name == "PCName")
                return "Courier";
            return std::nullopt;
        };
        EXPECT_EQ(formatMessage("Open it, &sUActnVats;.", {}, resolve), "Open it, V.");
        EXPECT_EQ(formatMessage("&PCName; has %.0f", { 3 }, resolve), "Courier has 3");
        // a name nobody knows, a token with no end and an ampersand that starts none stay as they are
        EXPECT_EQ(formatMessage("&unknown; &PCName & Q&A; &1;", {}, resolve), "&unknown; &PCName & Q&A; &1;");
        // without a resolver nothing is replaced
        EXPECT_EQ(formatMessage("&sUActnVats;", {}), "&sUActnVats;");
    }
}
