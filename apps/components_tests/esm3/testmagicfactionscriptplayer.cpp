#include "saveloadharness.hpp"

#include <components/esm/attr.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadscpt.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/player.hpp>
#include <components/esm3/statstate.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// The magic effects, the factions, the scripts and the player. What the loaders keep when a file has been edited by
// hand, or saved by an older version, is tested here: the flags that a plugin may not change, the reactions that a
// faction lists twice, the tables of variable names that are shorter than they should be, and the statistics that
// were saved with whole numbers.
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

        template <class T, class Write>
        std::string failureOf(FormatVersion format, Write&& write)
        {
            try
            {
                roundTripWith(format, write, [](ESMReader& reader) {
                    T record;
                    bool deleted = false;
                    record.load(reader, deleted);
                });
            }
            catch (const std::exception& e)
            {
                return e.what();
            }
            return {};
        }

        // The magic effects

        MagicEffect makeEffect(int index, int32_t flags = 0)
        {
            MagicEffect effect;
            effect.blank();
            effect.mId = MagicEffect::indexToRefId(index);
            effect.mData.mFlags = flags;
            return effect;
        }

        TEST(Esm3MagicEffectTest, effectsAreResistedByTheEffectOfTheirKind)
        {
            EXPECT_EQ(MagicEffect::getResistanceEffect(MagicEffect::FireDamage), MagicEffect::ResistFire);
            EXPECT_EQ(MagicEffect::getResistanceEffect(MagicEffect::FrostDamage), MagicEffect::ResistFrost);
            EXPECT_EQ(MagicEffect::getResistanceEffect(MagicEffect::Poison), MagicEffect::ResistPoison);
            EXPECT_EQ(MagicEffect::getResistanceEffect(MagicEffect::Paralyze), MagicEffect::ResistParalysis);
            EXPECT_EQ(MagicEffect::getResistanceEffect(MagicEffect::DrainHealth), MagicEffect::ResistMagicka);
            EXPECT_EQ(MagicEffect::getResistanceEffect(MagicEffect::DisintegrateArmor), MagicEffect::Sanctuary);
        }

        TEST(Esm3MagicEffectTest, effectsThatCannotBeResistedHaveNoResistanceEffect)
        {
            EXPECT_TRUE(MagicEffect::getResistanceEffect(MagicEffect::Light).empty());
            EXPECT_TRUE(MagicEffect::getResistanceEffect(RefId()).empty());
        }

        TEST(Esm3MagicEffectTest, effectsMakeTheirTargetWeakToTheEffectOfTheirKind)
        {
            EXPECT_EQ(MagicEffect::getWeaknessEffect(MagicEffect::FireDamage), MagicEffect::WeaknessToFire);
            EXPECT_EQ(MagicEffect::getWeaknessEffect(MagicEffect::ShockDamage), MagicEffect::WeaknessToShock);
            EXPECT_EQ(MagicEffect::getWeaknessEffect(MagicEffect::Corprus), MagicEffect::WeaknessToCorprusDisease);
            EXPECT_EQ(MagicEffect::getWeaknessEffect(MagicEffect::Charm), MagicEffect::WeaknessToMagicka);
            EXPECT_TRUE(MagicEffect::getWeaknessEffect(MagicEffect::Light).empty());
            EXPECT_TRUE(MagicEffect::getWeaknessEffect(RefId()).empty());
        }

        struct MagnitudeCase
        {
            int mIndex;
            int32_t mFlags;
            MagicEffect::MagnitudeDisplayType mExpected;
        };

        struct Esm3MagnitudeDisplayTest : public TestWithParam<MagnitudeCase>
        {
        };

        TEST_P(Esm3MagnitudeDisplayTest, magnitudeIsShownInTheUnitOfTheEffect)
        {
            const MagnitudeCase& param = GetParam();
            EXPECT_EQ(makeEffect(param.mIndex, param.mFlags).getMagnitudeDisplayType(), param.mExpected);
        }

        INSTANTIATE_TEST_SUITE_P(Effects, Esm3MagnitudeDisplayTest,
            Values(MagnitudeCase{ 0, 0, MagicEffect::MDT_Points },
                MagnitudeCase{ 17, MagicEffect::NoMagnitude, MagicEffect::MDT_None },
                MagnitudeCase{ 84, 0, MagicEffect::MDT_TimesInt }, MagnitudeCase{ 59, 0, MagicEffect::MDT_Feet },
                MagnitudeCase{ 64, 0, MagicEffect::MDT_Feet }, MagnitudeCase{ 66, 0, MagicEffect::MDT_Feet },
                MagnitudeCase{ 118, 0, MagicEffect::MDT_Level }, MagnitudeCase{ 119, 0, MagicEffect::MDT_Level },
                MagnitudeCase{ 28, 0, MagicEffect::MDT_Percentage },
                MagnitudeCase{ 36, 0, MagicEffect::MDT_Percentage },
                MagnitudeCase{ 90, 0, MagicEffect::MDT_Percentage },
                MagnitudeCase{ 99, 0, MagicEffect::MDT_Percentage },
                MagnitudeCase{ 40, 0, MagicEffect::MDT_Percentage },
                MagnitudeCase{ 47, 0, MagicEffect::MDT_Percentage },
                MagnitudeCase{ 57, 0, MagicEffect::MDT_Percentage },
                MagnitudeCase{ 68, 0, MagicEffect::MDT_Percentage }, MagnitudeCase{ 70, 0, MagicEffect::MDT_Points }));

        TEST(Esm3MagicEffectTest, colorIsTheGlowOfTheEffectAndIsInvertedForNegativeLight)
        {
            MagicEffect effect = makeEffect(0);
            effect.mData.mRed = 255;
            effect.mData.mGreen = 0;
            effect.mData.mBlue = 51;

            const osg::Vec4f plain = effect.getColor();
            EXPECT_FLOAT_EQ(plain.x(), 1.f);
            EXPECT_FLOAT_EQ(plain.y(), 0.f);
            EXPECT_FLOAT_EQ(plain.z(), 0.2f);
            EXPECT_FLOAT_EQ(plain.w(), 1.f);

            effect.mData.mFlags |= MagicEffect::NegativeLight;
            const osg::Vec4f inverted = effect.getColor();
            EXPECT_FLOAT_EQ(inverted.x(), 0.f);
            EXPECT_FLOAT_EQ(inverted.y(), 1.f);
            EXPECT_FLOAT_EQ(inverted.z(), 0.8f);
            EXPECT_FLOAT_EQ(inverted.w(), 1.f);
        }

        TEST(Esm3MagicEffectTest, effectsAreFoundByTheirIndexAndTheirGameSetting)
        {
            const RefId first = MagicEffect::indexToRefId(0);
            ASSERT_FALSE(first.empty());
            EXPECT_EQ(MagicEffect::refIdToIndex(first), 0);
            EXPECT_EQ(MagicEffect::refIdToIndex(MagicEffect::indexToRefId(42)), 42);
            EXPECT_EQ(MagicEffect::refIdToIndex(id("no_such_effect")), -1);

            EXPECT_TRUE(MagicEffect::indexToRefId(-1).empty());
            EXPECT_TRUE(MagicEffect::indexToRefId(MagicEffect::Length).empty());
            EXPECT_FALSE(MagicEffect::indexToRefId(MagicEffect::Length - 1).empty());

            EXPECT_FALSE(MagicEffect::indexToName(0).empty());
            EXPECT_TRUE(MagicEffect::indexToName(-1).empty());
            EXPECT_TRUE(MagicEffect::indexToName(MagicEffect::Length).empty());

            const std::string_view setting = MagicEffect::refIdToGmstString(first);
            ASSERT_FALSE(setting.empty());
            EXPECT_EQ(MagicEffect::effectGmstIdToRefId(setting), first);
            std::string shouted(setting);
            for (char& c : shouted)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            EXPECT_EQ(MagicEffect::effectGmstIdToRefId(shouted), first);
            EXPECT_TRUE(MagicEffect::effectGmstIdToRefId("sNoSuchEffect").empty());
            EXPECT_TRUE(MagicEffect::refIdToGmstString(id("no_such_effect")).empty());
        }

        TEST(Esm3MagicEffectTest, pluginMayOnlyChangeTheFlagsThatTheEditorLetsItChange)
        {
            MagicEffect record = makeEffect(3, 0x7fffffff);

            MagicEffect legacy;
            saveAndLoadRecord(record, DefaultFormatVersion, legacy);
            // The bits of the editor stay, and the bits that are not in any flag are dropped.
            EXPECT_TRUE(legacy.mData.mFlags & MagicEffect::AllowSpellmaking);
            EXPECT_TRUE(legacy.mData.mFlags & MagicEffect::AllowEnchanting);
            EXPECT_TRUE(legacy.mData.mFlags & MagicEffect::NegativeLight);
            EXPECT_FALSE(legacy.mData.mFlags & 0x40000000);

            MagicEffect current;
            saveAndLoadRecord(record, CurrentContentFormatVersion, current);
            EXPECT_EQ(current.mData.mFlags, 0x7fffffff);
        }

        TEST(Esm3MagicEffectTest, effectWithAnIndexThatDoesNotExistFails)
        {
            for (const int32_t index : { -1, MagicEffect::Length })
            {
                EXPECT_THAT(failureOf<MagicEffect>(CurrentContentFormatVersion,
                                [&](ESMWriter& writer) { writer.writeHNT("INDX", index); }),
                    HasSubstr("Invalid Index!"));
            }
        }

        TEST(Esm3MagicEffectTest, effectWithAnUnknownSubrecordFails)
        {
            const MagicEffect record = makeEffect(5);
            EXPECT_THAT(failureOf<MagicEffect>(CurrentContentFormatVersion,
                            [&](ESMWriter& writer) {
                                record.save(writer);
                                writer.writeHNT("ZZZZ", 1);
                            }),
                HasSubstr("Unknown subrecord"));
        }

        // The factions

        Faction makeFaction()
        {
            Faction faction;
            faction.blank();
            faction.mId = id("Thieves Guild");
            faction.mName = "Thieves Guild";
            faction.mData.mAttribute = { Attribute::indexToRefId(3), Attribute::indexToRefId(6) };
            faction.mData.mSkills = { Skill::indexToRefId(0), Skill::indexToRefId(1), Skill::indexToRefId(2),
                Skill::indexToRefId(3), Skill::indexToRefId(4), Skill::indexToRefId(5), Skill::indexToRefId(6) };
            faction.mData.mFlags = Faction::Hidden;
            faction.mRanks[0] = "Toad";
            faction.mRanks[1] = "Pickpocket";
            faction.mData.mRankData[1] = {
                .mAttribute1 = 30, .mAttribute2 = 31, .mPrimarySkill = 10, .mFavouredSkill = 5, .mFactReputation = 100
            };
            return faction;
        }

        struct Esm3FactionTest : public TestWithParam<FormatVersion>
        {
        };

        TEST_P(Esm3FactionTest, factionKeepsItsRanksItsSkillsAndItsReactions)
        {
            Faction record = makeFaction();
            record.mReactions = { { id("Fighters Guild"), -10 }, { id("Mages Guild"), 5 } };

            Faction result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mName, "Thieves Guild");
            EXPECT_EQ(result.mRanks[0], "Toad");
            EXPECT_EQ(result.mRanks[1], "Pickpocket");
            EXPECT_TRUE(result.mRanks[2].empty());
            EXPECT_EQ(result.mData.mAttribute, record.mData.mAttribute);
            EXPECT_EQ(result.mData.mSkills, record.mData.mSkills);
            EXPECT_EQ(result.mData.mFlags, Faction::Hidden);
            EXPECT_EQ(result.mData.mRankData[1].mFactReputation, 100);
            EXPECT_EQ(result.mData.mRankData[1].mPrimarySkill, 10);
            EXPECT_EQ(result.mReactions, record.mReactions);
        }

        TEST_P(Esm3FactionTest, deletedFactionKeepsItsId)
        {
            const Faction record = makeFaction();
            Faction result;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(deleted);
            EXPECT_EQ(result.mId, record.mId);
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3FactionTest, ValuesIn(getFormats()));

        TEST(Esm3FactionLoadingTest, reactionThatIsListedTwiceKeepsTheLowestOne)
        {
            const Faction record = makeFaction();
            Faction result;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    writer.writeHNCRefId("NAME", record.mId);
                    record.mData.save(writer);
                    for (const int32_t reaction : { 10, 3, 8 })
                    {
                        writer.writeHNRefId("ANAM", id("Fighters Guild"));
                        writer.writeHNT("INTV", reaction);
                    }
                },
                [&](ESMReader& reader) {
                    bool deleted = false;
                    result.load(reader, deleted);
                });

            EXPECT_THAT(result.mReactions, ElementsAre(Pair(id("Fighters Guild"), 3)));
        }

        TEST(Esm3FactionLoadingTest, factionWithMoreThanTenRanksFails)
        {
            const Faction record = makeFaction();
            EXPECT_THAT(failureOf<Faction>(CurrentContentFormatVersion,
                            [&](ESMWriter& writer) {
                                writer.writeHNCRefId("NAME", record.mId);
                                for (int i = 0; i < 11; ++i)
                                    writer.writeHNString("RNAM", "rank", 32);
                            }),
                HasSubstr("Rank out of range"));
        }

        TEST(Esm3FactionLoadingTest, factionWithoutANameFails)
        {
            const Faction record = makeFaction();
            EXPECT_THAT(
                failureOf<Faction>(CurrentContentFormatVersion, [&](ESMWriter& writer) { record.mData.save(writer); }),
                HasSubstr("Missing NAME subrecord"));
        }

        TEST(Esm3FactionLoadingTest, factionWithoutDataFails)
        {
            EXPECT_THAT(failureOf<Faction>(CurrentContentFormatVersion,
                            [&](ESMWriter& writer) { writer.writeHNCRefId("NAME", id("Thieves Guild")); }),
                HasSubstr("Missing FADT subrecord"));
        }

        TEST(Esm3FactionLoadingTest, factionWithAnUnknownSubrecordFails)
        {
            EXPECT_THAT(failureOf<Faction>(CurrentContentFormatVersion,
                            [&](ESMWriter& writer) {
                                writer.writeHNCRefId("NAME", id("Thieves Guild"));
                                writer.writeHNT("ZZZZ", 1);
                            }),
                HasSubstr("Unknown subrecord"));
        }

        TEST(Esm3FactionLoadingTest, skillsOfTheFactionAreFoundByTheirPosition)
        {
            Faction faction = makeFaction();
            const Faction& constFaction = faction;

            EXPECT_EQ(faction.mData.getSkill(2), Skill::indexToRefId(2));
            EXPECT_EQ(constFaction.mData.getSkill(6), Skill::indexToRefId(6));
            faction.mData.getSkill(0) = Skill::indexToRefId(9);
            EXPECT_EQ(constFaction.mData.getSkill(0), Skill::indexToRefId(9));
            EXPECT_THROW(faction.mData.getSkill(7), std::out_of_range);
            EXPECT_THROW(constFaction.mData.getSkill(7), std::out_of_range);
        }

        TEST(Esm3FactionLoadingTest, blankFactionRequiresNothing)
        {
            Faction faction = makeFaction();
            faction.mReactions[id("Fighters Guild")] = 4;
            faction.blank();

            EXPECT_TRUE(faction.mName.empty());
            EXPECT_TRUE(faction.mReactions.empty());
            EXPECT_TRUE(faction.mRanks[1].empty());
            EXPECT_EQ(faction.mData.mFlags, 0);
            EXPECT_EQ(faction.mData.mRankData[1].mFactReputation, 0);
            EXPECT_TRUE(faction.mData.mAttribute[0].empty());
            EXPECT_TRUE(faction.mData.mSkills[3].empty());
        }

        // The scripts

        // The header of the script, with the numbers of the variables and the sizes of the tables that follow it.
        void writeHeader(ESMWriter& writer, std::string_view name, std::uint32_t shorts, std::uint32_t longs,
            std::uint32_t floats, std::uint32_t dataSize, std::uint32_t tableSize)
        {
            writer.startSubRecord("SCHD");
            writer.writeMaybeFixedSizeRefId(id(name), 32);
            const std::uint32_t values[5] = { shorts, longs, floats, dataSize, tableSize };
            writer.write(reinterpret_cast<const char*>(values), sizeof(values));
            writer.endRecord("SCHD");
        }

        void writeBytes(ESMWriter& writer, NAME name, std::string_view bytes)
        {
            writer.startSubRecord(name);
            writer.write(bytes.data(), bytes.size());
            writer.endRecord(name);
        }

        template <class Write>
        Script loadScript(Write&& write)
        {
            Script script;
            roundTripWith(CurrentContentFormatVersion, write, [&](ESMReader& reader) {
                bool deleted = false;
                script.load(reader, deleted);
            });
            return script;
        }

        TEST(Esm3ScriptTest, variablesOfOldScriptsAreSeparatedByCarriageReturns)
        {
            const Script script = loadScript([](ESMWriter& writer) {
                writeHeader(writer, "a_script", 2, 0, 0, 0, 8);
                writeBytes(writer, "SCVR", std::string_view("one\rtwo\r", 8));
                writeBytes(writer, "SCDT", "");
            });

            EXPECT_THAT(script.mVarNames, ElementsAre("one", "two"));
        }

        TEST(Esm3ScriptTest, tableOfVariablesThatDoesNotEndWithAZeroIsStillRead)
        {
            const Script script = loadScript([](ESMWriter& writer) {
                writeHeader(writer, "a_script", 1, 0, 0, 0, 3);
                writeBytes(writer, "SCVR", "abc");
                writeBytes(writer, "SCDT", "");
            });

            EXPECT_THAT(script.mVarNames, ElementsAre("abc"));
        }

        TEST(Esm3ScriptTest, bytesThatFollowTheTableOfVariablesAreSkipped)
        {
            const Script script = loadScript([](ESMWriter& writer) {
                writeHeader(writer, "a_script", 1, 0, 0, 3, 2);
                writeBytes(writer, "SCVR", std::string_view("a\0junk", 6));
                writeBytes(writer, "SCDT", "xyz");
                writer.writeHNOString("SCTX", "Begin a_script\nEnd");
            });

            EXPECT_THAT(script.mVarNames, ElementsAre("a"));
            EXPECT_THAT(script.mScriptData, ElementsAre('x', 'y', 'z'));
            EXPECT_EQ(script.mScriptText, "Begin a_script\nEnd");
        }

        TEST(Esm3ScriptTest, variablesThatAreMissingFromTheTableAreCutOffTheList)
        {
            const Script script = loadScript([](ESMWriter& writer) {
                writeHeader(writer, "a_script", 3, 0, 0, 0, 2);
                writeBytes(writer, "SCVR", std::string_view("a\0", 2));
                writeBytes(writer, "SCDT", "");
            });

            EXPECT_THAT(script.mVarNames, ElementsAre("a"));
        }

        TEST(Esm3ScriptTest, emptyTableOfVariablesLeavesTheNamesEmpty)
        {
            const Script script = loadScript([](ESMWriter& writer) {
                writeHeader(writer, "a_script", 2, 0, 0, 0, 0);
                writeBytes(writer, "SCVR", "");
                writeBytes(writer, "SCDT", "");
            });

            EXPECT_THAT(script.mVarNames, ElementsAre("", ""));
        }

        TEST(Esm3ScriptTest, emptyTableOfVariablesForAScriptWithoutVariablesIsFine)
        {
            const Script script = loadScript([](ESMWriter& writer) {
                writeHeader(writer, "a_script", 0, 0, 0, 0, 0);
                writeBytes(writer, "SCVR", "");
                writeBytes(writer, "SCDT", "");
            });

            EXPECT_TRUE(script.mVarNames.empty());
        }

        TEST(Esm3ScriptTest, tableOfVariablesThatIsSmallerThanItsHeaderSaysFails)
        {
            EXPECT_THAT(failureOf<Script>(CurrentContentFormatVersion,
                            [](ESMWriter& writer) {
                                writeHeader(writer, "a_script", 1, 0, 0, 0, 10);
                                writeBytes(writer, "SCVR", std::string_view("a\0", 2));
                            }),
                HasSubstr("SCVR string list is smaller than specified"));
        }

        TEST(Esm3ScriptTest, variablesBeforeTheHeaderFail)
        {
            EXPECT_THAT(failureOf<Script>(CurrentContentFormatVersion,
                            [](ESMWriter& writer) { writeBytes(writer, "SCVR", std::string_view("a\0", 2)); }),
                HasSubstr("SCVR is placed before SCHD record"));
        }

        TEST(Esm3ScriptTest, compiledScriptBeforeTheHeaderFails)
        {
            EXPECT_THAT(failureOf<Script>(
                            CurrentContentFormatVersion, [](ESMWriter& writer) { writeBytes(writer, "SCDT", "abc"); }),
                HasSubstr("SCDT is placed before SCHD record"));
        }

        TEST(Esm3ScriptTest, compiledScriptOfAnotherSizeThanTheHeaderSaysIsStillRead)
        {
            const Script script = loadScript([](ESMWriter& writer) {
                writeHeader(writer, "a_script", 0, 0, 0, 9, 0);
                writeBytes(writer, "SCDT", "abc");
            });

            EXPECT_THAT(script.mScriptData, ElementsAre('a', 'b', 'c'));
        }

        TEST(Esm3ScriptTest, scriptWithoutAHeaderFails)
        {
            EXPECT_THAT(failureOf<Script>(CurrentContentFormatVersion,
                            [](ESMWriter& writer) { writer.writeHNOString("SCTX", "Begin x\nEnd"); }),
                HasSubstr("Missing SCHD subrecord"));
        }

        TEST(Esm3ScriptTest, scriptWithAnUnknownSubrecordFails)
        {
            EXPECT_THAT(failureOf<Script>(CurrentContentFormatVersion,
                            [](ESMWriter& writer) {
                                writeHeader(writer, "a_script", 0, 0, 0, 0, 0);
                                writer.writeHNT("ZZZZ", 1);
                            }),
                HasSubstr("Unknown subrecord"));
        }

        TEST(Esm3ScriptTest, deletedScriptKeepsItsId)
        {
            Script record;
            record.blank();
            record.mId = id("a_script");

            Script result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(deleted);
            EXPECT_EQ(result.mId, id("a_script"));
        }

        TEST(Esm3ScriptTest, blankScriptHasTheSourceOfAScriptThatDoesNothing)
        {
            Script plain;
            plain.mId = id("plain_script");
            plain.blank();
            EXPECT_EQ(plain.mScriptText, "Begin plain_script\n\nEnd plain_script\n");
            EXPECT_TRUE(plain.mScriptData.empty());

            Script quoted;
            quoted.mId = id("quoted::script");
            quoted.blank();
            EXPECT_EQ(quoted.mScriptText, "Begin \"quoted::script\"\n\nEnd quoted::script\n");
        }

        TEST(Esm3ScriptTest, tableOfVariablesHasAZeroAfterEveryName)
        {
            EXPECT_EQ(computeScriptStringTableSize({}), 0u);
            EXPECT_EQ(computeScriptStringTableSize({ "a", "bcd", "" }), 7u);
        }

        // The player

        struct Esm3PlayerTest : public TestWithParam<FormatVersion>
        {
        };

        void fillPlayer(Player& player)
        {
            player.mObject.blank();
            player.mBirthsign = id("wombburned");
            player.mObject.mRef.mRefID = id("player");
            player.mCellId = RefId::esm3ExteriorCell(0, 0);
            player.mLastKnownExteriorPosition[0] = 1.f;
            player.mHasMark = false;
            player.mCurrentCrimeId = 3;
            player.mPaidCrimeId = 4;
        }

        TEST_P(Esm3PlayerTest, playerKeepsTheStatisticsThatItHadBeforeItBecameAWerewolf)
        {
            // The statistics of the formats up to this one are tested with the old player.
            if (GetParam() <= MaxOldSkillsAndAttributesFormatVersion)
                return;

            Player record{};
            fillPlayer(record);
            record.mSaveAttributes = { { Attribute::Strength, 33.f }, { Attribute::Luck, 44.f } };
            record.mSaveSkills = { { Skill::Acrobatics, 55.f } };

            Player result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mSaveAttributes.at(Attribute::Strength), 33.f);
            EXPECT_EQ(result.mSaveAttributes.at(Attribute::Luck), 44.f);
            EXPECT_EQ(result.mSaveSkills.at(Skill::Acrobatics), 55.f);
            EXPECT_FALSE(result.mSetWerewolfAcrobatics);
        }

        TEST_P(Esm3PlayerTest, playerKeepsItsMark)
        {
            // The statistics of the formats up to this one are tested with the old player.
            if (GetParam() <= MaxOldSkillsAndAttributesFormatVersion)
                return;

            Player record{};
            fillPlayer(record);
            record.mHasMark = true;
            record.mMarkedCell = id("Balmora");
            record.mMarkedPosition = { .pos = { 5.f, 6.f, 7.f }, .rot = { 0.f, 0.f, 1.f } };

            Player result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_TRUE(result.mHasMark);
            EXPECT_EQ(result.mMarkedCell, id("Balmora"));
            EXPECT_EQ(result.mMarkedPosition.pos[1], 6.f);
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3PlayerTest, ValuesIn(getFormats()));

        // The statistics of the player in the saved games of the versions that kept them next to the player.
        void writeOldPlayer(ESMWriter& writer, const Player& player, bool wholeNumbers, float base, float modifier,
            float damage, bool automove)
        {
            player.mObject.save(writer);
            writer.writeCellId(player.mCellId);
            writer.writeHNT("LKEP", player.mLastKnownExteriorPosition);
            if (automove)
                writer.writeHNT("AMOV", std::int32_t{ 1 });
            writer.writeHNRefId("SIGN", player.mBirthsign);
            writer.writeHNT("CURD", player.mCurrentCrimeId);
            writer.writeHNT("PAYD", player.mPaidCrimeId);
            writer.writeHNRefId("BOUN", id("bound_dagger"));
            writer.writeHNRefId("PREV", id("iron_dagger"));

            for (int i = 0; i < Attribute::Length + Skill::Length; ++i)
            {
                if (wholeNumbers)
                {
                    writer.writeHNT("STBA", static_cast<std::int32_t>(base + i));
                    writer.writeHNT("STMO", static_cast<std::int32_t>(modifier));
                }
                else
                {
                    writer.writeHNT("STBA", base + i);
                    writer.writeHNT("STMO", modifier);
                    writer.writeHNT("STDF", damage);
                }
            }
        }

        template <class Write>
        void loadOldPlayer(Player& player, FormatVersion format, Write&& write)
        {
            roundTripWith(format, write, [&](ESMReader& reader) { player.load(reader); });
        }

        TEST(Esm3OldPlayerTest, statisticsThatWereSavedAsWholeNumbersAreCombinedWithTheirModifiersWhenTheyAreKept)
        {
            Player record{};
            fillPlayer(record);

            Player result{};
            loadOldPlayer(result, MaxOldFogOfWarFormatVersion - 5,
                [&](ESMWriter& writer) { writeOldPlayer(writer, record, true, 10.f, 4.f, 0.f, false); });

            // The modifiers of that version are dropped, so that the player does not keep a spell that has ended.
            EXPECT_EQ(result.mSaveAttributes.at(Attribute::indexToRefId(2)), 12.f);
            EXPECT_EQ(result.mSaveSkills.at(Skill::indexToRefId(3)), 10.f + Attribute::Length + 3);
            EXPECT_EQ(result.mPreviousItems.at(id("bound_dagger")), id("iron_dagger"));
            EXPECT_EQ(result.mCurrentCrimeId, 3);
            EXPECT_EQ(result.mBirthsign, id("wombburned"));
        }

        TEST(Esm3OldPlayerTest, statisticsAreReducedByTheirDamage)
        {
            Player record{};
            fillPlayer(record);

            Player result{};
            loadOldPlayer(result, MaxClearModifiersFormatVersion + 1,
                [&](ESMWriter& writer) { writeOldPlayer(writer, record, false, 10.f, 4.f, 3.f, false); });

            // The modifiers of this version are kept, since they are part of the statistics.
            EXPECT_EQ(result.mSaveAttributes.at(Attribute::indexToRefId(0)), 10.f + 4.f - 3.f);
            EXPECT_EQ(result.mSaveSkills.at(Skill::indexToRefId(0)), 10.f + Attribute::Length + 4.f - 3.f);
        }

        TEST(Esm3OldPlayerTest, modifiersAreDroppedFromTheStatisticsOfThePlayerThatIsNotAWerewolf)
        {
            Player record{};
            fillPlayer(record);

            Player result{};
            loadOldPlayer(result, MaxClearModifiersFormatVersion,
                [&](ESMWriter& writer) { writeOldPlayer(writer, record, false, 10.f, 4.f, 3.f, false); });

            EXPECT_EQ(result.mSaveAttributes.at(Attribute::indexToRefId(0)), 10.f - 3.f);
        }

        TEST(Esm3OldPlayerTest, werewolfKeepsTheStatisticsThatAreInUse)
        {
            Player record{};
            fillPlayer(record);
            record.mObject.mNpcStats.mIsWerewolf = true;
            record.mObject.mNpcStats.mSkills[Skill::Acrobatics].mBase = 7;

            Player result{};
            loadOldPlayer(result, MaxClearModifiersFormatVersion,
                [&](ESMWriter& writer) { writeOldPlayer(writer, record, false, 10.f, 4.f, 3.f, false); });

            ASSERT_TRUE(result.mObject.mNpcStats.mIsWerewolf);
            EXPECT_EQ(result.mObject.mCreatureStats.mAttributes.at(Attribute::indexToRefId(1)).mBase, 11.f);
            EXPECT_EQ(result.mObject.mNpcStats.mSkills.at(Skill::indexToRefId(2)).mMod, 4.f);
            // The acrobatics that the werewolf has differ from the ones that the player had.
            EXPECT_TRUE(result.mSetWerewolfAcrobatics);
            EXPECT_EQ(result.mSaveSkills.at(Skill::Acrobatics),
                10.f + static_cast<float>(Attribute::Length + Skill::refIdToIndex(Skill::Acrobatics)) + 4.f - 3.f);
        }

        TEST(Esm3OldPlayerTest, automoveOfOldSavedGamesIsSkipped)
        {
            Player record{};
            fillPlayer(record);

            Player result{};
            loadOldPlayer(result, MaxClearModifiersFormatVersion + 1,
                [&](ESMWriter& writer) { writeOldPlayer(writer, record, false, 10.f, 4.f, 3.f, true); });

            EXPECT_EQ(result.mBirthsign, id("wombburned"));
            EXPECT_EQ(result.mPaidCrimeId, 4);
        }
    }
}
