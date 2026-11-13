#include <components/esm4/combatcensus.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadexpl.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadproj.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    // Loads the one record of a plugin that is a Fallout 3 file (or of the header version the caller gives), by its
    // record only: the group headers of Fallout have another size than the synthetic plugins have.
    template <class Record>
    Record loadFromAPlugin(float version, std::uint32_t formVersion, std::string_view type, const std::string& data)
    {
        std::string hedr;
        append<float>(hedr, version);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        const std::string plugin = versionedRecord("TES4", 0, subRecord("HEDR", hedr), formVersion)
            + versionedRecord(type, 1, data, formVersion);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "synthetic.esp", nullptr, nullptr);
        EXPECT_TRUE(reader.getRecordHeader());
        reader.getRecordData();
        Record result;
        result.load(reader);
        return result;
    }

    // DNAM of a weapon of Fallout 3 and New Vegas: the 136 bytes both games have, with a value in each field
    std::string weaponGameData(std::uint32_t animationType = 5, std::size_t size = 136)
    {
        std::string data;
        append<std::uint32_t>(data, animationType); // 0
        append<float>(data, 1.25f); // 4 animation multiplier
        append<float>(data, 64.f); // 8 reach
        append<std::uint8_t>(data, 0x02 | 0x04); // 12 automatic and scope
        append<std::uint8_t>(data, 231); // 13 grip
        append<std::uint8_t>(data, 3); // 14 ammo use
        append<std::uint8_t>(data, 4); // 15 reload animation
        append<float>(data, 0.5f); // 16 min spread
        append<float>(data, 2.5f); // 20 spread
        append<float>(data, 0.f); // 24
        append<float>(data, 45.f); // 28 sight fov
        append<float>(data, 0.f); // 32
        append<std::uint32_t>(data, 0x00010020); // 36 projectile
        append<std::uint8_t>(data, 60); // 40 vats chance
        append<std::uint8_t>(data, 5); // 41 attack animation
        append<std::uint8_t>(data, 2); // 42 projectile count
        append<std::uint8_t>(data, 0); // 43
        append<float>(data, 100.f); // 44 min range
        append<float>(data, 3000.f); // 48 max range
        append<std::uint32_t>(data, 1); // 52 on hit
        append<std::uint32_t>(data, 0x08 | 0x200); // 56 flags 2
        append<float>(data, 1.5f); // 60 attack multiplier
        append<float>(data, 6.f); // 64 fire rate
        append<float>(data, 20.f); // 68 override action points
        append<float>(data, 0.f); // 72
        append<float>(data, 0.f); // 76
        append<float>(data, 0.f); // 80
        append<float>(data, 0.75f); // 84 damage to weapon multiplier
        append<float>(data, 4.f); // 88 attacks per second
        append<float>(data, 2.2f); // 92 reload time
        append<float>(data, 1.1f); // 96 jam time
        append<float>(data, 7.f); // 100 aim arc
        append<std::uint32_t>(data, 38); // 104 skill
        append<std::uint32_t>(data, 0); // 108
        append<float>(data, 0.f); // 112
        append<float>(data, 1.6f); // 116 limb damage multiplier
        append<std::uint32_t>(data, 0xffffffffu); // 120 resist
        append<float>(data, 0.4f); // 124 sight usage
        append<float>(data, 0.1f); // 128 semi automatic delay min
        append<float>(data, 0.3f); // 132 max
        data.resize(size);
        return data;
    }

    // DATA of a weapon of Fallout 3 and New Vegas, 15 bytes
    std::string weaponData(std::uint16_t damage, std::uint8_t clip)
    {
        std::string data;
        append<std::int32_t>(data, 100); // value
        append<std::int32_t>(data, 250); // health
        append<float>(data, 5.5f); // weight
        append(data, damage);
        append(data, clip);
        return data;
    }

    std::string criticalData()
    {
        std::string data;
        append<std::uint16_t>(data, 12); // damage
        append<std::uint16_t>(data, 0);
        append<float>(data, 2.f); // chance multiplier
        append<std::uint8_t>(data, 1); // on death
        data.append(3, '\0');
        append<std::uint32_t>(data, 0x00010040); // effect
        return data;
    }

    std::string vatsData()
    {
        std::string data;
        append<std::uint32_t>(data, 0x00010050); // effect
        append<float>(data, 30.f); // skill
        append<float>(data, 1.5f); // damage multiplier
        append<float>(data, 25.f); // action points
        append<std::uint8_t>(data, 1); // silent
        append<std::uint8_t>(data, 0); // mod required
        append<std::uint8_t>(data, 2); // padding
        append<std::uint8_t>(data, 0);
        return data;
    }

    std::string weaponRecord(std::string_view editorId, std::uint32_t animationType = 5)
    {
        return zString("EDID", editorId) + subRecord("DATA", weaponData(21, 15))
            + subRecord("DNAM", weaponGameData(animationType)) + subRecord("CRDT", criticalData())
            + subRecord("VATS", vatsData()) + valueSubRecord<std::uint32_t>("NAM0", 0x00010060)
            + valueSubRecord<std::uint32_t>("INAM", 0x00010070) + valueSubRecord<std::uint32_t>("WNAM", 0x00010080)
            + valueSubRecord<std::int32_t>("ETYP", 5) + valueSubRecord<std::uint32_t>("VNAM", 2);
    }

    TEST(ESM4CombatDataTest, readsTheGameDataOfAWeaponOfFallout)
    {
        const ESM4::Weapon weapon = loadFromAPlugin<ESM4::Weapon>(1.34f, 15, "WEAP", weaponRecord("Rifle"));

        EXPECT_EQ(weapon.mData.damage, 21);
        EXPECT_EQ(weapon.mData.clipSize, 15);
        ASSERT_TRUE(weapon.mHasGameData);
        const ESM4::Weapon::GameData& game = weapon.mGame;
        EXPECT_EQ(game.mAnimationType, ESM4::Weapon::GameData::RifleBallistic);
        EXPECT_FLOAT_EQ(game.mAnimationMultiplier, 1.25f);
        EXPECT_FLOAT_EQ(game.mReach, 64.f);
        EXPECT_EQ(game.mFlags1, ESM4::Weapon::GameData::Automatic | ESM4::Weapon::GameData::HasScope);
        EXPECT_EQ(game.mGripAnimation, 231);
        EXPECT_EQ(game.mAmmoUse, 3);
        EXPECT_EQ(game.mReloadAnimation, 4);
        EXPECT_FLOAT_EQ(game.mMinSpread, 0.5f);
        EXPECT_FLOAT_EQ(game.mSpread, 2.5f);
        EXPECT_FLOAT_EQ(game.mSightFov, 45.f);
        EXPECT_EQ(game.mProjectile.toUint32(), 0x00010020u);
        EXPECT_EQ(game.mBaseVatsChance, 60);
        EXPECT_EQ(game.mAttackAnimation, 5);
        EXPECT_EQ(game.mProjectileCount, 2);
        EXPECT_FLOAT_EQ(game.mMinRange, 100.f);
        EXPECT_FLOAT_EQ(game.mMaxRange, 3000.f);
        EXPECT_EQ(game.mOnHit, 1u);
        EXPECT_EQ(game.mFlags2, ESM4::Weapon::GameData::OverrideActionPoints | ESM4::Weapon::GameData::BurstShot);
        EXPECT_FLOAT_EQ(game.mAttackMultiplier, 1.5f);
        EXPECT_FLOAT_EQ(game.mFireRate, 6.f);
        EXPECT_FLOAT_EQ(game.mOverrideActionPoints, 20.f);
        EXPECT_FLOAT_EQ(game.mOverrideDamageToWeaponMult, 0.75f);
        EXPECT_FLOAT_EQ(game.mAttacksPerSecond, 4.f);
        EXPECT_FLOAT_EQ(game.mReloadTime, 2.2f);
        EXPECT_FLOAT_EQ(game.mJamTime, 1.1f);
        EXPECT_FLOAT_EQ(game.mAimArc, 7.f);
        EXPECT_EQ(game.mSkill, 38u);
        EXPECT_FLOAT_EQ(game.mLimbDamageMultiplier, 1.6f);
        EXPECT_EQ(game.mResist, 0xffffffffu);
        EXPECT_FLOAT_EQ(game.mSightUsage, 0.4f);
        EXPECT_FLOAT_EQ(game.mSemiAutomaticDelayMin, 0.1f);
        EXPECT_FLOAT_EQ(game.mSemiAutomaticDelayMax, 0.3f);
    }

    TEST(ESM4CombatDataTest, readsTheCriticalVatsAmmunitionAndImpactDataOfAWeapon)
    {
        const ESM4::Weapon weapon = loadFromAPlugin<ESM4::Weapon>(1.34f, 15, "WEAP", weaponRecord("Rifle"));

        ASSERT_TRUE(weapon.mHasCritical);
        EXPECT_EQ(weapon.mCritical.mDamage, 12);
        EXPECT_FLOAT_EQ(weapon.mCritical.mChanceMultiplier, 2.f);
        EXPECT_EQ(weapon.mCritical.mFlags, 1);
        EXPECT_EQ(weapon.mCritical.mEffect.toUint32(), 0x00010040u);
        ASSERT_TRUE(weapon.mHasVats);
        EXPECT_EQ(weapon.mVats.mEffect.toUint32(), 0x00010050u);
        EXPECT_FLOAT_EQ(weapon.mVats.mSkill, 30.f);
        EXPECT_FLOAT_EQ(weapon.mVats.mDamageMultiplier, 1.5f);
        EXPECT_FLOAT_EQ(weapon.mVats.mActionPoints, 25.f);
        EXPECT_EQ(weapon.mVats.mSilent, 1);
        EXPECT_EQ(weapon.mVats.mModRequired, 0);
        EXPECT_EQ(weapon.mAmmo.toUint32(), 0x00010060u);
        EXPECT_EQ(weapon.mImpactDataSet.toUint32(), 0x00010070u);
        EXPECT_EQ(weapon.mFirstPersonObject.toUint32(), 0x00010080u);
        EXPECT_EQ(weapon.mEquipType, 5u);
        EXPECT_EQ(weapon.mSoundLevel, 2u);
    }

    TEST(ESM4CombatDataTest, readsTheSoundsOfAWeaponOfFallout)
    {
        // The first SNAM is the shot, a second one (New Vegas) the shot heard from far away, XNAM the shot that the
        // player hears, TNAM the click of a weapon with no ammunition and NAM7 the loop of an automatic weapon
        const std::string sounds = zString("EDID", "Sounds") + valueSubRecord<std::uint32_t>("SNAM", 0x00010001)
            + valueSubRecord<std::uint32_t>("SNAM", 0x00010002) + valueSubRecord<std::uint32_t>("XNAM", 0x00010003)
            + valueSubRecord<std::uint32_t>("TNAM", 0x00010004) + valueSubRecord<std::uint32_t>("NAM7", 0x00010005);
        const ESM4::Weapon weapon = loadFromAPlugin<ESM4::Weapon>(1.34f, 15, "WEAP", sounds);
        EXPECT_EQ(weapon.mShotSound.toUint32(), 0x00010001u);
        EXPECT_EQ(weapon.mShotSound2D.toUint32(), 0x00010003u);
        EXPECT_EQ(weapon.mDryFireSound.toUint32(), 0x00010004u);
        EXPECT_EQ(weapon.mLoopSound.toUint32(), 0x00010005u);

        const ESM4::Weapon without = loadFromAPlugin<ESM4::Weapon>(1.34f, 15, "WEAP", zString("EDID", "Silent"));
        EXPECT_TRUE(without.mShotSound.isZeroOrUnset());
        EXPECT_TRUE(without.mShotSound2D.isZeroOrUnset());
        EXPECT_TRUE(without.mDryFireSound.isZeroOrUnset());
        EXPECT_TRUE(without.mLoopSound.isZeroOrUnset());

        // A first SNAM with no sound is not replaced by the second, which is the shot heard from far away
        const ESM4::Weapon silent = loadFromAPlugin<ESM4::Weapon>(1.34f, 15, "WEAP",
            zString("EDID", "FarOnly") + valueSubRecord<std::uint32_t>("SNAM", 0)
                + valueSubRecord<std::uint32_t>("SNAM", 0x00010002));
        EXPECT_TRUE(silent.mShotSound.isZeroOrUnset());

        // The weapons of Skyrim use these subrecords for other things
        const ESM4::Weapon skyrim = loadFromAPlugin<ESM4::Weapon>(0.94f, 43, "WEAP", sounds);
        EXPECT_TRUE(skyrim.mShotSound.isZeroOrUnset());
        EXPECT_TRUE(skyrim.mShotSound2D.isZeroOrUnset());
        EXPECT_TRUE(skyrim.mDryFireSound.isZeroOrUnset());
        EXPECT_TRUE(skyrim.mLoopSound.isZeroOrUnset());
    }

    TEST(ESM4CombatDataTest, readsTheEquipIdleAndBlockSoundsOfAWeaponOfFallout)
    {
        // NAM9 takes the weapon out and NAM8 puts it away, UNAM goes on while it is held out and NAM6 is a blow it blocks
        const std::string sounds = zString("EDID", "Hum") + valueSubRecord<std::uint32_t>("NAM9", 0x00010011)
            + valueSubRecord<std::uint32_t>("NAM8", 0x00010012) + valueSubRecord<std::uint32_t>("UNAM", 0x00010013)
            + valueSubRecord<std::uint32_t>("NAM6", 0x00010014);
        const ESM4::Weapon weapon = loadFromAPlugin<ESM4::Weapon>(1.34f, 15, "WEAP", sounds);
        EXPECT_EQ(weapon.mEquipSound.toUint32(), 0x00010011u);
        EXPECT_EQ(weapon.mUnequipSound.toUint32(), 0x00010012u);
        EXPECT_EQ(weapon.mIdleSound.toUint32(), 0x00010013u);
        EXPECT_EQ(weapon.mBlockSound.toUint32(), 0x00010014u);

        const ESM4::Weapon without = loadFromAPlugin<ESM4::Weapon>(1.34f, 15, "WEAP", zString("EDID", "Quiet"));
        EXPECT_TRUE(without.mEquipSound.isZeroOrUnset());
        EXPECT_TRUE(without.mUnequipSound.isZeroOrUnset());
        EXPECT_TRUE(without.mIdleSound.isZeroOrUnset());
        EXPECT_TRUE(without.mBlockSound.isZeroOrUnset());

        // The weapons of Skyrim use these subrecords for other things
        const ESM4::Weapon skyrim = loadFromAPlugin<ESM4::Weapon>(0.94f, 43, "WEAP", sounds);
        EXPECT_TRUE(skyrim.mEquipSound.isZeroOrUnset());
        EXPECT_TRUE(skyrim.mIdleSound.isZeroOrUnset());
    }

    TEST(ESM4CombatDataTest, readsALongerGameDataAndLeavesAShorterOne)
    {
        // New Vegas has more after the 136 bytes; the first of them are the same
        const ESM4::Weapon longer = loadFromAPlugin<ESM4::Weapon>(
            1.34f, 15, "WEAP", zString("EDID", "Long") + subRecord("DNAM", weaponGameData(4, 204)));
        ASSERT_TRUE(longer.mHasGameData);
        EXPECT_EQ(longer.mGame.mAnimationType, ESM4::Weapon::GameData::PistolEnergy);
        EXPECT_FLOAT_EQ(longer.mGame.mFireRate, 6.f);

        const ESM4::Weapon shorter = loadFromAPlugin<ESM4::Weapon>(
            1.34f, 15, "WEAP", zString("EDID", "Short") + subRecord("DNAM", weaponGameData(4, 40)));
        EXPECT_FALSE(shorter.mHasGameData);
        EXPECT_EQ(shorter.mGame.mAnimationType, ESM4::Weapon::GameData::HandToHand);

        // Some records of Fallout 3 end after 120 bytes: the skill and the resistance are there, what follows is not
        const ESM4::Weapon ending = loadFromAPlugin<ESM4::Weapon>(
            0.94f, 15, "WEAP", zString("EDID", "Ending") + subRecord("DNAM", weaponGameData(3, 120)));
        ASSERT_TRUE(ending.mHasGameData);
        EXPECT_EQ(ending.mGame.mAnimationType, ESM4::Weapon::GameData::PistolBallistic);
        EXPECT_FLOAT_EQ(ending.mGame.mFireRate, 6.f);
        EXPECT_EQ(ending.mGame.mResist, 0xffffffffu);
        EXPECT_FLOAT_EQ(ending.mGame.mSightUsage, 0.f);
    }

    TEST(ESM4CombatDataTest, doesNotReadTheWeaponDataOfAnotherGameAsOneOfFallout)
    {
        // Fallout 4 has 132 bytes of another layout in its DNAM, Skyrim has a record of another size
        const ESM4::Weapon fallout4 = loadFromAPlugin<ESM4::Weapon>(
            0.95f, 131, "WEAP", zString("EDID", "Four") + subRecord("DNAM", weaponGameData(4, 132)));
        EXPECT_FALSE(fallout4.mHasGameData);
        const ESM4::Weapon skyrim = loadFromAPlugin<ESM4::Weapon>(
            0.94f, 43, "WEAP", zString("EDID", "Five") + subRecord("DNAM", weaponGameData(4, 136)));
        EXPECT_FALSE(skyrim.mHasGameData);
    }

    TEST(ESM4CombatDataTest, readsTheRatingAndThresholdOfArmor)
    {
        // Fallout 3: the rating times 100 and flags
        std::string fo3;
        append<std::int16_t>(fo3, 2500);
        append<std::uint16_t>(fo3, 1);
        const ESM4::Armor first
            = loadFromAPlugin<ESM4::Armor>(0.94f, 15, "ARMO", zString("EDID", "A") + subRecord("DNAM", fo3));
        ASSERT_TRUE(first.mHasRating);
        EXPECT_FLOAT_EQ(first.mRating, 25.f);
        EXPECT_EQ(first.mDnamFlags, 1);
        EXPECT_FLOAT_EQ(first.mDamageThreshold, 0.f);

        // New Vegas: then the damage threshold
        std::string nv = fo3;
        append<float>(nv, 12.5f);
        append<std::uint32_t>(nv, 0);
        const ESM4::Armor second
            = loadFromAPlugin<ESM4::Armor>(1.34f, 15, "ARMO", zString("EDID", "B") + subRecord("DNAM", nv));
        ASSERT_TRUE(second.mHasRating);
        EXPECT_FLOAT_EQ(second.mRating, 25.f);
        EXPECT_FLOAT_EQ(second.mDamageThreshold, 12.5f);

        // Another size is not this record
        const ESM4::Armor other = loadFromAPlugin<ESM4::Armor>(
            1.34f, 15, "ARMO", zString("EDID", "C") + subRecord("DNAM", std::string(8, '\0')));
        EXPECT_FALSE(other.mHasRating);
    }

    TEST(ESM4CombatDataTest, doesNotReadTheArmorRatingOfSkyrimAsOneOfFallout)
    {
        // The DNAM of Skyrim has the size of the one of Fallout 3 and holds one number
        std::string skyrim;
        append<std::int32_t>(skyrim, 2500);
        const ESM4::Armor armor
            = loadFromAPlugin<ESM4::Armor>(0.94f, 43, "ARMO", zString("EDID", "S") + subRecord("DNAM", skyrim));
        EXPECT_FALSE(armor.mHasRating);
    }

    std::string aiData(std::uint8_t aggression = 2, std::uint8_t confidence = 3, std::uint8_t assistance = 2,
        std::uint8_t aggroRadiusFlags = 1)
    {
        std::string data;
        append<std::uint8_t>(data, aggression); // very aggressive
        append<std::uint8_t>(data, confidence); // brave
        append<std::uint8_t>(data, 40); // energy
        append<std::uint8_t>(data, 60); // responsibility
        append<std::uint8_t>(data, 5); // mood
        data.append(3, '\0');
        append<std::uint32_t>(data, 0x1234); // services
        append<std::int8_t>(data, 4); // trained skill
        append<std::uint8_t>(data, 50); // max training level
        append<std::uint8_t>(data, assistance); // helps friends and allies
        append<std::uint8_t>(data, aggroRadiusFlags); // the aggro radius counts
        append<std::int32_t>(data, 2000);
        return data;
    }

    void expectAiData(const ESM4::FalloutAIData& ai)
    {
        ASSERT_TRUE(ai.mKnown);
        EXPECT_EQ(ai.mAggression, ESM4::FalloutAIData::VeryAggressive);
        EXPECT_EQ(ai.mConfidence, ESM4::FalloutAIData::Brave);
        EXPECT_EQ(ai.mEnergyLevel, 40);
        EXPECT_EQ(ai.mResponsibility, 60);
        EXPECT_EQ(ai.mMood, 5);
        EXPECT_EQ(ai.mServices, 0x1234u);
        EXPECT_EQ(ai.mTrainedSkill, 4);
        EXPECT_EQ(ai.mMaxTrainingLevel, 50);
        EXPECT_EQ(ai.mAssistance, ESM4::FalloutAIData::HelpsFriendsAndAllies);
        EXPECT_EQ(ai.mAggroRadiusFlags, 1);
        EXPECT_EQ(ai.mAggroRadius, 2000);
    }

    TEST(ESM4CombatDataTest, readsTheBehaviourOfACharacterAndACreatureInAFight)
    {
        const std::vector<ESM4::Npc> npcs
            = loadRecords<ESM4::Npc>("NPC_", record("NPC_", 1, zString("EDID", "Guard") + subRecord("AIDT", aiData())));
        expectAiData(npcs.at(0).mFalloutAI);

        // A creature also has combat skills, a damage and a reach
        std::string stats(17, '\0');
        stats[0] = 2; // type
        stats[1] = 70; // combat skill
        stats[2] = 5; // magic skill
        stats[3] = 30; // stealth skill
        stats[4] = 80; // health
        stats[8] = 14; // damage
        const std::vector<ESM4::Creature> creatures = loadRecords<ESM4::Creature>("CREA",
            record("CREA", 1,
                zString("EDID", "Beast") + subRecord("AIDT", aiData()) + subRecord("DATA", stats)
                    + subRecord("RNAM", std::string(1, '\x58'))));
        const ESM4::Creature& creature = creatures.at(0);
        expectAiData(creature.mFalloutAI);
        EXPECT_EQ(creature.mFalloutStats.mHealth, 80);
        EXPECT_EQ(creature.mFalloutStats.mCombatSkill, 70);
        EXPECT_EQ(creature.mFalloutStats.mMagicSkill, 5);
        EXPECT_EQ(creature.mFalloutStats.mStealthSkill, 30);
        EXPECT_EQ(creature.mFalloutStats.mDamage, 14);
        EXPECT_EQ(creature.mAttackReach, 0x58);
    }

    TEST(ESM4CombatDataTest, readsTheEffectsOfAPotion)
    {
        std::string weight;
        append<float>(weight, 0.5f);
        std::string effect;
        append<std::uint32_t>(effect, 30); // magnitude
        append<std::uint32_t>(effect, 0); // area
        append<std::uint32_t>(effect, 10); // duration
        append<std::uint32_t>(effect, 0); // range
        append<std::int32_t>(effect, 16); // actor value
        const std::vector<ESM4::Potion> potions = loadRecords<ESM4::Potion>("ALCH",
            record("ALCH", 1,
                zString("EDID", "Stimpak") + subRecord("DATA", weight)
                    + valueSubRecord<std::uint32_t>("EFID", 0x00010090) + subRecord("EFIT", effect)
                    + valueSubRecord<std::uint32_t>("EFID", 0x00010091) + subRecord("EFIT", effect)));
        const ESM4::Potion& potion = potions.at(0);

        ASSERT_EQ(potion.mEffects.size(), 2u);
        EXPECT_EQ(potion.mEffects[0].mBaseEffect.toUint32(), 0x00010090u);
        EXPECT_TRUE(potion.mEffects[0].mHasData);
        EXPECT_EQ(potion.mEffects[0].mData.mMagnitude, 30u);
        EXPECT_EQ(potion.mEffects[0].mData.mDuration, 10u);
        EXPECT_EQ(potion.mEffects[0].mData.mActorValue, 16);
        EXPECT_EQ(potion.mEffects[1].mBaseEffect.toUint32(), 0x00010091u);
    }

    TEST(ESM4CombatDataTest, readsTheConditionsOfTheEffectsOfAPotion)
    {
        std::string effect;
        append<std::uint32_t>(effect, 30); // magnitude
        append<std::uint32_t>(effect, 0); // area
        append<std::uint32_t>(effect, 10); // duration
        append<std::uint32_t>(effect, 0); // range
        append<std::int32_t>(effect, 16); // actor value
        // The conditions follow the effect they are for; the second effect has none
        const std::vector<ESM4::Potion> potions = loadRecords<ESM4::Potion>("ALCH",
            record("ALCH", 1,
                zString("EDID", "Stimpak") + valueSubRecord<std::uint32_t>("EFID", 0x00010090)
                    + subRecord("EFIT", effect) + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00010005))
                    + subRecord("CTDA", conditionData(0x60, 2.5f, 15, 0x00010006))
                    + valueSubRecord<std::uint32_t>("EFID", 0x00010091) + subRecord("EFIT", effect)));
        const ESM4::Potion& potion = potions.at(0);

        ASSERT_EQ(potion.mEffects.size(), 2u);
        ASSERT_EQ(potion.mEffects[0].mConditions.size(), 2u);
        EXPECT_EQ(potion.mEffects[0].mConditions[0].functionIndex, 14u);
        EXPECT_EQ(potion.mEffects[0].mConditions[1].reference, 0x00010006u);
        EXPECT_TRUE(potion.mEffects[1].mConditions.empty());
    }

    TEST(ESM4CombatDataTest, leavesTheConditionsOfAPotionInTheLongerLayoutOfSkyrimUnread)
    {
        std::string effect;
        append<std::uint32_t>(effect, 30); // magnitude
        append<std::uint32_t>(effect, 0); // area
        append<std::uint32_t>(effect, 10); // duration
        append<std::uint32_t>(effect, 0); // range
        append<std::int32_t>(effect, 16); // actor value
        // A condition of Skyrim has one more parameter than the games of this project: the record still loads
        std::string longer = conditionData(0x40, 2.5f, 14, 0x00010005);
        append<std::uint32_t>(longer, 0);
        const std::vector<ESM4::Potion> potions = loadRecords<ESM4::Potion>("ALCH",
            record("ALCH", 1,
                zString("EDID", "Potion") + valueSubRecord<std::uint32_t>("EFID", 0x00010090)
                    + subRecord("EFIT", effect) + subRecord("CTDA", longer)));
        const ESM4::Potion& potion = potions.at(0);

        ASSERT_EQ(potion.mEffects.size(), 1u);
        EXPECT_TRUE(potion.mEffects[0].mConditions.empty());
    }

    TEST(ESM4CombatDataTest, decodesTheDataOfAProjectile)
    {
        // The explosion, two lights, three sounds and the default weapon source are form IDs
        std::string bytes = bytesWithFormIds(68, 19, { 16, 20, 36, 40, 56, 60, 64 }, 0x00010001);
        const std::uint16_t flags = 0x0001 | 0x0002;
        const std::uint16_t type = 4;
        const float speed = 6000.f;
        const float range = 7000.f;
        std::memcpy(bytes.data(), &flags, 2);
        std::memcpy(bytes.data() + 2, &type, 2);
        std::memcpy(bytes.data() + 8, &speed, 4);
        std::memcpy(bytes.data() + 12, &range, 4);

        const std::vector<ESM4::Projectile> projectiles = loadRecords<ESM4::Projectile>(
            "PROJ", record("PROJ", 1, zString("EDID", "Bullet") + subRecord("DATA", bytes)));
        const ESM4::Projectile::GameData data = projectiles.at(0).gameData();

        EXPECT_EQ(data.mFlags, flags);
        EXPECT_EQ(data.mType, type);
        EXPECT_FLOAT_EQ(data.mSpeed, 6000.f);
        EXPECT_FLOAT_EQ(data.mRange, 7000.f);
        EXPECT_EQ(data.mExplosion.toUint32(), 0x00010001u);
        EXPECT_EQ(data.mSound.toUint32(), 0x00010001u);
        EXPECT_EQ(data.mDefaultWeaponSource.toUint32(), 0x00010001u);
        EXPECT_EQ(data.mLight.toUint32(), 0x00010001u);
    }

    TEST(ESM4CombatDataTest, decodesTheDataOfAnExplosion)
    {
        std::string bytes = bytesWithFormIds(52, 19, { 12, 16, 28, 32 }, 0x00010002);
        const float force = 250.f;
        const float damage = 40.f;
        const float radius = 384.f;
        std::memcpy(bytes.data(), &force, 4);
        std::memcpy(bytes.data() + 4, &damage, 4);
        std::memcpy(bytes.data() + 8, &radius, 4);

        const std::vector<ESM4::Explosion> explosions = loadRecords<ESM4::Explosion>(
            "EXPL", record("EXPL", 1, zString("EDID", "Boom") + subRecord("DATA", bytes)));
        const ESM4::Explosion::GameData data = explosions.at(0).gameData();

        EXPECT_FLOAT_EQ(data.mForce, 250.f);
        EXPECT_FLOAT_EQ(data.mDamage, 40.f);
        EXPECT_FLOAT_EQ(data.mRadius, 384.f);
        EXPECT_EQ(data.mLight.toUint32(), 0x00010002u);
        EXPECT_EQ(data.mImpactDataSet.toUint32(), 0x00010002u);
        EXPECT_EQ(data.mSound2.toUint32(), 0x00010002u);
    }

    // A Fallout file: its records and groups have the headers of Fallout, 24 bytes
    std::string falloutRecord(
        std::string_view recordType, std::uint32_t id, std::string_view data, std::uint32_t recordFlags = 0)
    {
        std::string result(recordType);
        append<std::uint32_t>(result, static_cast<std::uint32_t>(data.size()));
        append<std::uint32_t>(result, recordFlags);
        append<std::uint32_t>(result, id);
        append<std::uint32_t>(result, 0); // revision
        append<std::uint16_t>(result, 15); // form version
        append<std::uint16_t>(result, 0);
        result.append(data);
        return result;
    }

    std::string falloutGroup(std::string_view label, std::string_view children)
    {
        std::string result("GRUP");
        append<std::uint32_t>(result, static_cast<std::uint32_t>(24 + children.size()));
        result.append(label);
        append<std::int32_t>(result, 0); // top level group
        append<std::uint16_t>(result, 0); // stamp
        append<std::uint16_t>(result, 0);
        append<std::uint16_t>(result, 0); // version
        append<std::uint16_t>(result, 0);
        result.append(children);
        return result;
    }

    std::string censusPlugin()
    {
        std::string armour;
        append<std::int16_t>(armour, 3000);
        append<std::uint16_t>(armour, 0);
        append<float>(armour, 6.f);
        append<std::uint32_t>(armour, 0);

        std::string bytes = bytesWithFormIds(68, 19, { 16, 20, 36, 40, 56, 60, 64 }, 0x00010001);
        const std::uint16_t flags = 0x0001;
        const std::uint16_t type = 1;
        const float speed = 5000.f;
        std::memcpy(bytes.data(), &flags, 2);
        std::memcpy(bytes.data() + 2, &type, 2);
        std::memcpy(bytes.data() + 8, &speed, 4);
        const float range = 10000.f;
        std::memcpy(bytes.data() + 12, &range, 4);

        std::string effect;
        append<std::uint32_t>(effect, 30);
        append<std::uint32_t>(effect, 0);
        append<std::uint32_t>(effect, 0);
        append<std::uint32_t>(effect, 0);
        append<std::int32_t>(effect, 16);
        std::string weight;
        append<float>(weight, 0.5f);

        std::string stats(17, '\0');
        stats[4] = 80;
        stats[8] = 14;

        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);

        return falloutRecord("TES4", 0, subRecord("HEDR", hedr))
            + falloutGroup("WEAP",
                falloutRecord("WEAP", 0x1001, weaponRecord("Rifle", 5))
                    + falloutRecord("WEAP", 0x1002, weaponRecord("Pistol", 3))
                    // an animation type that does not exist
                    + falloutRecord("WEAP", 0x1003, weaponRecord("Odd", 40))
                    + falloutRecord("WEAP", 0x1004, weaponRecord("Gone", 5))
                    + falloutRecord("WEAP", 0x1004, "", ESM4::Rec_Deleted))
            + falloutGroup("PROJ", falloutRecord("PROJ", 0x2001, zString("EDID", "Bullet") + subRecord("DATA", bytes)))
            + falloutGroup("ARMO", falloutRecord("ARMO", 0x3001, zString("EDID", "Suit") + subRecord("DNAM", armour)))
            + falloutGroup("CREA",
                falloutRecord(
                    "CREA", 0x4001, zString("EDID", "Beast") + subRecord("AIDT", aiData()) + subRecord("DATA", stats)))
            + falloutGroup("ALCH",
                falloutRecord("ALCH", 0x5001,
                    zString("EDID", "Stimpak") + subRecord("DATA", weight)
                        + valueSubRecord<std::uint32_t>("EFID", 0x00010090) + subRecord("EFIT", effect)));
    }

    TEST(ESM4CombatCensusTest, countsTheCombatDataAndTheValuesNoGameCouldMean)
    {
        ESM4::CombatCensus census;
        ESM4::Reader reader(std::make_unique<std::istringstream>(censusPlugin()), "base.esm", nullptr, nullptr);
        census.collect(reader);
        const ESM4::CombatCensus::Summary summary = census.summarize();

        // the deleted weapon is left out
        EXPECT_EQ(summary.mWeapons, 3u);
        EXPECT_EQ(summary.mWeaponsWithGameData, 3u);
        EXPECT_EQ(summary.mWeaponsWithVats, 3u);
        EXPECT_EQ(summary.mAnimationType[ESM4::Weapon::GameData::RifleBallistic], 1u);
        EXPECT_EQ(summary.mAnimationType[ESM4::Weapon::GameData::PistolBallistic], 1u);
        // the one with the type that does not exist is counted with the last
        EXPECT_EQ(summary.mAnimationType[ESM4::CombatCensus::animationTypes - 1], 1u);
        EXPECT_EQ(summary.mImplausible, 1u);
        EXPECT_EQ(summary.mAutomatic, 3u);
        EXPECT_THAT(summary.mSkill, UnorderedElementsAre(Pair(38u, 3u)));
        EXPECT_EQ(summary.mMaxDamage, 21);

        EXPECT_EQ(summary.mProjectiles, 1u);
        EXPECT_EQ(summary.mHitscan, 1u);
        EXPECT_EQ(summary.mWithExplosion, 1u);
        EXPECT_THAT(summary.mProjectileType, UnorderedElementsAre(Pair(1, 1u)));
        EXPECT_EQ(summary.mProjectileImplausible, 0u);

        EXPECT_EQ(summary.mArmour, 1u);
        EXPECT_EQ(summary.mArmourWithRating, 1u);
        EXPECT_EQ(summary.mArmourWithThreshold, 1u);
        EXPECT_FLOAT_EQ(summary.mMaxRating, 30.f);
        EXPECT_EQ(summary.mRatingBucket[3], 1u);

        EXPECT_EQ(summary.mCreatures, 1u);
        EXPECT_EQ(summary.mCreaturesWithAi, 1u);
        EXPECT_EQ(summary.mAggression[ESM4::FalloutAIData::VeryAggressive], 1u);
        EXPECT_EQ(summary.mConfidence[ESM4::FalloutAIData::Brave], 1u);
        EXPECT_EQ(summary.mCreaturesWithDamage, 1u);
        EXPECT_EQ(summary.mMaxHealth, 80);

        EXPECT_EQ(summary.mPotions, 1u);
        EXPECT_EQ(summary.mPotionsWithEffects, 1u);
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4CombatCensusTest, writesTheCounts)
    {
        ESM4::CombatCensus census;
        ESM4::Reader reader(std::make_unique<std::istringstream>(censusPlugin()), "base.esm", nullptr, nullptr);
        census.collect(reader);
        std::ostringstream out;
        census.write(out);

        EXPECT_THAT(out.str(), HasSubstr("Weapons (WEAP records): 3\n"));
        EXPECT_THAT(out.str(), HasSubstr("RifleBallistic"));
        EXPECT_THAT(out.str(), HasSubstr("Projectiles (PROJ records): 1\n"));
        EXPECT_THAT(out.str(), HasSubstr("very aggressive"));
        EXPECT_THAT(out.str(), Not(HasSubstr("Reading stopped early")));
    }

    TEST(ESM4CombatCensusTest, namesTheAnimationTypes)
    {
        EXPECT_EQ(ESM4::CombatCensus::animationTypeName(0), "HandToHand");
        EXPECT_EQ(ESM4::CombatCensus::animationTypeName(7), "RifleEnergy");
        EXPECT_EQ(ESM4::CombatCensus::animationTypeName(20), "type 20");
    }

    // A plugin of Fallout 3 with the groups the caller gives, whose version of the header is that of New Vegas
    std::string falloutPlugin(const std::string& groups)
    {
        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        return falloutRecord("TES4", 0, subRecord("HEDR", hedr)) + groups;
    }

    ESM4::CombatCensus censusOf(const std::string& plugin)
    {
        ESM4::CombatCensus census;
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "base.esm", nullptr, nullptr);
        census.collect(reader);
        return census;
    }

    std::string weaponWithValues(std::string_view editorId, std::uint32_t animationType, float fireRate,
        float reach = 64.f, float maxRange = 3000.f, std::uint16_t damage = 10, std::uint8_t clip = 6)
    {
        std::string game = weaponGameData(animationType);
        std::memcpy(game.data() + 8, &reach, 4);
        std::memcpy(game.data() + 48, &maxRange, 4);
        std::memcpy(game.data() + 64, &fireRate, 4);
        return zString("EDID", editorId) + subRecord("DATA", weaponData(damage, clip)) + subRecord("DNAM", game);
    }

    std::string projectileRecord(
        std::uint16_t type, float speed, float range, std::uint16_t flags = 0, std::uint32_t explosion = 0)
    {
        std::string bytes(68, '\0');
        std::memcpy(bytes.data(), &flags, 2);
        std::memcpy(bytes.data() + 2, &type, 2);
        std::memcpy(bytes.data() + 8, &speed, 4);
        std::memcpy(bytes.data() + 12, &range, 4);
        std::memcpy(bytes.data() + 36, &explosion, 4);
        return zString("EDID", "Shot") + subRecord("DATA", bytes);
    }

    std::string explosionRecord(float damage, float radius)
    {
        std::string bytes(52, '\0');
        std::memcpy(bytes.data() + 4, &damage, 4);
        std::memcpy(bytes.data() + 8, &radius, 4);
        return zString("EDID", "Blast") + subRecord("DATA", bytes);
    }

    std::string armorRecord(std::int16_t rating, float threshold)
    {
        std::string data;
        append<std::int16_t>(data, rating);
        append<std::uint16_t>(data, 0);
        append<float>(data, threshold);
        append<std::uint32_t>(data, 0);
        return zString("EDID", "Suit") + subRecord("DNAM", data);
    }

    std::string characterStats(std::int32_t health)
    {
        std::string data;
        append(data, health);
        data.append(7, '\x05');
        return data;
    }

    std::string potionRecord(std::size_t effects)
    {
        std::string weight;
        append<float>(weight, 0.5f);
        std::string effect;
        append<std::uint32_t>(effect, 30);
        append<std::uint32_t>(effect, 0);
        append<std::uint32_t>(effect, 0);
        append<std::uint32_t>(effect, 0);
        append<std::int32_t>(effect, 16);
        std::string result = zString("EDID", "Drink") + subRecord("DATA", weight);
        for (std::size_t i = 0; i < effects; ++i)
            result += valueSubRecord<std::uint32_t>("EFID", 0x00010090) + subRecord("EFIT", effect);
        return result;
    }

    TEST(ESM4CombatCensusMoreTest, tellsTheWeaponsThatFireWithNoFireRateAndTheValuesNoGameCouldMean)
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        std::string weapons;
        // one weapon of each animation type, with a fire rate of 0: only the types that fire ammunition are counted
        for (std::uint32_t type = 0; type < ESM4::CombatCensus::animationTypes; ++type)
            weapons += falloutRecord("WEAP", 0x1100 + type, weaponWithValues("Weapon", type, 0.f));
        weapons += falloutRecord("WEAP", 0x1201, weaponWithValues("NoRate", 5, nan));
        weapons += falloutRecord("WEAP", 0x1202, weaponWithValues("LongReach", 0, 1.f, 5000.f));
        weapons += falloutRecord("WEAP", 0x1203, weaponWithValues("NegativeRange", 0, 1.f, 64.f, -5.f));
        weapons += falloutRecord("WEAP", 0x1204, weaponWithValues("NoRange", 0, 1.f, 64.f, nan));
        weapons += falloutRecord("WEAP", 0x1205, weaponWithValues("NoSuchType", 99, 1.f));
        // a weapon with no game data is counted, and counts for nothing more
        weapons += falloutRecord("WEAP", 0x1206, zString("EDID", "Club") + subRecord("DATA", weaponData(3, 0)));
        const ESM4::CombatCensus census = censusOf(falloutPlugin(falloutGroup("WEAP", weapons)));
        const ESM4::CombatCensus::Summary summary = census.summarize();

        EXPECT_EQ(summary.mWeapons, 19u);
        EXPECT_EQ(summary.mWeaponsWithGameData, 18u);
        // the pistols, the rifles, the two handed weapons and the launcher
        EXPECT_EQ(summary.mNoFireRate, 7u);
        EXPECT_EQ(summary.mImplausible, 5u);
        EXPECT_EQ(summary.mMinDamage, 3);
        EXPECT_EQ(summary.mMaxDamage, 10);
        EXPECT_EQ(summary.mMaxClip, 6);
        // a range that is not a number counts for nothing
        EXPECT_FLOAT_EQ(summary.mMaxRange, 3000.f);
        std::size_t typed = 0;
        for (const std::size_t number : summary.mAnimationType)
            typed += number;
        EXPECT_EQ(typed, 18u);
        EXPECT_EQ(summary.mAnimationType[ESM4::CombatCensus::animationTypes - 1], 2u);
    }

    TEST(ESM4CombatCensusMoreTest, countsProjectilesOfEveryTypeAndTheOnesWithValuesNoGameCouldMean)
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        std::string projectiles;
        std::uint32_t id = 0x2100;
        for (const std::uint16_t type : { 1, 2, 4, 8, 16 })
            projectiles += falloutRecord("PROJ", id++, projectileRecord(type, 100.f, 500.f));
        projectiles += falloutRecord("PROJ", id++, projectileRecord(1, 100.f, 500.f, 1, 0x00010001));
        // a type that is not one, a speed that is not a number, a range below 0 and a speed and a range too great
        projectiles += falloutRecord("PROJ", id++, projectileRecord(3, 100.f, 500.f));
        projectiles += falloutRecord("PROJ", id++, projectileRecord(1, nan, 500.f));
        projectiles += falloutRecord("PROJ", id++, projectileRecord(1, 100.f, -1.f));
        projectiles += falloutRecord("PROJ", id++, projectileRecord(1, 2000000.f, 500.f));
        projectiles += falloutRecord("PROJ", id++, projectileRecord(1, 100.f, 20000000.f));
        const ESM4::CombatCensus::Summary summary
            = censusOf(falloutPlugin(falloutGroup("PROJ", projectiles))).summarize();

        EXPECT_EQ(summary.mProjectiles, 11u);
        EXPECT_THAT(summary.mProjectileType,
            UnorderedElementsAre(Pair(1, 6u), Pair(2, 1u), Pair(3, 1u), Pair(4, 1u), Pair(8, 1u), Pair(16, 1u)));
        EXPECT_EQ(summary.mHitscan, 1u);
        EXPECT_EQ(summary.mWithExplosion, 1u);
        EXPECT_EQ(summary.mProjectileImplausible, 5u);
        EXPECT_FLOAT_EQ(summary.mMaxSpeed, 2000000.f);
    }

    TEST(ESM4CombatCensusMoreTest, countsExplosionsAndAmmunitionAndLeavesOtherRecordsAlone)
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const std::string explosions = falloutRecord("EXPL", 0x7001, explosionRecord(40.f, 384.f))
            + falloutRecord("EXPL", 0x7002, explosionRecord(0.f, 10.f))
            + falloutRecord("EXPL", 0x7003, explosionRecord(5.f, nan))
            + falloutRecord("EXPL", 0x7004, explosionRecord(-1.f, 5.f))
            + falloutRecord("EXPL", 0x7005, explosionRecord(200000.f, 5.f));
        const std::string ammunition = falloutRecord("AMMO", 0x6001, zString("EDID", "Round"))
            + falloutRecord("AMMO", 0x6002, zString("EDID", "Shell"));
        const std::string other = falloutRecord("MISC", 0x9001, zString("EDID", "Junk"));
        const ESM4::CombatCensus::Summary summary
            = censusOf(falloutPlugin(falloutGroup("MISC", other) + falloutGroup("EXPL", explosions)
                           + falloutGroup("AMMO", ammunition)))
                  .summarize();

        EXPECT_EQ(summary.mAmmunition, 2u);
        EXPECT_EQ(summary.mExplosions, 5u);
        EXPECT_EQ(summary.mExplosionsWithDamage, 3u);
        EXPECT_EQ(summary.mExplosionImplausible, 3u);
        EXPECT_FLOAT_EQ(summary.mMaxExplosionRadius, 384.f);
        EXPECT_EQ(summary.mWeapons, 0u);
    }

    TEST(ESM4CombatCensusMoreTest, sortsArmourByRatingAndCountsTheValuesNoGameCouldMean)
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        std::string armour;
        std::uint32_t id = 0x3100;
        armour += falloutRecord("ARMO", id++, armorRecord(0, 0.f)); // none
        armour += falloutRecord("ARMO", id++, armorRecord(500, 2.f)); // up to 10
        armour += falloutRecord("ARMO", id++, armorRecord(2000, 30.f)); // up to 25
        armour += falloutRecord("ARMO", id++, armorRecord(4000, 0.f)); // up to 50
        armour += falloutRecord("ARMO", id++, armorRecord(8000, 0.f)); // up to 100
        armour += falloutRecord("ARMO", id++, armorRecord(15000, 0.f)); // more
        armour += falloutRecord("ARMO", id++, armorRecord(1000, nan));
        armour += falloutRecord("ARMO", id++, armorRecord(1000, -1.f));
        // an armour with no rating at all
        armour += falloutRecord("ARMO", id++, zString("EDID", "Hat"));
        const ESM4::CombatCensus::Summary summary = censusOf(falloutPlugin(falloutGroup("ARMO", armour))).summarize();

        EXPECT_EQ(summary.mArmour, 9u);
        EXPECT_EQ(summary.mArmourWithRating, 8u);
        EXPECT_THAT(summary.mRatingBucket, ElementsAre(1u, 3u, 1u, 1u, 1u, 1u));
        EXPECT_EQ(summary.mArmourWithThreshold, 2u);
        EXPECT_EQ(summary.mArmourImplausible, 2u);
        EXPECT_FLOAT_EQ(summary.mMaxRating, 150.f);
        EXPECT_FLOAT_EQ(summary.mMaxThreshold, 30.f);
    }

    TEST(ESM4CombatCensusMoreTest, countsCharactersCreaturesAndPotions)
    {
        const std::string characters
            = falloutRecord("NPC_", 0x8001,
                  zString("EDID", "Guard") + subRecord("AIDT", aiData()) + subRecord("DATA", characterStats(120)))
            // a behaviour that no game has, and no aggro radius
            + falloutRecord("NPC_", 0x8002, zString("EDID", "Odd") + subRecord("AIDT", aiData(9, 3, 2, 0)))
            + falloutRecord("NPC_", 0x8003, zString("EDID", "Plain") + subRecord("DATA", characterStats(40)));
        std::string stats(17, '\0');
        stats[4] = 80;
        stats[8] = 14;
        const std::string creatures = falloutRecord("CREA", 0x4001,
            zString("EDID", "Beast") + subRecord("AIDT", aiData(0, 0, 0, 1)) + subRecord("DATA", stats));
        const std::string potions = falloutRecord("ALCH", 0x5001, potionRecord(0))
            + falloutRecord("ALCH", 0x5002, potionRecord(6)) + falloutRecord("ALCH", 0x5003, potionRecord(8));
        const ESM4::CombatCensus::Summary summary
            = censusOf(falloutPlugin(falloutGroup("NPC_", characters) + falloutGroup("CREA", creatures)
                           + falloutGroup("ALCH", potions)))
                  .summarize();

        EXPECT_EQ(summary.mCharacters, 3u);
        EXPECT_EQ(summary.mCharactersWithAi, 2u);
        EXPECT_EQ(summary.mCreatures, 1u);
        EXPECT_EQ(summary.mCreaturesWithAi, 1u);
        EXPECT_EQ(summary.mAiImplausible, 1u);
        EXPECT_EQ(summary.mAggroRadius, 2u);
        EXPECT_EQ(summary.mAggression[ESM4::FalloutAIData::VeryAggressive], 1u);
        EXPECT_EQ(summary.mAggression[0], 1u);
        EXPECT_EQ(summary.mMinHealth, 40);
        EXPECT_EQ(summary.mMaxHealth, 120);
        EXPECT_EQ(summary.mCreaturesWithDamage, 1u);
        EXPECT_EQ(summary.mMaxCreatureDamage, 14);

        EXPECT_EQ(summary.mPotions, 3u);
        EXPECT_EQ(summary.mPotionsWithEffects, 2u);
        // six effects and more are counted together
        EXPECT_THAT(summary.mEffectCount, UnorderedElementsAre(Pair(0u, 1u), Pair(6u, 2u)));
    }

    TEST(ESM4CombatCensusMoreTest, countsARecordThatALoaderRejectsAndKeepsNothingOfIt)
    {
        const std::string fine = weaponRecord("Fine");
        const std::string broken = weaponRecord("Broken") + subRecord("ZZZZ", "x");
        const std::string plugin = falloutPlugin(
            falloutGroup("WEAP",
                falloutRecord("WEAP", 0x1301, fine)
                    + falloutRecord("WEAP", 0x1302, fine)
                    // a later definition that cannot be read replaces the earlier one with nothing
                    + falloutRecord("WEAP", 0x1302, broken))
            + falloutGroup("AMMO", falloutRecord("AMMO", 0x6301, zString("EDID", "Round") + subRecord("ZZZZ", "x"))));
        const ESM4::CombatCensus census = censusOf(plugin);

        EXPECT_EQ(census.summarize().mWeapons, 1u);
        EXPECT_EQ(census.summarize().mAmmunition, 0u);
        EXPECT_THAT(census.getFatalErrors(), IsEmpty());
        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("Records that a loader rejected\n  AMMO: 1\n  WEAP: 1\n"));
        EXPECT_THAT(out.str(), Not(HasSubstr("Reading stopped early")));
    }

    std::string groupHeader(std::uint32_t size, std::string_view label, std::int32_t type)
    {
        std::string result("GRUP");
        append(result, size);
        result.append(label);
        append(result, type);
        append<std::uint16_t>(result, 0);
        append<std::uint16_t>(result, 0);
        return result;
    }

    TEST(ESM4CombatCensusMoreTest, writesTheFatalErrorOfAPluginTheReaderCannotWalk)
    {
        // a group that says it holds more than the group around it has room for
        const std::string cell = record("CELL", 0x801, "");
        const std::string inner = groupHeader(
            static_cast<std::uint32_t>(20 + 20 + cell.size()), std::string_view("\0\0\0\0", 4), ESM4::Grp_CellChild);
        const std::string plugin = header() + groupHeader(static_cast<std::uint32_t>(20 + inner.size()), "WEAP", 0)
            + inner + cell + cell + cell;
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "base.esm", nullptr, nullptr);
        reader.setModIndex(0);
        ESM4::CombatCensus census;
        census.collect(reader);

        ASSERT_THAT(census.getFatalErrors(), SizeIs(1));
        EXPECT_EQ(census.getFatalErrors().front(), "Read more records than available");
        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("Reading stopped early\n  Read more records than available\n"));
    }

    TEST(ESM4CombatCensusMoreTest, writesNothingOfWeaponValuesWhenThereAreNoWeapons)
    {
        std::ostringstream out;
        ESM4::CombatCensus().write(out);
        EXPECT_THAT(out.str(), StartsWith("Weapons (WEAP records): 0\n"));
        EXPECT_THAT(out.str(), Not(HasSubstr("lowest damage")));
        EXPECT_THAT(out.str(), Not(HasSubstr("Records that a loader rejected")));
    }
}
