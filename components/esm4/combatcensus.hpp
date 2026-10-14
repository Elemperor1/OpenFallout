#ifndef OPENFALLOUT_COMPONENTS_ESM4_COMBATCENSUS_H
#define OPENFALLOUT_COMPONENTS_ESM4_COMBATCENSUS_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>

namespace ESM4
{
    class Reader;

    // Counts the combat data that the records of one or more Fallout 3 and New Vegas plugins hold, read with the
    // loaders of the game: the weapons (how they are held and fired), the ammunition, projectiles and explosions, the
    // armour (rating and damage threshold), the way characters and creatures behave in a fight, and the potions with
    // their effects. It checks the loaders on real files: a value that no game could mean (an animation type that does
    // not exist, a projectile of a type that is not one, a rating that is not a number) is counted as implausible, and
    // a record that a loader rejects is counted as failed. It keeps record types, numbers and counts, never record
    // contents.
    //
    // The files must be read in load order, each with its mod index and the indices of its masters set on the reader
    // (Reader::setModIndex and Reader::updateModIndices). A record that a later file defines again counts once, as the
    // later file has it, and one that a later file deletes does not count.
    class CombatCensus
    {
    public:
        // Weapons with this animation type or more are counted together.
        static constexpr std::size_t animationTypes = 13;

        struct Summary
        {
            std::size_t mWeapons = 0;
            std::size_t mWeaponsWithGameData = 0; // DNAM
            std::size_t mWeaponsWithCritical = 0; // CRDT
            std::size_t mWeaponsWithVats = 0; // VATS
            std::size_t mWeaponsWithAmmo = 0; // NAM0
            std::size_t mWeaponsWithProjectile = 0;
            std::size_t mWeaponsWithImpactDataSet = 0;
            std::size_t mWeaponsWithFirstPersonObject = 0;
            std::size_t mAutomatic = 0;
            std::size_t mNonPlayable = 0;
            std::size_t mEmbedded = 0;
            std::size_t mNoFireRate = 0; // a weapon of an animation type that fires, with a fire rate of 0
            std::array<std::size_t, animationTypes> mAnimationType{};
            std::map<std::uint32_t, std::size_t> mSkill; // the actor value of the skill
            std::size_t mImplausible = 0;
            std::uint16_t mMinDamage = 0xffff;
            std::uint16_t mMaxDamage = 0;
            std::uint8_t mMaxClip = 0;
            float mMaxRange = 0.f;

            std::size_t mAmmunition = 0;

            std::size_t mProjectiles = 0;
            std::map<std::uint16_t, std::size_t> mProjectileType;
            std::size_t mHitscan = 0;
            std::size_t mWithExplosion = 0;
            std::size_t mProjectileImplausible = 0;
            float mMaxSpeed = 0.f;

            std::size_t mExplosions = 0;
            std::size_t mExplosionsWithDamage = 0;
            std::size_t mExplosionImplausible = 0;
            float mMaxExplosionRadius = 0.f;

            std::size_t mArmour = 0;
            std::size_t mArmourWithRating = 0;
            std::size_t mArmourWithThreshold = 0;
            std::size_t mArmourImplausible = 0;
            std::array<std::size_t, 6> mRatingBucket{}; // 0, up to 10, 25, 50, 100, more
            float mMaxRating = 0.f;
            float mMaxThreshold = 0.f;

            std::size_t mCharacters = 0;
            std::size_t mCreatures = 0;
            std::size_t mCharactersWithAi = 0;
            std::size_t mCreaturesWithAi = 0;
            std::array<std::size_t, 4> mAggression{}; // characters and creatures together
            std::array<std::size_t, 5> mConfidence{};
            std::array<std::size_t, 3> mAssistance{};
            std::size_t mAiImplausible = 0;
            std::size_t mAggroRadius = 0; // with a radius that counts
            std::int32_t mMinHealth = 0;
            std::int32_t mMaxHealth = 0;
            std::size_t mCreaturesWithDamage = 0;
            std::uint16_t mMaxCreatureDamage = 0;

            std::size_t mPotions = 0;
            std::size_t mPotionsWithEffects = 0;
            std::map<std::size_t, std::size_t> mEffectCount;
        };

        // Reads every remaining record of the file. An error that leaves the reader in an unknown state stops the file
        // and is kept in getFatalErrors().
        void collect(Reader& reader);

        Summary summarize() const;

        const std::vector<std::string>& getFatalErrors() const { return mFatalErrors; }

        void write(std::ostream& stream) const;

        // The name of an animation type of WEAP
        static std::string animationTypeName(std::size_t type);

    private:
        struct WeaponFacts
        {
            bool mGameData = false;
            bool mCritical = false;
            bool mVats = false;
            bool mAmmo = false;
            bool mProjectile = false;
            bool mImpactDataSet = false;
            bool mFirstPersonObject = false;
            std::uint32_t mAnimationType = 0;
            std::uint32_t mSkill = 0;
            std::uint8_t mFlags1 = 0;
            float mFireRate = 0.f;
            float mMaxRange = 0.f;
            float mReach = 0.f;
            std::uint16_t mDamage = 0;
            std::uint8_t mClip = 0;
        };

        struct ProjectileFacts
        {
            std::uint16_t mFlags = 0;
            std::uint16_t mType = 0;
            float mSpeed = 0.f;
            float mRange = 0.f;
            bool mExplosion = false;
        };

        struct ExplosionFacts
        {
            float mDamage = 0.f;
            float mRadius = 0.f;
        };

        struct ArmourFacts
        {
            bool mHasRating = false;
            float mRating = 0.f;
            float mThreshold = 0.f;
        };

        struct ActorFacts
        {
            bool mCreature = false;
            bool mHasAi = false;
            std::uint8_t mAggression = 0;
            std::uint8_t mConfidence = 0;
            std::uint8_t mAssistance = 0;
            std::uint8_t mAggroFlags = 0;
            bool mHasStats = false;
            std::int32_t mHealth = 0;
            std::uint16_t mDamage = 0;
        };

        std::unordered_map<ESM::FormId, WeaponFacts> mWeapons;
        std::unordered_map<ESM::FormId, bool> mAmmunition;
        std::unordered_map<ESM::FormId, ProjectileFacts> mProjectiles;
        std::unordered_map<ESM::FormId, ExplosionFacts> mExplosions;
        std::unordered_map<ESM::FormId, ArmourFacts> mArmour;
        std::unordered_map<ESM::FormId, ActorFacts> mActors;
        std::unordered_map<ESM::FormId, std::size_t> mPotions; // the number of effects
        std::map<std::uint32_t, std::size_t> mFailed; // record type to number
        std::vector<std::string> mFatalErrors;
    };
}

#endif
