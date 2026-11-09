#include "saveloadharness.hpp"

#include <components/esm/attr.hpp>
#include <components/esm3/effectlist.hpp>
#include <components/esm3/loadacti.hpp>
#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadbody.hpp>
#include <components/esm3/loadbsgn.hpp>
#include <components/esm3/loadclot.hpp>
#include <components/esm3/loaddoor.hpp>
#include <components/esm3/loadench.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadlevlist.hpp>
#include <components/esm3/loadligh.hpp>
#include <components/esm3/loadlock.hpp>
#include <components/esm3/loadltex.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadmisc.hpp>
#include <components/esm3/loadpgrd.hpp>
#include <components/esm3/loadprob.hpp>
#include <components/esm3/loadrepa.hpp>
#include <components/esm3/loadskil.hpp>
#include <components/esm3/loadsndg.hpp>
#include <components/esm3/loadsoun.hpp>
#include <components/esm3/loadspel.hpp>
#include <components/esm3/loadsscr.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

// The records of the content files that the games of the Morrowind format share, saved and loaded again with every
// field set. The files of Fallout do not have them, but the loaders are shared with the engine, so a field that is
// read in the wrong place would still corrupt the other records that follow it.
namespace ESM
{
    namespace
    {
        using namespace ::testing;
        using namespace ::ESM::SaveLoadTest;

        struct Esm3ContentRecordTest : public Esm3SaveLoadRecordTest
        {
        };

        RefId id(std::string_view value)
        {
            return RefId::stringRefId(value);
        }

        Path path(std::string value)
        {
            Path result;
            result.set(std::move(value));
            return result;
        }

        EffectList makeEffects()
        {
            EffectList effects;
            effects.mList.push_back(IndexedENAMstruct{ {
                                                           .mEffectID = MagicEffect::RestoreHealth,
                                                           .mSkill = {},
                                                           .mAttribute = {},
                                                           .mRange = 1,
                                                           .mArea = 2,
                                                           .mDuration = 3,
                                                           .mMagnMin = 4,
                                                           .mMagnMax = 5,
                                                       },
                0 });
            return effects;
        }

        void expectEffects(const EffectList& effects)
        {
            ASSERT_EQ(effects.mList.size(), 1u);
            EXPECT_EQ(effects.mList[0].mData.mEffectID, MagicEffect::RestoreHealth);
            EXPECT_EQ(effects.mList[0].mData.mRange, 1);
            EXPECT_EQ(effects.mList[0].mData.mArea, 2);
            EXPECT_EQ(effects.mList[0].mData.mDuration, 3);
            EXPECT_EQ(effects.mList[0].mData.mMagnMin, 4);
            EXPECT_EQ(effects.mList[0].mData.mMagnMax, 5);
        }

        // The record is saved as deleted, and loaded again: only the ID and the flag are kept.
        template <class T>
        bool loadDeleted(const T& record, T& result)
        {
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion, [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });
            return deleted;
        }

        template <class T>
        void expectDeletedKeepsTheId(T record)
        {
            record.blank();
            record.mId = id("removed");
            T result;
            EXPECT_TRUE(loadDeleted(record, result));
            EXPECT_EQ(result.mId, id("removed"));
        }

        TEST_P(Esm3ContentRecordTest, potionKeepsItsEffects)
        {
            Potion record;
            record.blank();
            record.mId = id("p_restore");
            record.mScript = id("potionscript");
            record.mName = "Restore Health";
            record.mModel = path("m\\potion.nif");
            record.mIcon = path("potion.dds");
            record.mData = { .mWeight = 0.5f, .mValue = 25, .mFlags = Potion::Autocalc };
            record.mEffects = makeEffects();

            Potion result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mScript, record.mScript);
            EXPECT_EQ(result.mName, "Restore Health");
            EXPECT_EQ(result.mModel.getOriginal(), "m\\potion.nif");
            EXPECT_EQ(result.mIcon.getOriginal(), "potion.dds");
            EXPECT_EQ(result.mData.mWeight, 0.5f);
            EXPECT_EQ(result.mData.mValue, 25);
            EXPECT_EQ(result.mData.mFlags, Potion::Autocalc);
            expectEffects(result.mEffects);
        }

        TEST_P(Esm3ContentRecordTest, apparatusKeepsTheTypeAndTheQuality)
        {
            Apparatus record;
            record.blank();
            record.mId = id("alembic");
            record.mScript = id("script");
            record.mName = "Alembic";
            record.mModel = path("a.nif");
            record.mIcon = path("a.dds");
            record.mData = { .mType = Apparatus::Alembic, .mQuality = 1.5f, .mWeight = 2.f, .mValue = 80 };

            Apparatus result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mScript, record.mScript);
            EXPECT_EQ(result.mName, "Alembic");
            EXPECT_EQ(result.mModel.getOriginal(), "a.nif");
            EXPECT_EQ(result.mIcon.getOriginal(), "a.dds");
            EXPECT_EQ(result.mData.mType, Apparatus::Alembic);
            EXPECT_EQ(result.mData.mQuality, 1.5f);
            EXPECT_EQ(result.mData.mWeight, 2.f);
            EXPECT_EQ(result.mData.mValue, 80);
        }

        TEST_P(Esm3ContentRecordTest, repairItemKeepsItsUses)
        {
            Repair record;
            record.blank();
            record.mId = id("hammer");
            record.mScript = id("script");
            record.mName = "Hammer";
            record.mModel = path("h.nif");
            record.mIcon = path("h.dds");
            record.mData = { .mWeight = 3.f, .mValue = 30, .mUses = 25, .mQuality = 1.25f };

            Repair result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mScript, record.mScript);
            EXPECT_EQ(result.mName, "Hammer");
            EXPECT_EQ(result.mData.mWeight, 3.f);
            EXPECT_EQ(result.mData.mValue, 30);
            EXPECT_EQ(result.mData.mUses, 25);
            EXPECT_EQ(result.mData.mQuality, 1.25f);
        }

        TEST_P(Esm3ContentRecordTest, probeKeepsItsUses)
        {
            Probe record;
            record.blank();
            record.mId = id("probe");
            record.mScript = id("script");
            record.mName = "Probe";
            record.mModel = path("p.nif");
            record.mIcon = path("p.dds");
            record.mData = { .mWeight = 1.f, .mValue = 15, .mQuality = 0.75f, .mUses = 10 };

            Probe result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mName, "Probe");
            EXPECT_EQ(result.mData.mValue, 15);
            EXPECT_EQ(result.mData.mQuality, 0.75f);
            EXPECT_EQ(result.mData.mUses, 10);
        }

        TEST_P(Esm3ContentRecordTest, lockpickKeepsItsUses)
        {
            Lockpick record;
            record.blank();
            record.mId = id("pick");
            record.mScript = id("script");
            record.mName = "Lockpick";
            record.mModel = path("l.nif");
            record.mIcon = path("l.dds");
            record.mData = { .mWeight = 0.5f, .mValue = 5, .mQuality = 1.f, .mUses = 20 };

            Lockpick result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mName, "Lockpick");
            EXPECT_EQ(result.mData.mWeight, 0.5f);
            EXPECT_EQ(result.mData.mUses, 20);
        }

        TEST_P(Esm3ContentRecordTest, lightKeepsItsColourAndItsFlags)
        {
            Light record;
            record.blank();
            record.mId = id("lantern");
            record.mSound = id("lightsound");
            record.mScript = id("script");
            record.mName = "Lantern";
            record.mModel = path("lantern.nif");
            record.mIcon = path("lantern.dds");
            record.mData = { .mWeight = 2.f,
                .mValue = 40,
                .mTime = 600,
                .mRadius = 256,
                .mColor = 0xFF8040C0,
                .mFlags = Light::Carry | Light::Flicker };

            Light result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mSound, record.mSound);
            EXPECT_EQ(result.mScript, record.mScript);
            EXPECT_EQ(result.mName, "Lantern");
            EXPECT_EQ(result.mData.mTime, 600);
            EXPECT_EQ(result.mData.mRadius, 256);
            EXPECT_EQ(result.mData.mColor, 0xFF8040C0u);
            EXPECT_EQ(result.mData.mFlags, Light::Carry | Light::Flicker);
        }

        TEST_P(Esm3ContentRecordTest, miscellaneousItemKeepsTheKeyFlag)
        {
            Miscellaneous record;
            record.blank();
            record.mId = id("key");
            record.mScript = id("script");
            record.mName = "Key";
            record.mModel = path("k.nif");
            record.mIcon = path("k.dds");
            record.mData = { .mWeight = 0.1f, .mValue = 1, .mFlags = Miscellaneous::Key };

            Miscellaneous result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mName, "Key");
            EXPECT_EQ(result.mData.mWeight, 0.1f);
            EXPECT_EQ(result.mData.mFlags, Miscellaneous::Key);
        }

        TEST_P(Esm3ContentRecordTest, clothingKeepsItsPartsOfBothSexes)
        {
            Clothing record;
            record.blank();
            record.mId = id("shirt");
            record.mEnchant = id("shirtench");
            record.mScript = id("script");
            record.mName = "Shirt";
            record.mModel = path("shirt.nif");
            record.mIcon = path("shirt.dds");
            record.mData = { .mType = Clothing::Shirt, .mWeight = 1.f, .mValue = 12, .mEnchant = 100 };
            record.mParts.mParts.push_back(
                { .mPart = PRT_Cuirass, .mMale = id("malechest"), .mFemale = id("femalechest") });
            record.mParts.mParts.push_back({ .mPart = PRT_LUpperarm, .mMale = id("maleupperarm"), .mFemale = {} });

            Clothing result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mEnchant, record.mEnchant);
            EXPECT_EQ(result.mName, "Shirt");
            EXPECT_EQ(result.mData.mType, Clothing::Shirt);
            EXPECT_EQ(result.mData.mValue, 12);
            EXPECT_EQ(result.mData.mEnchant, 100);
            ASSERT_EQ(result.mParts.mParts.size(), 2u);
            EXPECT_EQ(result.mParts.mParts[0].mPart, PRT_Cuirass);
            EXPECT_EQ(result.mParts.mParts[0].mMale, id("malechest"));
            EXPECT_EQ(result.mParts.mParts[0].mFemale, id("femalechest"));
            EXPECT_EQ(result.mParts.mParts[1].mPart, PRT_LUpperarm);
            EXPECT_EQ(result.mParts.mParts[1].mMale, id("maleupperarm"));
            EXPECT_TRUE(result.mParts.mParts[1].mFemale.empty());
        }

        TEST_P(Esm3ContentRecordTest, armorKeepsItsPartsAndItsRating)
        {
            Armor record;
            record.blank();
            record.mId = id("helm");
            record.mEnchant = id("helmench");
            record.mScript = id("script");
            record.mName = "Helm";
            record.mModel = path("helm.nif");
            record.mIcon = path("helm.dds");
            record.mData = {
                .mType = Armor::Helmet, .mWeight = 5.f, .mValue = 100, .mHealth = 200, .mEnchant = 300, .mArmor = 40
            };
            record.mParts.mParts.push_back({ .mPart = PRT_Head, .mMale = id("malehead"), .mFemale = id("femalehead") });

            Armor result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mEnchant, record.mEnchant);
            EXPECT_EQ(result.mScript, record.mScript);
            EXPECT_EQ(result.mName, "Helm");
            EXPECT_EQ(result.mData.mType, Armor::Helmet);
            EXPECT_EQ(result.mData.mWeight, 5.f);
            EXPECT_EQ(result.mData.mValue, 100);
            EXPECT_EQ(result.mData.mHealth, 200);
            EXPECT_EQ(result.mData.mEnchant, 300);
            EXPECT_EQ(result.mData.mArmor, 40);
            ASSERT_EQ(result.mParts.mParts.size(), 1u);
            EXPECT_EQ(result.mParts.mParts[0].mMale, id("malehead"));
            EXPECT_EQ(result.mParts.mParts[0].mFemale, id("femalehead"));
        }

        TEST_P(Esm3ContentRecordTest, ingredientKeepsTheEffectsWithTheirSkillsAndAttributes)
        {
            Ingredient record;
            record.blank();
            record.mId = id("ingred");
            record.mScript = id("script");
            record.mName = "Ingredient";
            record.mModel = path("i.nif");
            record.mIcon = path("i.dds");
            record.mData.mWeight = 0.25f;
            record.mData.mValue = 7;
            record.mData.mEffectID[0] = MagicEffect::FortifyAttribute;
            record.mData.mAttributes[0] = Attribute::indexToRefId(2);
            record.mData.mEffectID[1] = MagicEffect::DamageSkill;
            record.mData.mSkills[1] = Skill::indexToRefId(5);

            Ingredient result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mName, "Ingredient");
            EXPECT_EQ(result.mData.mWeight, 0.25f);
            EXPECT_EQ(result.mData.mValue, 7);
            EXPECT_EQ(result.mData.mEffectID[0], MagicEffect::FortifyAttribute);
            EXPECT_EQ(result.mData.mAttributes[0], Attribute::indexToRefId(2));
            EXPECT_TRUE(result.mData.mSkills[0].empty());
            EXPECT_EQ(result.mData.mEffectID[1], MagicEffect::DamageSkill);
            EXPECT_EQ(result.mData.mSkills[1], Skill::indexToRefId(5));
            EXPECT_TRUE(result.mData.mAttributes[1].empty());
        }

        TEST_P(Esm3ContentRecordTest, bodyPartKeepsItsKindAndItsRace)
        {
            BodyPart record;
            record.blank();
            record.mId = id("b_n_argonian_f_hand");
            record.mRace = id("argonian");
            record.mModel = path("hand.nif");
            record.mData = {
                .mPart = BodyPart::MP_Hand, .mVampire = 1, .mFlags = BodyPart::BPF_Female, .mType = BodyPart::MT_Armor
            };

            BodyPart result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mRace, record.mRace);
            EXPECT_EQ(result.mModel.getOriginal(), "hand.nif");
            EXPECT_EQ(result.mData.mPart, BodyPart::MP_Hand);
            EXPECT_EQ(result.mData.mVampire, 1);
            EXPECT_EQ(result.mData.mFlags, BodyPart::BPF_Female);
            EXPECT_EQ(result.mData.mType, BodyPart::MT_Armor);
        }

        TEST_P(Esm3ContentRecordTest, spellKeepsItsEffects)
        {
            Spell record;
            record.blank();
            record.mId = id("heal");
            record.mName = "Heal";
            record.mData = { .mType = Spell::ST_Power, .mCost = 9, .mFlags = Spell::F_Autocalc | Spell::F_Always };
            record.mEffects = makeEffects();

            Spell result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mName, "Heal");
            EXPECT_EQ(result.mData.mType, Spell::ST_Power);
            EXPECT_EQ(result.mData.mCost, 9);
            EXPECT_EQ(result.mData.mFlags, Spell::F_Autocalc | Spell::F_Always);
            expectEffects(result.mEffects);
        }

        TEST_P(Esm3ContentRecordTest, enchantmentKeepsItsEffectsAndItsCharge)
        {
            Enchantment record;
            record.blank();
            record.mId = id("ench");
            record.mData
                = { .mType = Enchantment::WhenStrikes, .mCost = 4, .mCharge = 120, .mFlags = Enchantment::Autocalc };
            record.mEffects = makeEffects();

            Enchantment result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mData.mType, Enchantment::WhenStrikes);
            EXPECT_EQ(result.mData.mCost, 4);
            EXPECT_EQ(result.mData.mCharge, 120);
            EXPECT_EQ(result.mData.mFlags, Enchantment::Autocalc);
            expectEffects(result.mEffects);
        }

        TEST_P(Esm3ContentRecordTest, birthSignKeepsItsPowers)
        {
            BirthSign record;
            record.blank();
            record.mId = id("lady");
            record.mName = "The Lady";
            record.mDescription = "Warrior";
            record.mTexture = path("lady.dds");
            record.mPowers.mList = { id("power1"), id("power2") };

            BirthSign result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mName, "The Lady");
            EXPECT_EQ(result.mDescription, "Warrior");
            EXPECT_EQ(result.mTexture.getOriginal(), "lady.dds");
            EXPECT_THAT(result.mPowers.mList, ElementsAre(id("power1"), id("power2")));
        }

        TEST_P(Esm3ContentRecordTest, soundKeepsItsVolumeAndItsRange)
        {
            Sound record;
            record.blank();
            record.mId = id("splash");
            record.mSound = "splash.wav";
            record.mData = { .mVolume = 200, .mMinRange = 5, .mMaxRange = 80 };

            Sound result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mSound, "splash.wav");
            EXPECT_EQ(result.mData.mVolume, 200);
            EXPECT_EQ(result.mData.mMinRange, 5);
            EXPECT_EQ(result.mData.mMaxRange, 80);
        }

        TEST_P(Esm3ContentRecordTest, soundGeneratorKeepsItsCreatureAndItsSound)
        {
            SoundGenerator record;
            record.blank();
            record.mId = id("scrib left");
            record.mType = SoundGenerator::Roar;
            record.mCreature = id("scrib");
            record.mSound = id("scribroar");

            SoundGenerator result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mType, SoundGenerator::Roar);
            EXPECT_EQ(result.mCreature, record.mCreature);
            EXPECT_EQ(result.mSound, record.mSound);
        }

        TEST_P(Esm3ContentRecordTest, activatorKeepsItsScriptAndItsModel)
        {
            Activator record;
            record.blank();
            record.mId = id("lever");
            record.mScript = id("leverscript");
            record.mName = "Lever";
            record.mModel = path("lever.nif");

            Activator result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mScript, record.mScript);
            EXPECT_EQ(result.mName, "Lever");
            EXPECT_EQ(result.mModel.getOriginal(), "lever.nif");
        }

        TEST_P(Esm3ContentRecordTest, doorKeepsItsSounds)
        {
            Door record;
            record.blank();
            record.mId = id("door");
            record.mScript = id("doorscript");
            record.mOpenSound = id("dooropen");
            record.mCloseSound = id("doorclose");
            record.mName = "Door";
            record.mModel = path("door.nif");

            Door result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mOpenSound, record.mOpenSound);
            EXPECT_EQ(result.mCloseSound, record.mCloseSound);
            EXPECT_EQ(result.mName, "Door");
        }

        TEST_P(Esm3ContentRecordTest, landTextureKeepsItsIndex)
        {
            LandTexture record;
            record.blank();
            record.mId = id("sand");
            record.mIndex = 7;
            record.mTexture = path("sand.dds");

            LandTexture result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mIndex, 7u);
            EXPECT_EQ(result.mTexture.getOriginal(), "sand.dds");
        }

        TEST_P(Esm3ContentRecordTest, startScriptKeepsItsData)
        {
            StartScript record;
            record.blank();
            record.mId = id("startup");
            record.mData = "1234";

            StartScript result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mData, "1234");
        }

        TEST_P(Esm3ContentRecordTest, skillKeepsItsDescriptionAndItsUseValues)
        {
            Skill record;
            record.blank();
            record.mId = Skill::indexToRefId(4);
            record.mDescription = "Handles blunt weapons";
            record.mData.mAttribute = Attribute::indexToRefId(1);
            record.mData.mSpecialization = 2;
            record.mData.mUseValue[0] = 1.5f;
            record.mData.mUseValue[3] = 0.25f;

            Skill result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mDescription, "Handles blunt weapons");
            EXPECT_EQ(result.mData.mAttribute, Attribute::indexToRefId(1));
            EXPECT_EQ(result.mData.mSpecialization, 2);
            EXPECT_EQ(result.mData.mUseValue[0], 1.5f);
            EXPECT_EQ(result.mData.mUseValue[3], 0.25f);
        }

        TEST_P(Esm3ContentRecordTest, magicEffectKeepsItsSoundsItsParticlesAndItsCost)
        {
            MagicEffect record;
            record.blank();
            record.mId = MagicEffect::FireDamage;
            record.mData.mSchool = Skill::indexToRefId(10);
            record.mData.mBaseCost = 2.5f;
            record.mData.mFlags = MagicEffect::Harmful | MagicEffect::CastTarget;
            record.mData.mRed = 255;
            record.mData.mGreen = 128;
            record.mData.mBlue = 64;
            record.mData.mSpeed = 1.5f;
            record.mIcon = path("fire.dds");
            record.mParticle = path("fire.tga");
            record.mCasting = id("firecast");
            record.mHit = id("firehit");
            record.mArea = id("firearea");
            record.mBolt = id("firebolt");
            record.mCastSound = id("castsound");
            record.mBoltSound = id("boltsound");
            record.mHitSound = id("hitsound");
            record.mAreaSound = id("areasound");
            record.mDescription = "Burns";

            MagicEffect result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mId, MagicEffect::FireDamage);
            EXPECT_EQ(result.mData.mSchool, Skill::indexToRefId(10));
            EXPECT_EQ(result.mData.mBaseCost, 2.5f);
            EXPECT_EQ(result.mData.mFlags, MagicEffect::Harmful | MagicEffect::CastTarget);
            EXPECT_EQ(result.mData.mRed, 255);
            EXPECT_EQ(result.mData.mGreen, 128);
            EXPECT_EQ(result.mData.mBlue, 64);
            EXPECT_EQ(result.mData.mSpeed, 1.5f);
            EXPECT_EQ(result.mIcon.getOriginal(), "fire.dds");
            EXPECT_EQ(result.mParticle.getOriginal(), "fire.tga");
            EXPECT_EQ(result.mCasting, record.mCasting);
            EXPECT_EQ(result.mHit, record.mHit);
            EXPECT_EQ(result.mArea, record.mArea);
            EXPECT_EQ(result.mBolt, record.mBolt);
            EXPECT_EQ(result.mCastSound, record.mCastSound);
            EXPECT_EQ(result.mBoltSound, record.mBoltSound);
            EXPECT_EQ(result.mHitSound, record.mHitSound);
            EXPECT_EQ(result.mAreaSound, record.mAreaSound);
            EXPECT_EQ(result.mDescription, "Burns");
        }

        TEST_P(Esm3ContentRecordTest, pathgridKeepsThePointsAndTheEdges)
        {
            Pathgrid record;
            record.blank();
            record.mCell = id("Balmora");
            record.mData = { .mX = 0, .mY = 0, .mGranularity = 1024, .mPoints = 3 };
            record.mPoints = { Pathgrid::Point(1, 2, 3), Pathgrid::Point(4, 5, 6), Pathgrid::Point(7, 8, 9) };
            record.mEdges = { { 0, 1 }, { 0, 2 }, { 1, 0 }, { 2, 0 } };

            Pathgrid result;
            saveAndLoadRecord(record, GetParam(), result);

            EXPECT_EQ(result.mCell, record.mCell);
            EXPECT_EQ(result.mData.mGranularity, 1024);
            EXPECT_EQ(result.mData.mPoints, 3);
            ASSERT_EQ(result.mPoints.size(), 3u);
            EXPECT_EQ(result.mPoints[1].mX, 4);
            EXPECT_EQ(result.mPoints[1].mY, 5);
            EXPECT_EQ(result.mPoints[1].mZ, 6);
            EXPECT_EQ(result.mPoints[0].mConnectionNum, 2);
            EXPECT_EQ(result.mPoints[1].mConnectionNum, 1);
            ASSERT_EQ(result.mEdges.size(), 4u);
            EXPECT_EQ(result.mEdges[1].mV0, 0u);
            EXPECT_EQ(result.mEdges[1].mV1, 2u);
            EXPECT_EQ(result.mEdges[2].mV0, 1u);
            EXPECT_EQ(result.mEdges[2].mV1, 0u);
        }

        TEST_P(Esm3ContentRecordTest, levelledListsKeepTheirEntriesAndTheirFlags)
        {
            ItemLevList items;
            items.blank();
            items.mId = id("l_items");
            items.mFlags = ItemLevList::Each | ItemLevList::AllLevels;
            items.mChanceNone = 25;
            items.mList = { { id("sword"), 1 }, { id("axe"), 5 } };

            ItemLevList itemsResult;
            saveAndLoadRecord(items, GetParam(), itemsResult);

            EXPECT_EQ(itemsResult.mId, items.mId);
            EXPECT_EQ(itemsResult.mFlags, ItemLevList::Each | ItemLevList::AllLevels);
            EXPECT_EQ(itemsResult.mChanceNone, 25);
            ASSERT_EQ(itemsResult.mList.size(), 2u);
            EXPECT_EQ(itemsResult.mList[0].mId, id("sword"));
            EXPECT_EQ(itemsResult.mList[0].mLevel, 1);
            EXPECT_EQ(itemsResult.mList[1].mId, id("axe"));
            EXPECT_EQ(itemsResult.mList[1].mLevel, 5);

            CreatureLevList creatures;
            creatures.blank();
            creatures.mId = id("l_creatures");
            creatures.mFlags = CreatureLevList::AllLevels;
            creatures.mChanceNone = 0;
            creatures.mList = { { id("rat"), 2 } };

            CreatureLevList creaturesResult;
            saveAndLoadRecord(creatures, GetParam(), creaturesResult);

            EXPECT_EQ(creaturesResult.mId, creatures.mId);
            ASSERT_EQ(creaturesResult.mList.size(), 1u);
            EXPECT_EQ(creaturesResult.mList[0].mId, id("rat"));
            EXPECT_EQ(creaturesResult.mList[0].mLevel, 2);
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3ContentRecordTest, ValuesIn(getFormats()));

        TEST(Esm3DeletedRecordTest, aDeletedLandTextureKeepsItsIdAndItsIndex)
        {
            LandTexture record;
            record.blank();
            record.mId = id("removed");
            record.mIndex = 3;
            LandTexture result;
            EXPECT_TRUE(loadDeleted(record, result));
            EXPECT_EQ(result.mId, id("removed"));
            EXPECT_EQ(result.mIndex, 3u);
        }

        TEST(Esm3DeletedRecordTest, aDeletedRecordKeepsOnlyItsId)
        {
            expectDeletedKeepsTheId(Potion{});
            expectDeletedKeepsTheId(Apparatus{});
            expectDeletedKeepsTheId(Repair{});
            expectDeletedKeepsTheId(Probe{});
            expectDeletedKeepsTheId(Lockpick{});
            expectDeletedKeepsTheId(Light{});
            expectDeletedKeepsTheId(Miscellaneous{});
            expectDeletedKeepsTheId(Clothing{});
            expectDeletedKeepsTheId(Armor{});
            expectDeletedKeepsTheId(Ingredient{});
            expectDeletedKeepsTheId(BodyPart{});
            expectDeletedKeepsTheId(Spell{});
            expectDeletedKeepsTheId(Enchantment{});
            expectDeletedKeepsTheId(BirthSign{});
            expectDeletedKeepsTheId(Sound{});
            expectDeletedKeepsTheId(SoundGenerator{});
            expectDeletedKeepsTheId(Activator{});
            expectDeletedKeepsTheId(Door{});
            expectDeletedKeepsTheId(StartScript{});
        }
    }
}
