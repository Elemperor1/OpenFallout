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

    std::string aiData()
    {
        std::string data;
        append<std::uint8_t>(data, 2); // very aggressive
        append<std::uint8_t>(data, 3); // brave
        append<std::uint8_t>(data, 40); // energy
        append<std::uint8_t>(data, 60); // responsibility
        append<std::uint8_t>(data, 5); // mood
        data.append(3, '\0');
        append<std::uint32_t>(data, 0x1234); // services
        append<std::int8_t>(data, 4); // trained skill
        append<std::uint8_t>(data, 50); // max training level
        append<std::uint8_t>(data, 2); // helps friends and allies
        append<std::uint8_t>(data, 1); // the aggro radius counts
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

        // A Fallout file: its records and groups have the headers of Fallout, 24 bytes
        const auto fo
            = [](std::string_view recordType, std::uint32_t id, std::string_view data, std::uint32_t recordFlags = 0) {
                  std::string result(recordType);
                  append<std::uint32_t>(result, static_cast<std::uint32_t>(data.size()));
                  append<std::uint32_t>(result, recordFlags);
                  append<std::uint32_t>(result, id);
                  append<std::uint32_t>(result, 0); // revision
                  append<std::uint16_t>(result, 15); // form version
                  append<std::uint16_t>(result, 0);
                  result.append(data);
                  return result;
              };
        const auto group = [](std::string_view label, std::string_view children) {
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
        };
        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);

        return fo("TES4", 0, subRecord("HEDR", hedr))
            + group("WEAP",
                fo("WEAP", 0x1001, weaponRecord("Rifle", 5))
                    + fo("WEAP", 0x1002, weaponRecord("Pistol", 3))
                    // an animation type that does not exist
                    + fo("WEAP", 0x1003, weaponRecord("Odd", 40)) + fo("WEAP", 0x1004, weaponRecord("Gone", 5))
                    + fo("WEAP", 0x1004, "", ESM4::Rec_Deleted))
            + group("PROJ", fo("PROJ", 0x2001, zString("EDID", "Bullet") + subRecord("DATA", bytes)))
            + group("ARMO", fo("ARMO", 0x3001, zString("EDID", "Suit") + subRecord("DNAM", armour)))
            + group("CREA",
                fo("CREA", 0x4001, zString("EDID", "Beast") + subRecord("AIDT", aiData()) + subRecord("DATA", stats)))
            + group("ALCH",
                fo("ALCH", 0x5001,
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
}
