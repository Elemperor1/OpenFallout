#include "saveloadharness.hpp"

#include <components/esm/attr.hpp>
#include <components/esm3/aisequence.hpp>
#include <components/esm3/cellstate.hpp>
#include <components/esm3/containerstate.hpp>
#include <components/esm3/creaturelevliststate.hpp>
#include <components/esm3/creaturestate.hpp>
#include <components/esm3/creaturestats.hpp>
#include <components/esm3/custommarkerstate.hpp>
#include <components/esm3/doorstate.hpp>
#include <components/esm3/inventorystate.hpp>
#include <components/esm3/npcstate.hpp>
#include <components/esm3/npcstats.hpp>
#include <components/esm3/objectstate.hpp>
#include <components/esm3/queststate.hpp>
#include <components/esm3/statstate.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

// The states of the objects of the world and of the actors in a saved game, as the engine writes them: the states
// are saved and loaded again with every field set, so that a field that the writer or the reader leaves out shows.
namespace ESM
{
    namespace
    {
        using namespace ::testing;
        using namespace ::ESM::SaveLoadTest;

        RefId id(std::string_view value)
        {
            return RefId::stringRefId(value);
        }

        // Most of the states cannot be moved, so the result is on the heap.
        template <class T>
        std::unique_ptr<T> roundTrip(const T& record)
        {
            auto result = std::make_unique<T>();
            saveAndLoadRecord(record, CurrentSaveGameFormatVersion, *result);
            return result;
        }

        // The reference of an object is loaded before the state of the object, as the cell and the inventories do it.
        template <class T>
        std::unique_ptr<T> roundTripObject(const T& record)
        {
            ESMWriter writer;
            auto stream = std::make_unique<std::stringstream>();
            writer.setFormatVersion(CurrentSaveGameFormatVersion);
            writer.save(*stream);
            writer.startRecord(fakeRecordId);
            record.save(writer);
            writer.endRecord(fakeRecordId);

            ESMReader reader;
            reader.open(std::move(stream), "stream");
            EXPECT_TRUE(reader.hasMoreRecs());
            reader.getRecName();
            reader.getRecHeader();
            auto result = std::make_unique<T>();
            result->mRef.loadId(reader, true);
            result->load(reader);
            return result;
        }

        template <class T>
        void expectStat(const StatState<T>& stat, T base, T modifier, T current, float damage, float progress)
        {
            EXPECT_EQ(stat.mBase, base);
            EXPECT_EQ(stat.mMod, modifier);
            EXPECT_EQ(stat.mCurrent, current);
            EXPECT_EQ(stat.mDamage, damage);
            EXPECT_EQ(stat.mProgress, progress);
        }

        ObjectState makeObject()
        {
            ObjectState state;
            state.blank();
            state.mRef.mRefNum = RefNum{ .mIndex = 0x44, .mContentFile = -1 };
            state.mRef.mRefID = id("stimpak");
            state.mRef.mCount = 3;
            state.mRef.mPos.pos[0] = 1.f;
            state.mRef.mPos.pos[1] = 2.f;
            state.mRef.mPos.pos[2] = 3.f;
            state.mHasLocals = 1;
            Variant counter(7);
            counter.setType(VT_Short);
            state.mLocals.mVariables.emplace_back("counter", counter);
            state.mEnabled = 1;
            state.mPosition = state.mRef.mPos;
            state.mFlags = 0x10;
            AnimationState::ScriptedAnimation animation;
            animation.mGroup = "idle";
            animation.mTime = 0.5f;
            animation.mAbsolute = true;
            animation.mLoopCount = 4;
            state.mAnimationState.mScriptedAnims.push_back(animation);
            return state;
        }

        TEST(Esm3ObjectStateTest, keepsTheLocalsTheFlagsAndTheScriptedAnimations)
        {
            const auto statePtr = roundTripObject(makeObject());
            const ObjectState& state = *statePtr;

            EXPECT_EQ(state.mRef.mRefID, id("stimpak"));
            EXPECT_EQ(state.mRef.mCount, 3);
            ASSERT_EQ(state.mLocals.mVariables.size(), 1u);
            EXPECT_EQ(state.mLocals.mVariables[0].first, "counter");
            EXPECT_EQ(state.mLocals.mVariables[0].second.getInteger(), 7);
            EXPECT_EQ(state.mEnabled, 1);
            EXPECT_EQ(state.mFlags, 0x10u);
            ASSERT_EQ(state.mAnimationState.mScriptedAnims.size(), 1u);
            EXPECT_EQ(state.mAnimationState.mScriptedAnims[0].mGroup, "idle");
            EXPECT_EQ(state.mAnimationState.mScriptedAnims[0].mTime, 0.5f);
            EXPECT_TRUE(state.mAnimationState.mScriptedAnims[0].mAbsolute);
            EXPECT_EQ(state.mAnimationState.mScriptedAnims[0].mLoopCount, 4u);
            EXPECT_TRUE(state.mHasCustomState);
        }

        TEST(Esm3ObjectStateTest, keepsAPositionThatIsNotTheOneOfTheReference)
        {
            ObjectState record = makeObject();
            record.mPosition.pos[0] = 100.f;
            record.mPosition.rot[2] = 1.5f;
            record.mEnabled = 0;
            record.mHasCustomState = false;

            const auto statePtr = roundTripObject(record);
            const ObjectState& state = *statePtr;

            EXPECT_EQ(state.mPosition.pos[0], 100.f);
            EXPECT_EQ(state.mPosition.rot[2], 1.5f);
            EXPECT_EQ(state.mRef.mPos.pos[0], 1.f);
            EXPECT_EQ(state.mEnabled, 0);
            EXPECT_FALSE(state.mHasCustomState);
        }

        TEST(Esm3ObjectStateTest, anObjectThatIsNotOfTheKindIsRefusedToBeCastToIt)
        {
            ObjectState state;
            const ObjectState& constant = state;

            EXPECT_THROW(state.asNpcState(), std::logic_error);
            EXPECT_THROW(constant.asNpcState(), std::logic_error);
            EXPECT_THROW(state.asCreatureState(), std::logic_error);
            EXPECT_THROW(constant.asCreatureState(), std::logic_error);
            EXPECT_THROW(state.asContainerState(), std::logic_error);
            EXPECT_THROW(constant.asContainerState(), std::logic_error);
            EXPECT_THROW(state.asDoorState(), std::logic_error);
            EXPECT_THROW(constant.asDoorState(), std::logic_error);
            EXPECT_THROW(state.asCreatureLevListState(), std::logic_error);
            EXPECT_THROW(constant.asCreatureLevListState(), std::logic_error);
        }

        TEST(Esm3ObjectStateTest, anObjectOfTheKindIsItsOwnCast)
        {
            NpcState npc;
            CreatureState creature;
            ContainerState container;
            DoorState door;
            CreatureLevListState list;

            EXPECT_EQ(&npc.asNpcState(), &npc);
            EXPECT_EQ(&creature.asCreatureState(), &creature);
            EXPECT_EQ(&container.asContainerState(), &container);
            EXPECT_EQ(&door.asDoorState(), &door);
            EXPECT_EQ(&list.asCreatureLevListState(), &list);
        }

        TEST(Esm3InventoryStateTest, keepsTheItemsTheEquipmentAndTheSelectedEnchantedItem)
        {
            InventoryState record;
            record.mItems.push_back(makeObject());
            ObjectState second = makeObject();
            second.mRef.mRefID = id("pistol");
            second.mRef.mCount = 1;
            second.mRef.mRefNum = RefNum{ .mIndex = 0x45, .mContentFile = -1 };
            record.mItems.push_back(second);
            record.mEquipmentSlots[1] = 16;
            record.mEquipmentSlots[0] = 3;
            record.mPermanentMagicEffectMagnitudes[id("ring")] = { { 0.5f, 2.f }, { 0.25f, 3.f } };
            record.mSelectedEnchantItem = 1;

            InventoryState state;
            saveAndLoadRecord(record, CurrentSaveGameFormatVersion, state);

            ASSERT_EQ(state.mItems.size(), 2u);
            EXPECT_EQ(state.mItems[0].mRef.mRefID, id("stimpak"));
            EXPECT_EQ(state.mItems[0].mRef.mCount, 3);
            EXPECT_EQ(state.mItems[1].mRef.mRefID, id("pistol"));
            EXPECT_EQ(state.mItems[1].mRef.mCount, 1);
            EXPECT_THAT(state.mEquipmentSlots, ElementsAre(Pair(0u, 3), Pair(1u, 16)));
            ASSERT_EQ(state.mPermanentMagicEffectMagnitudes.size(), 1u);
            EXPECT_THAT(state.mPermanentMagicEffectMagnitudes.at(id("ring")),
                ElementsAre(Pair(0.5f, 2.f), Pair(0.25f, 3.f)));
            ASSERT_TRUE(state.mSelectedEnchantItem.has_value());
            EXPECT_EQ(*state.mSelectedEnchantItem, 1u);
        }

        TEST(Esm3InventoryStateTest, anEmptyInventoryHasNoSelectedItem)
        {
            const InventoryState state = [] {
                InventoryState result;
                saveAndLoadRecord(InventoryState{}, CurrentSaveGameFormatVersion, result);
                return result;
            }();

            EXPECT_TRUE(state.mItems.empty());
            EXPECT_TRUE(state.mEquipmentSlots.empty());
            EXPECT_FALSE(state.mSelectedEnchantItem.has_value());
        }

        TEST(Esm3InventoryStateTest, itemsWithoutACountAreLeftOut)
        {
            InventoryState record;
            record.mItems.push_back(makeObject());
            ObjectState spent = makeObject();
            spent.mRef.mRefID = id("spent");
            spent.mRef.mCount = 0;
            record.mItems.push_back(spent);

            InventoryState state;
            saveAndLoadRecord(record, CurrentSaveGameFormatVersion, state);

            ASSERT_EQ(state.mItems.size(), 1u);
            EXPECT_EQ(state.mItems[0].mRef.mRefID, id("stimpak"));
        }

        TEST(Esm3InventoryStateTest, aLevelledItemOfAnOldSaveGetsANegativeCount)
        {
            // Saves up to the format 2 kept the restocking levelled items in a map of their own.
            InventoryState result;
            ESMWriter writer;
            auto stream = std::make_unique<std::stringstream>();
            writer.setFormatVersion(CurrentSaveGameFormatVersion);
            writer.save(*stream);
            writer.startRecord(fakeRecordId);
            InventoryState inventory;
            inventory.mItems.push_back(makeObject());
            inventory.save(writer);
            writer.writeHNRefId("LEVM", id("stimpak"));
            writer.writeHNT("COUN", std::int32_t{ 3 });
            writer.writeHNString("LGRP", "group");
            writer.endRecord(fakeRecordId);
            ESMReader reader;
            reader.open(std::move(stream), "stream");
            ASSERT_TRUE(reader.hasMoreRecs());
            reader.getRecName();
            reader.getRecHeader();
            result.load(reader);

            ASSERT_EQ(result.mItems.size(), 1u);
            EXPECT_EQ(result.mItems[0].mRef.mCount, -3);
        }

        TEST(Esm3ContainerStateTest, keepsItsInventory)
        {
            ContainerState record;
            record.blank();
            record.mRef.mRefID = id("footlocker");
            record.mInventory.mItems.push_back(makeObject());

            const auto statePtr = roundTripObject(record);
            const ContainerState& state = *statePtr;

            EXPECT_EQ(state.mRef.mRefID, id("footlocker"));
            ASSERT_EQ(state.mInventory.mItems.size(), 1u);
            EXPECT_EQ(state.mInventory.mItems[0].mRef.mRefID, id("stimpak"));
        }

        TEST(Esm3DoorStateTest, keepsTheStateOfTheDoor)
        {
            DoorState record;
            record.blank();
            record.mRef.mRefID = id("door");
            record.mDoorState = 2;

            EXPECT_EQ(roundTripObject(record)->mDoorState, 2);

            record.mDoorState = 0;
            EXPECT_EQ(roundTripObject(record)->mDoorState, 0);
        }

        TEST(Esm3DoorStateTest, doesNotKeepAStateThatIsOutOfRange)
        {
            DoorState record;
            record.blank();
            record.mRef.mRefID = id("door");
            record.mDoorState = 7;

            EXPECT_EQ(roundTripObject(record)->mDoorState, 0);
        }

        TEST(Esm3CreatureLevListStateTest, keepsTheSpawnedActorAndTheRespawn)
        {
            CreatureLevListState record;
            record.blank();
            record.mRef.mRefID = id("levcreature");
            record.mSpawnedActor = RefNum{ .mIndex = 0x99, .mContentFile = -1 };
            record.mSpawn = true;

            const auto statePtr = roundTripObject(record);
            const CreatureLevListState& state = *statePtr;

            EXPECT_EQ(state.mSpawnedActor.mIndex, 0x99u);
            EXPECT_TRUE(state.mSpawn);

            record.mSpawnedActor = RefNum{};
            record.mSpawn = false;
            const auto idlePtr = roundTripObject(record);
            const CreatureLevListState& idle = *idlePtr;
            EXPECT_FALSE(idle.mSpawnedActor.isSet());
            EXPECT_FALSE(idle.mSpawn);
        }

        void fillCreatureStats(CreatureStats& stats)
        {
            stats.blank();
            stats.mAttributes[Attribute::indexToRefId(0)].mBase = 6.f;
            stats.mAttributes[Attribute::indexToRefId(0)].mMod = 2.f;
            stats.mAttributes[Attribute::indexToRefId(0)].mCurrent = 8.f;
            stats.mAttributes[Attribute::indexToRefId(5)].mBase = 4.f;
            stats.mDynamic[0].mBase = 100.f;
            stats.mDynamic[0].mCurrent = 70.f;
            stats.mDynamic[0].mDamage = 30.f;
            stats.mDynamic[1].mBase = 50.f;
            stats.mDynamic[2].mBase = 20.f;
            stats.mDynamic[2].mProgress = 0.5f;
            stats.mGoldPool = 250;
            stats.mTradeTime = { .mHour = 5.f, .mDay = 9 };
            stats.mDead = true;
            stats.mDied = true;
            stats.mTalkedTo = true;
            stats.mAttacked = true;
            stats.mKnockdownOneFrame = true;
            stats.mBlock = true;
            stats.mRecalcDynamicStats = true;
            stats.mMovementFlags = 0x21;
            stats.mFallHeight = 12.5f;
            stats.mLastHitObject = id("deathclaw");
            stats.mLastHitAttemptObject = id("raider");
            stats.mDrawState = 1;
            stats.mDeathAnimation = 3;
            stats.mTimeOfDeath = { .mHour = 7.f, .mDay = 11 };
            stats.mLevel = 14;
            stats.mActorId = -1;
            stats.mHasAiSettings = true;
            for (int i = 0; i < 4; ++i)
            {
                stats.mAiSettings[i].mBase = 10 * (i + 1);
                stats.mAiSettings[i].mMod = i;
            }
            stats.mSummonedCreatures.emplace(id("summoncreature"), RefNum{ .mIndex = 0x70, .mContentFile = -1 });
            stats.mSpells.mSpells = { id("fireball") };
            stats.mSpells.mSelectedSpell = id("fireball");
            stats.mMagicEffects.mEffects[id("fortifyattribute")] = { 10, 2.5f };
            stats.mMissingACDT = true;
        }

        TEST(Esm3CreatureStatsTest, keepsTheStatisticsTheFlagsAndTheSummons)
        {
            CreatureStats record;
            fillCreatureStats(record);
            const auto statsPtr = roundTrip(record);
            const CreatureStats& stats = *statsPtr;

            expectStat<float>(stats.mAttributes.at(Attribute::indexToRefId(0)), 6.f, 2.f, 8.f, 0.f, 0.f);
            EXPECT_EQ(stats.mAttributes.at(Attribute::indexToRefId(5)).mBase, 4.f);
            expectStat<float>(stats.mDynamic[0], 100.f, 0.f, 70.f, 30.f, 0.f);
            EXPECT_EQ(stats.mDynamic[1].mBase, 50.f);
            EXPECT_EQ(stats.mDynamic[2].mProgress, 0.5f);
            EXPECT_EQ(stats.mGoldPool, 250);
            EXPECT_EQ(stats.mTradeTime.mHour, 5.f);
            EXPECT_EQ(stats.mTradeTime.mDay, 9);
            EXPECT_TRUE(stats.mDead);
            EXPECT_FALSE(stats.mDeathAnimationFinished);
            EXPECT_TRUE(stats.mDied);
            EXPECT_FALSE(stats.mMurdered);
            EXPECT_TRUE(stats.mTalkedTo);
            EXPECT_FALSE(stats.mAlarmed);
            EXPECT_TRUE(stats.mAttacked);
            EXPECT_FALSE(stats.mKnockdown);
            EXPECT_TRUE(stats.mKnockdownOneFrame);
            EXPECT_FALSE(stats.mKnockdownOverOneFrame);
            EXPECT_FALSE(stats.mHitRecovery);
            EXPECT_TRUE(stats.mBlock);
            EXPECT_TRUE(stats.mRecalcDynamicStats);
            EXPECT_EQ(stats.mMovementFlags, 0x21u);
            EXPECT_EQ(stats.mFallHeight, 12.5f);
            EXPECT_EQ(stats.mLastHitObject, id("deathclaw"));
            EXPECT_EQ(stats.mLastHitAttemptObject, id("raider"));
            EXPECT_EQ(stats.mDrawState, 1);
            EXPECT_EQ(stats.mDeathAnimation, 3);
            EXPECT_EQ(stats.mTimeOfDeath.mHour, 7.f);
            EXPECT_EQ(stats.mTimeOfDeath.mDay, 11);
            EXPECT_EQ(stats.mLevel, 14);
            EXPECT_TRUE(stats.mHasAiSettings);
            EXPECT_EQ(stats.mAiSettings[3].mBase, 40);
            EXPECT_EQ(stats.mAiSettings[2].mMod, 2);
            ASSERT_EQ(stats.mSummonedCreatures.size(), 1u);
            EXPECT_EQ(stats.mSummonedCreatures.begin()->first, id("summoncreature"));
            EXPECT_EQ(stats.mSummonedCreatures.begin()->second.mIndex, 0x70u);
            EXPECT_THAT(stats.mSpells.mSpells, ElementsAre(id("fireball")));
            EXPECT_EQ(stats.mSpells.mSelectedSpell, id("fireball"));
            EXPECT_EQ(stats.mMagicEffects.mEffects.at(id("fortifyattribute")).first, 10);
            EXPECT_EQ(stats.mMagicEffects.mEffects.at(id("fortifyattribute")).second, 2.5f);
            EXPECT_TRUE(stats.mMissingACDT);
        }

        TEST(Esm3CreatureStatsTest, aCreatureThatNothingHappenedToIsKeptAsItWas)
        {
            CreatureStats record;
            record.blank();

            const auto statsPtr = roundTrip(record);
            const CreatureStats& stats = *statsPtr;

            EXPECT_EQ(stats.mGoldPool, 0);
            EXPECT_EQ(stats.mLevel, 1);
            EXPECT_EQ(stats.mDeathAnimation, -1);
            EXPECT_EQ(stats.mActorId, -1);
            EXPECT_FALSE(stats.mDead);
            EXPECT_FALSE(stats.mHasAiSettings);
            EXPECT_FALSE(stats.mMissingACDT);
            EXPECT_TRUE(stats.mSummonedCreatures.empty());
            EXPECT_TRUE(stats.mLastHitObject.empty());
        }

        TEST(Esm3NpcStatsTest, keepsTheFactionsTheSkillsAndTheCrime)
        {
            NpcStats record;
            record.blank();
            record.mFactions[id("ncr")].mRank = 2;
            record.mFactions[id("ncr")].mReputation = 12;
            record.mFactions[id("legion")].mExpelled = true;
            record.mDisposition = 15;
            record.mCrimeDispositionModifier = -5;
            record.mSkills[Skill::indexToRefId(3)].mBase = 40.f;
            record.mSkills[Skill::indexToRefId(3)].mProgress = 0.75f;
            record.mIsWerewolf = true;
            record.mBounty = 300;
            record.mReputation = 8;
            record.mWerewolfKills = 2;
            record.mLevelProgress = 4;
            record.mSkillIncrease[Attribute::indexToRefId(1)] = 2;
            record.mSpecIncreases = { 1, 2, 3 };
            record.mUsedIds = { id("book1"), id("book2") };
            record.mTimeToStartDrowning = 4.5f;
            record.mCrimeId = 17;

            const auto statsPtr = roundTrip(record);
            const NpcStats& stats = *statsPtr;

            ASSERT_EQ(stats.mFactions.size(), 2u);
            EXPECT_EQ(stats.mFactions.at(id("ncr")).mRank, 2);
            EXPECT_EQ(stats.mFactions.at(id("ncr")).mReputation, 12);
            EXPECT_FALSE(stats.mFactions.at(id("ncr")).mExpelled);
            EXPECT_TRUE(stats.mFactions.at(id("legion")).mExpelled);
            EXPECT_EQ(stats.mFactions.at(id("legion")).mRank, -1);
            EXPECT_EQ(stats.mDisposition, 15);
            EXPECT_EQ(stats.mCrimeDispositionModifier, -5);
            EXPECT_EQ(stats.mSkills.at(Skill::indexToRefId(3)).mBase, 40.f);
            EXPECT_EQ(stats.mSkills.at(Skill::indexToRefId(3)).mProgress, 0.75f);
            EXPECT_TRUE(stats.mIsWerewolf);
            EXPECT_EQ(stats.mBounty, 300);
            EXPECT_EQ(stats.mReputation, 8);
            EXPECT_EQ(stats.mWerewolfKills, 2);
            EXPECT_EQ(stats.mLevelProgress, 4);
            EXPECT_EQ(stats.mSkillIncrease.at(Attribute::indexToRefId(1)), 2);
            EXPECT_THAT(stats.mSpecIncreases, ElementsAre(1, 2, 3));
            EXPECT_THAT(stats.mUsedIds, ElementsAre(id("book1"), id("book2")));
            EXPECT_EQ(stats.mTimeToStartDrowning, 4.5f);
            EXPECT_EQ(stats.mCrimeId, 17);
        }

        TEST(Esm3NpcStateTest, keepsTheInventoryAndBothKindsOfStatistics)
        {
            NpcState record;
            record.blank();
            record.mRef.mRefID = id("sunnysmiles");
            record.mInventory.mItems.push_back(makeObject());
            record.mNpcStats.mBounty = 20;
            record.mCreatureStats.mLevel = 5;
            record.mCreatureStats.mGoldPool = 11;

            const auto statePtr = roundTripObject(record);
            const NpcState& state = *statePtr;

            EXPECT_EQ(state.mRef.mRefID, id("sunnysmiles"));
            ASSERT_EQ(state.mInventory.mItems.size(), 1u);
            EXPECT_EQ(state.mNpcStats.mBounty, 20);
            EXPECT_EQ(state.mCreatureStats.mLevel, 5);
            EXPECT_EQ(state.mCreatureStats.mGoldPool, 11);
        }

        TEST(Esm3NpcStateTest, anActorWithoutACustomStateHasNoInventory)
        {
            NpcState record;
            record.blank();
            record.mRef.mRefID = id("plain");
            record.mHasCustomState = false;
            record.mInventory.mItems.push_back(makeObject());

            const auto statePtr = roundTripObject(record);
            const NpcState& state = *statePtr;

            EXPECT_FALSE(state.mHasCustomState);
            EXPECT_TRUE(state.mInventory.mItems.empty());
        }

        TEST(Esm3CreatureStateTest, keepsTheInventoryAndTheStatistics)
        {
            CreatureState record;
            record.blank();
            record.mRef.mRefID = id("radscorpion");
            record.mInventory.mItems.push_back(makeObject());
            record.mCreatureStats.mDead = true;
            record.mCreatureStats.mLevel = 9;

            const auto statePtr = roundTripObject(record);
            const CreatureState& state = *statePtr;

            EXPECT_EQ(state.mRef.mRefID, id("radscorpion"));
            ASSERT_EQ(state.mInventory.mItems.size(), 1u);
            EXPECT_TRUE(state.mCreatureStats.mDead);
            EXPECT_EQ(state.mCreatureStats.mLevel, 9);

            record.mHasCustomState = false;
            EXPECT_TRUE(roundTripObject(record)->mInventory.mItems.empty());
        }

        TEST(Esm3CellStateTest, keepsTheWaterLevelOfAnInteriorOnly)
        {
            CellState record;
            record.mIsInterior = true;
            record.mWaterLevel = -12.5f;
            record.mHasFogOfWar = 1;
            record.mLastRespawn = { .mHour = 3.f, .mDay = 4 };

            const auto interiorPtr = roundTrip(record);
            const CellState& interior = *interiorPtr;

            EXPECT_EQ(interior.mWaterLevel, -12.5f);
            EXPECT_EQ(interior.mHasFogOfWar, 1);
            EXPECT_EQ(interior.mLastRespawn.mHour, 3.f);
            EXPECT_EQ(interior.mLastRespawn.mDay, 4);

            record.mIsInterior = false;
            const auto exteriorPtr = roundTrip(record);
            const CellState& exterior = *exteriorPtr;
            EXPECT_EQ(exterior.mWaterLevel, 0.f);
        }

        TEST(Esm3QuestStateTest, keepsTheStageAndWhetherTheQuestIsFinished)
        {
            QuestState record;
            record.mTopic = id("vmq01");
            record.mState = 40;
            record.mFinished = 1;

            const auto statePtr = roundTrip(record);
            const QuestState& state = *statePtr;

            EXPECT_EQ(state.mTopic, id("vmq01"));
            EXPECT_EQ(state.mState, 40);
            EXPECT_EQ(state.mFinished, 1);
        }

        TEST(Esm3CustomMarkerTest, keepsThePlaceTheCellAndTheNote)
        {
            CustomMarker record;
            record.mWorldX = 100.f;
            record.mWorldY = -200.f;
            record.mCell = RefId::esm3ExteriorCell(2, -3);
            record.mNote = "Water here";

            const auto markerPtr = roundTrip(record);
            const CustomMarker& marker = *markerPtr;

            EXPECT_EQ(marker, record);

            record.mNote.clear();
            record.mCell = id("Vault 21");
            const auto withoutNotePtr = roundTrip(record);
            const CustomMarker& withoutNote = *withoutNotePtr;
            EXPECT_EQ(withoutNote, record);
            EXPECT_TRUE(withoutNote.mNote.empty());
        }

        TEST(Esm3AiSequenceTest, keepsEachKindOfPackageInItsOrder)
        {
            AiSequence::AiSequence record;
            record.mLastAiPackage = 2;
            auto wander = std::make_unique<AiSequence::AiWander>();
            wander->mData = { .mDistance = 300, .mDuration = 10, .mTimeOfDay = 6, .mIdle = { 1, 2, 3, 4, 5, 6, 7, 8 }, .mShouldRepeat = 1 };
            wander->mDurationData.mRemainingDuration = 4.f;
            wander->mStoredInitialActorPosition = true;
            wander->mInitialActorPosition.mValues[0] = 1.f;
            record.mPackages.push_back({ AiSequence::Ai_Wander, std::move(wander) });
            auto travel = std::make_unique<AiSequence::AiTravel>();
            travel->mData = { .mX = 1.f, .mY = 2.f, .mZ = 3.f };
            travel->mHidden = true;
            travel->mRepeat = false;
            record.mPackages.push_back({ AiSequence::Ai_Travel, std::move(travel) });
            auto escort = std::make_unique<AiSequence::AiEscort>();
            escort->mData = { .mX = 4.f, .mY = 5.f, .mZ = 6.f, .mDuration = 9 };
            escort->mTargetActor = RefNum{ .mIndex = 0x50, .mContentFile = -1 };
            escort->mTargetId = id("player");
            escort->mCellId = "Goodsprings";
            escort->mRemainingDuration = 2.f;
            escort->mRepeat = true;
            record.mPackages.push_back({ AiSequence::Ai_Escort, std::move(escort) });
            auto follow = std::make_unique<AiSequence::AiFollow>();
            follow->mData = { .mX = 7.f, .mY = 8.f, .mZ = 9.f, .mDuration = 3 };
            follow->mTargetActor = RefNum{ .mIndex = 0x51, .mContentFile = -1 };
            follow->mTargetId = id("player");
            follow->mCellId = "";
            follow->mRemainingDuration = 1.f;
            follow->mAlwaysFollow = true;
            follow->mCommanded = true;
            follow->mActive = true;
            follow->mRepeat = false;
            record.mPackages.push_back({ AiSequence::Ai_Follow, std::move(follow) });
            auto activate = std::make_unique<AiSequence::AiActivate>();
            activate->mTargetId = id("terminal");
            activate->mRepeat = true;
            record.mPackages.push_back({ AiSequence::Ai_Activate, std::move(activate) });
            auto combat = std::make_unique<AiSequence::AiCombat>();
            combat->mTargetActor = RefNum{ .mIndex = 0x52, .mContentFile = -1 };
            record.mPackages.push_back({ AiSequence::Ai_Combat, std::move(combat) });
            auto pursue = std::make_unique<AiSequence::AiPursue>();
            pursue->mTargetActor = RefNum{ .mIndex = 0x53, .mContentFile = -1 };
            record.mPackages.push_back({ AiSequence::Ai_Pursue, std::move(pursue) });

            AiSequence::AiSequence result;
            saveAndLoadRecord(record, CurrentSaveGameFormatVersion, result);

            ASSERT_EQ(result.mPackages.size(), 7u);
            EXPECT_EQ(result.mLastAiPackage, 2);
            const auto& wanderResult = static_cast<const AiSequence::AiWander&>(*result.mPackages[0].mPackage);
            EXPECT_EQ(result.mPackages[0].mType, AiSequence::Ai_Wander);
            EXPECT_EQ(wanderResult.mData.mDistance, 300);
            EXPECT_EQ(wanderResult.mData.mIdle[7], 8);
            EXPECT_EQ(wanderResult.mDurationData.mRemainingDuration, 4.f);
            EXPECT_TRUE(wanderResult.mStoredInitialActorPosition);
            const auto& travelResult = static_cast<const AiSequence::AiTravel&>(*result.mPackages[1].mPackage);
            EXPECT_EQ(travelResult.mData.mZ, 3.f);
            EXPECT_TRUE(travelResult.mHidden);
            EXPECT_FALSE(travelResult.mRepeat);
            const auto& escortResult = static_cast<const AiSequence::AiEscort&>(*result.mPackages[2].mPackage);
            EXPECT_EQ(escortResult.mData.mDuration, 9);
            EXPECT_EQ(escortResult.mTargetActor.mIndex, 0x50u);
            EXPECT_EQ(escortResult.mTargetId, id("player"));
            EXPECT_EQ(escortResult.mCellId, "Goodsprings");
            EXPECT_TRUE(escortResult.mRepeat);
            const auto& followResult = static_cast<const AiSequence::AiFollow&>(*result.mPackages[3].mPackage);
            EXPECT_EQ(followResult.mTargetActor.mIndex, 0x51u);
            EXPECT_TRUE(followResult.mAlwaysFollow);
            EXPECT_TRUE(followResult.mCommanded);
            EXPECT_TRUE(followResult.mActive);
            EXPECT_FALSE(followResult.mRepeat);
            const auto& activateResult = static_cast<const AiSequence::AiActivate&>(*result.mPackages[4].mPackage);
            EXPECT_EQ(activateResult.mTargetId, id("terminal"));
            EXPECT_TRUE(activateResult.mRepeat);
            EXPECT_EQ(static_cast<const AiSequence::AiCombat&>(*result.mPackages[5].mPackage).mTargetActor.mIndex, 0x52u);
            EXPECT_EQ(static_cast<const AiSequence::AiPursue&>(*result.mPackages[6].mPackage).mTargetActor.mIndex, 0x53u);
        }

        TEST(Esm3AiSequenceTest, anEmptySequenceStaysEmpty)
        {
            AiSequence::AiSequence result;
            saveAndLoadRecord(AiSequence::AiSequence{}, CurrentSaveGameFormatVersion, result);

            EXPECT_TRUE(result.mPackages.empty());
            EXPECT_EQ(result.mLastAiPackage, -1);
        }
    }
}
