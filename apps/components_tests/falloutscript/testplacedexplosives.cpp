#include <components/falloutscript/placedexplosives.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace
{
    namespace EX = FalloutScript::Explosives;

    EX::Kind mine()
    {
        EX::Kind kind;
        kind.mAltTrigger = true;
        kind.mCanBeDisabled = true;
        kind.mHasExplosion = true;
        kind.mProximity = 100;
        kind.mFuseTimer = 2.0;
        return kind;
    }

    EX::Kind charge()
    {
        EX::Kind kind;
        kind.mDetonates = true;
        kind.mHasExplosion = true;
        return kind;
    }

    EX::Kind timed()
    {
        EX::Kind kind;
        kind.mAltTrigger = true;
        kind.mHasExplosion = true;
        kind.mFuseTimer = 1.5;
        return kind;
    }

    EX::Placed laid(const EX::Kind& kind, std::uint32_t owner = 0x7000, bool exterior = false)
    {
        EX::Placed placed;
        placed.mOwner = owner;
        placed.mProjectile = 0x500;
        placed.mKind = kind;
        placed.mExterior = exterior;
        return placed;
    }

    EX::Surroundings nobody()
    {
        EX::Surroundings around;
        around.character = [](const EX::Placed&, double) { return std::nullopt; };
        around.player = [](const EX::Placed&, double) { return false; };
        around.playerChance = [](const EX::Placed&) { return 100.0; };
        around.roll = [] { return 0; };
        around.skill = [] { return 50.0; };
        return around;
    }

    TEST(FalloutScriptExplosivesTest, theSettingsOfTheFilesChangeTheRules)
    {
        const std::map<std::string, double> settings{ { "fMineExteriorRadiusMult", 2.0 }, { "fMinesDelayMin", 0.5 },
            { "fMineAgeMax", 30 }, { "iProjectileMineShooterCanTrigger", 0 }, { "iMineDisarmExperience", 9 } };
        const EX::Rules rules = EX::rules([&settings](std::string_view name) -> std::optional<double> {
            const auto found = settings.find(std::string(name));
            return found == settings.end() ? std::nullopt : std::optional<double>(found->second);
        });
        EXPECT_DOUBLE_EQ(rules.mExteriorRadiusMult, 2.0);
        EXPECT_DOUBLE_EQ(rules.mDelayMin, 0.5);
        EXPECT_DOUBLE_EQ(rules.mMineAgeMax, 30);
        EXPECT_DOUBLE_EQ(rules.mGrenadeAgeMax, 90.0);
        EXPECT_FALSE(rules.mShooterCanTrigger);
        EXPECT_EQ(rules.mDisarmExperience, 9);
        const EX::Rules none = EX::rules({});
        EXPECT_DOUBLE_EQ(none.mMineAgeMax, 0.0);
        EXPECT_TRUE(none.mShooterCanTrigger);
    }

    TEST(FalloutScriptExplosivesTest, aMineIsAnAltTriggerWithProximityOrADetonator)
    {
        EXPECT_TRUE(EX::isMine(mine()));
        EXPECT_TRUE(EX::isMine(charge()));
        EXPECT_FALSE(EX::isMine(timed()));
        EX::Kind plain;
        EXPECT_FALSE(EX::isMine(plain));
        EX::Kind altNoProximity;
        altNoProximity.mAltTrigger = true;
        EXPECT_FALSE(EX::isMine(altNoProximity));
    }

    TEST(FalloutScriptExplosivesTest, theAgeLimitIsTheMineOrTheGrenadeOne)
    {
        EX::Rules rules;
        rules.mMineAgeMax = 0;
        rules.mGrenadeAgeMax = 90;
        EXPECT_DOUBLE_EQ(EX::ageLimit(rules, mine()), 0.0);
        EXPECT_DOUBLE_EQ(EX::ageLimit(rules, charge()), 0.0);
        EXPECT_DOUBLE_EQ(EX::ageLimit(rules, timed()), 90.0);
        rules.mMineAgeMax = 20;
        EXPECT_DOUBLE_EQ(EX::ageLimit(rules, mine()), 20.0);
    }

    TEST(FalloutScriptExplosivesTest, whoSetsAMineOff)
    {
        EX::Rules rules;
        rules.mShooterCanTrigger = true;
        EX::Layer nobodyLaid;
        EXPECT_TRUE(EX::triggers(rules, 0x100, nobodyLaid));
        EX::Layer layer;
        layer.mActor = 0x7000;
        // with no reaction known the layer's own is not asked
        EXPECT_TRUE(EX::triggers(rules, 0x7000, layer));
        rules.mShooterCanTrigger = false;
        EXPECT_FALSE(EX::triggers(rules, 0x7000, layer));
        EXPECT_TRUE(EX::triggers(rules, 0x7001, layer));
        // the faction of the mine is spared
        layer.mFactionMember = true;
        EXPECT_FALSE(EX::triggers(rules, 0x7001, layer));
        layer.mFactionMember = false;
        for (const auto& [reaction, sets] : std::map<int, bool>{ { EX::Neutral, true }, { EX::Enemy, true },
                 { EX::Ally, false }, { EX::Friend, false } })
        {
            layer.mReaction = reaction;
            EXPECT_EQ(EX::triggers(rules, 0x7001, layer), sets) << reaction;
        }
    }

    TEST(FalloutScriptExplosivesTest, thePlayerGetsTheTimeThatItsSkillBuys)
    {
        EX::Rules rules;
        rules.mDelayMin = 0.2;
        EXPECT_DOUBLE_EQ(EX::triggerDelay(rules, mine(), false, 100), 0.2);
        EXPECT_DOUBLE_EQ(EX::triggerDelay(rules, mine(), true, 0), 0.2);
        EXPECT_DOUBLE_EQ(EX::triggerDelay(rules, mine(), true, 50), 0.2 + 50 * 2.0 / 100.0);
        EXPECT_DOUBLE_EQ(EX::triggerDelay(rules, mine(), true, 50.9), 0.2 + 50 * 2.0 / 100.0);
    }

    TEST(FalloutScriptExplosivesTest, aMineWaitsAndATimedChargeCounts)
    {
        EX::Laid laidOnes;
        const std::uint32_t a = laidOnes.plant(laid(mine()));
        const std::uint32_t b = laidOnes.plant(laid(timed()));
        EXPECT_NE(a, b);
        EXPECT_EQ(laidOnes.find(a)->mTimer, EX::waiting);
        EXPECT_DOUBLE_EQ(laidOnes.find(b)->mTimer, 1.5);
        EXPECT_EQ(laidOnes.find(99), nullptr);
    }

    TEST(FalloutScriptExplosivesTest, aMineThatNobodyComesNearStays)
    {
        EX::Laid laidOnes;
        laidOnes.plant(laid(mine()));
        const EX::Rules rules;
        for (int i = 0; i < 5; ++i)
            EXPECT_TRUE(laidOnes.update(rules, 1.0, nobody()).empty());
        EXPECT_EQ(laidOnes.count(), 1u);
    }

    TEST(FalloutScriptExplosivesTest, aCharacterThatComesNearSetsItOffAfterTheDelay)
    {
        EX::Laid laidOnes;
        const std::uint32_t id = laidOnes.plant(laid(mine()));
        EX::Rules rules;
        rules.mDelayMin = 0.5;
        double asked = 0;
        EX::Surroundings around = nobody();
        around.character = [&asked](const EX::Placed&, double radius) -> std::optional<std::uint32_t> {
            asked = radius;
            return 0x8000u;
        };
        // inside the radius is the mine's, not multiplied: this one lies in an interior
        EXPECT_TRUE(laidOnes.update(rules, 0.25, around).empty());
        EXPECT_DOUBLE_EQ(asked, 100.0);
        EXPECT_DOUBLE_EQ(laidOnes.find(id)->mTimer, 0.25);
        const std::vector<EX::Event> events = laidOnes.update(rules, 0.25, around);
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mKind, EX::Event::Kind::Explode);
        EXPECT_EQ(events[0].mId, id);
        EXPECT_EQ(events[0].mCause, 0x8000u);
        EXPECT_EQ(laidOnes.count(), 0u);
    }

    TEST(FalloutScriptExplosivesTest, theRadiusIsLargerOutside)
    {
        EX::Laid laidOnes;
        laidOnes.plant(laid(mine(), 0x7000, true));
        EX::Rules rules;
        rules.mExteriorRadiusMult = 1.6;
        double asked = 0;
        EX::Surroundings around = nobody();
        around.character = [&asked](const EX::Placed&, double radius) -> std::optional<std::uint32_t> {
            asked = radius;
            return std::nullopt;
        };
        laidOnes.update(rules, 0.1, around);
        EXPECT_DOUBLE_EQ(asked, 160.0);
    }

    TEST(FalloutScriptExplosivesTest, thePlayerIsRolledAgainstTheChanceAndSparedForGood)
    {
        EX::Laid laidOnes;
        const std::uint32_t id = laidOnes.plant(laid(mine()));
        EX::Rules rules;
        rules.mDelayMin = 0.2;
        int roll = 80;
        double chance = 50;
        EX::Surroundings around = nobody();
        around.player = [](const EX::Placed&, double) { return true; };
        around.playerChance = [&chance](const EX::Placed&) { return chance; };
        around.roll = [&roll] { return roll; };
        around.skill = [] { return 50.0; };
        // a roll of 80 against 50 lets the player pass, and the mine remembers it
        EXPECT_TRUE(laidOnes.update(rules, 0.1, around).empty());
        EXPECT_NE(laidOnes.find(id)->mState & EX::statePlayerSpared, 0u);
        roll = 0;
        EXPECT_TRUE(laidOnes.update(rules, 0.1, around).empty());
        EXPECT_EQ(laidOnes.find(id)->mTimer, EX::waiting);
        // another mine, a roll below the chance: set off, with the player's delay (0.2 + 50 * 2 / 100 = 1.2)
        EX::Laid second;
        const std::uint32_t other = second.plant(laid(mine()));
        EXPECT_TRUE(second.update(rules, 0.1, around).empty());
        EXPECT_NEAR(second.find(other)->mTimer, 1.2 - 0.1, 1e-9);
        std::vector<EX::Event> events;
        for (int i = 0; i < 20 && events.empty(); ++i)
            events = second.update(rules, 0.1, around);
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mCause, 0x14u);
    }

    TEST(FalloutScriptExplosivesTest, aCharacterIsLookedAtBeforeThePlayer)
    {
        EX::Laid laidOnes;
        const std::uint32_t id = laidOnes.plant(laid(mine()));
        bool askedPlayer = false;
        EX::Surroundings around = nobody();
        around.character = [](const EX::Placed&, double) { return std::optional<std::uint32_t>(0x8000u); };
        around.player = [&askedPlayer](const EX::Placed&, double) {
            askedPlayer = true;
            return true;
        };
        laidOnes.update(EX::Rules(), 0.01, around);
        EXPECT_FALSE(askedPlayer);
        EXPECT_DOUBLE_EQ(laidOnes.find(id)->mTimer, EX::Rules().mDelayMin - 0.01);
    }

    TEST(FalloutScriptExplosivesTest, aTimedChargeGoesOffWhenItsTimerRunsOut)
    {
        EX::Laid laidOnes;
        laidOnes.plant(laid(timed()));
        const EX::Rules rules;
        EXPECT_TRUE(laidOnes.update(rules, 1.0, nobody()).empty());
        const std::vector<EX::Event> events = laidOnes.update(rules, 0.6, nobody());
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mKind, EX::Event::Kind::Explode);
        EXPECT_EQ(events[0].mCause, 0u);
    }

    TEST(FalloutScriptExplosivesTest, aDetonatorWaitsForTheScript)
    {
        EX::Laid laidOnes;
        const std::uint32_t mine1 = laidOnes.plant(laid(charge(), 0x14));
        laidOnes.plant(laid(charge(), 0x9000));
        laidOnes.plant(laid(mine(), 0x14));
        const EX::Rules rules;
        for (int i = 0; i < 3; ++i)
            EXPECT_TRUE(laidOnes.update(rules, 1.0, nobody()).empty());
        // only the explosives of the one that asks, whose record detonates, are told
        EXPECT_EQ(laidOnes.detonate(0x14), 1);
        const std::vector<EX::Event> events = laidOnes.update(rules, 0.1, nobody());
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mId, mine1);
        EXPECT_EQ(laidOnes.count(), 2u);
        EXPECT_EQ(laidOnes.detonate(0x555), 0);
    }

    TEST(FalloutScriptExplosivesTest, aRecordThatDetonatesAndHasTheAltTriggerGoesOffWhenTold)
    {
        EX::Kind both = mine();
        both.mDetonates = true;
        EX::Laid laidOnes;
        const std::uint32_t id = laidOnes.plant(laid(both, 0x14));
        const EX::Rules rules;
        // nobody comes near: the mine waits
        EXPECT_TRUE(laidOnes.update(rules, 1.0, nobody()).empty());
        EXPECT_EQ(laidOnes.detonate(0x14), 1);
        const std::vector<EX::Event> events = laidOnes.update(rules, 0.1, nobody());
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mId, id);
        EXPECT_EQ(events[0].mKind, EX::Event::Kind::Explode);
    }

    TEST(FalloutScriptExplosivesTest, aBlastThatDoesNoDamageIsKeptInASave)
    {
        EX::Laid laidOnes;
        EX::Placed harmless = laid(mine(), 0x14);
        harmless.mNoDamage = true;
        laidOnes.plant(harmless);
        laidOnes.plant(laid(mine(), 0x14));
        FalloutScript::SaveWriter out;
        EX::writeExplosives(laidOnes, out);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        EX::Laid back;
        EX::readExplosives(back, in, EX::explosivesSectionVersion);
        ASSERT_EQ(back.count(), 2u);
        EXPECT_TRUE(back.all()[0].mNoDamage);
        EXPECT_FALSE(back.all()[1].mNoDamage);
    }

    TEST(FalloutScriptExplosivesTest, disablingAllMinesSwitchesOffTheMinesThatCanBe)
    {
        EX::Laid laidOnes;
        const std::uint32_t armed = laidOnes.plant(laid(mine()));
        EX::Kind fixed = mine();
        fixed.mCanBeDisabled = false;
        const std::uint32_t fixedOne = laidOnes.plant(laid(fixed));
        laidOnes.plant(laid(timed()));
        EXPECT_EQ(laidOnes.disableAllMines(), 1);
        EXPECT_NE(laidOnes.find(armed)->mState & EX::stateDisabled, 0u);
        EXPECT_EQ(laidOnes.find(fixedOne)->mState & EX::stateDisabled, 0u);
        EXPECT_EQ(laidOnes.disableAllMines(), 0);
        // a disabled mine does not go off
        EX::Surroundings around = nobody();
        around.character = [](const EX::Placed&, double) { return std::optional<std::uint32_t>(0x8000u); };
        EX::Rules rules;
        rules.mDelayMin = 0.0;
        const std::vector<EX::Event> events = laidOnes.update(rules, 0.1, around);
        for (const EX::Event& event : events)
            EXPECT_NE(event.mId, armed);
        EXPECT_NE(laidOnes.find(armed), nullptr);
    }

    TEST(FalloutScriptExplosivesTest, aDisarmGivesExperienceToThePlayerOnlyWhenItIsNotSilent)
    {
        EX::Rules rules;
        rules.mDisarmExperience = 5;
        EX::Laid laidOnes;
        const std::uint32_t id = laidOnes.plant(laid(mine()));
        EX::Laid::Disarm silent = laidOnes.disable(rules, id, true, true, true);
        EXPECT_TRUE(silent.mDisabled);
        EXPECT_EQ(silent.mExperience, 0);
        // not twice
        EXPECT_FALSE(laidOnes.disable(rules, id, false, true, true).mDisabled);
        const std::uint32_t next = laidOnes.plant(laid(mine()));
        const EX::Laid::Disarm loud = laidOnes.disable(rules, next, false, true, false);
        EXPECT_TRUE(loud.mDisabled);
        EXPECT_EQ(loud.mExperience, 5);
        const std::uint32_t third = laidOnes.plant(laid(mine()));
        EXPECT_EQ(laidOnes.disable(rules, third, false, false, false).mExperience, 0);
        EX::Kind fixed = mine();
        fixed.mCanBeDisabled = false;
        const std::uint32_t fourth = laidOnes.plant(laid(fixed));
        EXPECT_FALSE(laidOnes.disable(rules, fourth, false, true, true).mDisabled);
        EXPECT_FALSE(laidOnes.disable(rules, 999, false, true, true).mDisabled);
        // a mine that is already counting down gives nothing to one that would not have set it off
        const std::uint32_t fifth = laidOnes.plant(laid(mine()));
        EX::Surroundings around = nobody();
        around.character = [](const EX::Placed&, double) { return std::optional<std::uint32_t>(0x8000u); };
        laidOnes.update(rules, 0.01, around);
        EXPECT_EQ(laidOnes.disable(rules, fifth, false, true, false).mExperience, 0);
    }

    TEST(FalloutScriptExplosivesTest, aProjectileThatIsOldEnoughIsTakenAway)
    {
        EX::Laid laidOnes;
        laidOnes.plant(laid(timed()));
        EX::Rules rules;
        rules.mGrenadeAgeMax = 1.0;
        // (the charge would go off at 1.5)
        const std::vector<EX::Event> events = laidOnes.update(rules, 1.0, nobody());
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mKind, EX::Event::Kind::Expire);
        EXPECT_EQ(laidOnes.count(), 0u);
    }

    TEST(FalloutScriptExplosivesTest, aProjectileWithNoExplosionNeverGoesOff)
    {
        EX::Kind kind = timed();
        kind.mHasExplosion = false;
        EX::Laid laidOnes;
        laidOnes.plant(laid(kind));
        EXPECT_TRUE(laidOnes.update(EX::Rules(), 5.0, nobody()).empty());
        EXPECT_EQ(laidOnes.count(), 1u);
    }

    TEST(FalloutScriptExplosivesTest, theStateIsSaved)
    {
        EX::Laid laidOnes;
        EX::Placed placed = laid(mine(), 0x14, true);
        placed.mPosition[0] = 1.5f;
        placed.mPosition[1] = -2.5f;
        placed.mPosition[2] = 30.f;
        placed.mCell = 0x1234;
        laidOnes.plant(placed);
        laidOnes.plant(laid(charge()));
        laidOnes.detonate(0x7000);
        FalloutScript::SaveWriter out;
        EX::writeExplosives(laidOnes, out);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        EX::Laid back;
        EX::readExplosives(back, in, EX::explosivesSectionVersion);
        ASSERT_EQ(back.count(), 2u);
        EXPECT_EQ(back.next(), laidOnes.next());
        EXPECT_EQ(back.all()[0].mOwner, 0x14u);
        EXPECT_FLOAT_EQ(back.all()[0].mPosition[1], -2.5f);
        EXPECT_EQ(back.all()[0].mCell, 0x1234u);
        EXPECT_TRUE(back.all()[0].mExterior);
        EXPECT_TRUE(back.all()[0].mKind.mAltTrigger);
        EXPECT_DOUBLE_EQ(back.all()[0].mKind.mProximity, 100);
        EXPECT_EQ(back.all()[0].mTimer, EX::waiting);
        EXPECT_NE(back.all()[1].mState & EX::stateDetonate, 0u);
        FalloutScript::SaveReader wrong(bytes);
        EX::Laid other;
        EXPECT_THROW(EX::readExplosives(other, wrong, 99), FalloutScript::SaveError);
    }

    TEST(FalloutScriptExplosivesTest, clearForgetsEverythingAndRestartsTheNumbers)
    {
        EX::Laid laidOnes;
        laidOnes.plant(laid(mine()));
        laidOnes.clear();
        EXPECT_EQ(laidOnes.count(), 0u);
        EXPECT_EQ(laidOnes.plant(laid(mine())), 1u);
    }
    TEST(FalloutScriptExplosivesTest, theEventKeepsWhatTheProjectileWasAndWhoSetItOff)
    {
        EX::Laid laidOnes;
        EX::Placed placed = laid(mine(), 0x14);
        placed.mPosition[0] = 10.f;
        placed.mCell = 0x77;
        laidOnes.plant(placed);
        EX::Rules rules;
        rules.mDelayMin = 0.0;
        EX::Surroundings around = nobody();
        around.character = [](const EX::Placed&, double) { return std::optional<std::uint32_t>(0x8000u); };
        const std::vector<EX::Event> events = laidOnes.update(rules, 0.1, around);
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mPlaced.mOwner, 0x14u);
        EXPECT_FLOAT_EQ(events[0].mPlaced.mPosition[0], 10.f);
        EXPECT_EQ(events[0].mPlaced.mCell, 0x77u);
        EXPECT_EQ(events[0].mCause, 0x8000u);
    }

    TEST(FalloutScriptExplosivesTest, whoSetAMineOffSurvivesASave)
    {
        EX::Laid laidOnes;
        laidOnes.plant(laid(mine()));
        EX::Rules rules;
        rules.mDelayMin = 5.0;
        EX::Surroundings around = nobody();
        around.character = [](const EX::Placed&, double) { return std::optional<std::uint32_t>(0x8001u); };
        EXPECT_TRUE(laidOnes.update(rules, 0.5, around).empty());
        FalloutScript::SaveWriter out;
        EX::writeExplosives(laidOnes, out);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        EX::Laid back;
        EX::readExplosives(back, in, EX::explosivesSectionVersion);
        ASSERT_EQ(back.count(), 1u);
        EXPECT_EQ(back.all()[0].mCause, 0x8001u);
        // the counting goes on where it was: the people who were near are not asked again
        std::vector<EX::Event> events;
        for (int i = 0; i < 12 && events.empty(); ++i)
            events = back.update(rules, 0.5, nobody());
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].mCause, 0x8001u);
    }

    TEST(FalloutScriptExplosivesTest, aSaveWithAnEntryThatIsCutShortIsRefused)
    {
        EX::Laid laidOnes;
        laidOnes.plant(laid(mine()));
        FalloutScript::SaveWriter out;
        EX::writeExplosives(laidOnes, out);
        std::string bytes = out.take();
        bytes.resize(bytes.size() - 10);
        FalloutScript::SaveReader in(bytes);
        EX::Laid back;
        EXPECT_THROW(EX::readExplosives(back, in, EX::explosivesSectionVersion), FalloutScript::SaveError);
    }
}
