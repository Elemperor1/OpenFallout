#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/combatstate.hpp>
#include <components/falloutscript/equipment.hpp>
#include <components/falloutscript/formlists.hpp>
#include <components/falloutscript/itemuse.hpp>
#include <components/falloutscript/perks.hpp>
#include <components/falloutscript/playerprogress.hpp>
#include <components/falloutscript/referencestate.hpp>
#include <components/falloutscript/savecharacters.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::ActorState;
    using FalloutScript::ActorValues;
    using FalloutScript::CombatState;
    using FalloutScript::Equipment;
    using FalloutScript::FormId;
    using FalloutScript::FormLists;
    using FalloutScript::ItemInfo;
    using FalloutScript::ItemKind;
    using FalloutScript::ItemUse;
    using FalloutScript::PerkEngine;
    using FalloutScript::PerkInfo;
    using FalloutScript::PlayerProgress;
    using FalloutScript::ReferenceState;
    using FalloutScript::SaveError;
    using FalloutScript::SaveReader;
    using FalloutScript::SaveWriter;

    constexpr FormId player = 0x00000014;
    constexpr FormId guard = 0x01000100;
    constexpr FormId raider = 0x01000101;
    constexpr FormId fromGonePlugin = 0x05000200;
    constexpr FormId pistol = 0x0100A001;
    constexpr FormId cartridge = 0x0100A003;
    constexpr FormId vest = 0x0100B001;
    constexpr FormId stimpak = 0x0100C001;
    constexpr FormId mentats = 0x0100C002;
    constexpr FormId faction = 0x01000900;
    constexpr FormId otherFaction = 0x01000901;
    constexpr FormId weapon = 0x0100A002;
    constexpr FormId withdrawal = 0x01000100;
    constexpr FormId swiftLearner = 0x01000400;
    constexpr FormId swiftAbility = 0x01000401;
    constexpr FormId note = 0x01000500;
    constexpr FormId door = 0x01000600;
    constexpr FormId list = 0x01000700;
    constexpr FormId cube = 0x01000701;

    constexpr int strength = FalloutScript::ActorValue::strength;
    constexpr int luck = FalloutScript::ActorValue::luck;
    constexpr int health = FalloutScript::ActorValue::health;

    class Records : public FalloutScript::ActorRecords, public FalloutScript::ItemRecords
    {
    public:
        bool initialItems(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
        bool initialFactions(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
        bool itemInfo(FormId item, ItemInfo& info) override
        {
            switch (item)
            {
                case pistol:
                    info = { ItemKind::Weapon, 0, 5, 0, 0 };
                    return true;
                case cartridge:
                    info = { ItemKind::Ammo, 0, 0, 0, 0 };
                    return true;
                case vest:
                    info = { ItemKind::Armor, FalloutScript::BipedSlot::upperBody, 0, 20, 2.5 };
                    return true;
                default:
                    return false;
            }
        }
    };

    TEST(FalloutScriptSaveCharactersTest, what_scripts_gave_and_took_and_who_died_comes_back)
    {
        Records records;
        ActorState state(records);
        state.addItem(guard, pistol, 2);
        state.addItem(guard, stimpak, 5);
        EXPECT_EQ(state.removeItem(guard, stimpak, 3), 3);
        state.setFactionRank(guard, faction, 2);
        state.removeFromFaction(guard, otherFaction);
        state.addItem(raider, cartridge, 40);
        state.kill(raider, guard);
        // The deaths and item changes that nobody has asked for yet are kept too
        state.addItem(player, mentats, 1);

        // The same bytes are written again by what was read: the order is that of the keys
        ActorState loaded(records);
        SaveWriter out;
        FalloutScript::writeActors(state, out);
        SaveReader in(out.bytes());
        FalloutScript::readActors(loaded, in, 1);
        EXPECT_TRUE(in.atEnd());
        SaveWriter again;
        FalloutScript::writeActors(loaded, again);
        EXPECT_EQ(again.bytes(), out.bytes());
        EXPECT_EQ(loaded.itemCount(guard, pistol), 2);
        EXPECT_EQ(loaded.itemCount(guard, stimpak), 2);
        EXPECT_EQ(loaded.factionRank(guard, faction), 2);
        EXPECT_EQ(loaded.factionRank(guard, otherFaction), -1);
        EXPECT_TRUE(loaded.isDead(raider));
        EXPECT_EQ(loaded.killerOf(raider), guard);
        EXPECT_FALSE(loaded.isDead(guard));
        const std::vector<ActorState::Death> deaths = loaded.takeDeaths();
        ASSERT_EQ(deaths.size(), 1u);
        EXPECT_EQ(deaths[0].mActor, raider);
        EXPECT_EQ(deaths[0].mKiller, guard);
        EXPECT_FALSE(loaded.takeChanges().empty());
    }

    TEST(FalloutScriptSaveCharactersTest, what_the_save_says_of_a_plugin_that_is_gone_is_left_out)
    {
        Records records;
        ActorState state(records);
        state.addItem(guard, pistol, 1);
        state.addItem(fromGonePlugin, pistol, 1);
        state.addItem(guard, fromGonePlugin, 1);
        SaveWriter out;
        FalloutScript::writeActors(state, out);

        ActorState loaded(records);
        SaveReader in(out.bytes(), [](FormId saved) -> std::optional<FormId> {
            if ((saved >> 24) == 5)
                return std::nullopt;
            return saved;
        });
        FalloutScript::readActors(loaded, in, 1);
        EXPECT_EQ(loaded.itemCount(guard, pistol), 1);
        EXPECT_EQ(loaded.itemCount(fromGonePlugin, pistol), 0);
        EXPECT_EQ(loaded.itemCount(guard, fromGonePlugin), 0);
    }

    TEST(FalloutScriptSaveCharactersTest, what_a_character_holds_and_wears_comes_back)
    {
        Records records;
        ActorState state(records);
        Equipment equipment(records, state);
        for (const FormId item : { pistol, cartridge, vest })
            state.addItem(guard, item, 1);
        EXPECT_EQ(equipment.equip(guard, pistol), Equipment::Outcome::Equipped);
        EXPECT_EQ(equipment.equip(guard, cartridge), Equipment::Outcome::Equipped);
        EXPECT_EQ(equipment.equip(guard, vest), Equipment::Outcome::Equipped);
        equipment.setWeaponDrawn(guard, true);

        // The items are those of the actors section, which is read first
        SaveWriter items;
        FalloutScript::writeActors(state, items);
        SaveWriter out;
        FalloutScript::writeEquipment(equipment, out);
        ActorState otherState(records);
        Equipment loaded(records, otherState);
        SaveReader itemsIn(items.bytes());
        FalloutScript::readActors(otherState, itemsIn, 1);
        SaveReader in(out.bytes());
        FalloutScript::readEquipment(loaded, in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.weapon(guard), pistol);
        EXPECT_EQ(loaded.ammo(guard), cartridge);
        EXPECT_THAT(loaded.armor(guard), ElementsAre(vest));
        EXPECT_TRUE(loaded.weaponDrawn(guard));
        SaveWriter again;
        FalloutScript::writeEquipment(loaded, again);
        EXPECT_EQ(again.bytes(), out.bytes());
    }

    TEST(FalloutScriptSaveCharactersTest, the_actor_values_that_scripts_changed_come_back)
    {
        ActorValues values;
        values.setBase(guard, strength, 8);
        values.modify(guard, strength, 2);
        values.damage(player, health, 30);
        values.setBase(player, luck, 7);
        const double current = values.current(guard, strength);

        ActorValues loaded;
        SaveWriter out;
        FalloutScript::writeActorValues(values, out);
        SaveReader in(out.bytes());
        FalloutScript::readActorValues(loaded, in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.current(guard, strength), current);
        EXPECT_EQ(loaded.base(guard, strength), 8);
        EXPECT_EQ(loaded.current(player, health), values.current(player, health));
        EXPECT_EQ(loaded.base(player, luck), 7);
        SaveWriter again;
        FalloutScript::writeActorValues(loaded, again);
        EXPECT_EQ(again.bytes(), out.bytes());
    }

    class Perks final : public FalloutScript::PerkRecords
    {
    public:
        std::map<FormId, PerkInfo> mPerks;
        std::map<FormId, std::vector<FalloutScript::AbilityEffect>> mAbilities;

        bool perk(FormId id, PerkInfo& info) override
        {
            const auto found = mPerks.find(id);
            if (found == mPerks.end())
                return false;
            info = found->second;
            return true;
        }
        std::vector<FormId> perks() override
        {
            std::vector<FormId> all;
            for (const auto& [id, info] : mPerks)
                all.push_back(id);
            return all;
        }
        bool abilityEffects(FormId ability, std::vector<FalloutScript::AbilityEffect>& effects) override
        {
            const auto found = mAbilities.find(ability);
            if (found == mAbilities.end())
                return false;
            effects = found->second;
            return true;
        }
    };

    struct Characters
    {
        Perks mRecords;
        PlayerProgress mProgress;
        ActorValues mValues;
        PerkEngine mEngine{ mRecords, mProgress, mValues };

        Characters()
        {
            PerkInfo info;
            info.mId = swiftLearner;
            info.mName = "Swift Learner";
            info.mRanks = 1;
            info.mPlayable = true;
            FalloutScript::PerkEntry entry;
            entry.mType = FalloutScript::PerkEntry::Ability;
            entry.mAbility = swiftAbility;
            entry.mRank = 0;
            info.mEntries = { entry };
            mRecords.mPerks[swiftLearner] = info;
            mRecords.mAbilities[swiftAbility] = { { strength, 2.0 } };
        }
    };

    TEST(FalloutScriptSaveCharactersTest, the_progress_of_the_player_and_its_perks_come_back_and_are_not_added_twice)
    {
        Characters game;
        game.mProgress.rewardExperience(1234);
        game.mProgress.rewardKarma(-250);
        game.mProgress.addSkillPoints(7);
        game.mProgress.modifyMiscStat(3, 4);
        game.mProgress.addNote(note);
        game.mProgress.addPerk(player, swiftLearner);
        game.mEngine.refresh(player);
        const double boosted = game.mValues.current(player, strength);
        EXPECT_EQ(boosted, ActorValues::startingValue(strength) + 2);

        SaveWriter progress;
        SaveWriter values;
        SaveWriter perks;
        FalloutScript::writeProgress(game.mProgress, progress);
        FalloutScript::writeActorValues(game.mValues, values);
        FalloutScript::writePerks(game.mEngine, game.mValues, perks);

        // The order of the sections: values, progress, perks
        Characters loaded;
        SaveReader valuesIn(values.bytes());
        FalloutScript::readActorValues(loaded.mValues, valuesIn, 1);
        SaveReader progressIn(progress.bytes());
        FalloutScript::readProgress(loaded.mProgress, progressIn, 1);
        SaveReader perksIn(perks.bytes());
        FalloutScript::readPerks(loaded.mEngine, loaded.mValues, perksIn, 1);
        EXPECT_TRUE(valuesIn.atEnd() && progressIn.atEnd() && perksIn.atEnd());

        EXPECT_EQ(loaded.mProgress.experience(), 1234);
        EXPECT_EQ(loaded.mProgress.karma(), -250);
        EXPECT_EQ(loaded.mProgress.skillPoints(), 7);
        EXPECT_EQ(loaded.mProgress.miscStat(3), 4);
        EXPECT_TRUE(loaded.mProgress.hasNote(note));
        EXPECT_EQ(loaded.mProgress.perkRank(player, swiftLearner), 1);
        EXPECT_EQ(loaded.mValues.current(player, strength), boosted);
        // The next refresh sees the perk applied and adds nothing
        loaded.mEngine.refresh(player);
        EXPECT_EQ(loaded.mValues.current(player, strength), boosted);

        SaveWriter again;
        FalloutScript::writePerks(loaded.mEngine, loaded.mValues, again);
        EXPECT_EQ(again.bytes(), perks.bytes());
    }

    TEST(FalloutScriptSaveCharactersTest, a_perk_that_is_not_what_it_was_has_its_old_effects_taken_back)
    {
        Characters game;
        game.mProgress.addPerk(player, swiftLearner);
        game.mEngine.refresh(player);
        SaveWriter values;
        SaveWriter progress;
        SaveWriter perks;
        FalloutScript::writeActorValues(game.mValues, values);
        FalloutScript::writeProgress(game.mProgress, progress);
        FalloutScript::writePerks(game.mEngine, game.mValues, perks);

        // A plugin changed the ability: it gives 5 now
        Characters loaded;
        loaded.mRecords.mAbilities[swiftAbility] = { { strength, 5.0 } };
        SaveReader valuesIn(values.bytes());
        FalloutScript::readActorValues(loaded.mValues, valuesIn, 1);
        SaveReader progressIn(progress.bytes());
        FalloutScript::readProgress(loaded.mProgress, progressIn, 1);
        SaveReader perksIn(perks.bytes());
        FalloutScript::readPerks(loaded.mEngine, loaded.mValues, perksIn, 1);
        loaded.mEngine.refresh(player);
        EXPECT_EQ(loaded.mValues.current(player, strength), ActorValues::startingValue(strength) + 5);
    }

    TEST(FalloutScriptSaveCharactersTest, who_fights_whom_and_who_is_down_comes_back)
    {
        CombatState combat;
        combat.start(guard, raider);
        combat.setAlert(raider, true);
        combat.setUnconscious(player, true, 30);
        combat.setRestrained(raider, true);
        combat.setEssential(guard, true);
        combat.setRelation(guard, raider, CombatState::Relation::Enemy);
        combat.setIgnoreFriendlyHits(guard, true);
        CombatState::Hit hit;
        hit.mAttacker = guard;
        hit.mWeapon = weapon;
        hit.mDamage = 12.5;
        hit.mCritical = true;
        hit.mLimb = 2;
        combat.noteHit(raider, hit);

        CombatState loaded;
        SaveWriter out;
        FalloutScript::writeCombat(combat, out);
        SaveReader in(out.bytes());
        FalloutScript::readCombat(loaded, in, 1);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.target(guard), raider);
        EXPECT_TRUE(loaded.alert(raider));
        EXPECT_TRUE(loaded.unconscious(player));
        EXPECT_TRUE(loaded.restrained(raider));
        EXPECT_THAT(loaded.essential(guard), Optional(true));
        EXPECT_THAT(loaded.relation(guard, raider), Optional(CombatState::Relation::Enemy));
        EXPECT_TRUE(loaded.ignoresFriendlyHits(guard));
        CombatState::Hit last;
        ASSERT_TRUE(loaded.lastHit(raider, last));
        EXPECT_EQ(last.mAttacker, guard);
        EXPECT_EQ(last.mDamage, 12.5);
        EXPECT_TRUE(last.mCritical);
        EXPECT_EQ(last.mLimb, 2);
        SaveWriter again;
        FalloutScript::writeCombat(loaded, again);
        EXPECT_EQ(again.bytes(), out.bytes());
        EXPECT_EQ(loaded.takeHits().size(), 1u);
    }

    class Uses final : public FalloutScript::UseRecords
    {
    public:
        std::map<FormId, FalloutScript::Consumable> mConsumables;

        bool consumable(FormId item, FalloutScript::Consumable& result) override
        {
            const auto found = mConsumables.find(item);
            if (found == mConsumables.end())
                return false;
            result = found->second;
            return true;
        }
        bool book(FormId, FalloutScript::BookInfo&) override { return false; }
    };

    TEST(FalloutScriptSaveCharactersTest, the_chems_that_are_running_and_the_addictions_come_back)
    {
        Records records;
        Uses uses;
        FalloutScript::ConsumableEffect effect;
        effect.mActorValue = strength;
        effect.mMagnitude = 5;
        effect.mDuration = 240;
        effect.mRecover = true;
        uses.mConsumables[mentats].mEffects = { effect };
        uses.mConsumables[mentats].mWithdrawal = withdrawal;
        uses.mConsumables[mentats].mAddictionChance = 1;
        ActorState state(records);
        ActorValues values;
        PlayerProgress progress;
        ItemUse use(uses, state, values, progress);
        use.setPlayer(player);
        use.setRandom([] { return 0.0; });
        state.addItem(player, mentats, 2);
        ASSERT_EQ(use.use(player, mentats).mOutcome, ItemUse::Outcome::Consumed);
        use.update(60);
        ASSERT_THAT(use.active(player), SizeIs(1));
        ASSERT_THAT(use.addictions(player), ElementsAre(mentats));

        SaveWriter out;
        FalloutScript::writeItemUse(use, out);
        ActorState otherState(records);
        ActorValues otherValues;
        PlayerProgress otherProgress;
        ItemUse loaded(uses, otherState, otherValues, otherProgress);
        SaveReader in(out.bytes());
        FalloutScript::readItemUse(loaded, in, 1);
        EXPECT_TRUE(in.atEnd());
        const std::vector<ItemUse::Active> active = loaded.active(player);
        ASSERT_THAT(active, SizeIs(1));
        EXPECT_EQ(active[0].mActorValue, strength);
        EXPECT_NEAR(active[0].mRemaining, 180, 1e-6);
        EXPECT_THAT(loaded.addictions(player), ElementsAre(mentats));
        SaveWriter again;
        FalloutScript::writeItemUse(loaded, again);
        EXPECT_EQ(again.bytes(), out.bytes());
    }

    class Locks final : public FalloutScript::ReferenceRecords
    {
    public:
        bool initialLock(FormId, bool& locked, int& level) override
        {
            locked = false;
            level = 0;
            return true;
        }
    };

    class Lists final : public FalloutScript::FormListRecords
    {
    public:
        bool formListEntries(FormId id, std::vector<FormId>& entries) override
        {
            entries.clear();
            return id == list;
        }
        bool baseFormOf(FormId, FormId&) override { return false; }
    };

    TEST(FalloutScriptSaveCharactersTest, locks_destroyed_references_and_form_lists_come_back)
    {
        Locks locks;
        ReferenceState state(locks);
        state.lock(door, 50);
        state.setDestroyed(cube, true);
        Lists records;
        FormLists lists(records);
        lists.add(list, cube);
        lists.add(list, door);

        ReferenceState loadedState(locks);
        SaveWriter stateOut;
        FalloutScript::writeReferenceState(state, stateOut);
        SaveReader stateIn(stateOut.bytes());
        FalloutScript::readReferenceState(loadedState, stateIn, 1);
        EXPECT_TRUE(stateIn.atEnd());
        EXPECT_TRUE(loadedState.isLocked(door));
        EXPECT_EQ(loadedState.lockLevel(door), 50);
        EXPECT_TRUE(loadedState.isDestroyed(cube));
        EXPECT_FALSE(loadedState.isDestroyed(door));

        FormLists loadedLists(records);
        SaveWriter listOut;
        FalloutScript::writeFormLists(lists, listOut);
        SaveReader listIn(listOut.bytes());
        FalloutScript::readFormLists(loadedLists, listIn, 1);
        EXPECT_TRUE(listIn.atEnd());
        EXPECT_TRUE(loadedLists.contains(list, cube));
        EXPECT_TRUE(loadedLists.contains(list, door));
        EXPECT_FALSE(loadedLists.contains(list, guard));
    }

    TEST(FalloutScriptSaveCharactersTest, a_section_that_is_cut_short_is_an_error_and_changes_nothing)
    {
        Records records;
        ActorState state(records);
        state.addItem(guard, pistol, 2);
        state.setFactionRank(guard, faction, 3);
        state.kill(raider, guard);
        SaveWriter out;
        FalloutScript::writeActors(state, out);
        const std::string bytes = out.bytes();

        ActorState loaded(records);
        loaded.addItem(player, stimpak, 4);
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            SaveReader in(std::string_view(bytes).substr(0, size));
            EXPECT_THROW(FalloutScript::readActors(loaded, in, 1), SaveError) << size;
        }
        // What the failed reads did not touch is what the load started with
        EXPECT_EQ(loaded.itemCount(player, stimpak), 4);
        EXPECT_EQ(loaded.itemCount(guard, pistol), 0);
        EXPECT_FALSE(loaded.isDead(raider));
    }
}
