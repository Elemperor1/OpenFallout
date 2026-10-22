/*
  Copyright (C) 2016, 2018-2020 cc9cii

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  cc9cii cc9c@iinet.net.au

  Much of the information on the data structures are based on the information
  from Tes4Mod:Mod_File_Format and Tes5Mod:File_Formats but also refined by
  trial & error.  See http://en.uesp.net/wiki for details.

*/
#ifndef ESM4_WEAP_H
#define ESM4_WEAP_H

#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>
#include <components/esm/formid.hpp>
#include <components/esm/path.hpp>

namespace ESM4
{
    class Reader;
    class Writer;

    struct Weapon
    {
        struct Data
        {
            // type
            // 0 = Blade One Hand
            // 1 = Blade Two Hand
            // 2 = Blunt One Hand
            // 3 = Blunt Two Hand
            // 4 = Staff
            // 5 = Bow
            std::uint32_t type;
            float speed;
            float reach;
            std::uint32_t flags;
            std::uint32_t value; // gold
            std::uint32_t health;
            float weight;
            std::uint16_t damage;
            std::uint8_t clipSize; // FO3/FONV only

            Data()
                : type(0)
                , speed(0.f)
                , reach(0.f)
                , flags(0)
                , value(0)
                , health(0)
                , weight(0.f)
                , damage(0)
                , clipSize(0)
            {
            }
        };

        /// DNAM of Fallout 3 and New Vegas: how the weapon is held, aimed and fired. The first 136 bytes are the same
        /// in both games; the rest (New Vegas only) is not decoded.
        struct GameData
        {
            /// Animation type: the hands, a melee weapon held in one hand or two, a pistol (ballistic or energy),
            /// a rifle (ballistic, automatic or energy), a weapon with a handle held in two hands (a minigun or
            /// flamer), a launcher, a grenade, a mine
            enum AnimationType : std::uint32_t
            {
                HandToHand = 0,
                Melee1Hand = 1,
                Melee2Hand = 2,
                PistolBallistic = 3,
                PistolEnergy = 4,
                RifleBallistic = 5,
                RifleAutomatic = 6,
                RifleEnergy = 7,
                Handle2Hand = 8,
                Launcher2Hand = 9,
                GrenadeThrown = 10,
                LandMine = 11,
                MineThrown = 12,
            };

            enum Flags1 : std::uint8_t
            {
                IgnoresNormalWeaponResistance = 0x01,
                Automatic = 0x02,
                HasScope = 0x04,
                CannotDrop = 0x08,
                HideBackpack = 0x10,
                EmbeddedWeapon = 0x20,
                NoFirstPersonImageSpace = 0x40,
                NonPlayable = 0x80,
            };

            enum Flags2 : std::uint32_t
            {
                PlayerOnly = 0x00000001,
                NpcsUseAmmo = 0x00000002,
                NoJamAfterReload = 0x00000004,
                OverrideActionPoints = 0x00000008,
                MinorCrime = 0x00000010,
                RangeFixed = 0x00000020,
                NotUsedInNormalCombat = 0x00000040,
                OverrideDamageToWeaponMult = 0x00000080,
                NoThirdPersonImageSpace = 0x00000100,
                BurstShot = 0x00000200,
                RumbleAlternate = 0x00000400,
                LongBurst = 0x00000800,
            };

            std::uint32_t mAnimationType = HandToHand;
            float mAnimationMultiplier = 1.f;
            float mReach = 0.f;
            std::uint8_t mFlags1 = 0;
            std::uint8_t mGripAnimation = 0;
            std::uint8_t mAmmoUse = 0; // how many rounds one attack uses
            std::uint8_t mReloadAnimation = 0;
            float mMinSpread = 0.f;
            float mSpread = 0.f;
            float mSightFov = 0.f;
            ESM::FormId mProjectile;
            std::uint8_t mBaseVatsChance = 0;
            std::uint8_t mAttackAnimation = 0;
            std::uint8_t mProjectileCount = 1;
            std::uint8_t mEmbeddedWeaponValue = 0;
            float mMinRange = 0.f;
            float mMaxRange = 0.f;
            std::uint32_t mOnHit = 0;
            std::uint32_t mFlags2 = 0;
            float mAttackMultiplier = 1.f;
            float mFireRate = 0.f;
            float mOverrideActionPoints = 0.f;
            float mOverrideDamageToWeaponMult = 1.f;
            float mAttacksPerSecond = 0.f;
            float mReloadTime = 0.f;
            float mJamTime = 0.f;
            float mAimArc = 0.f;
            std::uint32_t mSkill = 0; // the actor value of the skill that the weapon is used with
            float mLimbDamageMultiplier = 1.f;
            std::uint32_t mResist = 0; // the actor value of the resistance to this weapon (0xffffffff: none)
            float mSightUsage = 0.f;
            float mSemiAutomaticDelayMin = 0.f;
            float mSemiAutomaticDelayMax = 0.f;
        };

        /// CRDT: what a critical hit does
        struct Critical
        {
            std::uint16_t mDamage = 0;
            float mChanceMultiplier = 0.f;
            std::uint8_t mFlags = 0; // 1: on death
            ESM::FormId mEffect;
        };

        /// VATS of New Vegas: the special attack and what the weapon is for in VATS
        struct Vats
        {
            ESM::FormId mEffect;
            float mSkill = 0.f; // the skill needed
            float mDamageMultiplier = 0.f;
            float mActionPoints = 0.f;
            std::uint8_t mSilent = 0;
            std::uint8_t mModRequired = 0;
        };

        ESM::FormId mId; // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId;
        std::string mFullName;
        ESM::Path mModel;
        std::string mText;
        std::string mIcon;
        std::string mMiniIcon;

        ESM::FormId mPickUpSound;
        ESM::FormId mDropSound;
        // Of Fallout 3 and New Vegas: the sound records (SOUN) of a shot (the first SNAM), of a shot heard by the
        // player (XNAM) and of the click of an empty weapon (TNAM)
        ESM::FormId mShotSound;
        ESM::FormId mShotSound2D;
        ESM::FormId mDryFireSound;

        float mBoundRadius;

        ESM::FormId mScriptId;
        std::uint16_t mEnchantmentPoints;
        ESM::FormId mEnchantment;

        Data mData;

        /// The game data from the bytes of a DNAM (at least 120); a value that the bytes do not reach is its default
        static GameData decodeGameData(const std::vector<std::uint8_t>& bytes);

        /// Of Fallout 3 and New Vegas (DNAM, CRDT, VATS), absent from a record that has none
        bool mHasGameData = false;
        GameData mGame;
        bool mHasCritical = false;
        Critical mCritical;
        bool mHasVats = false;
        Vats mVats;
        ESM::FormId mAmmo; // NAM0, an ammunition record or a list of them
        ESM::FormId mImpactDataSet; // INAM
        std::uint32_t mEquipType = 0xffffffffu; // ETYP
        std::uint32_t mSoundLevel = 0; // VNAM
        // WNAM, the static object whose model the player sees in hand (Fallout 3 and New Vegas)
        ESM::FormId mFirstPersonObject;

        void load(ESM4::Reader& reader);
        // void save(ESM4::Writer& writer) const;

        // void blank();
        static constexpr ESM::RecNameInts sRecordId = ESM::RecNameInts::REC_WEAP4;
    };
}

#endif // ESM4_WEAP_H
