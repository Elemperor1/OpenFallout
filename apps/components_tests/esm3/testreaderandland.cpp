#include "saveloadharness.hpp"

#include <components/esm/refid.hpp>
#include <components/esm3/cellid.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadland.hpp>
#include <components/esm3/readerscache.hpp>
#include <components/testing/util.hpp>
#include <components/toutf8/toutf8.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// What the reader does with a file that is not as it should be, and with the shapes that the numbers of the files
// can have; and the land, which is kept in the file until it is needed, so that it has to be found again.
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

        template <class Read>
        std::string failureOf(FormatVersion format, std::function<void(ESMWriter&)> write, Read&& read)
        {
            try
            {
                roundTripWith(format, write, read);
            }
            catch (const std::exception& e)
            {
                return e.what();
            }
            return {};
        }

        // The strings

        struct Esm3ReaderStringsTest : public TestWithParam<FormatVersion>
        {
        };

        TEST_P(Esm3ReaderStringsTest, stringsAndIdentifiersCanBeSkippedAndTheNextOnesAreStillRead)
        {
            std::string next;
            std::int32_t number = 0;
            roundTripWith(
                GetParam(),
                [](ESMWriter& writer) {
                    writer.writeHNCString("ONE_", "skipped");
                    writer.writeHNCRefId("TWO_", id("also skipped"));
                    writer.writeHNCString("THRE", "read");
                    writer.writeHNT("FOUR", std::int32_t{ 4 });
                },
                [&](ESMReader& reader) {
                    reader.getSubName();
                    reader.skipHString();
                    reader.getSubName();
                    reader.skipHRefId();
                    next = reader.getHNString("THRE");
                    reader.getHNT(number, "FOUR");
                });

            EXPECT_EQ(next, "read");
            EXPECT_EQ(number, 4);
        }

        TEST_P(Esm3ReaderStringsTest, optionalIdentifierIsOnlySkippedWhenItIsThere)
        {
            std::string next;
            RefId optional;
            roundTripWith(
                GetParam(),
                [](ESMWriter& writer) {
                    writer.writeHNCRefId("NAME", id("skipped"));
                    writer.writeHNCString("DATA", "kept");
                },
                [&](ESMReader& reader) {
                    reader.skipHNORefId("ANAM");
                    reader.skipHNORefId("NAME");
                    reader.skipHNORefId("NAME");
                    optional = reader.getHNORefId("MISS");
                    next = reader.getHNOString("DATA");
                });

            EXPECT_TRUE(optional.empty());
            EXPECT_EQ(next, "kept");
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3ReaderStringsTest, ValuesIn(getFormats()));

        // A string that the header says is empty but that has a zero byte, as one old plugin has them.
        void writeEmptyStringWithAZeroByte(ESMWriter& writer)
        {
            writer.startSubRecord("EMPT");
            writer.endRecord("EMPT");
            writer.write("\0", 1);
            writer.writeHNCString("NEXT", "after");
        }

        TEST(Esm3ReaderTest, stringThatIsEmptyButHasAZeroByteIsReadAsEmptyInOldFormats)
        {
            std::string empty = "unset";
            std::string next;
            roundTripWith(CurrentContentFormatVersion, writeEmptyStringWithAZeroByte, [&](ESMReader& reader) {
                reader.getSubName();
                empty = reader.getHString();
                next = reader.getHNString("NEXT");
            });

            EXPECT_EQ(empty, "");
            EXPECT_EQ(next, "after");
        }

        TEST(Esm3ReaderTest, stringThatIsEmptyButHasAZeroByteCanBeSkippedInOldFormats)
        {
            std::string next;
            roundTripWith(CurrentContentFormatVersion, writeEmptyStringWithAZeroByte, [&](ESMReader& reader) {
                reader.getSubName();
                reader.skipHString();
                next = reader.getHNString("NEXT");
            });

            EXPECT_EQ(next, "after");
        }

        TEST(Esm3ReaderTest, emptyStringIsReadAsEmptyInTheNewFormats)
        {
            std::string empty = "unset";
            roundTripWith(
                CurrentSaveGameFormatVersion, [](ESMWriter& writer) { writer.writeHNString("EMPT", ""); },
                [&](ESMReader& reader) { empty = reader.getHNString("EMPT"); });

            EXPECT_EQ(empty, "");
        }

        TEST(Esm3ReaderTest, stringThatDoesNotFitItsSubrecordFails)
        {
            const std::string message = failureOf(
                MaxStringRefIdFormatVersion,
                [](ESMWriter& writer) {
                    writer.startSubRecord("BIG_");
                    writer.writeT(std::uint32_t{ 100 });
                    writer.write("short", 5);
                    writer.endRecord("BIG_");
                },
                [](ESMReader& reader) {
                    reader.getSubName();
                    reader.getSubHeader();
                    reader.getMaybeFixedRefIdSize(32);
                });
            EXPECT_THAT(message, HasSubstr("String does not fit subrecord"));
        }

        // The sizes of the subrecords

        TEST(Esm3ReaderTest, subrecordWithTheExpectedSizeCanBeSkipped)
        {
            std::int32_t next = 0;
            roundTripWith(
                CurrentContentFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeHNT("ONE_", std::int64_t{ 1 });
                    writer.writeHNT("TWO_", std::int32_t{ 2 });
                },
                [&](ESMReader& reader) {
                    reader.getSubName();
                    reader.skipHSubSize(8);
                    reader.getHNT(next, "TWO_");
                });

            EXPECT_EQ(next, 2);
        }

        TEST(Esm3ReaderTest, subrecordWithAnotherSizeThanExpectedFails)
        {
            const std::string message = failureOf(
                CurrentContentFormatVersion, [](ESMWriter& writer) { writer.writeHNT("ONE_", std::int64_t{ 1 }); },
                [](ESMReader& reader) {
                    reader.getSubName();
                    reader.skipHSubSize(4);
                });
            EXPECT_THAT(message, HasSubstr("record size mismatch"));
        }

        TEST(Esm3ReaderTest, subrecordsCanBeSkippedUntilTheOneThatIsWanted)
        {
            std::int32_t wanted = 0;
            bool more = true;
            roundTripWith(
                CurrentContentFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeHNT("ONE_", std::int32_t{ 1 });
                    writer.writeHNT("TWO_", std::int32_t{ 2 });
                    writer.writeHNT("WANT", std::int32_t{ 3 });
                    writer.writeHNT("LAST", std::int32_t{ 4 });
                },
                [&](ESMReader& reader) {
                    reader.skipHSubUntil("WANT");
                    reader.getHNT(wanted, "WANT");
                    reader.skipHSubUntil("NOPE");
                    more = reader.hasMoreSubs();
                });

            EXPECT_EQ(wanted, 3);
            EXPECT_FALSE(more);
        }

        // The cells

        TEST(Esm3ReaderTest, cellIdOfAnOldFileTellsTheInteriorFromTheExterior)
        {
            RefId interior;
            RefId exterior;
            roundTripWith(
                CurrentContentFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeCellId(id("Balmora, Council Club"));
                    writer.writeCellId(RefId::esm3ExteriorCell(-2, 7));
                },
                [&](ESMReader& reader) {
                    interior = reader.getCellId();
                    exterior = reader.getCellId();
                });

            EXPECT_EQ(interior, id("Balmora, Council Club"));
            EXPECT_EQ(exterior, RefId::esm3ExteriorCell(-2, 7));
        }

        TEST(Esm3ReaderTest, cellIdOfANewFileIsAnIdentifier)
        {
            RefId interior;
            RefId exterior;
            roundTripWith(
                CurrentSaveGameFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeCellId(id("Balmora, Council Club"));
                    writer.writeCellId(RefId::esm3ExteriorCell(-2, 7));
                },
                [&](ESMReader& reader) {
                    interior = reader.getCellId();
                    exterior = reader.getCellId();
                });

            EXPECT_EQ(interior, id("Balmora, Council Club"));
            EXPECT_EQ(exterior, RefId::esm3ExteriorCell(-2, 7));
        }

        // The identifiers

        TEST(Esm3ReaderTest, identifiersOfEveryKindAreKept)
        {
            const std::vector<RefId> ids = {
                RefId(),
                id("string"),
                RefId(FormId{ .mIndex = 0x1234, .mContentFile = 3 }),
                RefId::generated(0x1122334455667788ull),
                RefId::index(REC_STAT, 77),
                RefId::esm3ExteriorCell(-5, 6),
            };
            std::vector<RefId> loaded;
            roundTripWith(
                CurrentSaveGameFormatVersion,
                [&](ESMWriter& writer) {
                    for (const RefId& value : ids)
                        writer.writeHNRefId("ID__", value);
                },
                [&](ESMReader& reader) {
                    for (std::size_t i = 0; i < ids.size(); ++i)
                        loaded.push_back(reader.getHNRefId("ID__"));
                });

            EXPECT_EQ(loaded, ids);
        }

        TEST(Esm3ReaderTest, identifierOfAFileThatIsNotLoadedAnymoreIsEmpty)
        {
            const std::map<int, int> mapping{ { 1, 5 } };
            std::vector<RefId> loaded;
            roundTripWith(
                CurrentSaveGameFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeHNRefId("ID__", RefId(FormId{ .mIndex = 7, .mContentFile = 1 }));
                    writer.writeHNRefId("ID__", RefId(FormId{ .mIndex = 8, .mContentFile = 2 }));
                },
                [&](ESMReader& reader) {
                    reader.setContentFileMapping(&mapping);
                    for (int i = 0; i < 2; ++i)
                        loaded.push_back(reader.getHNRefId("ID__"));
                });

            ASSERT_EQ(loaded.size(), 2u);
            EXPECT_EQ(loaded[0], RefId(FormId{ .mIndex = 7, .mContentFile = 5 }));
            EXPECT_TRUE(loaded[1].empty());
        }

        TEST(Esm3ReaderTest, formIdsAreRenumberedToo)
        {
            const std::map<int, int> mapping{ { 1, 5 } };
            FormId kept{ .mIndex = 1, .mContentFile = 1 };
            FormId removed{ .mIndex = 1, .mContentFile = 4 };
            FormId generated{ .mIndex = 1, .mContentFile = -1 };
            ESMReader reader;
            EXPECT_TRUE(reader.applyContentFileMapping(kept));
            EXPECT_EQ(kept.mContentFile, 1);

            reader.setContentFileMapping(&mapping);
            EXPECT_TRUE(reader.applyContentFileMapping(kept));
            EXPECT_EQ(kept.mContentFile, 5);
            EXPECT_FALSE(reader.applyContentFileMapping(removed));
            EXPECT_TRUE(reader.applyContentFileMapping(generated));
            EXPECT_EQ(generated.mContentFile, -1);
        }

        // An identifier as a subrecord that was written by hand.
        std::string failureOfIdentifier(const std::string& bytes, std::size_t size)
        {
            return failureOf(
                CurrentSaveGameFormatVersion,
                [&](ESMWriter& writer) {
                    writer.startSubRecord("ID__");
                    writer.write(bytes.data(), bytes.size());
                    writer.endRecord("ID__");
                },
                [&](ESMReader& reader) {
                    reader.getSubName();
                    reader.getSubHeader();
                    reader.getRefId(size);
                });
        }

        TEST(Esm3ReaderTest, identifierThatIsTooSmallToHoldItsSizeFails)
        {
            EXPECT_THAT(failureOfIdentifier(std::string("\x01\x01", 2), 2),
                HasSubstr("Requested RefId record size is too small"));
        }

        TEST(Esm3ReaderTest, identifierWhoseStringDoesNotFitItsSubrecordFails)
        {
            const std::string bytes("\x01\x40\x00\x00\x00abc", 8);
            EXPECT_THAT(
                failureOfIdentifier(bytes, bytes.size()), HasSubstr("RefId string does not fit subrecord size"));
        }

        TEST(Esm3ReaderTest, identifierWithoutRoomForItsTypeFails)
        {
            EXPECT_THAT(
                failureOfIdentifier(std::string("\x02", 1), 0), HasSubstr("Requested RefId record size is too small"));
        }

        TEST(Esm3ReaderTest, identifierOfAnUnknownKindFails)
        {
            EXPECT_THAT(failureOfIdentifier(std::string("\x63", 1), 1), HasSubstr("Unsupported RefIdType: 99"));
        }

        // The records

        TEST(Esm3ReaderTest, recordCannotBeSkippedAwayFromBeforeThePreviousOneWasRead)
        {
            std::string message;
            try
            {
                ESMWriter writer;
                auto stream = std::make_unique<std::stringstream>();
                writer.setFormatVersion(CurrentContentFormatVersion);
                writer.save(*stream);
                for (int i = 0; i < 2; ++i)
                {
                    writer.startRecord(fakeRecordId);
                    writer.writeHNT("DATA", std::int32_t{ i });
                    writer.endRecord(fakeRecordId);
                }

                ESMReader reader;
                reader.open(std::move(stream), "stream");
                reader.getRecName();
                reader.getRecHeader();
                reader.getRecName();
            }
            catch (const std::exception& e)
            {
                message = e.what();
            }
            EXPECT_THAT(message, HasSubstr("Previous record contains unread bytes"));
        }

        TEST(Esm3ReaderTest, noRecordCanBeReadAfterTheLastOne)
        {
            const std::string message = failureOf(
                CurrentContentFormatVersion, [](ESMWriter& writer) { writer.writeHNT("DATA", std::int32_t{ 1 }); },
                [](ESMReader& reader) {
                    reader.skipRecord();
                    EXPECT_FALSE(reader.hasMoreRecs());
                    reader.getRecName();
                });
            EXPECT_THAT(message, HasSubstr("No more records"));
        }

        std::string makeFile(FormatVersion format, std::string_view tail = {})
        {
            ESMWriter writer;
            std::stringstream stream;
            writer.setFormatVersion(format);
            writer.save(stream);
            writer.close();
            return stream.str() + std::string(tail);
        }

        std::string openAndReadFirstRecord(const std::string& content)
        {
            try
            {
                ESMReader reader;
                reader.open(std::make_unique<std::istringstream>(content), "stream");
                reader.getRecName();
                reader.getRecHeader();
            }
            catch (const std::exception& e)
            {
                return e.what();
            }
            return {};
        }

        TEST(Esm3ReaderTest, fileThatDoesNotStartWithTheHeaderFails)
        {
            std::string content("FAKE", 4);
            content.append(12, '\0');
            EXPECT_THAT(openAndReadFirstRecord(content), HasSubstr("Not a valid Morrowind file"));
        }

        TEST(Esm3ReaderTest, fileThatEndsInsideTheHeaderOfARecordFails)
        {
            EXPECT_THAT(openAndReadFirstRecord(makeFile(CurrentContentFormatVersion, std::string("FAKE\x01\x00", 6))),
                HasSubstr("End of file while reading record header"));
        }

        TEST(Esm3ReaderTest, recordThatIsLongerThanTheFileFails)
        {
            std::string tail("FAKE", 4);
            const std::uint32_t size = 1000;
            tail.append(reinterpret_cast<const char*>(&size), sizeof(size));
            tail.append(8, '\0');
            tail.append(10, 'x');
            EXPECT_THAT(
                openAndReadFirstRecord(makeFile(CurrentContentFormatVersion, tail)), HasSubstr("record size mismatch"));
        }

        TEST(Esm3ReaderTest, subrecordHeaderThatDoesNotFitItsRecordFails)
        {
            const std::string message = failureOf(
                CurrentContentFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeHNT("ONE_", std::int32_t{ 1 });
                    // Two bytes of a record that is not complete.
                    writer.write("AB", 2);
                },
                [](ESMReader& reader) {
                    reader.getSubName();
                    reader.skipHSub();
                    reader.skipHSub();
                });
            EXPECT_THAT(message, HasSubstr("End of record while reading sub-record header"));
        }

        // The encodings

        TEST(Esm3ReaderTest, textOfAnOldEncodingIsConvertedWhenItIsReadAndWritten)
        {
            ToUTF8::Utf8Encoder encoder(ToUTF8::WINDOWS_1252);
            std::string fixed;
            std::string unlimited;
            std::string raw;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    writer.setEncoder(&encoder);
                    writer.writeHNCString("ONE_", "caf\xc3\xa9");
                    writer.writeHNString("TWO_", "na\xc3\xafve", 8);
                    writer.setEncoder(nullptr);
                    writer.writeHNCString("RAW_", "caf\xe9");
                },
                [&](ESMReader& reader) {
                    reader.setEncoder(&encoder);
                    unlimited = reader.getHNString("ONE_");
                    fixed = reader.getHNString("TWO_");
                    reader.setEncoder(nullptr);
                    raw = reader.getHNString("RAW_");
                });

            EXPECT_EQ(unlimited, "caf\xc3\xa9");
            EXPECT_EQ(fixed, "na\xc3\xafve");
            // The bytes of the file are the ones of the encoding, not the ones of UTF-8.
            EXPECT_EQ(raw, "caf\xe9");
        }

        TEST(Esm3ReaderTest, encodingsAreFoundByTheirNames)
        {
            EXPECT_EQ(ToUTF8::calculateEncoding("win1250"), ToUTF8::WINDOWS_1250);
            EXPECT_EQ(ToUTF8::calculateEncoding("win1251"), ToUTF8::WINDOWS_1251);
            EXPECT_EQ(ToUTF8::calculateEncoding("win1252"), ToUTF8::WINDOWS_1252);
            EXPECT_THROW(ToUTF8::calculateEncoding("win1253"), std::runtime_error);
            EXPECT_THAT(ToUTF8::encodingUsingMessage("win1250"), HasSubstr("Central and Eastern"));
            EXPECT_THAT(ToUTF8::encodingUsingMessage("win1251"), HasSubstr("Cyrillic"));
            EXPECT_THAT(ToUTF8::encodingUsingMessage("win1252"), HasSubstr("English"));
        }

        // The header and the writer

        TEST(Esm3WriterTest, headerKeepsTheMastersThatWereNotClearedAndTheTypeThatWasSet)
        {
            ESMWriter writer;
            std::stringstream stream;
            writer.setFormatVersion(CurrentContentFormatVersion);
            writer.setType(1);
            writer.setAuthor("Somebody");
            writer.addMaster("first.esm", 111);
            writer.addMaster("second.esm", 222);
            writer.clearMaster();
            writer.addMaster("third.esm", 333);
            writer.save(stream);
            writer.close();

            ESMReader reader;
            reader.open(std::make_unique<std::istringstream>(stream.str()), "stream");
            EXPECT_EQ(reader.getAuthor(), "Somebody");
            ASSERT_EQ(reader.getGameFiles().size(), 1u);
            EXPECT_EQ(reader.getGameFiles()[0].name, "third.esm");
            EXPECT_EQ(reader.getGameFiles()[0].size, 333u);
        }

        TEST(Esm3WriterTest, writerThatStillHasARecordOpenCannotBeClosed)
        {
            ESMWriter writer;
            std::stringstream stream;
            writer.setFormatVersion(CurrentContentFormatVersion);
            writer.save(stream);
            writer.startRecord(fakeRecordId);

            EXPECT_THROW(writer.close(), std::runtime_error);
        }

        // The parents of a plugin

        TEST(Esm3ReaderTest, parentsOfAPluginAreFoundAmongThePluginsThatWereLoadedBeforeIt)
        {
            ReadersCache readers(5);
            {
                const auto first = readers.get(0);
                first->openRaw(std::make_unique<std::istringstream>("123"), "First.esm");
                const auto second = readers.get(1);
                // A content file that is not in the format of the reader has no size, and is never a parent.
                second->openRaw(std::make_unique<std::istringstream>(""), "Second.esm");
                const auto third = readers.get(2);
                third->openRaw(std::make_unique<std::istringstream>("12345"), "Third.esp");
            }

            ESMWriter writer;
            std::stringstream stream;
            writer.setFormatVersion(CurrentContentFormatVersion);
            writer.addMaster("first.ESM", 1);
            writer.addMaster("second.esm", 1);
            writer.addMaster("unknown.esm", 1);
            writer.addMaster("THIRD.esp", 1);
            writer.save(stream);
            writer.close();

            const auto reader = readers.get(3);
            reader->open(std::make_unique<std::istringstream>(stream.str()), "Fourth.esp");
            reader->setIndex(3);
            reader->resolveParentFileIndices(readers);

            // The ones that are not found are taken as the plugin itself.
            EXPECT_THAT(reader->getParentFileIndices(), ElementsAre(0, 3, 3, 2));
        }

        // The land

        LandRecordData makeLandData()
        {
            LandRecordData data;
            for (unsigned i = 0; i < LandRecordData::sLandNumVerts; ++i)
                data.mHeights[i] = static_cast<float>(static_cast<int>((i * 7) % 23) - 11) * Land::sHeightScale;
            data.mMinHeight = *std::min_element(data.mHeights.begin(), data.mHeights.end());
            data.mMaxHeight = *std::max_element(data.mHeights.begin(), data.mHeights.end());
            std::iota(data.mNormals.begin(), data.mNormals.end(), 2);
            std::iota(data.mTextures.begin(), data.mTextures.end(), 3);
            std::iota(data.mColours.begin(), data.mColours.end(), 4);
            data.mDataLoaded = Land::DATA_VNML | Land::DATA_VHGT | Land::DATA_VCLR | Land::DATA_VTEX;
            return data;
        }

        Land makeLand(std::uint32_t flags = Land::Flag_HeightsNormals | Land::Flag_Colors | Land::Flag_Textures)
        {
            Land land;
            land.mFlags = flags;
            land.mX = 4;
            land.mY = -6;
            land.mDataTypes = Land::DATA_VNML | Land::DATA_VHGT | Land::DATA_WNAM | Land::DATA_VCLR | Land::DATA_VTEX;
            land.mLandData = std::make_unique<LandRecordData>(makeLandData());
            generateWnam(land.mLandData->mHeights, land.mWnam);
            return land;
        }

        // The land is written to a file, because that is where it is found again when its data is wanted.
        std::filesystem::path writeLandFile(const Land& land, bool deleted = false)
        {
            const std::filesystem::path path = TestingOpenMW::outputFilePath("land_in_a_file.esp");
            std::ofstream file(path, std::ios::binary);
            ESMWriter writer;
            writer.setFormatVersion(CurrentContentFormatVersion);
            writer.save(file);
            writer.startRecord(REC_LAND);
            land.save(writer, deleted);
            writer.endRecord(REC_LAND);
            writer.close();
            return path;
        }

        void loadLandFrom(const std::filesystem::path& path, Land& land, bool& deleted)
        {
            ESMReader reader;
            reader.open(path);
            reader.getRecName();
            reader.getRecHeader();
            land.load(reader, deleted);
        }

        TEST(Esm3LandTest, landKeepsItsDataInTheFileUntilItIsWanted)
        {
            const Land record = makeLand();
            const std::filesystem::path path = writeLandFile(record);

            Land land;
            bool deleted = true;
            loadLandFrom(path, land, deleted);

            EXPECT_FALSE(deleted);
            EXPECT_EQ(land.mX, 4);
            EXPECT_EQ(land.mY, -6);
            EXPECT_EQ(land.mDataTypes, record.mDataTypes);
            EXPECT_EQ(land.mWnam, record.mWnam);
            EXPECT_EQ(land.mLandData, nullptr);
            EXPECT_EQ(land.getLandData(), nullptr);
            EXPECT_FALSE(land.isDataLoaded(Land::DATA_VHGT));

            const Land::LandData* heights = land.getLandData(Land::DATA_VHGT);
            ASSERT_NE(heights, nullptr);
            EXPECT_TRUE(land.isDataLoaded(Land::DATA_VHGT));
            EXPECT_FALSE(land.isDataLoaded(Land::DATA_VNML | Land::DATA_VHGT));
            EXPECT_EQ(heights->mHeights, record.mLandData->mHeights);
            EXPECT_EQ(heights->mMinHeight, record.mLandData->mMinHeight);
            EXPECT_EQ(heights->mMaxHeight, record.mLandData->mMaxHeight);

            // What is wanted later is added to what is loaded.
            const Land::LandData* all = land.getLandData(Land::DATA_VNML | Land::DATA_VHGT | Land::DATA_VTEX);
            ASSERT_NE(all, nullptr);
            EXPECT_TRUE(land.isDataLoaded(Land::DATA_VNML | Land::DATA_VHGT | Land::DATA_VTEX));
            EXPECT_EQ(all->mNormals, record.mLandData->mNormals);
            EXPECT_EQ(all->mTextures, record.mLandData->mTextures);
        }

        TEST(Esm3LandTest, landDataThatIsUnloadedIsLoadedAgainFromTheFile)
        {
            const std::filesystem::path path = writeLandFile(makeLand());
            Land land;
            bool deleted = false;
            loadLandFrom(path, land, deleted);

            ASSERT_NE(land.getLandData(Land::DATA_VCLR), nullptr);
            land.unloadData();
            EXPECT_EQ(land.getLandData(), nullptr);
            EXPECT_FALSE(land.isDataLoaded(Land::DATA_VCLR));

            const Land::LandData* colours = land.getLandData(Land::DATA_VCLR);
            ASSERT_NE(colours, nullptr);
            EXPECT_EQ(colours->mColours, makeLandData().mColours);
        }

        TEST(Esm3LandTest, landDataCanBeLoadedIntoAnotherObject)
        {
            const std::filesystem::path path = writeLandFile(makeLand());
            Land land;
            bool deleted = false;
            loadLandFrom(path, land, deleted);

            Land::LandData data{};
            land.loadData(Land::DATA_VHGT | Land::DATA_VNML, data);

            EXPECT_EQ(data.mDataLoaded, Land::DATA_VHGT | Land::DATA_VNML);
            EXPECT_EQ(data.mHeights, makeLandData().mHeights);
            // Nothing was loaded into the land itself.
            EXPECT_EQ(land.getLandData(), nullptr);

            // Data that is loaded does not need to be read again.
            const auto loaded = data.mHeights;
            land.loadData(Land::DATA_VHGT, data);
            EXPECT_EQ(data.mHeights, loaded);
        }

        TEST(Esm3LandTest, landOnlyHasTheDataThatItsFlagsSay)
        {
            const Land record = makeLand(Land::Flag_HeightsNormals);
            const std::filesystem::path path = writeLandFile(record);

            Land land;
            bool deleted = false;
            loadLandFrom(path, land, deleted);

            EXPECT_EQ(land.getLandData(Land::DATA_VCLR), nullptr);
            EXPECT_EQ(land.getLandData(Land::DATA_VTEX), nullptr);
            EXPECT_NE(land.getLandData(Land::DATA_VHGT), nullptr);
        }

        TEST(Esm3LandTest, blankLandHasAFlatSurfaceAndNoFile)
        {
            Land land;
            land.blank();

            EXPECT_TRUE(land.mContext.filename.empty());
            ASSERT_NE(land.getLandData(), nullptr);
            EXPECT_EQ(land.getLandData()->mHeights[100], Land::DEFAULT_HEIGHT);
            EXPECT_EQ(land.getLandData()->mNormals[2], 127);
            EXPECT_EQ(land.getLandData()->mColours[0], 255);
            EXPECT_TRUE(land.isDataLoaded(Land::DATA_VHGT | Land::DATA_VCLR | Land::DATA_VTEX | Land::DATA_VNML));

            // The data is taken from the land itself, since there is no file.
            Land::LandData data{};
            land.loadData(Land::DATA_VHGT, data);
            EXPECT_EQ(data.mHeights[100], Land::DEFAULT_HEIGHT);

            land.loadData(Land::DATA_VHGT);
            EXPECT_NE(land.getLandData(Land::DATA_VHGT), nullptr);
        }

        TEST(Esm3LandTest, landIsLoadedFromItselfWhenItHasNoDataYet)
        {
            Land land;
            land.blank();
            land.unloadData();

            // Without data and without a file there is nothing to load, and nothing is made up.
            land.loadData(Land::DATA_VHGT);
            EXPECT_NE(land.getLandData(), nullptr);
        }

        TEST(Esm3LandTest, dataCanBeAddedToALandThatHasNone)
        {
            Land land;
            land.add(Land::DATA_VHGT | Land::DATA_VCLR);

            ASSERT_NE(land.getLandData(), nullptr);
            EXPECT_EQ(land.mDataTypes, Land::DATA_VHGT | Land::DATA_VCLR);
            EXPECT_TRUE(land.isDataLoaded(Land::DATA_VHGT | Land::DATA_VCLR));

            land.add(Land::DATA_VTEX);
            EXPECT_TRUE(land.isDataLoaded(Land::DATA_VHGT | Land::DATA_VCLR | Land::DATA_VTEX));
        }

        TEST(Esm3LandTest, copiedLandHasItsOwnData)
        {
            Land original = makeLand();

            const Land copy(original);
            Land assigned;
            assigned = original;

            ASSERT_NE(copy.getLandData(), nullptr);
            ASSERT_NE(assigned.getLandData(), nullptr);
            EXPECT_NE(copy.getLandData(), original.getLandData());
            EXPECT_NE(assigned.getLandData(), original.getLandData());
            EXPECT_EQ(copy.getLandData()->mHeights, original.getLandData()->mHeights);
            EXPECT_EQ(assigned.getLandData()->mTextures, original.getLandData()->mTextures);
            EXPECT_EQ(assigned.mX, 4);
            EXPECT_EQ(assigned.mY, -6);

            const Land empty{};
            const Land emptyCopy(empty);
            EXPECT_EQ(emptyCopy.getLandData(), nullptr);
        }

        TEST(Esm3LandTest, pluginOfTheLandIsTheOneOfItsContext)
        {
            Land land;
            land.setPlugin(7);
            EXPECT_EQ(land.getPlugin(), 7);
            EXPECT_EQ(land.mContext.index, 7);
        }

        TEST(Esm3LandTest, deletedLandKeepsItsPlace)
        {
            const std::filesystem::path path = writeLandFile(makeLand(), true);

            Land land;
            bool deleted = false;
            loadLandFrom(path, land, deleted);

            EXPECT_TRUE(deleted);
            EXPECT_EQ(land.mX, 4);
            EXPECT_EQ(land.mY, -6);
            EXPECT_EQ(land.mDataTypes, 0);
        }

        TEST(Esm3LandTest, landWithoutAPlaceFails)
        {
            const std::string message = failureOf(
                CurrentContentFormatVersion, [](ESMWriter& writer) { writer.writeHNT("DATA", std::uint32_t{ 1 }); },
                [](ESMReader& reader) {
                    Land land;
                    bool deleted = false;
                    land.load(reader, deleted);
                });
            EXPECT_THAT(message, HasSubstr("Missing INTV subrecord"));
        }

        TEST(Esm3LandTest, landWithAnUnknownSubrecordFails)
        {
            const std::string message = failureOf(
                CurrentContentFormatVersion,
                [](ESMWriter& writer) {
                    const std::int32_t place[2] = { 1, 2 };
                    writer.writeHNT("INTV", place);
                    writer.writeHNT("DATA", std::uint32_t{ 1 });
                    writer.writeHNT("ZZZZ", std::int32_t{ 1 });
                },
                [](ESMReader& reader) {
                    Land land;
                    bool deleted = false;
                    land.load(reader, deleted);
                });
            EXPECT_THAT(message, HasSubstr("Unknown subrecord"));
        }
    }
}
