#include "saveloadharness.hpp"

#include <components/esm/attr.hpp>
#include <components/esm3/aipackage.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/transport.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

// The creatures and the characters of the content files, saved and loaded again with everything that a record can
// hold: the scale, the inventory, the spells, the destinations that they travel to, and every kind of the AI
// packages. They are loaded before the cells that place them, so a field that is read in the wrong place would
// corrupt the next record as well.
namespace ESM
{
    namespace
    {
        using namespace ::testing;
        using namespace ::ESM::SaveLoadTest;

        struct Esm3ActorRecordTest : public Esm3SaveLoadRecordTest
        {
        };

        RefId id(std::string_view value)
        {
            return RefId::stringRefId(value);
        }

        void addPackages(AIPackageList& list)
        {
            AIPackage wander{};
            wander.mType = AI_Wander;
            wander.mWander = { .mDistance = 512,
                .mDuration = 8,
                .mTimeOfDay = 3,
                .mIdle = { 10, 20, 30, 40, 50, 60, 70, 80 },
                .mShouldRepeat = 1 };
            list.mList.push_back(wander);

            AIPackage travel{};
            travel.mType = AI_Travel;
            travel.mTravel = { .mX = 1.f, .mY = 2.f, .mZ = 3.f, .mShouldRepeat = 1 };
            list.mList.push_back(travel);

            AIPackage escort{};
            escort.mType = AI_Escort;
            escort.mTarget
                = { .mX = 4.f, .mY = 5.f, .mZ = 6.f, .mDuration = 24, .mId = NAME32("player"), .mShouldRepeat = 0 };
            escort.mCellName = "Seyda Neen";
            list.mList.push_back(escort);

            AIPackage follow{};
            follow.mType = AI_Follow;
            follow.mTarget
                = { .mX = 7.f, .mY = 8.f, .mZ = 9.f, .mDuration = 12, .mId = NAME32("guard"), .mShouldRepeat = 1 };
            list.mList.push_back(follow);

            AIPackage activate{};
            activate.mType = AI_Activate;
            activate.mActivate = { .mName = NAME32("lever"), .mShouldRepeat = 1 };
            list.mList.push_back(activate);
        }

        void expectPackages(const AIPackageList& list)
        {
            ASSERT_EQ(list.mList.size(), 5u);

            ASSERT_EQ(list.mList[0].mType, AI_Wander);
            EXPECT_EQ(list.mList[0].mWander.mDistance, 512);
            EXPECT_EQ(list.mList[0].mWander.mDuration, 8);
            EXPECT_EQ(list.mList[0].mWander.mTimeOfDay, 3);
            EXPECT_EQ(list.mList[0].mWander.mIdle[0], 10);
            EXPECT_EQ(list.mList[0].mWander.mIdle[7], 80);
            EXPECT_EQ(list.mList[0].mWander.mShouldRepeat, 1);

            ASSERT_EQ(list.mList[1].mType, AI_Travel);
            EXPECT_EQ(list.mList[1].mTravel.mX, 1.f);
            EXPECT_EQ(list.mList[1].mTravel.mY, 2.f);
            EXPECT_EQ(list.mList[1].mTravel.mZ, 3.f);
            EXPECT_EQ(list.mList[1].mTravel.mShouldRepeat, 1);

            ASSERT_EQ(list.mList[2].mType, AI_Escort);
            EXPECT_EQ(list.mList[2].mTarget.mX, 4.f);
            EXPECT_EQ(list.mList[2].mTarget.mDuration, 24);
            EXPECT_EQ(list.mList[2].mTarget.mId.toStringView(), "player");
            EXPECT_EQ(list.mList[2].mTarget.mShouldRepeat, 0);
            EXPECT_EQ(list.mList[2].mCellName, "Seyda Neen");

            ASSERT_EQ(list.mList[3].mType, AI_Follow);
            EXPECT_EQ(list.mList[3].mTarget.mZ, 9.f);
            EXPECT_EQ(list.mList[3].mTarget.mDuration, 12);
            EXPECT_EQ(list.mList[3].mTarget.mId.toStringView(), "guard");
            EXPECT_EQ(list.mList[3].mCellName, "");

            ASSERT_EQ(list.mList[4].mType, AI_Activate);
            EXPECT_EQ(list.mList[4].mActivate.mName.toStringView(), "lever");
            EXPECT_EQ(list.mList[4].mActivate.mShouldRepeat, 1);
        }

        void addDestinations(Transport& transport)
        {
            Transport::Dest first;
            first.mPos = { .pos = { 1.f, 2.f, 3.f }, .rot = { 0.f, 0.f, 1.5f } };
            first.mCellName = "Balmora";
            transport.mList.push_back(first);

            Transport::Dest second;
            second.mPos = { .pos = { 4.f, 5.f, 6.f }, .rot = { 0.f, 0.f, 0.f } };
            transport.mList.push_back(second);
        }

        void expectDestinations(const Transport& transport)
        {
            ASSERT_EQ(transport.mList.size(), 2u);
            EXPECT_EQ(transport.mList[0].mPos.pos[1], 2.f);
            EXPECT_EQ(transport.mList[0].mPos.rot[2], 1.5f);
            EXPECT_EQ(transport.mList[0].mCellName, "Balmora");
            EXPECT_EQ(transport.mList[1].mPos.pos[2], 6.f);
            EXPECT_EQ(transport.mList[1].mCellName, "");
        }

        template <class T>
        void addTradeGoods(T& record)
        {
            record.mInventory.mList.push_back({ 3, id("iron_dagger") });
            record.mInventory.mList.push_back({ -2, id("gold_001") });
            record.mSpells.mList.push_back(id("fireball"));
            record.mSpells.mList.push_back(id("heal"));
        }

        template <class T>
        void expectTradeGoods(const T& record)
        {
            ASSERT_EQ(record.mInventory.mList.size(), 2u);
            EXPECT_EQ(record.mInventory.mList[0].mCount, 3);
            EXPECT_EQ(record.mInventory.mList[0].mItem, id("iron_dagger"));
            EXPECT_EQ(record.mInventory.mList[1].mCount, -2);
            EXPECT_EQ(record.mInventory.mList[1].mItem, id("gold_001"));
            EXPECT_THAT(record.mSpells.mList, ElementsAre(id("fireball"), id("heal")));
            EXPECT_TRUE(record.mSpells.exists(id("heal")));
            EXPECT_FALSE(record.mSpells.exists(id("poison")));
        }

        Creature makeCreature()
        {
            Creature record;
            record.blank();
            record.mId = id("scrib");
            record.mName = "Scrib";
            record.mScale = 1.5f;
            record.mFlags = Creature::Base | Creature::Walks;
            record.mBloodType = 2;
            record.mData.mLevel = 4;
            record.mData.mAttributes = { { Attribute::Strength, 11 }, { Attribute::Speed, 22 } };
            addTradeGoods(record);
            addDestinations(record.mTransport);
            addPackages(record.mAiPackage);
            return record;
        }

        NPC makeNpc()
        {
            NPC record;
            record.blank();
            record.mId = id("fargoth");
            record.mName = "Fargoth";
            record.mRace = id("Dark Elf");
            record.mClass = id("Thief");
            record.mFaction = id("Thieves Guild");
            record.mHead = id("b_n_dark elf_m_head_01");
            record.mHair = id("b_n_dark elf_m_hair_01");
            record.mFlags = NPC::Base;
            record.mNpdt.mLevel = 3;
            record.mNpdt.mRank = 2;
            record.mNpdt.mGold = 100;
            record.mNpdt.mAttributes = { { Attribute::Agility, 40 }, { Attribute::Luck, 50 } };
            record.mNpdt.mSkills = { { Skill::Acrobatics, 60 } };
            addTradeGoods(record);
            addDestinations(record.mTransport);
            addPackages(record.mAiPackage);
            return record;
        }

        // A subrecord of the size, with zeros for all the values.
        void writeZeros(ESMWriter& writer, NAME name, std::size_t size)
        {
            const std::string zeros(size, '\0');
            writer.startSubRecord(name);
            writer.write(zeros.data(), zeros.size());
            writer.endRecord(name);
        }

        TEST_P(Esm3ActorRecordTest, creatureKeepsItsScaleItsInventoryAndItsPackages)
        {
            const Creature record = makeCreature();

            Creature result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, id("scrib"));
            EXPECT_EQ(result.mName, "Scrib");
            EXPECT_EQ(result.mScale, 1.5f);
            EXPECT_EQ(result.mFlags, Creature::Base | Creature::Walks);
            EXPECT_EQ(result.mBloodType, 2);
            EXPECT_EQ(result.mData.getAttribute(Attribute::Strength), 11);
            EXPECT_EQ(result.mData.getAttribute(Attribute::Speed), 22);
            expectTradeGoods(result);
            expectDestinations(result.mTransport);
            ASSERT_EQ(result.getTransport().size(), 2u);
            expectPackages(result.mAiPackage);
        }

        TEST_P(Esm3ActorRecordTest, npcKeepsItsInventoryItsDestinationsAndItsPackages)
        {
            const NPC record = makeNpc();

            NPC result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, id("fargoth"));
            EXPECT_EQ(result.mNpdt.mLevel, 3);
            EXPECT_EQ(result.mNpdt.mGold, 100);
            EXPECT_EQ(result.mNpdt.getAttribute(Attribute::Luck), 50);
            EXPECT_EQ(result.mNpdt.getSkill(Skill::Acrobatics), 60);
            expectTradeGoods(result);
            expectDestinations(result.mTransport);
            ASSERT_EQ(result.getTransport().size(), 2u);
            expectPackages(result.mAiPackage);
        }

        TEST_P(Esm3ActorRecordTest, npcKnowsItsFactionRankOnlyWhenItHasAFaction)
        {
            NPC result;
            saveAndLoadRecord(makeNpc(), GetParam(), result);
            EXPECT_EQ(result.getFactionRank(), 2);

            result.mFaction = RefId();
            EXPECT_EQ(result.getFactionRank(), -1);
        }

        TEST_P(Esm3ActorRecordTest, deletedActorsKeepTheirIds)
        {
            Creature creature = makeCreature();
            Creature loadedCreature;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { creature.save(writer, true); },
                [&](ESMReader& reader) { loadedCreature.load(reader, deleted); });
            EXPECT_TRUE(deleted);
            EXPECT_EQ(loadedCreature.mId, id("scrib"));

            NPC npc = makeNpc();
            NPC loadedNpc;
            deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { npc.save(writer, true); },
                [&](ESMReader& reader) { loadedNpc.load(reader, deleted); });
            EXPECT_TRUE(deleted);
            EXPECT_EQ(loadedNpc.mId, id("fargoth"));
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3ActorRecordTest, ValuesIn(getFormats()));

        struct Esm3ActorLoadingTest : public Test
        {
        };

        // Reads what the test writes by hand, as the reader sees a file that somebody has edited.
        template <class T, class Write>
        std::string failureOf(Write&& write)
        {
            try
            {
                roundTripWith(CurrentContentFormatVersion, write, [](ESMReader& reader) {
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

        TEST_F(Esm3ActorLoadingTest, creatureWithAnUnknownSubrecordFails)
        {
            EXPECT_THAT(failureOf<Creature>([](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("scrib"));
                writer.writeHNT("ZZZZ", 1);
            }),
                HasSubstr("Unknown subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, creatureWithoutANameFails)
        {
            EXPECT_THAT(failureOf<Creature>([](ESMWriter& writer) { writer.writeHNT("FLAG", 1); }),
                HasSubstr("Missing NAME subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, creatureWithoutStatsFails)
        {
            EXPECT_THAT(failureOf<Creature>([](ESMWriter& writer) { writer.writeHNCRefId("NAME", id("scrib")); }),
                HasSubstr("Missing NPDT subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, creatureWithoutFlagsFails)
        {
            EXPECT_THAT(failureOf<Creature>([](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("scrib"));
                writeZeros(writer, "NPDT", 96);
            }),
                HasSubstr("Missing FLAG subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, npcWithAnUnknownSubrecordFails)
        {
            EXPECT_THAT(failureOf<NPC>([](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("fargoth"));
                writer.writeHNT("ZZZZ", 1);
            }),
                HasSubstr("Unknown subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, npcWithoutANameFails)
        {
            EXPECT_THAT(failureOf<NPC>([](ESMWriter& writer) { writer.writeHNT("FLAG", 1); }),
                HasSubstr("Missing NAME subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, npcWithoutStatsFails)
        {
            EXPECT_THAT(failureOf<NPC>([](ESMWriter& writer) { writer.writeHNCRefId("NAME", id("fargoth")); }),
                HasSubstr("Missing NPDT subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, npcWithoutFlagsFails)
        {
            EXPECT_THAT(failureOf<NPC>([](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("fargoth"));
                writeZeros(writer, "NPDT", 52);
            }),
                HasSubstr("Missing FLAG subrecord"));
        }

        TEST_F(Esm3ActorLoadingTest, npcWithStatsOfAnotherSizeFails)
        {
            EXPECT_THAT(failureOf<NPC>([](ESMWriter& writer) {
                writer.writeHNCRefId("NAME", id("fargoth"));
                writeZeros(writer, "NPDT", 20);
            }),
                HasSubstr("NPC_NPDT must be 12 or 52 bytes long"));
        }

        TEST_F(Esm3ActorLoadingTest, npcWithAutocalculatedStatsOnlyHasALevelADispositionAndAGold)
        {
            NPC record = makeNpc();
            record.mNpdtType = NPC::NPC_WITH_AUTOCALCULATED_STATS;
            record.mNpdt.mDisposition = 40;
            record.mNpdt.mReputation = 7;

            NPC result;
            saveAndLoadRecord(record, CurrentContentFormatVersion, result);

            EXPECT_EQ(result.mNpdtType, NPC::NPC_WITH_AUTOCALCULATED_STATS);
            EXPECT_EQ(result.mNpdt.mLevel, 3);
            EXPECT_EQ(result.mNpdt.mDisposition, 40);
            EXPECT_EQ(result.mNpdt.mReputation, 7);
            EXPECT_EQ(result.mNpdt.mRank, 2);
            EXPECT_EQ(result.mNpdt.mGold, 100);
            EXPECT_EQ(result.mNpdt.mHealth, 0);
            EXPECT_EQ(result.mNpdt.mMana, 0);
        }

        TEST_F(Esm3ActorLoadingTest, creatureToleratesTheIndexThatSavedGamesHave)
        {
            Creature record = makeCreature();
            Creature result;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    writer.writeHNT("INDX", 4);
                    record.save(writer);
                },
                [&](ESMReader& reader) {
                    bool deleted = false;
                    result.load(reader, deleted);
                });
            EXPECT_EQ(result.mId, id("scrib"));
            EXPECT_EQ(result.mScale, 1.5f);
        }

        TEST_F(Esm3ActorLoadingTest, aPackageConditionWithoutAPackageFails)
        {
            AIPackageList list;
            std::string message;
            try
            {
                roundTripWith(
                    CurrentContentFormatVersion, [](ESMWriter& writer) { writer.writeHNCString("CNDT", "Balmora"); },
                    [&](ESMReader& reader) {
                        reader.getSubName();
                        list.add(reader);
                    });
            }
            catch (const std::exception& e)
            {
                message = e.what();
            }
            EXPECT_THAT(message, HasSubstr("AIPackge with an AI_CNDT applying to no cell."));
        }

        TEST_F(Esm3ActorLoadingTest, aDestinationNameWithoutADestinationIsIgnored)
        {
            Transport transport;
            roundTripWith(
                CurrentContentFormatVersion, [](ESMWriter& writer) { writer.writeHNCString("DNAM", "Balmora"); },
                [&](ESMReader& reader) {
                    reader.getSubName();
                    transport.add(reader);
                });
            EXPECT_TRUE(transport.mList.empty());
        }

        TEST_F(Esm3ActorLoadingTest, blankActorsHaveTheDefaultsOfTheGame)
        {
            Creature creature = makeCreature();
            creature.blank();
            EXPECT_EQ(creature.mScale, 1.f);
            EXPECT_EQ(creature.mAiData.mFight, 90);
            EXPECT_EQ(creature.mAiData.mFlee, 20);
            EXPECT_TRUE(creature.mInventory.mList.empty());
            EXPECT_TRUE(creature.mSpells.mList.empty());
            EXPECT_TRUE(creature.mAiPackage.mList.empty());
            EXPECT_TRUE(creature.getTransport().empty());
            EXPECT_EQ(creature.mData.getAttribute(Attribute::Strength), 0);
            EXPECT_TRUE(creature.mName.empty());
            EXPECT_TRUE(creature.mScript.empty());

            NPC npc = makeNpc();
            npc.blank();
            EXPECT_EQ(npc.mAiData.mHello, 30);
            EXPECT_EQ(npc.mAiData.mFight, 30);
            EXPECT_EQ(npc.mNpdtType, NPC::NPC_DEFAULT);
            EXPECT_EQ(npc.mNpdt.mLevel, 0);
            EXPECT_TRUE(npc.mNpdt.mAttributes.empty());
            EXPECT_TRUE(npc.mNpdt.mSkills.empty());
            EXPECT_TRUE(npc.mInventory.mList.empty());
            EXPECT_TRUE(npc.getTransport().empty());
            EXPECT_TRUE(npc.mRace.empty());
            EXPECT_EQ(npc.getFactionRank(), -1);
            EXPECT_EQ(npc.mNpdt.getAttribute(Attribute::Luck), 0);
            EXPECT_EQ(npc.mNpdt.getSkill(Skill::Acrobatics), 0);
        }

        TEST_F(Esm3ActorLoadingTest, theGenderOfACharacterIsTheFemaleFlag)
        {
            NPC npc;
            npc.blank();
            EXPECT_TRUE(npc.isMale());

            npc.setIsMale(false);
            EXPECT_FALSE(npc.isMale());
            EXPECT_EQ(npc.mFlags & NPC::Female, NPC::Female);

            npc.setIsMale(true);
            EXPECT_TRUE(npc.isMale());
            EXPECT_EQ(npc.mFlags & NPC::Female, 0);
        }
    }
}
