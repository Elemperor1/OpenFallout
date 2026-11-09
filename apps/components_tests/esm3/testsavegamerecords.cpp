#include "saveloadharness.hpp"

#include <components/esm/attr.hpp>
#include <components/esm3/activespells.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/cellid.hpp>
#include <components/esm3/controlsstate.hpp>
#include <components/esm3/debugprofile.hpp>
#include <components/esm3/dialoguestate.hpp>
#include <components/esm3/fogstate.hpp>
#include <components/esm3/globalmap.hpp>
#include <components/esm3/globalscript.hpp>
#include <components/esm3/journalentry.hpp>
#include <components/esm3/projectilestate.hpp>
#include <components/esm3/savedgame.hpp>
#include <components/esm3/selectiongroup.hpp>
#include <components/esm3/spellstate.hpp>
#include <components/esm3/statstate.hpp>
#include <components/esm3/stolenitems.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// The records that a saved game holds, saved and loaded again in the format that the engine writes, and for the
// records that old saves wrote in another way, read from the bytes of the old layout.
namespace ESM
{
    namespace
    {
        using namespace ::testing;
        using namespace ::ESM::SaveLoadTest;

        struct Esm3SaveGameRecordTest : public Esm3SaveLoadRecordTest
        {
        };

        RefId id(std::string_view value)
        {
            return RefId::stringRefId(value);
        }

        template <std::size_t size>
        void setValues(float (&destination)[size], std::initializer_list<float> values)
        {
            std::copy(values.begin(), values.end(), destination);
        }

        // Write a record with the function, in a file of the format version, and read it with the other function.
        TEST_P(Esm3SaveGameRecordTest, journalEntryOfTheJournalKeepsItsDate)
        {
            JournalEntry record;
            record.mType = JournalEntry::Type_Journal;
            record.mTopic = id("myquest");
            record.mInfo = id("info1");
            record.mText = "I found the vault";
            record.mDay = 12;
            record.mMonth = 3;
            record.mDayOfMonth = 7;

            JournalEntry result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mType, JournalEntry::Type_Journal);
            EXPECT_EQ(result.mTopic, record.mTopic);
            EXPECT_EQ(result.mInfo, record.mInfo);
            EXPECT_EQ(result.mText, record.mText);
            EXPECT_EQ(result.mDay, 12);
            EXPECT_EQ(result.mMonth, 3);
            EXPECT_EQ(result.mDayOfMonth, 7);
        }

        TEST_P(Esm3SaveGameRecordTest, journalEntryOfATopicKeepsTheNameOfTheActor)
        {
            JournalEntry record;
            record.mType = JournalEntry::Type_Topic;
            record.mTopic = id("rumors");
            record.mInfo = id("info2");
            record.mText = "They say the vault is open";
            record.mActorName = "Moira";
            record.mDay = record.mMonth = record.mDayOfMonth = 0;

            JournalEntry result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mType, JournalEntry::Type_Topic);
            EXPECT_EQ(result.mActorName, "Moira");
            EXPECT_EQ(result.mText, record.mText);
        }

        TEST_P(Esm3SaveGameRecordTest, journalEntryOfAQuestHasNoDateAndNoActor)
        {
            JournalEntry record;
            record.mType = JournalEntry::Type_Quest;
            record.mTopic = id("quest");
            record.mInfo = id("info3");
            record.mText = "done";
            record.mDay = record.mMonth = record.mDayOfMonth = 0;

            JournalEntry result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mType, JournalEntry::Type_Quest);
            EXPECT_EQ(result.mText, "done");
            EXPECT_TRUE(result.mActorName.empty());
        }

        TEST_P(Esm3SaveGameRecordTest, stolenItemsKeepTheOwnersAndTheFactions)
        {
            StolenItems record;
            record.mStolenItems[id("gold")][{ id("moira"), false }] = 5;
            record.mStolenItems[id("gold")][{ id("brotherhood"), true }] = 9;
            record.mStolenItems[id("pistol")][{ id("moira"), false }] = 1;

            StolenItems result;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.write(writer); },
                [&](ESMReader& reader) { result.load(reader); });

            ASSERT_EQ(result.mStolenItems.size(), 2u);
            const auto& gold = result.mStolenItems.at(id("gold"));
            EXPECT_EQ(gold.at({ id("moira"), false }), 5);
            EXPECT_EQ(gold.at({ id("brotherhood"), true }), 9);
            EXPECT_EQ(result.mStolenItems.at(id("pistol")).at({ id("moira"), false }), 1);
        }

        TEST_P(Esm3SaveGameRecordTest, dialogueStateKeepsTheKnownTopicsAndTheChangedReactions)
        {
            DialogueState record;
            record.mKnownTopics = { id("vault"), id("water") };
            record.mChangedFactionReaction[id("ncr")][id("legion")] = -20;
            record.mChangedFactionReaction[id("ncr")][id("brotherhood")] = 5;
            record.mChangedFactionReaction[id("legion")][id("ncr")] = -30;

            DialogueState result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_THAT(result.mKnownTopics, ElementsAre(id("vault"), id("water")));
            EXPECT_EQ(result.mChangedFactionReaction.at(id("ncr")).at(id("legion")), -20);
            EXPECT_EQ(result.mChangedFactionReaction.at(id("ncr")).at(id("brotherhood")), 5);
            EXPECT_EQ(result.mChangedFactionReaction.at(id("legion")).at(id("ncr")), -30);
        }

        TEST_P(Esm3SaveGameRecordTest, controlsStateKeepsEachFlagApart)
        {
            ControlsState record;
            record.mViewSwitchDisabled = true;
            record.mJumpingDisabled = true;
            record.mVanityModeDisabled = true;
            record.mSpellDrawingDisabled = true;

            ControlsState result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_TRUE(result.mViewSwitchDisabled);
            EXPECT_FALSE(result.mControlsDisabled);
            EXPECT_TRUE(result.mJumpingDisabled);
            EXPECT_FALSE(result.mLookingDisabled);
            EXPECT_TRUE(result.mVanityModeDisabled);
            EXPECT_FALSE(result.mWeaponDrawingDisabled);
            EXPECT_TRUE(result.mSpellDrawingDisabled);
        }

        TEST_P(Esm3SaveGameRecordTest, controlsStateKeepsTheOtherFlags)
        {
            ControlsState record;
            record.mControlsDisabled = true;
            record.mLookingDisabled = true;
            record.mWeaponDrawingDisabled = true;

            ControlsState result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_FALSE(result.mViewSwitchDisabled);
            EXPECT_TRUE(result.mControlsDisabled);
            EXPECT_FALSE(result.mJumpingDisabled);
            EXPECT_TRUE(result.mLookingDisabled);
            EXPECT_FALSE(result.mVanityModeDisabled);
            EXPECT_TRUE(result.mWeaponDrawingDisabled);
            EXPECT_FALSE(result.mSpellDrawingDisabled);
        }

        TEST_P(Esm3SaveGameRecordTest, cellIdOfAnExteriorCellKeepsItsIndex)
        {
            CellId record;
            record.mWorldspace = "wasteland";
            record.mIndex = { -3, 7 };
            record.mPaged = true;

            CellId result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mWorldspace, "wasteland");
            EXPECT_TRUE(result.mPaged);
            EXPECT_EQ(result.mIndex.mX, -3);
            EXPECT_EQ(result.mIndex.mY, 7);
            EXPECT_EQ(result, record);
        }

        TEST_P(Esm3SaveGameRecordTest, cellIdOfAnInteriorCellHasNoIndex)
        {
            CellId record;
            record.mWorldspace = "Goodsprings Saloon";
            record.mIndex = { 4, 5 };
            record.mPaged = false;

            CellId result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mWorldspace, "Goodsprings Saloon");
            EXPECT_FALSE(result.mPaged);
            EXPECT_EQ(result.mIndex.mX, 0);
            EXPECT_EQ(result.mIndex.mY, 0);
        }

        TEST(Esm3CellIdTest, cellsAreOrderedByTheKindThenTheIndexThenTheWorldspace)
        {
            CellId interior;
            interior.mWorldspace = "b";
            interior.mIndex = { 0, 0 };
            interior.mPaged = false;
            CellId otherInterior = interior;
            otherInterior.mWorldspace = "c";
            CellId exterior;
            exterior.mWorldspace = "a";
            exterior.mIndex = { 1, 2 };
            exterior.mPaged = true;
            CellId higherX = exterior;
            higherX.mIndex = { 2, 0 };
            CellId higherY = exterior;
            higherY.mIndex = { 1, 3 };
            CellId otherWorld = exterior;
            otherWorld.mWorldspace = "z";

            EXPECT_TRUE(interior < exterior);
            EXPECT_FALSE(exterior < interior);
            EXPECT_TRUE(interior < otherInterior);
            EXPECT_TRUE(exterior < higherX);
            EXPECT_FALSE(higherX < exterior);
            EXPECT_TRUE(exterior < higherY);
            EXPECT_FALSE(higherY < exterior);
            EXPECT_TRUE(exterior < otherWorld);
            EXPECT_NE(exterior, otherWorld);
            EXPECT_NE(interior, exterior);
        }

        TEST(Esm3CellIdTest, theCellOfAReferenceIsTakenFromItsId)
        {
            const CellId exterior = CellId::extractFromRefId(RefId::esm3ExteriorCell(-2, 5));
            EXPECT_TRUE(exterior.mPaged);
            EXPECT_EQ(exterior.mIndex.mX, -2);
            EXPECT_EQ(exterior.mIndex.mY, 5);

            const CellId interior = CellId::extractFromRefId(RefId::stringRefId("Vault 21"));
            EXPECT_FALSE(interior.mPaged);
            EXPECT_EQ(interior.mWorldspace, "Vault 21");

            const CellId none = CellId::extractFromRefId(RefId());
            EXPECT_TRUE(none.mPaged);
            EXPECT_EQ(none.mIndex.mX, 0);

            EXPECT_THROW(CellId::extractFromRefId(RefId::formIdRefId(FormId{ .mIndex = 5, .mContentFile = 0 })),
                std::runtime_error);
        }

        TEST_P(Esm3SaveGameRecordTest, debugProfileKeepsItsScript)
        {
            DebugProfile record;
            record.blank();
            record.mId = id("profile");
            record.mDescription = "Starts in the saloon";
            record.mScriptText = "coc GSSaloon";
            record.mFlags = DebugProfile::Flag_Default | DebugProfile::Flag_Global;

            DebugProfile result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mDescription, record.mDescription);
            EXPECT_EQ(result.mScriptText, record.mScriptText);
            EXPECT_EQ(result.mFlags, 5u);
        }

        TEST_F(Esm3SaveLoadRecordTest, selectionGroupKeepsTheInstances)
        {
            // The editor writes the instances as plain text, in the format of the content files.
            SelectionGroup record;
            record.blank();
            record.mId = id("group");
            record.selectedInstances = { "Instance::cell::1", "Instance::cell::2", "Instance::cell::3" };

            SelectionGroup result;
            saveAndLoadRecord(record, CurrentContentFormatVersion, result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.selectedInstances, record.selectedInstances);
        }

        TEST_P(Esm3SaveGameRecordTest, globalMapKeepsTheImageAndTheMarkers)
        {
            GlobalMap record;
            record.mBounds = { .mMinX = -4, .mMaxX = 6, .mMinY = -2, .mMaxY = 8 };
            record.mImageData = { 'a', 'b', 'c', '\0', 'd' };
            record.mMarkers = { { 1, 2 }, { -3, 4 } };

            GlobalMap result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mBounds.mMinX, -4);
            EXPECT_EQ(result.mBounds.mMaxX, 6);
            EXPECT_EQ(result.mBounds.mMinY, -2);
            EXPECT_EQ(result.mBounds.mMaxY, 8);
            EXPECT_EQ(result.mImageData, record.mImageData);
            EXPECT_EQ(result.mMarkers, record.mMarkers);
        }

        // The fog of war of an interior cell has its own bounds and centre, and the fog of an exterior cell has none.
        void saveFog(const FogState& record, bool interior, FormatVersion format, FogState& result)
        {
            roundTripWith(
                format, [&](ESMWriter& writer) { record.save(writer, interior); },
                [&](ESMReader& reader) { result.load(reader); });
        }

        TEST_P(Esm3SaveGameRecordTest, fogOfAnInteriorCellKeepsTheBoundsTheCentreAndTheTextures)
        {
            FogState record;
            record.mBounds = { .mMinX = -10.f, .mMinY = -20.f, .mMaxX = 30.f, .mMaxY = 40.f };
            record.mNorthMarkerAngle = 1.5f;
            record.mCenterX = 3.f;
            record.mCenterY = 4.f;
            record.mFogTextures.push_back({ .mX = 1, .mY = 2, .mImageData = { 'x', 'y', 'z' } });
            record.mFogTextures.push_back({ .mX = -1, .mY = -2, .mImageData = { 'q' } });

            FogState result;
            saveFog(record, true, GetParam(), result);

            EXPECT_EQ(result.mBounds.mMinX, -10.f);
            EXPECT_EQ(result.mBounds.mMinY, -20.f);
            EXPECT_EQ(result.mBounds.mMaxX, 30.f);
            EXPECT_EQ(result.mBounds.mMaxY, 40.f);
            EXPECT_EQ(result.mNorthMarkerAngle, 1.5f);
            EXPECT_EQ(result.mCenterX, 3.f);
            EXPECT_EQ(result.mCenterY, 4.f);
            ASSERT_EQ(result.mFogTextures.size(), 2u);
            EXPECT_EQ(result.mFogTextures[0].mX, 1);
            EXPECT_EQ(result.mFogTextures[0].mY, 2);
            EXPECT_EQ(result.mFogTextures[0].mImageData, (std::vector<char>{ 'x', 'y', 'z' }));
            EXPECT_EQ(result.mFogTextures[1].mImageData, (std::vector<char>{ 'q' }));
            // the images of the old saves are TGA files, the images of the new ones are not
            EXPECT_EQ(result.mFogTextures[0].mLegacyTgaImageData, GetParam() <= MaxOldFogOfWarFormatVersion);
        }

        TEST_P(Esm3SaveGameRecordTest, fogOfAnExteriorCellHasTexturesOnly)
        {
            FogState record;
            record.mFogTextures.push_back({ .mX = 7, .mY = 8, .mImageData = { 'a' } });

            FogState result;
            result.mBounds = { .mMinX = 2.f, .mMinY = 4.f, .mMaxX = 6.f, .mMaxY = 8.f };
            saveFog(record, false, GetParam(), result);

            // without a centre in the record it is the middle of the bounds
            EXPECT_EQ(result.mCenterX, 4.f);
            EXPECT_EQ(result.mCenterY, 6.f);
            ASSERT_EQ(result.mFogTextures.size(), 1u);
            EXPECT_EQ(result.mFogTextures[0].mX, 7);
            EXPECT_EQ(result.mFogTextures[0].mY, 8);
        }

        TEST_P(Esm3SaveGameRecordTest, savedGameKeepsTheHeaderOfTheSave)
        {
            if (GetParam() <= MaxSavedGameCellNameAsRefIdFormatVersion)
                return; // the name of the cell was an ID before
            SavedGame record;
            record.mContentFiles = { "FalloutNV.esm", "DeadMoney.esm" };
            record.mPlayerName = "Courier";
            record.mPlayerLevel = 12;
            record.mPlayerClassId = id("courier");
            record.mPlayerCellName = "Goodsprings";
            record.mInGameTime = { .mGameHour = 14.5f, .mDay = 3, .mMonth = 2, .mYear = 281 };
            record.mTimePlayed = 3600.5;
            record.mDescription = "Before the vault";
            record.mScreenshot = { 1, 2, 3, 4, 5 };
            record.mCurrentDay = 9;
            record.mCurrentHealth = 80.f;
            record.mMaximumHealth = 120.f;

            SavedGame result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mPlayerName, "Courier");
            EXPECT_EQ(result.mPlayerLevel, 12);
            EXPECT_EQ(result.mPlayerClassId, record.mPlayerClassId);
            EXPECT_EQ(result.mInGameTime.mGameHour, 14.5f);
            EXPECT_EQ(result.mInGameTime.mDay, 3);
            EXPECT_EQ(result.mInGameTime.mMonth, 2);
            EXPECT_EQ(result.mInGameTime.mYear, 281);
            EXPECT_EQ(result.mTimePlayed, 3600.5);
            EXPECT_EQ(result.mDescription, "Before the vault");
            EXPECT_THAT(result.mContentFiles, ElementsAre("FalloutNV.esm", "DeadMoney.esm"));
            EXPECT_EQ(result.mScreenshot, record.mScreenshot);
            EXPECT_EQ(result.mCurrentDay, 9);
            EXPECT_EQ(result.mCurrentHealth, 80.f);
            EXPECT_EQ(result.mMaximumHealth, 120.f);
            EXPECT_EQ(result.mPlayerCellName, "Goodsprings");
        }

        TEST_P(Esm3SaveGameRecordTest, savedGameOfACustomClassKeepsTheNameOfTheClass)
        {
            if (GetParam() <= MaxSavedGameCellNameAsRefIdFormatVersion)
                return;
            SavedGame record;
            record.mPlayerName = "Courier";
            record.mPlayerLevel = 1;
            record.mPlayerClassName = "Wasteland Wanderer";
            record.mPlayerCellName = "Goodsprings";
            record.mInGameTime = { .mGameHour = 1.f, .mDay = 1, .mMonth = 1, .mYear = 1 };
            record.mTimePlayed = 1;

            SavedGame result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_TRUE(result.mPlayerClassId.empty());
            EXPECT_EQ(result.mPlayerClassName, "Wasteland Wanderer");
            EXPECT_TRUE(result.mContentFiles.empty());
            EXPECT_TRUE(result.mScreenshot.empty());
        }

        TEST(Esm3SavedGameTest, listsTheContentFilesThatTheGameDoesNotHave)
        {
            SavedGame save;
            save.mContentFiles = { "FalloutNV.esm", "HonestHearts.esm", "Mod.esp" };

            const std::vector<std::string> loaded = { "falloutnv.esm", "MOD.ESP" };
            EXPECT_THAT(save.getMissingContentFiles(loaded), ElementsAre("HonestHearts.esm"));
            EXPECT_THAT(save.getMissingContentFiles({}), SizeIs(3));
            EXPECT_THAT(SavedGame{}.getMissingContentFiles(loaded), IsEmpty());
        }

        TEST_P(Esm3SaveGameRecordTest, globalScriptKeepsTheTarget)
        {
            if (GetParam() < CurrentSaveGameFormatVersion)
                return; // the target reference was a form ID of its own before
            GlobalScript record;
            record.mId = id("myscript");
            record.mRunning = true;
            record.mTargetId = id("target");
            record.mTargetRef = RefNum{ .mIndex = 0x1234, .mContentFile = -1 };
            Variant count(3);
            count.setType(VT_Short);
            record.mLocals.mVariables.emplace_back("count", count);

            GlobalScript result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_TRUE(result.mRunning);
            ASSERT_EQ(result.mLocals.mVariables.size(), 1u);
            EXPECT_EQ(result.mLocals.mVariables[0].first, "count");
            EXPECT_EQ(result.mLocals.mVariables[0].second.getInteger(), 3);
        }

        TEST_P(Esm3SaveGameRecordTest, globalScriptThatIsNotRunningHasNoTarget)
        {
            GlobalScript record;
            record.mId = id("idle");
            record.mRunning = false;
            record.mTargetRef = RefNum{};

            GlobalScript result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_FALSE(result.mRunning);
            EXPECT_TRUE(result.mTargetId.empty());
            EXPECT_FALSE(result.mTargetRef.isSet());
        }

        TEST_P(Esm3SaveGameRecordTest, spellStateKeepsTheSpellsTheUsedPowersAndTheSelectedSpell)
        {
            SpellState record;
            record.mSpells = { id("spell1"), id("spell2") };
            record.mUsedPowers[id("power1")] = { .mHour = 3.5f, .mDay = 4 };
            record.mSelectedSpell = id("spell2");

            SpellState result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_THAT(result.mSpells, ElementsAre(id("spell1"), id("spell2")));
            ASSERT_EQ(result.mUsedPowers.size(), 1u);
            EXPECT_EQ(result.mUsedPowers.at(id("power1")).mHour, 3.5f);
            EXPECT_EQ(result.mUsedPowers.at(id("power1")).mDay, 4);
            EXPECT_EQ(result.mSelectedSpell, id("spell2"));
        }

        TEST_F(Esm3SaveLoadRecordTest, spellStateOfAnOldSaveKeepsTheRandomMagnitudesAndThePurgedEffects)
        {
            // Saves up to MaxClearModifiersFormatVersion kept the random magnitudes of the effects of each spell.
            SpellState result;
            roundTripWith(
                MaxClearModifiersFormatVersion,
                [&](ESMWriter& writer) {
                    writer.writeHNRefId("SPEL", id("oldspell"));
                    writer.writeHNT("INDX", std::int32_t{ 2 });
                    writer.writeHNT("RAND", 0.25f);
                    writer.writeHNT("INDX", std::int32_t{ 3 });
                    writer.writeHNT("RAND", 0.75f);
                    writer.writeHNT("PURG", std::int32_t{ 1 });
                    writer.writeHNRefId("SPEL", id("plain"));
                    writer.writeHNRefId("PERM", id("permanent"));
                    writer.writeHNT("EFID", std::int32_t{ 10 });
                    writer.writeHNT("ARG_", std::int32_t{ 5 });
                    writer.writeHNT("MAGN", 1.5f);
                    writer.writeHNRefId("CORP", id("corprus"));
                    writer.writeHNT("WORS", std::int32_t{ 3 });
                    writer.writeHNT("TIME", TimeStamp{ .mHour = 1.f, .mDay = 2 });
                },
                [&](ESMReader& reader) { result.load(reader); });

            EXPECT_THAT(result.mSpells, ElementsAre(id("oldspell"), id("plain")));
            const SpellState::SpellParams& params = result.mSpellParams.at(id("oldspell"));
            EXPECT_EQ(params.mEffectRands.at(2), 0.25f);
            EXPECT_EQ(params.mEffectRands.at(3), 0.75f);
            EXPECT_THAT(params.mPurgedEffects, ElementsAre(1));
            EXPECT_TRUE(result.mSpellParams.at(id("plain")).mEffectRands.empty());
            const auto& permanent = result.mPermanentSpellEffects.at(id("permanent"));
            ASSERT_EQ(permanent.size(), 1u);
            EXPECT_EQ(permanent[0].mId, 10);
            EXPECT_EQ(permanent[0].mArg, 5);
            EXPECT_EQ(permanent[0].mMagnitude, 1.5f);
            EXPECT_EQ(result.mCorprusSpells.at(id("corprus")).mWorsenings, 3);
        }

        ActiveSpells::ActiveSpellParams makeActiveSpell()
        {
            ActiveSpells::ActiveSpellParams params;
            params.mActiveSpellId = id("active1");
            params.mSourceSpellId = id("stimpak");
            params.mDisplayName = "Stimpak";
            params.mCaster = RefNum{ .mIndex = 0x10, .mContentFile = -1 };
            params.mItem = RefNum{ .mIndex = 0x20, .mContentFile = -1 };
            params.mFlags = static_cast<ActiveSpells::Flags>(ActiveSpells::Flag_Temporary | ActiveSpells::Flag_Stackable);
            params.mWorsenings = 2;
            params.mNextWorsening = { .mHour = 6.f, .mDay = 7 };
            ActiveEffect effect;
            effect.mEffectId = id("restorehealth");
            effect.mMagnitude = 5.f;
            effect.mMinMagnitude = 4.f;
            effect.mMaxMagnitude = 6.f;
            effect.mArg = id("strength");
            effect.mDuration = 30.f;
            effect.mTimeLeft = 12.f;
            effect.mEffectIndex = 1;
            effect.mFlags = ActiveEffect::Flag_Applied | ActiveEffect::Flag_Remove;
            params.mEffects.push_back(effect);
            ActiveEffect summon;
            summon.mEffectId = id("summoncreature");
            summon.mMagnitude = summon.mMinMagnitude = summon.mMaxMagnitude = 1.f;
            summon.mArg = RefNum{ .mIndex = 0x30, .mContentFile = -1 };
            summon.mDuration = 60.f;
            summon.mTimeLeft = 60.f;
            summon.mEffectIndex = 0;
            summon.mFlags = ActiveEffect::Flag_None;
            params.mEffects.push_back(summon);
            return params;
        }

        TEST_F(Esm3SaveLoadRecordTest, activeSpellsKeepTheEffectsAndTheQueue)
        {
            ActiveSpells record;
            record.mSpells.push_back(makeActiveSpell());
            ActiveSpells::ActiveSpellParams queued = makeActiveSpell();
            queued.mActiveSpellId = id("queued1");
            queued.mItem = RefNum{};
            queued.mWorsenings = -1;
            queued.mEffects.resize(1);
            record.mQueue.push_back(queued);

            ActiveSpells result;
            saveAndLoadRecord(record, CurrentSaveGameFormatVersion, result);

            ASSERT_EQ(result.mSpells.size(), 1u);
            const ActiveSpells::ActiveSpellParams& spell = result.mSpells[0];
            EXPECT_EQ(spell.mActiveSpellId, id("active1"));
            EXPECT_EQ(spell.mSourceSpellId, id("stimpak"));
            EXPECT_EQ(spell.mDisplayName, "Stimpak");
            EXPECT_EQ(spell.mCaster.mIndex, 0x10u);
            EXPECT_EQ(spell.mItem.mIndex, 0x20u);
            EXPECT_EQ(spell.mFlags, ActiveSpells::Flag_Temporary | ActiveSpells::Flag_Stackable);
            EXPECT_EQ(spell.mWorsenings, 2);
            EXPECT_EQ(spell.mNextWorsening.mHour, 6.f);
            EXPECT_EQ(spell.mNextWorsening.mDay, 7);
            ASSERT_EQ(spell.mEffects.size(), 2u);
            const ActiveEffect& effect = spell.mEffects[0];
            EXPECT_EQ(effect.mEffectId, id("restorehealth"));
            EXPECT_EQ(effect.mMagnitude, 5.f);
            EXPECT_EQ(effect.mMinMagnitude, 4.f);
            EXPECT_EQ(effect.mMaxMagnitude, 6.f);
            EXPECT_EQ(effect.getSkillOrAttribute(), id("strength"));
            EXPECT_FALSE(effect.getActor().isSet());
            EXPECT_EQ(effect.mDuration, 30.f);
            EXPECT_EQ(effect.mTimeLeft, 12.f);
            EXPECT_EQ(effect.mEffectIndex, 1);
            EXPECT_EQ(effect.mFlags, ActiveEffect::Flag_Applied | ActiveEffect::Flag_Remove);
            EXPECT_EQ(spell.mEffects[1].getActor().mIndex, 0x30u);
            EXPECT_TRUE(spell.mEffects[1].getSkillOrAttribute().empty());

            ASSERT_EQ(result.mQueue.size(), 1u);
            EXPECT_EQ(result.mQueue[0].mActiveSpellId, id("queued1"));
            EXPECT_FALSE(result.mQueue[0].mItem.isSet());
            EXPECT_EQ(result.mQueue[0].mWorsenings, -1);
        }

        TEST_F(Esm3SaveLoadRecordTest, activeSpellsOfAnOldSaveGetTheFlagsOfTheirType)
        {
            // Between MaxClearModifiersFormatVersion and MaxActiveSpellTypeVersion saves named the type of the spell
            // and kept no flags, and the argument of an effect was the index of an attribute, a skill or an actor.
            ActiveSpells result;
            roundTripWith(
                MaxActiveSpellTypeVersion,
                [&](ESMWriter& writer) {
                    writer.writeHNRefId("ID__", id("oldspell"));
                    writer.writeHNT("CAST", std::uint32_t{ 7 });
                    writer.writeHNString("DISP", "Old spell");
                    writer.writeHNT("TYPE", Compatibility::ActiveSpells::Type_Ability);
                    writer.writeHNT("MGEF", std::int32_t{ MagicEffect::refIdToIndex(MagicEffect::FortifyAttribute) });
                    writer.writeHNT("ARG_", std::int32_t{ 2 });
                    writer.writeHNT("MAGN", 10.f);
                    writer.writeHNT("MAGN", 10.f);
                    writer.writeHNT("MAGN", 10.f);
                    writer.writeHNT("DURA", 5.f);
                    writer.writeHNT("EIND", std::int32_t{ 0 });
                    writer.writeHNT("LEFT", 5.f);
                    writer.writeHNT("FLAG", std::int32_t{ 0 });
                },
                [&](ESMReader& reader) { result.load(reader); });

            ASSERT_EQ(result.mSpells.size(), 1u);
            EXPECT_EQ(result.mSpells[0].mSourceSpellId, id("oldspell"));
            EXPECT_EQ(result.mSpells[0].mFlags, Compatibility::ActiveSpells::Type_Ability_Flags);
            EXPECT_EQ(result.mSpells[0].mWorsenings, -1);
            ASSERT_EQ(result.mSpells[0].mEffects.size(), 1u);
            EXPECT_EQ(result.mSpells[0].mEffects[0].getSkillOrAttribute(), Attribute::indexToRefId(2));
        }

        TEST_P(Esm3SaveGameRecordTest, projectileStateKeepsTheArrowInFlight)
        {
            if (GetParam() < CurrentSaveGameFormatVersion)
                return;
            ProjectileState record;
            record.mId = id("arrow");
            setValues(record.mPosition.mValues, { 1.f, 2.f, 3.f });
            setValues(record.mOrientation.mValues, { 0.f, 0.f, 0.f, 1.f });
            record.mCaster = RefNum{ .mIndex = 5, .mContentFile = -1 };
            record.mBowId = id("huntingrifle");
            setValues(record.mVelocity.mValues, { 4.f, 5.f, 6.f });
            record.mAttackStrength = 0.5f;
            record.mAttackWindUp = 0.25f;

            ProjectileState result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, id("arrow"));
            EXPECT_EQ(result.mPosition.mValues[2], 3.f);
            EXPECT_EQ(result.mOrientation.mValues[3], 1.f);
            EXPECT_EQ(result.mCaster.mIndex, 5u);
            EXPECT_EQ(result.mBowId, id("huntingrifle"));
            EXPECT_EQ(result.mVelocity.mValues[0], 4.f);
            EXPECT_EQ(result.mAttackStrength, 0.5f);
            EXPECT_EQ(result.mAttackWindUp, 0.25f);
        }

        TEST_P(Esm3SaveGameRecordTest, magicBoltKeepsTheSpellTheSpeedAndTheItem)
        {
            if (GetParam() < CurrentSaveGameFormatVersion)
                return;
            MagicBoltState record;
            record.mId = id("bolt");
            setValues(record.mPosition.mValues, { 1.f, 2.f, 3.f });
            setValues(record.mOrientation.mValues, { 0.f, 1.f, 0.f, 0.f });
            record.mCaster = RefNum{ .mIndex = 5, .mContentFile = -1 };
            record.mSpellId = id("fireball");
            record.mSpeed = 1500.f;
            record.mItem = RefNum{ .mIndex = 9, .mContentFile = -1 };

            MagicBoltState result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mSpellId, id("fireball"));
            EXPECT_EQ(result.mSpeed, 1500.f);
            EXPECT_EQ(result.mItem.mIndex, 9u);

            record.mItem = RefNum{};
            MagicBoltState withoutItem;
            saveAndLoadRecord(record, GetParam(), withoutItem);
            EXPECT_FALSE(withoutItem.mItem.isSet());
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3SaveGameRecordTest, ValuesIn(getFormats()));
    }
}
