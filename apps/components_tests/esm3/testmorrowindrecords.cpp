#include "saveloadharness.hpp"

#include <components/esm/attr.hpp>
#include <components/esm3/loadbook.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loadglob.hpp>
#include <components/esm3/loadlevlist.hpp>
#include <components/esm3/loadpgrd.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadregn.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadstat.hpp>
#include <components/esm3/loadweap.hpp>
#include <components/esm3/variantimp.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

// What the loaders of the records that the Morrowind format shares do with the marks and the damage that the records
// of the content files can have: a deletion, a sub-record that is not theirs, a missing name or data, and the values
// that cannot be saved. The files of Fallout do not have these records, but the same readers walk the sub-records of
// the ones that follow them.
namespace ESM
{
    namespace
    {
        using namespace ::testing;
        using namespace ::ESM::SaveLoadTest;

        struct Esm3MorrowindRecordTest : public Esm3SaveLoadRecordTest
        {
        };

        RefId id(std::string_view value)
        {
            return RefId::stringRefId(value);
        }

        template <class T>
        void expectBlankRecordSavesAndLoads(FormatVersion format)
        {
            SCOPED_TRACE(std::string(T::getRecordType()));
            T blank;
            blank.blank();
            blank.mId = id("blank_record");
            T loaded;
            saveAndLoadRecord(blank, format, loaded);
            EXPECT_EQ(loaded.mId, blank.mId);
        }

        template <class T>
        void expectDeletedRecordIsMarked(FormatVersion format)
        {
            SCOPED_TRACE(std::string(T::getRecordType()));
            T record;
            record.mId = id("deleted_record");
            T loaded;
            bool deleted = false;
            roundTripWith(
                format, [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { loaded.load(reader, deleted); });
            EXPECT_TRUE(deleted);
            EXPECT_EQ(loaded.mId, record.mId);
        }

        template <class T>
        void expectFailure(FormatVersion format, std::string_view message, auto&& write)
        {
            SCOPED_TRACE(std::string(T::getRecordType()));
            T loaded;
            bool deleted = false;
            roundTripWith(format, write, [&](ESMReader& reader) {
                EXPECT_THAT([&] { loaded.load(reader, deleted); },
                    ThrowsMessage<std::runtime_error>(HasSubstr(std::string(message))));
            });
        }

        // A record that is read the way every record is: by name, by the sub-records it has, and what it needs
        template <class T>
        void expectRecordIsStrictAbout(FormatVersion format, std::string_view dataName)
        {
            expectFailure<T>(format, "Missing NAME subrecord", [](ESMWriter&) {});
            expectFailure<T>(format, "Unknown subrecord", [](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("strict_record"));
                writer.writeHNString("ZZZZ", "x");
            });
            if (!dataName.empty())
                expectFailure<T>(format, "Missing " + std::string(dataName) + " subrecord",
                    [](ESMWriter& writer) { writer.writeHNCRefId("NAME", id("strict_record")); });
        }

        TEST_P(Esm3MorrowindRecordTest, savesAndLoadsTheBlankRecordOfEveryKind)
        {
            const FormatVersion format = GetParam();
            expectBlankRecordSavesAndLoads<Weapon>(format);
            expectBlankRecordSavesAndLoads<Race>(format);
            expectBlankRecordSavesAndLoads<Class>(format);
            expectBlankRecordSavesAndLoads<Region>(format);
            expectBlankRecordSavesAndLoads<Container>(format);
            expectBlankRecordSavesAndLoads<Book>(format);
            expectBlankRecordSavesAndLoads<Static>(format);
            expectBlankRecordSavesAndLoads<CreatureLevList>(format);
            expectBlankRecordSavesAndLoads<ItemLevList>(format);
        }

        TEST_P(Esm3MorrowindRecordTest, marksTheRecordsThatAreDeleted)
        {
            const FormatVersion format = GetParam();
            expectDeletedRecordIsMarked<Weapon>(format);
            expectDeletedRecordIsMarked<Race>(format);
            expectDeletedRecordIsMarked<Class>(format);
            expectDeletedRecordIsMarked<Region>(format);
            expectDeletedRecordIsMarked<Container>(format);
            expectDeletedRecordIsMarked<Book>(format);
            expectDeletedRecordIsMarked<Static>(format);
            expectDeletedRecordIsMarked<CreatureLevList>(format);
            expectDeletedRecordIsMarked<ItemLevList>(format);
        }

        TEST_P(Esm3MorrowindRecordTest, rejectsARecordWithoutItsNameOrDataOrWithASubRecordItDoesNotHave)
        {
            const FormatVersion format = GetParam();
            expectRecordIsStrictAbout<Weapon>(format, "WPDT");
            expectRecordIsStrictAbout<Race>(format, "RADT");
            expectRecordIsStrictAbout<Class>(format, "CLDT");
            expectRecordIsStrictAbout<Region>(format, {});
            expectRecordIsStrictAbout<Container>(format, "CNDT");
            expectRecordIsStrictAbout<Book>(format, "BKDT");
            expectRecordIsStrictAbout<Static>(format, {});
            expectFailure<CreatureLevList>(format, "Missing NAME subrecord", [](ESMWriter&) {});
            expectFailure<ItemLevList>(format, "Missing NAME subrecord", [](ESMWriter&) {});
        }

        TEST_P(Esm3MorrowindRecordTest, ignoresWhatFollowsTheNameOfALevelledListThatHasNoListYet)
        {
            // the original engine ignores the rest of such a record
            ItemLevList loaded;
            bool deleted = false;
            roundTripWith(
                GetParam(),
                [](ESMWriter& writer) {
                    writer.writeHNCRefId("NAME", id("list"));
                    writer.writeHNString("ZZZZ", "x");
                },
                [&](ESMReader& reader) { loaded.load(reader, deleted); });
            EXPECT_EQ(loaded.mId, id("list"));
            EXPECT_THAT(loaded.mList, IsEmpty());

            // but not once it has one
            expectFailure<ItemLevList>(GetParam(), "Unknown subrecord", [](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("list"));
                writer.writeHNT("INDX", std::uint32_t(0));
                writer.writeHNString("ZZZZ", "x");
            });
        }

        TEST_P(Esm3MorrowindRecordTest, failsToSaveAWeaponOfATypeThatHasNoNumber)
        {
            Weapon weapon;
            weapon.blank();
            weapon.mId = id("sword");
            weapon.mData.mType = id("NotAWeaponType");

            ESMWriter writer;
            writer.setFormatVersion(GetParam());
            auto stream = std::make_unique<std::stringstream>();
            writer.save(*stream);
            writer.startRecord(fakeRecordId);
            EXPECT_THAT([&] { weapon.save(writer); },
                ThrowsMessage<std::runtime_error>(HasSubstr("Cannot serialize weapon type")));
        }

        TEST(Esm3WeaponTypesTest, mapsTheTypesOfWeaponsToTheirNumbersAndBack)
        {
            for (int i = 0; i < 14; ++i)
            {
                const RefId type = Weapon::indexToRefId(i);
                EXPECT_FALSE(type.empty()) << i;
                EXPECT_EQ(Weapon::refIdToIndex(type), i);
            }
            EXPECT_TRUE(Weapon::indexToRefId(-1).empty());
            EXPECT_TRUE(Weapon::indexToRefId(14).empty());
            EXPECT_EQ(Weapon::refIdToIndex(id("NotAWeaponType")), -1);
            EXPECT_EQ(Weapon::refIdToIndex(RefId()), -1);
        }

        TEST(Esm3WeaponTypesTest, doesNotLoadOrSaveTheRecordOfAWeaponType)
        {
            WeaponType type;
            ESMReader reader;
            ESMWriter writer;
            bool deleted = false;
            EXPECT_THAT([&] { type.load(reader, deleted); },
                ThrowsMessage<std::runtime_error>(HasSubstr("not yet implemented")));
            EXPECT_THAT(
                [&] { type.save(writer); }, ThrowsMessage<std::runtime_error>(HasSubstr("not yet implemented")));
        }

        TEST_P(Esm3MorrowindRecordTest, savesTheAttributesPowersAndBonusesOfARace)
        {
            Race race;
            race.blank();
            race.mId = id("breton");
            race.mName = "Breton";
            race.mDescription = "Of the north";
            race.mData.mFlags = Race::Playable;
            race.mData.mMaleHeight = 1.f;
            race.mData.mFemaleHeight = 0.9f;
            race.mData.mMaleWeight = 1.1f;
            race.mData.mFemaleWeight = 0.8f;
            race.mData.mBonus[0].mSkill = Skill::indexToRefId(3);
            race.mData.mBonus[0].mBonus = 10;
            race.mData.setAttribute(Attribute::Strength, true, 40);
            race.mData.setAttribute(Attribute::Strength, false, 30);
            race.mData.setAttribute(Attribute::Luck, false, 50);
            // an attribute with nothing for either sex, and one without a name, are left out
            race.mData.setAttribute(Attribute::Speed, true, 0);
            race.mData.setAttribute(RefId(), true, 25);
            race.mPowers.mList = { id("power_one"), id("power_two") };

            Race loaded;
            saveAndLoadRecord(race, GetParam(), loaded);
            EXPECT_EQ(loaded.mName, "Breton");
            EXPECT_EQ(loaded.mDescription, "Of the north");
            EXPECT_EQ(loaded.mData.mFlags, Race::Playable);
            EXPECT_EQ(loaded.mData.mBonus[0].mSkill, Skill::indexToRefId(3));
            EXPECT_EQ(loaded.mData.mBonus[0].mBonus, 10);
            EXPECT_THAT(loaded.mPowers.mList, ElementsAre(id("power_one"), id("power_two")));
            EXPECT_EQ(loaded.mData.getAttribute(Attribute::Strength, true), 40);
            EXPECT_EQ(loaded.mData.getAttribute(Attribute::Strength, false), 30);
            EXPECT_EQ(loaded.mData.getAttribute(Attribute::Luck, true), 0);
            EXPECT_EQ(loaded.mData.getAttribute(Attribute::Luck, false), 50);
            EXPECT_EQ(loaded.mData.getAttribute(Attribute::Speed, true), 0);
            EXPECT_EQ(loaded.mData.getAttribute(Attribute::Willpower, true), 0);
        }

        TEST(Esm3ClassTest, picksTheMajorAndMinorSkillsByIndex)
        {
            Class record;
            record.blank();
            record.mData.mMajorSkills[2] = Skill::indexToRefId(7);
            record.mData.mMinorSkills[4] = Skill::indexToRefId(9);

            EXPECT_EQ(record.mData.getSkill(2, true), Skill::indexToRefId(7));
            EXPECT_EQ(record.mData.getSkill(4, false), Skill::indexToRefId(9));
            const Class::CLDTstruct& data = record.mData;
            EXPECT_EQ(data.getSkill(2, true), Skill::indexToRefId(7));
            EXPECT_EQ(data.getSkill(4, false), Skill::indexToRefId(9));
            EXPECT_THAT([&] { record.mData.getSkill(5, true); }, Throws<std::out_of_range>());
            EXPECT_THAT([&] { data.getSkill(5, false); }, Throws<std::out_of_range>());
        }

        TEST_P(Esm3MorrowindRecordTest, savesTheWeatherSoundsAndSleepListOfARegion)
        {
            Region region;
            region.blank();
            region.mId = id("ascadian_isles");
            region.mName = "Ascadian Isles";
            region.mSleepList = id("sleep_list");
            region.mMapColor = 0x00FF8040;
            region.mData.mProbabilities[Weather::indexToRefId(0)] = 30;
            region.mData.mProbabilities[Weather::indexToRefId(4)] = 70;
            // a weather that has no number is not saved
            region.mData.mProbabilities[id("NotAWeather")] = 10;
            region.mSoundList.push_back({ id("sound_one"), 40 });

            Region loaded;
            saveAndLoadRecord(region, GetParam(), loaded);
            EXPECT_EQ(loaded.mName, "Ascadian Isles");
            EXPECT_EQ(loaded.mSleepList, id("sleep_list"));
            EXPECT_EQ(loaded.mMapColor, 0x00FF8040u);
            EXPECT_EQ(loaded.mData.mProbabilities.at(Weather::indexToRefId(0)), 30);
            EXPECT_EQ(loaded.mData.mProbabilities.at(Weather::indexToRefId(4)), 70);
            EXPECT_EQ(loaded.mData.mProbabilities.count(id("NotAWeather")), 0u);
            ASSERT_EQ(loaded.mSoundList.size(), 1u);
            EXPECT_EQ(loaded.mSoundList[0].mSound, id("sound_one"));
            EXPECT_EQ(loaded.mSoundList[0].mChance, 40);
        }

        TEST_P(Esm3MorrowindRecordTest, readsTheWeatherChancesOfARegionThatHasNoColdWeather)
        {
            // before Tribunal the chances were two bytes shorter, and the ones that are missing are 0
            Region loaded;
            bool deleted = false;
            roundTripWith(
                GetParam(),
                [](ESMWriter& writer) {
                    writer.writeHNCRefId("NAME", id("old_region"));
                    writer.writeHNString("FNAM", "Old Region");
                    writer.startSubRecord("WEAT");
                    for (std::uint8_t chance = 1; chance <= static_cast<std::uint8_t>(Weather::Length - 2); ++chance)
                        writer.writeT(chance);
                    writer.endRecord("WEAT");
                },
                [&](ESMReader& reader) { loaded.load(reader, deleted); });
            EXPECT_EQ(loaded.mName, "Old Region");
            EXPECT_EQ(loaded.mData.mProbabilities.at(Weather::indexToRefId(0)), 1);
            EXPECT_EQ(loaded.mData.mProbabilities.at(Weather::indexToRefId(Weather::Length - 3)), Weather::Length - 2);
            EXPECT_EQ(loaded.mData.mProbabilities.count(Weather::indexToRefId(Weather::Length - 2)), 0u);

            expectFailure<Region>(GetParam(), "Don't know what to do in this version", [](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("odd_region"));
                writer.writeHNString("WEAT", "123");
            });
        }

        TEST_P(Esm3MorrowindRecordTest, readsTheNameAndScriptOfAContainerAndRejectsItsBadFlags)
        {
            Container container;
            container.blank();
            container.mId = id("chest");
            container.mName = "Chest";
            container.mScript = id("chest_script");
            container.mWeight = 12.f;
            container.mFlags = Container::Unknown | Container::Organic | Container::Respawn;

            Container loaded;
            saveAndLoadRecord(container, GetParam(), loaded);
            EXPECT_EQ(loaded.mName, "Chest");
            EXPECT_EQ(loaded.mScript, id("chest_script"));
            EXPECT_EQ(loaded.mWeight, 12.f);

            const auto flags = [](std::int32_t value) {
                return [value](ESMWriter& writer) {
                    writer.writeHNCRefId("NAME", id("chest"));
                    writer.writeHNT("CNDT", 1.f);
                    writer.writeHNT("FLAG", value);
                };
            };
            expectFailure<Container>(GetParam(), "Unknown flags", flags(0x8 | 0x10));
            expectFailure<Container>(GetParam(), "Flag 8 not set", flags(0x1));
            expectFailure<Container>(GetParam(), "Missing FLAG subrecord", [](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("chest"));
                writer.writeHNT("CNDT", 1.f);
            });
        }

        TEST_P(Esm3MorrowindRecordTest, savesAndLoadsEveryKindOfGlobalVariable)
        {
            for (const Variant& value : { Variant(3.5f), Variant(std::int16_t(7)), Variant(std::int32_t(70000)) })
            {
                Global global;
                global.mId = id("a_global");
                global.mValue = value;

                Global loaded;
                saveAndLoadRecord(global, GetParam(), loaded);
                EXPECT_EQ(loaded.mId, id("a_global"));
                EXPECT_EQ(loaded.mValue.getType(), value.getType());
                EXPECT_TRUE(loaded == global);
            }

            Global global;
            global.mId = id("gone");
            Global loaded;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { global.save(writer, true); },
                [&](ESMReader& reader) { loaded.load(reader, deleted); });
            EXPECT_TRUE(deleted);
            EXPECT_EQ(loaded.mId, id("gone"));

            Global other = global;
            other.mId = id("other");
            EXPECT_FALSE(other == global);
            global.blank();
            EXPECT_EQ(global.mValue.getType(), VT_None);
        }

        Pathgrid::DATAstruct pathgridData(std::uint16_t points)
        {
            Pathgrid::DATAstruct data{};
            data.mX = 1;
            data.mY = -2;
            data.mGranularity = 1024;
            data.mPoints = points;
            return data;
        }

        void writePoint(ESMWriter& writer, std::int32_t x, std::uint8_t connections)
        {
            writer.writeT<std::int32_t>(x);
            writer.writeT<std::int32_t>(0);
            writer.writeT<std::int32_t>(0);
            writer.writeT<std::uint8_t>(0);
            writer.writeT<std::uint8_t>(connections);
            // and two bytes that are not used
            writer.writeT<std::uint16_t>(0);
        }

        TEST_P(Esm3MorrowindRecordTest, rejectsAPathgridWhoseSubRecordsDoNotAddUp)
        {
            const FormatVersion format = GetParam();
            expectFailure<Pathgrid>(format, "Missing DATA subrecord", [](ESMWriter& writer) {});
            expectFailure<Pathgrid>(format, "Unknown subrecord", [](ESMWriter& writer) {
                writer.writeHNT("DATA", pathgridData(0));
                writer.writeHNString("ZZZZ", "x");
            });
            // the points say there are two, the sub-record holds the bytes of one and one more
            expectFailure<Pathgrid>(format, "Path point subrecord size mismatch", [](ESMWriter& writer) {
                writer.writeHNT("DATA", pathgridData(2));
                writer.startSubRecord("PGRP");
                writePoint(writer, 1, 0);
                writer.writeT<std::uint8_t>(0);
                writer.endRecord("PGRP");
            });
            expectFailure<Pathgrid>(format, "PGRC size not a multiple of 4", [](ESMWriter& writer) {
                writer.writeHNT("DATA", pathgridData(1));
                writer.startSubRecord("PGRP");
                writePoint(writer, 1, 0);
                writer.endRecord("PGRP");
                writer.startSubRecord("PGRC");
                writer.writeT<std::uint8_t>(0);
                writer.writeT<std::uint8_t>(0);
                writer.writeT<std::uint8_t>(0);
                writer.endRecord("PGRC");
            });
            // a point that has three connections and one of them in the list
            expectFailure<Pathgrid>(format, "Not enough connections", [](ESMWriter& writer) {
                writer.writeHNT("DATA", pathgridData(1));
                writer.startSubRecord("PGRP");
                writePoint(writer, 1, 3);
                writer.endRecord("PGRP");
                writer.startSubRecord("PGRC");
                writer.writeT<std::uint32_t>(0);
                writer.endRecord("PGRC");
            });
        }

        TEST_P(Esm3MorrowindRecordTest, marksAPathgridThatIsDeleted)
        {
            Pathgrid pathgrid;
            pathgrid.blank();
            pathgrid.mCell = id("a cell");
            pathgrid.mData = pathgridData(0);

            Pathgrid loaded;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { pathgrid.save(writer, true); },
                [&](ESMReader& reader) { loaded.load(reader, deleted); });
            EXPECT_TRUE(deleted);
            EXPECT_EQ(loaded.mCell, id("a cell"));
        }

        TEST(Esm3PathgridPointTest, makesAPointOfThreeCoordinates)
        {
            const float coordinates[3] = { 1.9f, -2.9f, 3.f };
            const Pathgrid::Point made(coordinates);
            EXPECT_EQ(made.mX, 1);
            EXPECT_EQ(made.mY, -2);
            EXPECT_EQ(made.mZ, 3);
            EXPECT_EQ(made.mAutogenerated, 0);
            EXPECT_EQ(made.mConnectionNum, 0);

            Pathgrid::Point assigned(5, 6, 7);
            assigned.mAutogenerated = 1;
            assigned.mConnectionNum = 2;
            assigned = coordinates;
            EXPECT_EQ(assigned.mX, 1);
            EXPECT_EQ(assigned.mAutogenerated, 0);
            EXPECT_EQ(assigned.mConnectionNum, 0);
        }

        TEST(Esm3VariantValueTest, readsAndWritesAnIntegerInTheFormOfEachKindOfRecord)
        {
            const auto roundTrip = [](Variant::Format format, VarType type, std::int32_t value) {
                std::int32_t result = 0;
                roundTripWith(
                    CurrentContentFormatVersion,
                    [&](ESMWriter& writer) { writeESMVariantValue(writer, format, type, value); },
                    [&](ESMReader& reader) {
                        // the name of a global variable is in the record before its value
                        if (format == Variant::Format_Global)
                            reader.getHNString("FNAM");
                        else
                            reader.getSubName();
                        readESMVariantValue(reader, format, type, result);
                    });
                return result;
            };
            EXPECT_EQ(roundTrip(Variant::Format_Global, VT_Short, -5), -5);
            EXPECT_EQ(roundTrip(Variant::Format_Global, VT_Long, 70000), 70000);
            EXPECT_EQ(roundTrip(Variant::Format_Gmst, VT_Int, 12), 12);
            EXPECT_EQ(roundTrip(Variant::Format_Info, VT_Int, -12), -12);
            EXPECT_EQ(roundTrip(Variant::Format_Local, VT_Short, 300), 300);
            EXPECT_EQ(roundTrip(Variant::Format_Local, VT_Int, 70000), 70000);
        }

        TEST(Esm3VariantValueTest, doesNotWriteAnIntegerOfAKindTheFormatDoesNotHave)
        {
            ESMWriter writer;
            const auto write = [&](Variant::Format format, VarType type) {
                return [&writer, format, type] { writeESMVariantValue(writer, format, type, std::int32_t(1)); };
            };
            EXPECT_THAT(write(Variant::Format_Global, VT_Int), ThrowsMessage<std::runtime_error>(HasSubstr("global")));
            EXPECT_THAT(write(Variant::Format_Gmst, VT_Short), ThrowsMessage<std::runtime_error>(HasSubstr("gmst")));
            EXPECT_THAT(write(Variant::Format_Info, VT_Long), ThrowsMessage<std::runtime_error>(HasSubstr("info")));
            EXPECT_THAT(write(Variant::Format_Local, VT_Long), ThrowsMessage<std::runtime_error>(HasSubstr("local")));
            EXPECT_THAT(write(Variant::Format_Global, VT_Float), Throws<std::logic_error>());
            EXPECT_THAT(write(Variant::Format_Global, VT_String), Throws<std::logic_error>());
        }

        TEST(Esm3VariantValueTest, doesNotReadAnIntegerOfAKindTheFormatDoesNotHave)
        {
            const auto readFails = [](Variant::Format format, VarType type, bool isFloat, std::string_view text) {
                roundTripWith(
                    CurrentContentFormatVersion,
                    [&](ESMWriter& writer) {
                        if (isFloat)
                            writer.writeHNT("FLTV", 1.f);
                        else
                            writer.writeHNT("INTV", std::int32_t(1));
                    },
                    [&](ESMReader& reader) {
                        std::int32_t value = 0;
                        EXPECT_THAT([&] { readESMVariantValue(reader, format, type, value); },
                            ThrowsMessage<std::runtime_error>(HasSubstr(std::string(text))))
                            << text;
                    });
            };
            readFails(Variant::Format_Global, VT_Int, true, "unsupported global variable integer type");
            readFails(Variant::Format_Gmst, VT_Short, false, "unsupported gmst variable integer type");
            readFails(Variant::Format_Info, VT_Long, false, "unsupported info variable integer type");
            readFails(Variant::Format_Local, VT_Long, false, "unsupported local variable integer type");

            ESMReader reader;
            std::int32_t value = 0;
            EXPECT_THAT([&] { readESMVariantValue(reader, Variant::Format_Gmst, VT_Float, value); },
                Throws<std::logic_error>());
        }

        TEST(Esm3VariantValueTest, convertsAFloatThatDoesNotFitAnIntegerToTheLowestOne)
        {
            const auto read = [](float stored, VarType type) {
                std::int32_t result = 0;
                roundTripWith(
                    CurrentContentFormatVersion, [&](ESMWriter& writer) { writer.writeHNT("FLTV", stored); },
                    [&](ESMReader& reader) { readESMVariantValue(reader, Variant::Format_Global, type, result); });
                return result;
            };
            EXPECT_EQ(read(1e20f, VT_Long), std::numeric_limits<std::int32_t>::lowest());
            EXPECT_EQ(read(-1e20f, VT_Long), std::numeric_limits<std::int32_t>::lowest());
            EXPECT_EQ(read(std::numeric_limits<float>::quiet_NaN(), VT_Short),
                static_cast<std::int16_t>(std::numeric_limits<std::int32_t>::lowest()));
            EXPECT_EQ(read(-7.9f, VT_Long), -7);
        }

        TEST(Esm3VariantValueTest, readsAndWritesAFloatAndAStringOnlyWhereTheFormatHasThem)
        {
            const auto roundTripFloat = [](Variant::Format format, float value) {
                float result = 0;
                roundTripWith(
                    CurrentContentFormatVersion,
                    [&](ESMWriter& writer) { writeESMVariantValue(writer, format, VT_Float, value); },
                    [&](ESMReader& reader) {
                        if (format == Variant::Format_Global)
                            reader.getHNString("FNAM");
                        else
                            reader.getSubName();
                        readESMVariantValue(reader, format, VT_Float, result);
                    });
                return result;
            };
            EXPECT_EQ(roundTripFloat(Variant::Format_Global, 1.5f), 1.5f);
            EXPECT_EQ(roundTripFloat(Variant::Format_Gmst, 2.5f), 2.5f);
            EXPECT_EQ(roundTripFloat(Variant::Format_Info, -2.5f), -2.5f);
            EXPECT_EQ(roundTripFloat(Variant::Format_Local, 4.5f), 4.5f);

            ESMWriter writer;
            ESMReader reader;
            std::string text;
            float number = 0;
            EXPECT_THAT([&] { writeESMVariantValue(writer, Variant::Format_Gmst, VT_Int, number); },
                Throws<std::logic_error>());
            EXPECT_THAT(
                [&] { readESMVariantValue(reader, Variant::Format_Gmst, VT_Int, number); }, Throws<std::logic_error>());
            EXPECT_THAT(
                [&] { writeESMVariantValue(writer, Variant::Format_Gmst, VT_Int, text); }, Throws<std::logic_error>());
            EXPECT_THAT(
                [&] { readESMVariantValue(reader, Variant::Format_Gmst, VT_Int, text); }, Throws<std::logic_error>());
            for (const auto format : { Variant::Format_Global, Variant::Format_Info, Variant::Format_Local })
            {
                EXPECT_THAT([&] { writeESMVariantValue(writer, format, VT_String, std::string("x")); },
                    Throws<std::runtime_error>());
                EXPECT_THAT(
                    [&] { readESMVariantValue(reader, format, VT_String, text); }, Throws<std::runtime_error>());
            }
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3MorrowindRecordTest, ValuesIn(getFormats()),
            [](const auto& info) { return "V" + std::to_string(info.param); });
    }
}
