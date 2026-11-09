#include "saveloadharness.hpp"

#include <components/esm3/cellid.hpp>
#include <components/esm3/cellref.hpp>
#include <components/esm3/loadcell.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

// The cells of the content files: what is saved with them, how the references that they place are found again
// after the cell has been skipped, and how the references that were moved from one cell to another are told from
// the rest. Placing an object in the wrong cell is what is at stake, so the numbers of the references of the plugins
// that a plugin depends on are tested as well.
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

        struct Esm3CellTest : public TestWithParam<FormatVersion>
        {
        };

        // The 12 bytes of the DATA subrecord of a cell: the flags and the grid.
        void writeData(ESMWriter& writer, std::int32_t flags, std::int32_t x = 0, std::int32_t y = 0)
        {
            const std::int32_t data[3] = { flags, x, y };
            writer.writeHNT("DATA", data);
        }

        // The marker of a reference that was moved to the exterior cell, which is followed by the reference itself.
        void writeMoved(ESMWriter& writer, std::uint32_t refNum, std::int32_t x, std::int32_t y)
        {
            writer.writeHNT("MVRF", refNum);
            const std::int32_t target[2] = { x, y };
            writer.writeHNT("CNDT", target);
        }

        Cell makeInteriorCell()
        {
            Cell cell;
            cell.blank();
            cell.mName = "Balmora, Caius Cosades' House";
            cell.mData.mFlags = Cell::Interior | Cell::HasWater;
            cell.mWater = 12.5f;
            cell.mHasWaterHeightSub = true;
            cell.mHasAmbi = true;
            cell.mAmbi = { .mAmbient = 0x112233, .mSunlight = 0x445566, .mFog = 0x778899, .mFogDensity = 0.25f };
            cell.updateId();
            return cell;
        }

        Cell makeExteriorCell()
        {
            Cell cell;
            cell.blank();
            cell.mData.mFlags = 0;
            cell.mData.mX = 3;
            cell.mData.mY = -4;
            cell.mRegion = id("Ascadian Isles Region");
            cell.mMapColor = 0x123456;
            cell.updateId();
            return cell;
        }

        template <class Save>
        Cell loadCellWith(FormatVersion format, Save&& save, bool saveContext = true)
        {
            Cell result;
            roundTripWith(format, save, [&](ESMReader& reader) {
                bool deleted = false;
                result.load(reader, deleted, saveContext);
            });
            return result;
        }

        TEST_P(Esm3CellTest, interiorCellKeepsItsWaterAndItsAmbientLight)
        {
            const Cell record = makeInteriorCell();

            const Cell result = loadCellWith(GetParam(), [&](ESMWriter& writer) { record.save(writer); });

            EXPECT_EQ(result.mName, record.mName);
            EXPECT_EQ(result.mId, id("Balmora, Caius Cosades' House"));
            EXPECT_FALSE(result.isExterior());
            EXPECT_TRUE(result.hasWater());
            EXPECT_EQ(result.mWater, 12.5f);
            EXPECT_TRUE(result.mHasWaterHeightSub);
            EXPECT_TRUE(result.hasAmbient());
            EXPECT_EQ(result.mAmbi.mAmbient, 0x112233u);
            EXPECT_EQ(result.mAmbi.mSunlight, 0x445566u);
            EXPECT_EQ(result.mAmbi.mFog, 0x778899u);
            EXPECT_EQ(result.mAmbi.mFogDensity, 0.25f);
            EXPECT_EQ(result.mContextList.size(), 1u);
        }

        TEST_P(Esm3CellTest, exteriorCellKeepsItsGridItsRegionAndItsMapColor)
        {
            const Cell record = makeExteriorCell();

            const Cell result = loadCellWith(GetParam(), [&](ESMWriter& writer) { record.save(writer); });

            EXPECT_TRUE(result.isExterior());
            EXPECT_TRUE(result.hasWater());
            EXPECT_EQ(result.getGridX(), 3);
            EXPECT_EQ(result.getGridY(), -4);
            EXPECT_EQ(result.mId, RefId::esm3ExteriorCell(3, -4));
            EXPECT_EQ(result.mRegion, id("Ascadian Isles Region"));
            EXPECT_EQ(result.mMapColor, 0x123456);
            EXPECT_FALSE(result.hasAmbient());
        }

        TEST_P(Esm3CellTest, interiorCellThatBehavesLikeAnExteriorKeepsItsRegionInsteadOfItsAmbientLight)
        {
            Cell record = makeInteriorCell();
            record.mData.mFlags |= Cell::QuasiEx;
            record.mRegion = id("Solstheim Region");

            const Cell result = loadCellWith(GetParam(), [&](ESMWriter& writer) { record.save(writer); });

            EXPECT_EQ(result.mRegion, id("Solstheim Region"));
            EXPECT_FALSE(result.hasAmbient());
            EXPECT_EQ(result.mWater, 12.5f);
        }

        TEST_P(Esm3CellTest, interiorCellWithoutWaterHeightKeepsItNotSet)
        {
            Cell record = makeInteriorCell();
            record.mHasWaterHeightSub = false;

            const Cell result = loadCellWith(GetParam(), [&](ESMWriter& writer) { record.save(writer); });

            EXPECT_FALSE(result.mHasWaterHeightSub);
            EXPECT_TRUE(result.hasAmbient());
        }

        TEST_P(Esm3CellTest, deletedCellKeepsItsNameAndItsData)
        {
            const Cell record = makeInteriorCell();
            Cell result;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(deleted);
            EXPECT_EQ(result.mName, record.mName);
            EXPECT_EQ(result.mData.mFlags, record.mData.mFlags);
            EXPECT_FALSE(result.mHasWaterHeightSub);
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3CellTest, ValuesIn(getFormats()));

        TEST(Esm3CellLoadingTest, waterHeightOfAnOldFileIsAnInteger)
        {
            const Cell result = loadCellWith(CurrentContentFormatVersion, [](ESMWriter& writer) {
                writer.writeHNCString("NAME", "Cave");
                writeData(writer, Cell::Interior);
                writer.writeHNT("INTV", std::int32_t{ -37 });
            });

            EXPECT_TRUE(result.mHasWaterHeightSub);
            EXPECT_EQ(result.mWater, -37.f);
            EXPECT_FALSE(result.hasAmbient());
        }

        TEST(Esm3CellLoadingTest, waterHeightThatIsNotANumberIsReplacedByTheLargestHeightInANewCell)
        {
            const Cell result = loadCellWith(CurrentContentFormatVersion, [](ESMWriter& writer) {
                writer.writeHNCString("NAME", "");
                writeData(writer, 0, 1, 2);
                writer.writeHNT("WHGT", std::numeric_limits<float>::quiet_NaN());
            });

            EXPECT_TRUE(result.mHasWaterHeightSub);
            EXPECT_EQ(result.mWater, std::numeric_limits<float>::max());
        }

        TEST(Esm3CellLoadingTest, waterHeightThatIsNotANumberKeepsTheOldHeightOfACellThatIsOverridden)
        {
            const Cell result = loadCellWith(CurrentContentFormatVersion, [](ESMWriter& writer) {
                writer.writeHNCString("NAME", "Seyda Neen");
                writeData(writer, 0, 1, 2);
                writer.writeHNT("WHGT", std::numeric_limits<float>::infinity());
            });

            EXPECT_TRUE(result.mHasWaterHeightSub);
            EXPECT_EQ(result.mWater, 0.f);
        }

        TEST(Esm3CellLoadingTest, theCounterOfTheReferencesIsKept)
        {
            const Cell result = loadCellWith(CurrentContentFormatVersion, [](ESMWriter& writer) {
                writer.writeHNCString("NAME", "Cave");
                writeData(writer, Cell::Interior);
                writer.writeHNT("NAM0", std::int32_t{ 42 });
            });

            EXPECT_EQ(result.mRefNumCounter, 42);
        }

        TEST(Esm3CellLoadingTest, cellWithoutDataFails)
        {
            try
            {
                loadCellWith(
                    CurrentContentFormatVersion, [](ESMWriter& writer) { writer.writeHNCString("NAME", "Cave"); });
                FAIL() << "Expected an exception";
            }
            catch (const std::exception& e)
            {
                EXPECT_THAT(e.what(), HasSubstr("Missing DATA subrecord"));
            }
        }

        TEST(Esm3CellLoadingTest, theContextIsNotSavedWhenTheCallerAsksToKeepReading)
        {
            const Cell record = makeInteriorCell();
            Cell result;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer); },
                [&](ESMReader& reader) {
                    bool deleted = false;
                    result.loadNameAndData(reader, deleted);
                    result.loadCell(reader, false);
                    EXPECT_TRUE(result.mContextList.empty());
                    result.postLoad(reader);
                });

            EXPECT_EQ(result.mContextList.size(), 1u);
            EXPECT_EQ(result.mName, record.mName);
            EXPECT_EQ(result.mAmbi.mFogDensity, 0.25f);
        }

        TEST(Esm3CellLoadingTest, temporaryMarkerIsOnlyWrittenWhenThereAreTemporaryReferences)
        {
            const Cell record = makeInteriorCell();
            std::int32_t counter = -1;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    record.saveTempMarker(writer, 0);
                    record.saveTempMarker(writer, 3);
                },
                [&](ESMReader& reader) { reader.getHNT(counter, "NAM0"); });
            EXPECT_EQ(counter, 3);
        }

        TEST(Esm3CellLoadingTest, theDescriptionTellsWhereAnExteriorCellIs)
        {
            Cell interior = makeInteriorCell();
            EXPECT_EQ(interior.getDescription(), "Balmora, Caius Cosades' House");

            Cell named = makeExteriorCell();
            named.mName = "Seyda Neen";
            EXPECT_EQ(named.getDescription(), "Seyda Neen (3, -4)");

            Cell inRegion = makeExteriorCell();
            EXPECT_EQ(inRegion.getDescription(), "Ascadian Isles Region (3, -4)");

            Cell wilderness = makeExteriorCell();
            wilderness.mRegion = RefId();
            EXPECT_EQ(wilderness.getDescription(), "Wilderness (3, -4)");
        }

        TEST(Esm3CellLoadingTest, theIdOfACellIsItsNameOrItsGrid)
        {
            EXPECT_EQ(Cell::generateIdForCell(false, "Cave", 5, 6), id("Cave"));
            EXPECT_EQ(Cell::generateIdForCell(true, "Seyda Neen", 5, 6), RefId::esm3ExteriorCell(5, 6));
        }

        // The reference of the plugin at the position in the list of the masters, or 0 for the plugin itself.
        CellRef makeRef(std::uint32_t index, std::string_view name, std::int32_t master = 0)
        {
            CellRef ref = makeBlankCellRef();
            ref.mRefNum.mIndex = index;
            ref.mRefNum.mContentFile = master;
            ref.mRefID = id(name);
            ref.mPos.pos[0] = 100.f;
            return ref;
        }

        // The references that follow the cell in the same record, as the content files have them.
        struct CellWithReferences
        {
            std::vector<CellRef> mReferences;

            void write(ESMWriter& writer) const
            {
                makeExteriorCell().save(writer);
                for (const CellRef& ref : mReferences)
                    ref.save(writer);
            }
        };

        TEST(Esm3CellReferencesTest, referencesAreReadInTheOrderThatTheyWereWritten)
        {
            CellWithReferences cell;
            cell.mReferences = { makeRef(1, "barrel"), makeRef(2, "crate"), makeRef(3, "chest") };

            std::vector<std::string> names;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { cell.write(writer); },
                [&](ESMReader& reader) {
                    reader.setIndex(4);
                    Cell loaded;
                    bool deleted = false;
                    loaded.load(reader, deleted);
                    loaded.restore(reader, 0);

                    CellRef ref;
                    while (Cell::getNextRef(reader, ref, deleted))
                    {
                        names.push_back(ref.mRefID.getRefIdString());
                        // The numbers of the references of the plugin itself belong to the plugin.
                        EXPECT_EQ(ref.mRefNum.mContentFile, 4);
                        EXPECT_FALSE(deleted);
                    }
                });

            EXPECT_THAT(names, ElementsAre("barrel", "crate", "chest"));
        }

        TEST(Esm3CellReferencesTest, referenceOfAPluginThatTheCurrentOneDependsOnIsRenumbered)
        {
            CellWithReferences cell;
            // The most significant byte is the position of the plugin in the list of the masters of the current one.
            cell.mReferences = { makeRef(7, "barrel", 2), makeRef(8, "crate", 9), makeRef(9, "chest") };

            std::vector<CellRef> refs;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { cell.write(writer); },
                [&](ESMReader& reader) {
                    reader.setIndex(4);
                    Cell loaded;
                    bool deleted = false;
                    loaded.load(reader, deleted);
                    loaded.mContextList[0].parentFileIndices = { 1, 3 };
                    loaded.restore(reader, 0);

                    CellRef ref;
                    while (Cell::getNextRef(reader, ref, deleted))
                        refs.push_back(ref);
                });

            ASSERT_EQ(refs.size(), 3u);
            // A reference that was already placed by the second master overwrites that reference.
            EXPECT_EQ(refs[0].mRefNum.mIndex, 7u);
            EXPECT_EQ(refs[0].mRefNum.mContentFile, 3);
            // A number that points beyond the masters can only be a mistake of the plugin, and it keeps the plugin.
            EXPECT_EQ(refs[1].mRefNum.mContentFile, 4);
            EXPECT_EQ(refs[2].mRefNum.mIndex, 9u);
            EXPECT_EQ(refs[2].mRefNum.mContentFile, 4);
        }

        TEST(Esm3CellReferencesTest, movedReferencesAreSkippedByTheSimpleReader)
        {
            std::vector<std::string> names;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    makeExteriorCell().save(writer);
                    writeMoved(writer, 55, 7, 8);
                    makeRef(55, "moved").save(writer);
                    makeRef(56, "kept").save(writer);
                },
                [&](ESMReader& reader) {
                    Cell loaded;
                    bool deleted = false;
                    loaded.load(reader, deleted);
                    loaded.restore(reader, 0);

                    CellRef ref;
                    while (Cell::getNextRef(reader, ref, deleted))
                        names.push_back(ref.mRefID.getRefIdString());
                });

            EXPECT_THAT(names, ElementsAre("kept"));
        }

        TEST(Esm3CellReferencesTest, movedMarkerWithoutAReferenceEndsTheListOfTheSimpleReader)
        {
            bool found = true;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    makeExteriorCell().save(writer);
                    writeMoved(writer, 55, 7, 8);
                },
                [&](ESMReader& reader) {
                    Cell loaded;
                    bool deleted = false;
                    loaded.load(reader, deleted);
                    loaded.restore(reader, 0);

                    CellRef ref;
                    found = Cell::getNextRef(reader, ref, deleted);
                });

            EXPECT_FALSE(found);
        }

        struct ReadReferences
        {
            std::vector<std::string> mNames;
            std::vector<bool> mMoved;
            std::vector<std::int32_t> mTargetX;
        };

        ReadReferences readWithMode(Cell::GetNextRefMode mode)
        {
            ReadReferences result;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    makeExteriorCell().save(writer);
                    makeRef(1, "still").save(writer);
                    writeMoved(writer, 2, 7, 8);
                    makeRef(2, "moved").save(writer);
                    makeRef(3, "also_still").save(writer);
                },
                [&](ESMReader& reader) {
                    Cell loaded;
                    bool deleted = false;
                    loaded.load(reader, deleted);
                    loaded.restore(reader, 0);

                    CellRef ref;
                    MovedCellRef movedRef{};
                    bool moved = false;
                    while (Cell::getNextRef(reader, ref, deleted, movedRef, moved, mode))
                    {
                        if (mode == Cell::GetNextRefMode::LoadOnlyMoved && !moved)
                            continue;
                        if (mode == Cell::GetNextRefMode::LoadOnlyNotMoved && moved)
                            continue;
                        result.mNames.push_back(ref.mRefID.getRefIdString());
                        result.mMoved.push_back(moved);
                        if (moved)
                            result.mTargetX.push_back(movedRef.mTarget[0]);
                    }
                });
            return result;
        }

        TEST(Esm3CellReferencesTest, everyReferenceCanBeReadAlongWithTheMovedOnes)
        {
            const ReadReferences result = readWithMode(Cell::GetNextRefMode::LoadAll);

            EXPECT_THAT(result.mNames, ElementsAre("still", "moved", "also_still"));
            EXPECT_THAT(result.mMoved, ElementsAre(false, true, false));
            EXPECT_THAT(result.mTargetX, ElementsAre(7));
        }

        TEST(Esm3CellReferencesTest, onlyTheMovedReferencesCanBeRead)
        {
            const ReadReferences result = readWithMode(Cell::GetNextRefMode::LoadOnlyMoved);

            EXPECT_THAT(result.mNames, ElementsAre("moved"));
            EXPECT_THAT(result.mMoved, ElementsAre(true));
        }

        TEST(Esm3CellReferencesTest, onlyTheReferencesThatStayCanBeRead)
        {
            const ReadReferences result = readWithMode(Cell::GetNextRefMode::LoadOnlyNotMoved);

            EXPECT_THAT(result.mNames, ElementsAre("still", "also_still"));
            EXPECT_THAT(result.mMoved, ElementsAre(false, false));
        }

        TEST(Esm3CellReferencesTest, movedReferenceMarkerHasTheNumberAndTheTargetOfTheReference)
        {
            MovedCellRef moved{};
            roundTripWith(
                CurrentContentFormatVersion, [](ESMWriter& writer) { writeMoved(writer, 0x01000064, -3, 4); },
                [&](ESMReader& reader) {
                    reader.setIndex(6);
                    reader.getSubName();
                    EXPECT_TRUE(Cell::getNextMVRF(reader, moved));
                });

            // There is no master to the plugin, so the reference is its own.
            EXPECT_EQ(moved.mRefNum.mIndex, 0x01000064u);
            EXPECT_EQ(moved.mRefNum.mContentFile, 6);
            EXPECT_EQ(moved.mTarget[0], -3);
        }

        TEST(Esm3CellReferencesTest, listOfReferencesEndsWhenThereAreNoMoreSubrecords)
        {
            bool found = true;
            bool movedFound = true;
            roundTripWith(
                CurrentContentFormatVersion, [](ESMWriter& writer) { makeExteriorCell().save(writer); },
                [&](ESMReader& reader) {
                    Cell loaded;
                    bool deleted = false;
                    loaded.load(reader, deleted);
                    loaded.restore(reader, 0);

                    CellRef ref;
                    found = Cell::getNextRef(reader, ref, deleted);
                    MovedCellRef movedRef{};
                    bool moved = false;
                    movedFound = Cell::getNextRef(reader, ref, deleted, movedRef, moved);
                });

            EXPECT_FALSE(found);
            EXPECT_FALSE(movedFound);
        }

        TEST(Esm3CellReferencesTest, trackedReferencesAreFoundByTheirNumbers)
        {
            CellRef ref = makeRef(12, "barrel");
            MovedCellRef moved{};
            moved.mRefNum = ref.mRefNum;

            EXPECT_TRUE(moved == ref.mRefNum);
            EXPECT_TRUE(ref == ref.mRefNum);
            RefNum other;
            other.mIndex = 13;
            EXPECT_FALSE(ref == other);

            CellRefTracker tracker;
            tracker.emplace_back(ref, false);
            EXPECT_TRUE(CellRefTrackerPredicate(ref.mRefNum)(tracker.front()));
            EXPECT_FALSE(CellRefTrackerPredicate(other)(tracker.front()));
        }

        // The data of a reference that the loader has to keep apart from the other references that follow it.
        CellRef makeFullRef()
        {
            CellRef ref = makeRef(21, "door_cell");
            ref.mScale = 1.5f;
            ref.mOwner = id("fargoth");
            ref.mGlobalVariable = "rent_room";
            ref.mSoul = id("scrib");
            ref.mFaction = id("Thieves Guild");
            ref.mFactionRank = 3;
            ref.mEnchantmentCharge = 25.f;
            ref.mChargeInt = 40;
            ref.mCount = 5;
            ref.mTeleport = true;
            ref.mDoorDest = { .pos = { 1.f, 2.f, 3.f }, .rot = { 0.f, 0.f, 2.f } };
            ref.mDestCell = "Caldera";
            ref.mLockLevel = 60;
            ref.mIsLocked = true;
            ref.mKey = id("key_standard");
            ref.mTrap = id("trap_fire");
            ref.mReferenceBlocked = 1;
            return ref;
        }

        TEST(Esm3CellReferencesTest, referenceKeepsEverythingThatCanBeSetOnIt)
        {
            const CellRef record = makeFullRef();
            CellRef result;
            bool deleted = true;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_FALSE(deleted);
            EXPECT_EQ(result.mRefID, id("door_cell"));
            EXPECT_EQ(result.mScale, 1.5f);
            EXPECT_EQ(result.mOwner, id("fargoth"));
            EXPECT_EQ(result.mGlobalVariable, "rent_room");
            EXPECT_EQ(result.mSoul, id("scrib"));
            EXPECT_EQ(result.mFaction, id("Thieves Guild"));
            EXPECT_EQ(result.mFactionRank, 3);
            EXPECT_EQ(result.mEnchantmentCharge, 25.f);
            EXPECT_EQ(result.mChargeInt, 40);
            EXPECT_EQ(result.mCount, 5);
            EXPECT_TRUE(result.mTeleport);
            EXPECT_EQ(result.mDoorDest.pos[2], 3.f);
            EXPECT_EQ(result.mDoorDest.rot[2], 2.f);
            EXPECT_EQ(result.mDestCell, "Caldera");
            EXPECT_EQ(result.mLockLevel, 60);
            EXPECT_TRUE(result.mIsLocked);
            EXPECT_EQ(result.mKey, id("key_standard"));
            EXPECT_EQ(result.mTrap, id("trap_fire"));
            EXPECT_EQ(result.mReferenceBlocked, 1);
            EXPECT_EQ(result.mPos.pos[0], 100.f);
        }

        TEST(Esm3CellReferencesTest, lockWithoutALevelIsStillLockedAndKeepsALevelOfZero)
        {
            CellRef record = makeRef(22, "chest");
            record.mIsLocked = true;
            record.mLockLevel = 0;
            record.mKey = id("key_chest");
            CellRef result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(result.mIsLocked);
            EXPECT_EQ(result.mLockLevel, 0);
            EXPECT_EQ(result.mKey, id("key_chest"));
        }

        TEST(Esm3CellReferencesTest, referenceWithAKeyIsLockedInAContentFileButNotInASavedGame)
        {
            auto write = [](ESMWriter& w) {
                w.writeFormId(FormId{ .mIndex = 30, .mContentFile = 0 });
                w.writeHNCRefId("NAME", RefId::stringRefId("chest"));
                w.writeHNOCRefId("KNAM", RefId::stringRefId("key_chest"));
            };

            for (const bool contentFile : { true, false })
            {
                CellRef result;
                bool deleted = false;
                roundTripWith(contentFile ? DefaultFormatVersion : CurrentSaveGameFormatVersion, write,
                    [&](ESMReader& reader) { result.load(reader, deleted); });
                EXPECT_EQ(result.mIsLocked, contentFile) << contentFile;
            }
        }

        TEST(Esm3CellReferencesTest, wideReferenceNumbersKeepTheContentFile)
        {
            CellRef record = makeRef(31, "barrel");
            record.mRefNum.mContentFile = 5;
            CellRef result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted, true); });

            EXPECT_EQ(result.mRefNum.mIndex, 31u);
            EXPECT_EQ(result.mRefNum.mContentFile, 5);
        }

        TEST(Esm3CellReferencesTest, deletedReferenceKeepsItsNumberAndItsId)
        {
            const CellRef record = makeRef(32, "barrel");
            CellRef result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer, false, false, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(deleted);
            EXPECT_EQ(result.mRefNum.mIndex, 32u);
            EXPECT_EQ(result.mRefID, id("barrel"));
        }

        TEST(Esm3CellReferencesTest, itemInAnInventoryHasNoOwnerNoLockAndNoPosition)
        {
            CellRef record = makeFullRef();
            CellRef result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer, false, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(result.mOwner.empty());
            EXPECT_TRUE(result.mFaction.empty());
            EXPECT_FALSE(result.mTeleport);
            EXPECT_FALSE(result.mIsLocked);
            EXPECT_TRUE(result.mKey.empty());
            EXPECT_EQ(result.mPos.pos[0], 0.f);
            // The rest is kept with the item.
            EXPECT_EQ(result.mSoul, id("scrib"));
            EXPECT_EQ(result.mCount, 5);
            EXPECT_EQ(result.mEnchantmentCharge, 25.f);
            EXPECT_EQ(result.mGlobalVariable, "rent_room");
        }

        TEST(Esm3CellReferencesTest, scaleOfAReferenceStaysInTheRangeThatTheGameAllows)
        {
            CellRef small = makeRef(40, "barrel");
            small.mScale = 0.1f;
            CellRef large = makeRef(41, "crate");
            large.mScale = 9.f;

            std::vector<float> scales;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    small.save(writer);
                    large.save(writer);
                },
                [&](ESMReader& reader) {
                    bool deleted = false;
                    CellRef ref;
                    ref.load(reader, deleted);
                    scales.push_back(ref.mScale);
                    ref.load(reader, deleted);
                    scales.push_back(ref.mScale);
                });

            EXPECT_THAT(scales, ElementsAre(0.5f, 2.f));
        }

        TEST(Esm3CellReferencesTest, markerOfTheTemporaryReferencesIsSkipped)
        {
            CellRef result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    writer.writeHNT("NAM0", std::int32_t{ 12 });
                    makeRef(50, "barrel").save(writer);
                    writer.writeHNT("NAM0", std::int32_t{ 12 });
                },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_EQ(result.mRefNum.mIndex, 50u);
            EXPECT_EQ(result.mRefID, id("barrel"));
        }

        TEST(Esm3CellReferencesTest, referenceWithoutAnIdIsStillRead)
        {
            CellRef result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) { writer.writeFormId(FormId{ .mIndex = 51, .mContentFile = 0 }); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_EQ(result.mRefNum.mIndex, 51u);
            EXPECT_TRUE(result.mRefID.empty());
        }

        TEST(Esm3CellReferencesTest, referencesThatDidNotMoveAreSkippedButStillWalkedOver)
        {
            std::vector<std::string> names;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    makeExteriorCell().save(writer);
                    makeFullRef().save(writer);
                    makeRef(70, "next").save(writer);
                },
                [&](ESMReader& reader) {
                    Cell loaded;
                    bool deleted = false;
                    loaded.load(reader, deleted);
                    loaded.restore(reader, 0);

                    CellRef ref;
                    MovedCellRef movedRef{};
                    bool moved = false;
                    while (Cell::getNextRef(reader, ref, deleted, movedRef, moved, Cell::GetNextRefMode::LoadOnlyMoved))
                        names.push_back(ref.mRefID.getRefIdString());
                });

            // Both references are skipped, since neither of them was moved, but the reader still walks over them.
            EXPECT_EQ(names.size(), 2u);
        }
    }
}
