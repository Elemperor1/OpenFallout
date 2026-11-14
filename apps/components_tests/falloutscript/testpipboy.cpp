#include <components/falloutscript/pipboy.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>

namespace
{
    using namespace testing;
    using namespace FalloutScript;
    using namespace FalloutScript::Pipboy;

    Lookup lookupOf(const std::map<std::string, double>& settings)
    {
        return [&settings](std::string_view name) -> std::optional<double> {
            const auto found = settings.find(std::string(name));
            if (found == settings.end())
                return std::nullopt;
            return found->second;
        };
    }

    TEST(FalloutScriptPipboyTest, tellsTheClassOfKarmaFromTheLimitsOfTheGames)
    {
        const KarmaRules rules = karmaRules(lookupOf({}));
        EXPECT_EQ(karmaClass(rules, 0), KarmaClass::Neutral);
        EXPECT_EQ(karmaClass(rules, 249), KarmaClass::Neutral);
        EXPECT_EQ(karmaClass(rules, 250), KarmaClass::Good);
        EXPECT_EQ(karmaClass(rules, 750), KarmaClass::VeryGood);
        EXPECT_EQ(karmaClass(rules, -249), KarmaClass::Neutral);
        EXPECT_EQ(karmaClass(rules, -250), KarmaClass::Evil);
        EXPECT_EQ(karmaClass(rules, -750), KarmaClass::VeryEvil);
        EXPECT_EQ(karmaClass(rules, 1000), KarmaClass::VeryGood);
    }

    TEST(FalloutScriptPipboyTest, takesTheLimitsOfKarmaFromTheSettings)
    {
        const std::map<std::string, double> settings{ { "fAlignEvilMaxKarma", -100 }, { "fAlignGoodMinKarma", 100 } };
        const KarmaRules rules = karmaRules(lookupOf(settings));
        EXPECT_EQ(karmaClass(rules, 100), KarmaClass::Good);
        EXPECT_EQ(karmaClass(rules, -100), KarmaClass::Evil);
        EXPECT_EQ(karmaClass(rules, 99), KarmaClass::Neutral);
    }

    TEST(FalloutScriptPipboyTest, namesTheSettingOfTheTitleByLevelAndKarma)
    {
        EXPECT_EQ(karmaTitleSetting(1, KarmaClass::Neutral), "sKarmicTitleNeutral01");
        EXPECT_EQ(karmaTitleSetting(7, KarmaClass::Good), "sKarmicTitleGood07");
        EXPECT_EQ(karmaTitleSetting(12, KarmaClass::VeryGood), "sKarmicTitleGood12");
        EXPECT_EQ(karmaTitleSetting(20, KarmaClass::Evil), "sKarmicTitleEvil20");
        EXPECT_EQ(karmaTitleSetting(3, KarmaClass::VeryEvil), "sKarmicTitleEvil03");
        // the levels above the last title have the last, and a level below the first has the first
        EXPECT_EQ(karmaTitleSetting(45, KarmaClass::Good), "sKarmicTitleGood30");
        EXPECT_EQ(karmaTitleSetting(0, KarmaClass::Evil), "sKarmicTitleEvil01");
    }

    TEST(FalloutScriptPipboyTest, theGeneralPageShowsTheWordAndThePictureOfTheKindOfKarma)
    {
        EXPECT_EQ(karmaPage(KarmaClass::Neutral).mWordSetting, "sAlignNeutral");
        EXPECT_EQ(karmaPage(KarmaClass::Good).mWordSetting, "sAlignGood");
        EXPECT_EQ(karmaPage(KarmaClass::Evil).mWordSetting, "sAlignEvil");
        EXPECT_EQ(karmaPage(KarmaClass::VeryGood).mWordSetting, "sAlignVeryGood");
        EXPECT_EQ(karmaPage(KarmaClass::VeryEvil).mWordSetting, "sAlignVeryEvil");
        // The very good have the saintly picture, the evil the bad one and the very evil the evil one
        EXPECT_THAT(karmaPage(KarmaClass::VeryGood).mPicture, EndsWith("karma_saintly.dds"));
        EXPECT_THAT(karmaPage(KarmaClass::Evil).mPicture, EndsWith("karma_bad.dds"));
        EXPECT_THAT(karmaPage(KarmaClass::VeryEvil).mPicture, EndsWith("karma_evil.dds"));
        EXPECT_THAT(karmaPage(KarmaClass::Good).mPicture, EndsWith("karma_good.dds"));
        EXPECT_THAT(karmaPage(KarmaClass::Neutral).mPicture, EndsWith("karma_neutral.dds"));
    }

    TEST(FalloutScriptPipboyTest, theTitleOfAReputationIsNamedByItsTwoLevels)
    {
        EXPECT_EQ(reputationTitleSetting(0, 0), "sRepTitlePosNoneNegNone");
        EXPECT_EQ(reputationTitleSetting(3, 0), "sRepTitlePosThreeNegNone");
        EXPECT_EQ(reputationTitleSetting(0, 2), "sRepTitlePosNoneNegTwo");
        EXPECT_EQ(reputationTitleSetting(1, 3), "sRepTitlePosOneNegThree");
        EXPECT_EQ(reputationTitleSetting(3, 3), "sRepTitlePosThreeNegThree");
        // a level out of range is held to the table
        EXPECT_EQ(reputationTitleSetting(9, -1), "sRepTitlePosThreeNegNone");
    }

    TEST(FalloutScriptPipboyTest, aReputationIsListedWhenEitherSideHoldsPoints)
    {
        EXPECT_FALSE(reputationListed(0, 0));
        EXPECT_TRUE(reputationListed(0.5, 0));
        EXPECT_TRUE(reputationListed(0, 12));
        EXPECT_TRUE(reputationListed(3, 4));
        EXPECT_FALSE(reputationListed(-1, 0));
    }

    TEST(FalloutScriptPipboyTest, theCountsOfTheGeneralPageHaveThreeDigitSettingNames)
    {
        EXPECT_EQ(miscStatSetting(0), "sMiscStat000");
        EXPECT_EQ(miscStatSetting(9), "sMiscStat009");
        EXPECT_EQ(miscStatSetting(42), "sMiscStat042");
        EXPECT_EQ(miscStatSetting(-3), "sMiscStat000");
    }

    TEST(FalloutScriptPipboyTest, namesTheWordForTheValueOfASkill)
    {
        // The lookup keeps the map it is given, which has to outlast it
        const std::map<std::string, double> empty;
        const Lookup none = lookupOf(empty);
        EXPECT_EQ(skillLevelSetting(none, 0), "sSkillLevelNovice");
        EXPECT_EQ(skillLevelSetting(none, 24), "sSkillLevelNovice");
        EXPECT_EQ(skillLevelSetting(none, 25), "sSkillLevelApprentice");
        EXPECT_EQ(skillLevelSetting(none, 50), "sSkillLevelJourneyman");
        EXPECT_EQ(skillLevelSetting(none, 75), "sSkillLevelExpert");
        EXPECT_EQ(skillLevelSetting(none, 100), "sSkillLevelMaster");
        const std::map<std::string, double> settings{ { "iSkillApprenticeMin", 30 } };
        EXPECT_EQ(skillLevelSetting(lookupOf(settings), 29), "sSkillLevelNovice");
        EXPECT_EQ(skillLevelSetting(lookupOf(settings), 30), "sSkillLevelApprentice");
    }

    TEST(FalloutScriptPipboyTest, listsItemsUnderTheTabsOfTheInventory)
    {
        EXPECT_EQ(categoryOf(ItemKind::Weapon), ItemCategory::Weapons);
        EXPECT_EQ(categoryOf(ItemKind::Armor), ItemCategory::Apparel);
        EXPECT_EQ(categoryOf(ItemKind::Aid), ItemCategory::Aid);
        EXPECT_EQ(categoryOf(ItemKind::Ammo), ItemCategory::Ammo);
        EXPECT_EQ(categoryOf(ItemKind::Other), ItemCategory::Misc);
        EXPECT_STREQ(categorySetting(ItemCategory::Apparel), "sInventoryApparel");
        EXPECT_STREQ(categoryName(ItemCategory::Ammo), "ammo");
    }
}
