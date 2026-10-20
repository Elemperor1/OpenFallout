#include "scriptbuilder.hpp"

#include <components/falloutscript/chargen.hpp>
#include <components/falloutscript/extendercommands.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::Chargen;
    using FalloutScript::ChargenPage;
    using FalloutScript::FormId;
    using FalloutScript::Game;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr int strength = 5;
    constexpr int perception = 6;
    constexpr int endurance = 7;
    constexpr int luck = 11;
    constexpr int barter = 32;
    constexpr int bigGuns = 33;
    constexpr int guns = 41;
    constexpr int survival = 44;
    constexpr int unarmed = 45;

    std::vector<int> numbersOf(const std::vector<FalloutScript::ChargenSkill>& skills)
    {
        std::vector<int> numbers;
        for (const FalloutScript::ChargenSkill& skill : skills)
            numbers.push_back(skill.mNumber);
        return numbers;
    }

    TEST(FalloutScriptChargenTest, a_new_character_has_five_in_each_attribute_and_five_points_to_give)
    {
        const Chargen chargen;
        for (int attribute = 5; attribute <= 11; ++attribute)
            EXPECT_EQ(chargen.attribute(attribute), 5);
        EXPECT_EQ(chargen.pointsLeft(), 5);
        // the numbers that are not attributes
        EXPECT_EQ(chargen.attribute(4), 0);
        EXPECT_EQ(chargen.attribute(12), 0);
    }

    TEST(FalloutScriptChargenTest, points_are_given_only_while_there_are_some_and_within_the_limits)
    {
        Chargen chargen;
        EXPECT_TRUE(chargen.adjustAttribute(strength, 3));
        EXPECT_EQ(chargen.attribute(strength), 8);
        EXPECT_EQ(chargen.pointsLeft(), 2);
        // two are left: one more fits, and so do two, but not three
        EXPECT_FALSE(chargen.adjustAttribute(perception, 3));
        EXPECT_EQ(chargen.attribute(perception), 5);
        EXPECT_TRUE(chargen.adjustAttribute(perception, 2));
        EXPECT_EQ(chargen.pointsLeft(), 0);
        EXPECT_FALSE(chargen.adjustAttribute(endurance, 1));
        // taking points back gives them to the others
        EXPECT_TRUE(chargen.adjustAttribute(strength, -4));
        EXPECT_EQ(chargen.attribute(strength), 4);
        EXPECT_TRUE(chargen.adjustAttribute(endurance, 1));
        EXPECT_EQ(chargen.pointsLeft(), 3);
    }

    TEST(FalloutScriptChargenTest, an_attribute_stays_between_one_and_ten)
    {
        Chargen chargen;
        EXPECT_TRUE(chargen.adjustAttribute(luck, -4));
        EXPECT_EQ(chargen.attribute(luck), 1);
        EXPECT_FALSE(chargen.adjustAttribute(luck, -1));
        EXPECT_EQ(chargen.attribute(luck), 1);
        EXPECT_EQ(chargen.pointsLeft(), 9);
        EXPECT_TRUE(chargen.adjustAttribute(strength, 5));
        EXPECT_EQ(chargen.attribute(strength), 10);
        EXPECT_FALSE(chargen.adjustAttribute(strength, 1));
        EXPECT_FALSE(chargen.adjustAttribute(3, 1));
        EXPECT_FALSE(chargen.adjustAttribute(12, 1));
    }

    TEST(FalloutScriptChargenTest, a_change_far_out_of_the_range_changes_nothing)
    {
        Chargen chargen;
        EXPECT_FALSE(chargen.adjustAttribute(strength, std::numeric_limits<int>::max()));
        EXPECT_FALSE(chargen.adjustAttribute(strength, std::numeric_limits<int>::min()));
        EXPECT_FALSE(chargen.adjustAttribute(strength, 10));
        EXPECT_FALSE(chargen.adjustAttribute(strength, -10));
        EXPECT_EQ(chargen.attribute(strength), 5);
    }

    TEST(FalloutScriptChargenTest, attributes_that_were_set_are_kept_within_the_limits_and_can_cost_more_than_there_is)
    {
        Chargen chargen;
        chargen.setAttributes({ 10, 10, 10, 10, 0, 12, 5 });
        EXPECT_EQ(chargen.attribute(strength), 10);
        EXPECT_EQ(chargen.attribute(9), 1);
        EXPECT_EQ(chargen.attribute(10), 10);
        EXPECT_EQ(chargen.pointsLeft(), 40 - 56);
        // nothing can be given, but points can be taken back
        EXPECT_FALSE(chargen.adjustAttribute(luck, 1));
        EXPECT_TRUE(chargen.adjustAttribute(strength, -1));
    }

    TEST(FalloutScriptChargenTest, a_skill_is_two_plus_twice_its_attribute_plus_half_the_luck_and_fifteen_when_tagged)
    {
        Chargen chargen;
        // Barter is governed by charisma: 2 + 2 * 5 + 5 / 2
        EXPECT_EQ(chargen.skillValue(barter), 14);
        chargen.adjustAttribute(luck, 1);
        EXPECT_EQ(chargen.skillValue(barter), 2 + 10 + 3);
        ASSERT_TRUE(chargen.toggleTag(barter));
        EXPECT_EQ(chargen.skillValue(barter), 2 + 10 + 3 + 15);
        // the unarmed skill is governed by endurance
        chargen.adjustAttribute(endurance, 2);
        EXPECT_EQ(chargen.skillValue(unarmed), 2 + 14 + 3);
        EXPECT_EQ(chargen.skillValue(4), 0);
    }

    TEST(FalloutScriptChargenTest, the_skills_of_a_game_are_thirteen_and_differ_in_two)
    {
        const std::vector<int> fallout3 = numbersOf(Chargen::skillsOf(Game::Fallout3));
        const std::vector<int> newVegas = numbersOf(Chargen::skillsOf(Game::NewVegas));
        EXPECT_THAT(fallout3, ElementsAre(32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 45));
        EXPECT_THAT(newVegas, ElementsAre(32, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45));
        const Chargen vegas(Game::NewVegas);
        EXPECT_TRUE(vegas.isSkill(survival));
        EXPECT_FALSE(vegas.isSkill(bigGuns));
        EXPECT_EQ(Chargen::skillsOf(Game::NewVegas)[8].mEditorId, std::string("Guns"));
        EXPECT_EQ(Chargen::skillsOf(Game::Fallout3)[9].mEditorId, std::string("SmallGuns"));
    }

    TEST(FalloutScriptChargenTest, three_skills_are_tagged_and_a_tag_can_be_taken_off)
    {
        Chargen chargen;
        EXPECT_TRUE(chargen.toggleTag(guns));
        EXPECT_TRUE(chargen.toggleTag(barter));
        EXPECT_TRUE(chargen.toggleTag(survival));
        EXPECT_FALSE(chargen.toggleTag(unarmed));
        EXPECT_FALSE(chargen.tagged(unarmed));
        EXPECT_THAT(chargen.tags(), ElementsAre(barter, guns, survival));
        EXPECT_TRUE(chargen.toggleTag(guns));
        EXPECT_FALSE(chargen.tagged(guns));
        EXPECT_TRUE(chargen.toggleTag(unarmed));
        EXPECT_THAT(chargen.tags(), ElementsAre(barter, survival, unarmed));
    }

    TEST(FalloutScriptChargenTest, what_is_no_skill_of_the_game_is_not_tagged)
    {
        Chargen chargen(Game::NewVegas);
        EXPECT_FALSE(chargen.toggleTag(bigGuns));
        EXPECT_FALSE(chargen.toggleTag(strength));
        chargen.setTags({ bigGuns, barter, barter, guns, survival, unarmed });
        EXPECT_THAT(chargen.tags(), ElementsAre(barter, guns, survival));
    }

    TEST(FalloutScriptChargenTest, two_traits_are_picked_and_a_pick_can_be_let_go)
    {
        Chargen chargen;
        EXPECT_TRUE(chargen.toggleTrait(0x100));
        EXPECT_TRUE(chargen.toggleTrait(0x200));
        EXPECT_FALSE(chargen.toggleTrait(0x300));
        EXPECT_THAT(chargen.traits(), ElementsAre(0x100u, 0x200u));
        EXPECT_TRUE(chargen.toggleTrait(0x100));
        EXPECT_FALSE(chargen.hasTrait(0x100));
        EXPECT_TRUE(chargen.toggleTrait(0x300));
        EXPECT_THAT(chargen.traits(), ElementsAre(0x200u, 0x300u));
        EXPECT_FALSE(chargen.toggleTrait(0));
        chargen.setTraits({ 0x1, 0x1, 0, 0x2, 0x3 });
        EXPECT_THAT(chargen.traits(), ElementsAre(0x1u, 0x2u));
    }

    TEST(FalloutScriptChargenTest, a_name_is_cleaned_and_cut_to_the_length_that_fits)
    {
        Chargen chargen;
        EXPECT_EQ(chargen.setName("  Courier\tSix \n"), "CourierSix");
        EXPECT_EQ(chargen.name(), "CourierSix");
        EXPECT_EQ(chargen.setName("  The Chosen One  "), "The Chosen One");
        EXPECT_EQ(chargen.setName(""), "");
        const std::string longName(40, 'a');
        EXPECT_EQ(chargen.setName(longName), std::string(31, 'a'));
        // the cut does not split a character of UTF-8: 30 letters and a two-byte character end at 32
        const std::string accented = std::string(30, 'b') + "\xC3\xA9" + "xyz";
        EXPECT_EQ(chargen.setName(accented), std::string(30, 'b'));
        // a name that ends in a space after the cut loses it
        EXPECT_EQ(chargen.setName(std::string(30, 'c') + " dd"), std::string(30, 'c'));
    }

    TEST(FalloutScriptChargenTest, the_sex_and_race_are_none_until_the_player_picks)
    {
        Chargen chargen;
        EXPECT_FALSE(chargen.female().has_value());
        EXPECT_EQ(chargen.race(), 0u);
        chargen.setFemale(true);
        chargen.setRace(0x19);
        EXPECT_EQ(chargen.female(), std::optional<bool>(true));
        EXPECT_EQ(chargen.race(), 0x19u);
        chargen.clear();
        EXPECT_FALSE(chargen.female().has_value());
        EXPECT_EQ(chargen.race(), 0u);
    }

    TEST(FalloutScriptChargenTest, the_screens_of_a_menu_come_one_after_the_other)
    {
        Chargen chargen;
        EXPECT_FALSE(chargen.menuOpen());
        EXPECT_EQ(chargen.page(), std::nullopt);
        EXPECT_EQ(chargen.finishPage(), std::nullopt);

        chargen.open({ ChargenPage::Special, ChargenPage::Skills }, { 3, 4 });
        EXPECT_TRUE(chargen.menuOpen());
        EXPECT_EQ(chargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_THAT(chargen.values(), ElementsAre(3, 4));
        const std::uint32_t first = chargen.serial();

        EXPECT_EQ(chargen.finishPage(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_EQ(chargen.page(), std::optional<ChargenPage>(ChargenPage::Skills));
        // the next screen is another one even if it were the same page
        EXPECT_NE(chargen.serial(), first);

        EXPECT_EQ(chargen.finishPage(), std::optional<ChargenPage>(ChargenPage::Skills));
        EXPECT_FALSE(chargen.menuOpen());
        EXPECT_EQ(chargen.page(), std::nullopt);
    }

    TEST(FalloutScriptChargenTest, the_character_is_a_new_one_until_the_player_has_been_through_special)
    {
        Chargen chargen;
        EXPECT_FALSE(chargen.specialMade());
        chargen.open({ ChargenPage::Name, ChargenPage::Special });
        chargen.finishPage();
        EXPECT_FALSE(chargen.specialMade());
        chargen.finishPage();
        EXPECT_TRUE(chargen.specialMade());
        chargen.clear();
        EXPECT_FALSE(chargen.specialMade());
    }

    TEST(FalloutScriptChargenTest, a_menu_asked_for_while_one_is_open_waits_its_turn)
    {
        Chargen chargen;
        chargen.open({ ChargenPage::Name }, { 1 });
        chargen.open({ ChargenPage::Race }, { 2 });
        EXPECT_EQ(chargen.page(), std::optional<ChargenPage>(ChargenPage::Name));
        EXPECT_THAT(chargen.values(), ElementsAre(1));
        chargen.finishPage();
        EXPECT_EQ(chargen.page(), std::optional<ChargenPage>(ChargenPage::Race));
        // and it has the values it was asked for with, not those of the menu before
        EXPECT_THAT(chargen.values(), ElementsAre(2));
    }

    TEST(FalloutScriptChargenTest, a_screen_has_the_number_of_the_menu_it_belongs_to)
    {
        Chargen chargen;
        EXPECT_EQ(chargen.menu(), 0);
        chargen.open({ ChargenPage::Race }, {}, 1036);
        chargen.open({ ChargenPage::Special, ChargenPage::Skills }, {}, 1048);
        EXPECT_EQ(chargen.menu(), 1036);
        chargen.finishPage();
        EXPECT_EQ(chargen.menu(), 1048);
        chargen.finishPage();
        EXPECT_EQ(chargen.menu(), 1048);
        chargen.finishPage();
        EXPECT_EQ(chargen.menu(), 0);
        EXPECT_FALSE(chargen.menuOpen());
    }

    TEST(FalloutScriptChargenTest, the_screen_of_the_sex_and_race_does_not_ask_for_a_name_by_itself)
    {
        Chargen chargen;
        chargen.open({ ChargenPage::Race });
        EXPECT_EQ(chargen.page(), std::optional<ChargenPage>(ChargenPage::Race));
        chargen.finishPage();
        EXPECT_FALSE(chargen.menuOpen());
    }

    TEST(FalloutScriptChargenTest, a_screen_of_special_gives_out_the_points_it_was_asked_for)
    {
        Chargen chargen;
        EXPECT_EQ(chargen.pointTotal(), 40);
        chargen.open({ ChargenPage::Special }, {}, 1060, 30);
        EXPECT_EQ(chargen.pointTotal(), 30);
        chargen.setAttributes({ 1, 1, 1, 1, 1, 1, 1 });
        EXPECT_EQ(chargen.pointsLeft(), 23);
        EXPECT_TRUE(chargen.adjustAttribute(5, 9));
        EXPECT_FALSE(chargen.adjustAttribute(6, 9 + 14));
        EXPECT_TRUE(chargen.adjustAttribute(6, 9));
        EXPECT_FALSE(chargen.adjustAttribute(7, 6));
        EXPECT_TRUE(chargen.adjustAttribute(7, 5));
        EXPECT_EQ(chargen.pointsLeft(), 0);
        // a count out of what the attributes can come to is brought within it
        chargen.finishPage();
        chargen.open({ ChargenPage::Special }, {}, 0, 500);
        EXPECT_EQ(chargen.pointTotal(), 70);
        chargen.finishPage();
        chargen.open({ ChargenPage::Special }, {}, 0, 2);
        EXPECT_EQ(chargen.pointTotal(), 7);
    }

    TEST(FalloutScriptChargenTest, the_second_screen_of_special_has_its_own_total)
    {
        Chargen chargen;
        chargen.open({ ChargenPage::Special }, {}, 0, 40);
        chargen.open({ ChargenPage::Special }, {}, 0, 2, true);
        EXPECT_EQ(chargen.pointTotal(), 40);
        chargen.finishPage();
        EXPECT_EQ(chargen.pointTotal(), 35 + 2);
    }

    TEST(FalloutScriptChargenTest, a_screen_of_skills_lets_as_many_be_tagged_as_it_was_asked_for)
    {
        Chargen chargen;
        EXPECT_EQ(chargen.tagsAllowed(), 3);
        chargen.open({ ChargenPage::Skills }, {}, 0, 1);
        EXPECT_EQ(chargen.tagsAllowed(), 1);
        EXPECT_TRUE(chargen.toggleTag(32));
        EXPECT_FALSE(chargen.toggleTag(41));
        chargen.setTags({ 41, 42, 43 });
        EXPECT_THAT(chargen.tags(), ElementsAre(41));
        chargen.finishPage();
        // the tags that are there stay when more are added
        chargen.open({ ChargenPage::Skills }, {}, 0, 2, true);
        EXPECT_EQ(chargen.tagsAllowed(), 3);
        chargen.finishPage();
        chargen.open({ ChargenPage::Skills }, {}, 0, 900);
        EXPECT_EQ(chargen.tagsAllowed(), static_cast<int>(chargen.skills().size()));
    }

    TEST(FalloutScriptChargenTest, a_skill_that_is_tagged_in_a_place_counts_as_tagged)
    {
        Chargen chargen;
        chargen.setAttributes({ 5, 5, 5, 5, 5, 5, 5 });
        const int untagged = chargen.skillValue(41);
        EXPECT_TRUE(chargen.setTagSlot(2, 41));
        EXPECT_TRUE(chargen.tagged(41));
        EXPECT_EQ(chargen.skillValue(41), untagged + Chargen::tagBonus);
        EXPECT_THAT(chargen.tags(), ElementsAre(41));
        EXPECT_TRUE(chargen.toggleTag(41));
        EXPECT_FALSE(chargen.tagged(41));
        EXPECT_FALSE(chargen.tagged(0));
    }

    TEST(FalloutScriptChargenTest, a_screen_of_special_is_not_done_while_more_points_are_given_than_it_has)
    {
        Chargen chargen;
        chargen.open({ ChargenPage::Special }, {}, 0, 30);
        chargen.setAttributes({ 5, 5, 5, 5, 5, 5, 5 });
        EXPECT_EQ(chargen.pointsLeft(), -5);
        EXPECT_EQ(chargen.finishPage(), std::nullopt);
        EXPECT_EQ(chargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_FALSE(chargen.specialMade());

        // The points are taken back, and then the screen is done, also with some left
        EXPECT_TRUE(chargen.adjustAttribute(5, -3));
        EXPECT_TRUE(chargen.adjustAttribute(6, -2));
        EXPECT_EQ(chargen.pointsLeft(), 0);
        EXPECT_TRUE(chargen.adjustAttribute(6, -1));
        EXPECT_EQ(chargen.finishPage(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_TRUE(chargen.specialMade());
    }

    TEST(FalloutScriptChargenTest, a_screen_of_skills_tells_what_was_tagged_and_let_go_when_it_was_asked_to_add)
    {
        Chargen chargen;
        chargen.setTags({ 32, 41, 45 });
        chargen.open({ ChargenPage::Skills }, {}, 0, 1, true);
        EXPECT_EQ(chargen.tagsAllowed(), 4);
        EXPECT_TRUE(chargen.toggleTag(41));
        EXPECT_TRUE(chargen.toggleTag(34));
        EXPECT_EQ(chargen.finishPage(), std::optional<ChargenPage>(ChargenPage::Skills));
        EXPECT_TRUE(chargen.lastFinishAdded());
        EXPECT_THAT(chargen.lastTagged(), ElementsAre(34));
        EXPECT_THAT(chargen.lastUntagged(), ElementsAre(41));

        // A screen that sets the tags is not one that adds
        chargen.open({ ChargenPage::Skills }, {}, 0, 3);
        EXPECT_EQ(chargen.finishPage(), std::optional<ChargenPage>(ChargenPage::Skills));
        EXPECT_FALSE(chargen.lastFinishAdded());
        EXPECT_THAT(chargen.lastTagged(), IsEmpty());
        EXPECT_THAT(chargen.lastUntagged(), IsEmpty());
    }

    TEST(FalloutScriptChargenTest, fallout_3_has_no_screen_for_traits)
    {
        Chargen chargen(Game::Fallout3);
        EXPECT_FALSE(chargen.hasTraits());
        chargen.open({ ChargenPage::Traits });
        EXPECT_FALSE(chargen.menuOpen());
        chargen.open({ ChargenPage::Traits, ChargenPage::Skills });
        EXPECT_EQ(chargen.page(), std::optional<ChargenPage>(ChargenPage::Skills));
    }

    constexpr FormId crate = 0x01000100;
    constexpr FormId scriptOfCrates = 0x01002000;
    constexpr std::uint16_t OpShowRaceMenu = 0x10DA;
    constexpr std::uint16_t OpTtwShowGeneProjector = 0x3800;
    constexpr std::uint16_t OpShowNameMenu = 0x10DC;
    constexpr std::uint16_t OpGetPlayerName = 0x11E1;
    constexpr std::uint16_t OpSetInChargen = 0x10E7;
    constexpr std::uint16_t OpGetInCharGen = 0x127F;
    constexpr std::uint16_t OpShowSpecialBook = 0x11E8;
    constexpr std::uint16_t OpShowSpecialBookParams = 0x11EE;
    constexpr std::uint16_t OpShowChargenMenu = 0x1197;
    constexpr std::uint16_t OpShowChargenMenuParams = 0x11B2;
    constexpr std::uint16_t OpShowTraitMenu = 0x1268;
    constexpr std::uint16_t OpSexChange = 0x11E7;
    constexpr std::uint16_t OpSetSpecialPoints = 0x1218;
    constexpr std::uint16_t OpAddSpecialPoints = 0x1219;
    constexpr std::uint16_t OpSetTagSkills = 0x121A;
    constexpr std::uint16_t OpAddTagSkills = 0x121B;
    constexpr std::uint16_t OpSetPlayerTagSkill = 0x11CD;
    constexpr std::uint16_t OpIsPlayerTagSkill = 0x11CE;
    constexpr std::uint32_t integerVariable = 1;

    struct Runner
    {
        FalloutScript::CommandTable mCommands;
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter;
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        Chargen mChargen;
        int mOpened = 0;

        explicit Runner(Game game = Game::NewVegas)
            : mCommands(FalloutScript::vanillaCommands(game))
            , mInterpreter(mHost, mCommands)
            , mChargen(game)
        {
            FalloutScript::addExtenderCommands(mCommands, game);
            FalloutScript::addChargenCommands(mInterpreter, mChargen, [this] { ++mOpened; });
        }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfCrates];
            record.mId = ESM::FormId::fromUint32(scriptOfCrates);
            record.mScript = builder.definition();
            mObjects.add(crate, scriptOfCrates);
            mObjects.trigger(crate, BlockType::OnActivate);
        }
    };

    TEST(FalloutScriptChargenCommandsTest, the_menu_commands_open_their_screens)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpShowNameMenu, arguments(0))
            + command(OpShowRaceMenu, arguments(0)) + command(OpShowSpecialBook, arguments(0)) + end());
        setup.run(builder);

        EXPECT_EQ(setup.mOpened, 3);
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Name));
        setup.mChargen.finishPage();
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Race));
        setup.mChargen.finishPage();
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptChargenCommandsTest, the_gene_projector_of_tale_of_two_wastelands_opens_the_menu_of_the_race)
    {
        // The bytes of the call in the quest CG00 (stage 60) of the plugin: the code, a length of 0, and then the next
        // statement; where Fallout 3 has ShowRaceMenu
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpTtwShowGeneProjector, arguments(0))
            + command(OpGetPlayerName, arguments(0)) + end());
        setup.run(builder);

        EXPECT_EQ(setup.mOpened, 2);
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Race));
        EXPECT_EQ(setup.mChargen.menu(), 1036);
        setup.mChargen.finishPage();
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Name));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
        EXPECT_EQ(setup.mInterpreter.takeMissingSummary(), "");
    }

    TEST(FalloutScriptChargenCommandsTest, the_gene_projector_is_not_a_command_of_fallout_3)
    {
        Runner setup(Game::Fallout3);
        ScriptBuilder builder;
        builder.code(
            scriptName() + begin(BlockType::OnActivate) + command(OpTtwShowGeneProjector, arguments(0)) + end());
        setup.run(builder);

        EXPECT_FALSE(setup.mChargen.menuOpen());
        EXPECT_EQ(setup.mInterpreter.takeMissingSummary(), "0x3800 (script extender) 1");
    }

    TEST(FalloutScriptChargenCommandsTest, the_name_is_asked_for_after_the_race_as_the_scripts_of_the_games_do)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpShowRaceMenu, arguments(0))
            + command(OpGetPlayerName, arguments(0)) + end());
        setup.run(builder);

        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Race));
        EXPECT_EQ(setup.mChargen.menu(), 1036);
        setup.mChargen.finishPage();
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Name));
        EXPECT_EQ(setup.mChargen.menu(), 1051);
        setup.mChargen.finishPage();
        EXPECT_FALSE(setup.mChargen.menuOpen());
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptChargenCommandsTest, the_menus_have_the_numbers_the_games_give_them)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpShowSpecialBook, arguments(0))
            + command(OpSetSpecialPoints, arguments(1).add(intArg(40)))
            + command(OpSetTagSkills, arguments(1).add(intArg(3))) + command(OpShowTraitMenu, arguments(0)) + end());
        setup.run(builder);

        std::vector<int> menus;
        while (setup.mChargen.menuOpen())
        {
            menus.push_back(setup.mChargen.menu());
            setup.mChargen.finishPage();
        }
        EXPECT_THAT(menus, ElementsAre(1060, 1048, 1048, 1084));
    }

    TEST(FalloutScriptChargenCommandsTest, the_menu_with_numbers_opens_the_screen_of_the_first_number_or_else_the_third)
    {
        {
            Runner setup;
            ScriptBuilder builder;
            builder.code(scriptName() + begin(BlockType::OnActivate)
                + command(OpShowChargenMenuParams, arguments(3).add(intArg(42)).add(intArg(0)).add(intArg(2))) + end());
            setup.run(builder);
            EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
            EXPECT_EQ(setup.mChargen.pointTotal(), 42);
            EXPECT_THAT(setup.mChargen.values(), ElementsAre(42, 0, 2));
            setup.mChargen.finishPage();
            EXPECT_FALSE(setup.mChargen.menuOpen());
        }
        {
            Runner setup;
            ScriptBuilder builder;
            builder.code(scriptName() + begin(BlockType::OnActivate)
                + command(OpShowChargenMenuParams, arguments(3).add(intArg(0)).add(intArg(5)).add(intArg(2))) + end());
            setup.run(builder);
            EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Skills));
            EXPECT_EQ(setup.mChargen.tagsAllowed(), 2);
        }
        {
            // with no number, and without the numbers, the games open nothing
            Runner setup;
            ScriptBuilder builder;
            builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpShowChargenMenu, arguments(0))
                + command(OpShowChargenMenuParams, arguments(3).add(intArg(0)).add(intArg(4)).add(intArg(0))) + end());
            setup.run(builder);
            EXPECT_FALSE(setup.mChargen.menuOpen());
            EXPECT_EQ(setup.mOpened, 0);
        }
    }

    TEST(FalloutScriptChargenCommandsTest, the_points_of_special_are_given_out_or_added_as_the_script_asks)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSetSpecialPoints, arguments(1).add(intArg(33)))
            + command(OpAddSpecialPoints, arguments(1).add(intArg(2)))
            + command(OpSetSpecialPoints, arguments(1).add(intArg(0))) + end());
        setup.run(builder);

        // the call with 0 opens nothing
        EXPECT_EQ(setup.mOpened, 2);
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_EQ(setup.mChargen.pointTotal(), 33);
        // the points that are added are on top of what the attributes come to when the screen opens
        setup.mChargen.setAttributes({ 3, 3, 3, 3, 3, 3, 3 });
        setup.mChargen.finishPage();
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_EQ(setup.mChargen.pointTotal(), 21 + 2);
        setup.mChargen.setAttributes({ 4, 4, 4, 4, 4, 4, 4 });
        EXPECT_EQ(setup.mChargen.pointTotal(), 28 + 2);
        EXPECT_EQ(setup.mChargen.pointsLeft(), 2);
        setup.mChargen.finishPage();
        EXPECT_FALSE(setup.mChargen.menuOpen());
    }

    TEST(FalloutScriptChargenCommandsTest, a_count_that_is_huge_or_not_a_number_is_kept_within_what_a_screen_can_show)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpAddSpecialPoints, arguments(1).add(intArg(2147483647)))
            + command(OpSetTagSkills, arguments(1).add(intArg(-5))) + end());
        setup.run(builder);

        // the call with a negative count opens nothing, and the huge one gives what the limits allow
        EXPECT_EQ(setup.mOpened, 1);
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_EQ(setup.mChargen.pointTotal(), 70);
    }

    TEST(FalloutScriptChargenCommandsTest, the_tags_are_picked_up_to_the_count_the_script_asks_for)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSetTagSkills, arguments(2).add(intArg(2)).add(intArg(1)))
            + command(OpAddTagSkills, arguments(1).add(intArg(1))) + end());
        setup.run(builder);

        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Skills));
        EXPECT_EQ(setup.mChargen.tagsAllowed(), 2);
        EXPECT_TRUE(setup.mChargen.toggleTag(32));
        EXPECT_TRUE(setup.mChargen.toggleTag(41));
        EXPECT_FALSE(setup.mChargen.toggleTag(45));
        setup.mChargen.finishPage();
        // one more than the two that are tagged
        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Skills));
        EXPECT_EQ(setup.mChargen.tagsAllowed(), 3);
        EXPECT_TRUE(setup.mChargen.toggleTag(45));
        EXPECT_FALSE(setup.mChargen.toggleTag(43));
        EXPECT_THAT(setup.mChargen.tags(), ElementsAre(32, 41, 45));
    }

    TEST(FalloutScriptChargenCommandsTest, the_tags_a_script_puts_in_places_start_the_screen_of_skills)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.variable(1, integerVariable, "before");
        builder.variable(2, integerVariable, "other");
        builder.variable(3, integerVariable, "after");
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsPlayerTagSkill, arguments(1).add(Bytes().u16(41))))
            + command(OpSetPlayerTagSkill, arguments(2).add(Bytes().u16(41)).add(intArg(0)))
            + command(OpSetPlayerTagSkill, arguments(2).add(Bytes().u16(34)).add(intArg(1)))
            + command(OpSetPlayerTagSkill, arguments(2).add(Bytes().u16(37)).add(intArg(2)))
            + setTo(var('s', 2), callToken(OpIsPlayerTagSkill, arguments(1).add(Bytes().u16(40))))
            + setTo(var('s', 3), callToken(OpIsPlayerTagSkill, arguments(1).add(Bytes().u16(41))))
            + command(OpSetTagSkills, arguments(1).add(intArg(3))) + end());
        setup.run(builder);

        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), 0);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(2), 0);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(3), 1);
        EXPECT_THAT(setup.mChargen.tags(), ElementsAre(34, 37, 41));
        // all three are tagged, so a fourth is not
        EXPECT_FALSE(setup.mChargen.toggleTag(40));
        // a skill that goes to another place leaves the first, and a number that is no skill is no tag
        EXPECT_TRUE(setup.mChargen.setTagSlot(0, 34));
        EXPECT_THAT(setup.mChargen.tags(), ElementsAre(34, 37));
        // the free place is taken by the next skill that is ticked, and a skill that moves leaves its old place
        EXPECT_TRUE(setup.mChargen.toggleTag(41));
        EXPECT_THAT(setup.mChargen.tags(), ElementsAre(34, 37, 41));
        EXPECT_TRUE(setup.mChargen.setTagSlot(2, 34));
        EXPECT_THAT(setup.mChargen.tags(), ElementsAre(34, 41));
        EXPECT_FALSE(setup.mChargen.setTagSlot(0, 3));
        EXPECT_FALSE(setup.mChargen.setTagSlot(-1, 34));
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptChargenCommandsTest, the_sex_of_the_player_is_changed_by_sex_change_on_the_player_only)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t player = builder.form(0x14);
        const std::uint16_t other = builder.form(0x01000200);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSexChange, arguments(2).add(Bytes().u16(1)).add(intArg(1)), other)
            + command(OpSexChange, arguments(0), player) + end());
        setup.run(builder);
        // neither a character that is not the player nor a call with no sex changes anything
        EXPECT_EQ(setup.mChargen.female(), std::nullopt);

        Runner again;
        ScriptBuilder female;
        const std::uint16_t you = female.form(0x14);
        female.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSexChange, arguments(2).add(Bytes().u16(1)).add(intArg(1)), you) + end());
        again.run(female);
        EXPECT_EQ(again.mChargen.female(), std::optional<bool>(true));

        Runner male;
        ScriptBuilder back;
        const std::uint16_t me = back.form(0x14);
        back.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpSexChange, arguments(2).add(Bytes().u16(1)).add(intArg(1)), me)
            + command(OpSexChange, arguments(2).add(Bytes().u16(0)).add(intArg(1)), me) + end());
        male.run(back);
        EXPECT_EQ(male.mChargen.female(), std::optional<bool>(false));
    }

    TEST(FalloutScriptChargenCommandsTest, the_parameters_of_a_menu_are_kept_with_it)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + command(OpShowSpecialBookParams, arguments(1).add(intArg(7))) + end());
        setup.run(builder);

        EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Special));
        EXPECT_THAT(setup.mChargen.values(), ElementsAre(7));
    }

    TEST(FalloutScriptChargenCommandsTest, the_trait_menu_opens_in_new_vegas_and_in_no_other_game)
    {
        {
            Runner setup(Game::NewVegas);
            ScriptBuilder builder;
            builder.code(scriptName() + begin(BlockType::OnActivate) + command(OpShowTraitMenu, arguments(0)) + end());
            setup.run(builder);
            EXPECT_EQ(setup.mChargen.page(), std::optional<ChargenPage>(ChargenPage::Traits));
        }
        {
            // the command is not in the table of Fallout 3, so the call is no command at all
            Runner setup(Game::Fallout3);
            EXPECT_EQ(setup.mCommands.find("ShowTraitMenu"), nullptr);
            setup.mChargen.open({ ChargenPage::Traits });
            EXPECT_FALSE(setup.mChargen.menuOpen());
        }
    }

    TEST(FalloutScriptChargenCommandsTest, the_flag_of_the_making_of_a_character_is_set_and_asked)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetInCharGen, Bytes()))
            + command(OpSetInChargen, arguments(1).add(intArg(1)))
            + setTo(var('s', 2), callToken(OpGetInCharGen, Bytes()))
            + command(OpSetInChargen, arguments(1).add(intArg(0)))
            + setTo(var('s', 3), callToken(OpGetInCharGen, Bytes())) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "during")
            .variable(3, integerVariable, "after");
        setup.run(builder);

        EXPECT_EQ(setup.mObjects.instance(crate)->variable(1), 0);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(2), 1);
        EXPECT_EQ(setup.mObjects.instance(crate)->variable(3), 0);
        EXPECT_FALSE(setup.mChargen.inChargen());
    }
}
