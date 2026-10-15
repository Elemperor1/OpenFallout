#include "combatcensus.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <ostream>

#include <components/esm/common.hpp>

#include "censusreading.hpp"
#include "common.hpp"
#include "loadalch.hpp"
#include "loadammo.hpp"
#include "loadarmo.hpp"
#include "loadcrea.hpp"
#include "loadexpl.hpp"
#include "loadnpc.hpp"
#include "loadproj.hpp"
#include "loadweap.hpp"
#include "reader.hpp"
#include "readerutils.hpp"

namespace ESM4
{
    using namespace CensusReading;

    namespace
    {
        bool sensible(float value, float limit)
        {
            return std::isfinite(value) && value >= 0.f && value <= limit;
        }

        bool firesAmmo(std::uint32_t animationType)
        {
            using Type = ESM4::Weapon::GameData;
            return animationType == Type::PistolBallistic || animationType == Type::PistolEnergy
                || animationType == Type::RifleBallistic || animationType == Type::RifleAutomatic
                || animationType == Type::RifleEnergy || animationType == Type::Handle2Hand
                || animationType == Type::Launcher2Hand;
        }

        std::size_t ratingBucket(float rating)
        {
            if (rating <= 0.f)
                return 0;
            if (rating <= 10.f)
                return 1;
            if (rating <= 25.f)
                return 2;
            if (rating <= 50.f)
                return 3;
            if (rating <= 100.f)
                return 4;
            return 5;
        }
    }

    std::string CombatCensus::animationTypeName(std::size_t type)
    {
        static const char* const names[animationTypes] = { "HandToHand", "Melee1Hand", "Melee2Hand", "PistolBallistic",
            "PistolEnergy", "RifleBallistic", "RifleAutomatic", "RifleEnergy", "Handle2Hand", "Launcher2Hand",
            "GrenadeThrown", "LandMine", "MineThrown" };
        if (type < animationTypes)
            return names[type];
        return "type " + std::to_string(type);
    }

    void CombatCensus::collect(Reader& reader)
    {
        const auto visitRecord = [&](Reader& r) {
            const std::uint32_t type = r.hdr().record.typeId;
            if (type != REC_WEAP && type != REC_AMMO && type != REC_ARMO && type != REC_PROJ && type != REC_EXPL
                && type != REC_NPC_ && type != REC_CREA && type != REC_ALCH)
                return false;

            const ESM::FormId id = r.getFormIdFromHeader();
            const ReaderContext recordStart = r.getContext();
            mWeapons.erase(id);
            mAmmunition.erase(id);
            mProjectiles.erase(id);
            mExplosions.erase(id);
            mArmour.erase(id);
            mActors.erase(id);
            mPotions.erase(id);

            if ((r.hdr().record.flags & Rec_Deleted) == 0)
            {
                try
                {
                    r.getRecordData();
                    switch (type)
                    {
                        case REC_WEAP:
                        {
                            ESM4::Weapon record;
                            record.load(r);
                            WeaponFacts weapon;
                            weapon.mGameData = record.mHasGameData;
                            weapon.mCritical = record.mHasCritical;
                            weapon.mVats = record.mHasVats;
                            weapon.mAmmo = !record.mAmmo.isZeroOrUnset();
                            weapon.mProjectile = !record.mGame.mProjectile.isZeroOrUnset();
                            weapon.mImpactDataSet = !record.mImpactDataSet.isZeroOrUnset();
                            weapon.mFirstPersonObject = !record.mFirstPersonObject.isZeroOrUnset();
                            weapon.mAnimationType = record.mGame.mAnimationType;
                            weapon.mSkill = record.mGame.mSkill;
                            weapon.mFlags1 = record.mGame.mFlags1;
                            weapon.mFireRate = record.mGame.mFireRate;
                            weapon.mMaxRange = record.mGame.mMaxRange;
                            weapon.mReach = record.mGame.mReach;
                            weapon.mDamage = record.mData.damage;
                            weapon.mClip = record.mData.clipSize;
                            mWeapons[id] = weapon;
                            break;
                        }
                        case REC_AMMO:
                        {
                            ESM4::Ammunition record;
                            record.load(r);
                            mAmmunition[id] = true;
                            break;
                        }
                        case REC_PROJ:
                        {
                            ESM4::Projectile record;
                            record.load(r);
                            const ESM4::Projectile::GameData data = record.gameData();
                            ProjectileFacts projectile;
                            projectile.mFlags = data.mFlags;
                            projectile.mType = data.mType;
                            projectile.mSpeed = data.mSpeed;
                            projectile.mRange = data.mRange;
                            projectile.mExplosion = !data.mExplosion.isZeroOrUnset();
                            mProjectiles[id] = projectile;
                            break;
                        }
                        case REC_EXPL:
                        {
                            ESM4::Explosion record;
                            record.load(r);
                            const ESM4::Explosion::GameData data = record.gameData();
                            mExplosions[id] = { data.mDamage, data.mRadius };
                            break;
                        }
                        case REC_ARMO:
                        {
                            ESM4::Armor record;
                            record.load(r);
                            mArmour[id] = { record.mHasRating, record.mRating, record.mDamageThreshold };
                            break;
                        }
                        case REC_NPC_:
                        {
                            ESM4::Npc record;
                            record.load(r);
                            ActorFacts actor;
                            actor.mHasAi = record.mFalloutAI.mKnown;
                            actor.mAggression = record.mFalloutAI.mAggression;
                            actor.mConfidence = record.mFalloutAI.mConfidence;
                            actor.mAssistance = record.mFalloutAI.mAssistance;
                            actor.mAggroFlags = record.mFalloutAI.mAggroRadiusFlags;
                            actor.mHasStats = record.mFalloutStats.mKnown;
                            actor.mHealth = record.mFalloutStats.mHealth;
                            mActors[id] = actor;
                            break;
                        }
                        case REC_CREA:
                        {
                            ESM4::Creature record;
                            record.load(r);
                            ActorFacts actor;
                            actor.mCreature = true;
                            actor.mHasAi = record.mFalloutAI.mKnown;
                            actor.mAggression = record.mFalloutAI.mAggression;
                            actor.mConfidence = record.mFalloutAI.mConfidence;
                            actor.mAssistance = record.mFalloutAI.mAssistance;
                            actor.mAggroFlags = record.mFalloutAI.mAggroRadiusFlags;
                            actor.mHasStats = record.mFalloutStats.mKnown;
                            actor.mHealth = record.mFalloutStats.mHealth;
                            actor.mDamage = record.mFalloutStats.mDamage;
                            mActors[id] = actor;
                            break;
                        }
                        default:
                        {
                            ESM4::Potion record;
                            record.load(r);
                            mPotions[id] = record.mEffects.size();
                            break;
                        }
                    }
                }
                catch (const std::exception&)
                {
                    // The record cannot be read, so it counts for nothing, not even as the one an earlier file had.
                    ++mFailed[type];
                    mWeapons.erase(id);
                    mAmmunition.erase(id);
                    mProjectiles.erase(id);
                    mExplosions.erase(id);
                    mArmour.erase(id);
                    mActors.erase(id);
                    mPotions.erase(id);
                }
            }
            r.skipFailedRecord(recordStart);
            return true;
        };

        try
        {
            ReaderUtils::readAll(reader, visitRecord, [](Reader&) {});
        }
        catch (const std::exception& e)
        {
            // Raised by the reader while it walks record and group headers, so it holds offsets and sizes only.
            mFatalErrors.push_back(firstLine(e.what()));
        }
    }

    CombatCensus::Summary CombatCensus::summarize() const
    {
        Summary summary;

        for (const auto& [id, weapon] : mWeapons)
        {
            ++summary.mWeapons;
            summary.mWeaponsWithGameData += weapon.mGameData ? 1 : 0;
            summary.mWeaponsWithCritical += weapon.mCritical ? 1 : 0;
            summary.mWeaponsWithVats += weapon.mVats ? 1 : 0;
            summary.mWeaponsWithAmmo += weapon.mAmmo ? 1 : 0;
            summary.mWeaponsWithProjectile += weapon.mProjectile ? 1 : 0;
            summary.mWeaponsWithImpactDataSet += weapon.mImpactDataSet ? 1 : 0;
            summary.mWeaponsWithFirstPersonObject += weapon.mFirstPersonObject ? 1 : 0;
            summary.mMinDamage = std::min(summary.mMinDamage, weapon.mDamage);
            summary.mMaxDamage = std::max(summary.mMaxDamage, weapon.mDamage);
            summary.mMaxClip = std::max(summary.mMaxClip, weapon.mClip);
            if (!weapon.mGameData)
                continue;
            summary.mAutomatic += (weapon.mFlags1 & ESM4::Weapon::GameData::Automatic) != 0 ? 1 : 0;
            summary.mNonPlayable += (weapon.mFlags1 & ESM4::Weapon::GameData::NonPlayable) != 0 ? 1 : 0;
            summary.mEmbedded += (weapon.mFlags1 & ESM4::Weapon::GameData::EmbeddedWeapon) != 0 ? 1 : 0;
            ++summary.mAnimationType[std::min<std::size_t>(weapon.mAnimationType, animationTypes - 1)];
            ++summary.mSkill[weapon.mSkill];
            summary.mMaxRange = std::max(summary.mMaxRange, std::isfinite(weapon.mMaxRange) ? weapon.mMaxRange : 0.f);
            if (firesAmmo(weapon.mAnimationType) && weapon.mFireRate == 0.f)
                ++summary.mNoFireRate;
            if (weapon.mAnimationType >= animationTypes || !sensible(weapon.mFireRate, 1000.f)
                || !sensible(weapon.mReach, 1000.f) || !sensible(weapon.mMaxRange, 1000000.f))
                ++summary.mImplausible;
        }
        summary.mAmmunition = mAmmunition.size();

        for (const auto& [id, projectile] : mProjectiles)
        {
            ++summary.mProjectiles;
            ++summary.mProjectileType[projectile.mType];
            summary.mHitscan += (projectile.mFlags & ESM4::Projectile::Hitscan) != 0 ? 1 : 0;
            summary.mWithExplosion += projectile.mExplosion ? 1 : 0;
            summary.mMaxSpeed = std::max(summary.mMaxSpeed, std::isfinite(projectile.mSpeed) ? projectile.mSpeed : 0.f);
            const bool knownType = projectile.mType == 1 || projectile.mType == 2 || projectile.mType == 4
                || projectile.mType == 8 || projectile.mType == 16;
            if (!knownType || !sensible(projectile.mSpeed, 1000000.f) || !sensible(projectile.mRange, 10000000.f))
                ++summary.mProjectileImplausible;
        }

        for (const auto& [id, explosion] : mExplosions)
        {
            ++summary.mExplosions;
            summary.mExplosionsWithDamage += explosion.mDamage > 0.f ? 1 : 0;
            summary.mMaxExplosionRadius
                = std::max(summary.mMaxExplosionRadius, std::isfinite(explosion.mRadius) ? explosion.mRadius : 0.f);
            if (!sensible(explosion.mDamage, 100000.f) || !sensible(explosion.mRadius, 100000.f))
                ++summary.mExplosionImplausible;
        }

        for (const auto& [id, armour] : mArmour)
        {
            ++summary.mArmour;
            if (!armour.mHasRating)
                continue;
            ++summary.mArmourWithRating;
            ++summary.mRatingBucket[ratingBucket(armour.mRating)];
            summary.mArmourWithThreshold += armour.mThreshold > 0.f ? 1 : 0;
            if (std::isfinite(armour.mRating))
                summary.mMaxRating = std::max(summary.mMaxRating, armour.mRating);
            if (std::isfinite(armour.mThreshold))
                summary.mMaxThreshold = std::max(summary.mMaxThreshold, armour.mThreshold);
            if (!sensible(armour.mRating, 1000.f) || !sensible(armour.mThreshold, 1000.f))
                ++summary.mArmourImplausible;
        }

        bool firstHealth = true;
        for (const auto& [id, actor] : mActors)
        {
            ++(actor.mCreature ? summary.mCreatures : summary.mCharacters);
            if (actor.mHasStats)
            {
                if (firstHealth)
                {
                    summary.mMinHealth = summary.mMaxHealth = actor.mHealth;
                    firstHealth = false;
                }
                summary.mMinHealth = std::min(summary.mMinHealth, actor.mHealth);
                summary.mMaxHealth = std::max(summary.mMaxHealth, actor.mHealth);
            }
            if (actor.mCreature && actor.mDamage > 0)
                ++summary.mCreaturesWithDamage;
            summary.mMaxCreatureDamage = std::max(summary.mMaxCreatureDamage, actor.mDamage);
            if (!actor.mHasAi)
                continue;
            ++(actor.mCreature ? summary.mCreaturesWithAi : summary.mCharactersWithAi);
            if (actor.mAggression < summary.mAggression.size() && actor.mConfidence < summary.mConfidence.size()
                && actor.mAssistance < summary.mAssistance.size())
            {
                ++summary.mAggression[actor.mAggression];
                ++summary.mConfidence[actor.mConfidence];
                ++summary.mAssistance[actor.mAssistance];
            }
            else
                ++summary.mAiImplausible;
            summary.mAggroRadius += (actor.mAggroFlags & 1) != 0 ? 1 : 0;
        }

        for (const auto& [id, effects] : mPotions)
        {
            ++summary.mPotions;
            summary.mPotionsWithEffects += effects > 0 ? 1 : 0;
            ++summary.mEffectCount[std::min<std::size_t>(effects, 6)];
        }
        return summary;
    }

    void CombatCensus::write(std::ostream& stream) const
    {
        const Summary s = summarize();
        constexpr int nameWidth = 18;
        constexpr int countWidth = 9;

        const auto line = [&](const char* label, auto value) {
            stream << "  " << std::left << std::setw(nameWidth + 16) << label << std::right << std::setw(countWidth)
                   << value << '\n';
        };

        stream << "Weapons (WEAP records): " << s.mWeapons << '\n';
        line("with game data (DNAM)", s.mWeaponsWithGameData);
        line("with critical data (CRDT)", s.mWeaponsWithCritical);
        line("with VATS data", s.mWeaponsWithVats);
        line("with ammunition (NAM0)", s.mWeaponsWithAmmo);
        line("with a projectile", s.mWeaponsWithProjectile);
        line("with an impact data set", s.mWeaponsWithImpactDataSet);
        line("with a first person object", s.mWeaponsWithFirstPersonObject);
        line("automatic", s.mAutomatic);
        line("not playable", s.mNonPlayable);
        line("embedded", s.mEmbedded);
        line("that fire, with no fire rate", s.mNoFireRate);
        line("implausible values", s.mImplausible);
        if (s.mWeapons > 0)
        {
            line("lowest damage", s.mMinDamage);
            line("highest damage", s.mMaxDamage);
            line("largest clip", static_cast<int>(s.mMaxClip));
            line("longest range", s.mMaxRange);
        }

        stream << "\nWeapons by animation type\n";
        for (std::size_t type = 0; type < animationTypes; ++type)
            stream << "  " << std::left << std::setw(nameWidth) << animationTypeName(type) << std::right
                   << std::setw(countWidth) << s.mAnimationType[type] << '\n';
        stream << "\nWeapons by skill (actor value)\n";
        for (const auto& [skill, number] : s.mSkill)
            stream << "  " << std::left << std::setw(nameWidth) << skill << std::right << std::setw(countWidth)
                   << number << '\n';

        stream << "\nAmmunition (AMMO records): " << s.mAmmunition << '\n';

        stream << "\nProjectiles (PROJ records): " << s.mProjectiles << '\n';
        for (const auto& [type, number] : s.mProjectileType)
            stream << "  type " << std::left << std::setw(nameWidth - 5) << type << std::right << std::setw(countWidth)
                   << number << '\n';
        line("hitscan", s.mHitscan);
        line("that explode", s.mWithExplosion);
        line("fastest", s.mMaxSpeed);
        line("implausible values", s.mProjectileImplausible);

        stream << "\nExplosions (EXPL records): " << s.mExplosions << '\n';
        line("that do damage", s.mExplosionsWithDamage);
        line("largest radius", s.mMaxExplosionRadius);
        line("implausible values", s.mExplosionImplausible);

        stream << "\nArmour (ARMO records): " << s.mArmour << '\n';
        line("with a rating (DNAM)", s.mArmourWithRating);
        line("with a damage threshold", s.mArmourWithThreshold);
        line("highest rating", s.mMaxRating);
        line("highest threshold", s.mMaxThreshold);
        line("implausible values", s.mArmourImplausible);
        static const char* const buckets[] = { "no rating", "up to 10", "up to 25", "up to 50", "up to 100", "more" };
        for (std::size_t i = 0; i < s.mRatingBucket.size(); ++i)
            stream << "  " << std::left << std::setw(nameWidth) << buckets[i] << std::right << std::setw(countWidth)
                   << s.mRatingBucket[i] << '\n';

        stream << "\nCharacters (NPC_): " << s.mCharacters << ", with AI data " << s.mCharactersWithAi << '\n';
        stream << "Creatures (CREA): " << s.mCreatures << ", with AI data " << s.mCreaturesWithAi << '\n';
        line("lowest base health", s.mMinHealth);
        line("highest base health", s.mMaxHealth);
        line("creatures with an attack", s.mCreaturesWithDamage);
        line("strongest attack", s.mMaxCreatureDamage);
        line("with an aggro radius", s.mAggroRadius);
        line("implausible values", s.mAiImplausible);
        static const char* const aggression[] = { "unaggressive", "aggressive", "very aggressive", "frenzied" };
        static const char* const confidence[] = { "cowardly", "cautious", "average", "brave", "foolhardy" };
        static const char* const assistance[] = { "helps nobody", "helps allies", "helps friends+allies" };
        stream << "\nBy aggression\n";
        for (std::size_t i = 0; i < s.mAggression.size(); ++i)
            stream << "  " << std::left << std::setw(nameWidth) << aggression[i] << std::right << std::setw(countWidth)
                   << s.mAggression[i] << '\n';
        stream << "\nBy confidence\n";
        for (std::size_t i = 0; i < s.mConfidence.size(); ++i)
            stream << "  " << std::left << std::setw(nameWidth) << confidence[i] << std::right << std::setw(countWidth)
                   << s.mConfidence[i] << '\n';
        stream << "\nBy assistance\n";
        for (std::size_t i = 0; i < s.mAssistance.size(); ++i)
            stream << "  " << std::left << std::setw(nameWidth) << assistance[i] << std::right << std::setw(countWidth)
                   << s.mAssistance[i] << '\n';

        stream << "\nPotions (ALCH records): " << s.mPotions << ", with effects " << s.mPotionsWithEffects << '\n';
        for (const auto& [count, number] : s.mEffectCount)
            stream << "  " << count << (count == 6 ? " or more" : "") << " effects: " << number << '\n';

        if (!mFailed.empty())
        {
            stream << "\nRecords that a loader rejected\n";
            for (const auto& [type, number] : mFailed)
                stream << "  " << ESM::printName(type) << ": " << number << '\n';
        }
        if (!mFatalErrors.empty())
        {
            stream << "\nReading stopped early\n";
            for (const std::string& error : mFatalErrors)
                stream << "  " << error << '\n';
        }
    }
}
