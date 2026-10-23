#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadmgef.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadperk.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        namespace AV = FalloutScript::ActorValue;

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        /// The float an entry point of a perk works with
        double floatOf(const std::vector<std::uint8_t>& bytes)
        {
            if (bytes.size() < sizeof(float))
                return 0;
            float value = 0;
            std::memcpy(&value, bytes.data(), sizeof(float));
            return std::isfinite(value) ? value : 0.0;
        }

        /// The flags of a magic effect (the bits are those of the editor, which no executable has been read for)
        constexpr std::uint32_t effectHostile = 0x1;
        constexpr std::uint32_t effectRecovers = 0x2;
        constexpr std::uint32_t effectDetrimental = 0x4;

        /// The effect that the executables make for the engine's own healing
        constexpr std::uint32_t engineRestoreHealth = 0x14E;
    }

    std::optional<double> FalloutScripts::gameSetting(std::string_view name)
    {
        const std::lock_guard lock(mDerivedSettingsMutex);
        const std::map<std::string, double>& settings = gameSettingValues();
        const auto found = settings.find(Misc::StringUtils::lowerCase(name));
        return found != settings.end() ? std::optional<double>(found->second) : std::nullopt;
    }

    void FalloutScripts::ensureLevellingRules()
    {
        std::call_once(mLevellingRulesOnce, [this] {
            FalloutScript::LevellingRules rules = FalloutScript::LevellingRules::fromGameSettings(
                mGame, [this](std::string_view name) { return gameSetting(name); });
            rules.mMaxLevel = FalloutScript::Levelling::maxLevelForFiles(rules, mContentFiles);
            mProgress.setRules(rules);
            mItemUse.setBookRules(gameSetting("fBookPerkBonus").value_or(1.0));
            mItemUse.setChemDurationNeedsAddiction(mGame == FalloutScript::Game::Fallout3);
        });
    }

    bool FalloutScripts::perk(FalloutScript::FormId id, FalloutScript::PerkInfo& info)
    {
        const ESM4::Perk* record = mStore.get<ESM4::Perk>().search(refId(id));
        if (record == nullptr)
            return false;
        info = FalloutScript::PerkInfo();
        info.mId = id;
        info.mName = record->mFullName;
        info.mDescription = record->mDescription;
        info.mMinimumLevel = record->mData.mMinimumLevel;
        info.mRanks = std::max<int>(record->mData.mRanks, 1);
        info.mPlayable = record->mData.mPlayable != 0;
        info.mTrait = record->mData.mTrait != 0;
        info.mHidden = record->mData.mHidden != 0;
        info.mRequirements = record->mConditions;
        for (const ESM4::Perk::Entry& entry : record->mEntries)
        {
            FalloutScript::PerkEntry made;
            made.mType = entry.mType;
            made.mRank = entry.mRank;
            made.mPriority = entry.mPriority;
            made.mQuest = numberOf(entry.mQuest);
            made.mStage = entry.mQuestStage;
            made.mAbility = numberOf(entry.mAbility);
            made.mEntryPoint = entry.mEntryPoint;
            made.mFunction = entry.mFunction;
            // A float is what the functions that are used (set, add and multiply) have
            if (entry.mFunctionType == 1)
                made.mValue = floatOf(entry.mFunctionData);
            for (const ESM4::Perk::ConditionGroup& group : entry.mConditionGroups)
            {
                FalloutScript::PerkConditionGroup made_group;
                made_group.mHasRunOn = group.mHasRunOn;
                made_group.mRunOn = group.mRunOn;
                made_group.mConditions = group.mConditions;
                made.mGroups.push_back(std::move(made_group));
            }
            info.mEntries.push_back(std::move(made));
        }
        return true;
    }

    std::vector<FalloutScript::FormId> FalloutScripts::perks()
    {
        std::vector<FalloutScript::FormId> result;
        for (const ESM4::Perk& perk : mStore.get<ESM4::Perk>())
            if (const FalloutScript::FormId id = numberOf(perk.mId); id != 0)
                result.push_back(id);
        return result;
    }

    bool FalloutScripts::abilityEffects(
        FalloutScript::FormId ability, std::vector<FalloutScript::AbilityEffect>& effects)
    {
        const ESM4::Spell* spell = mStore.get<ESM4::Spell>().search(refId(ability));
        if (spell == nullptr)
            return false;
        for (const ESM4::EffectEntry& entry : spell->mEffects)
        {
            const ESM4::MagicEffect* effect = mStore.get<ESM4::MagicEffect>().search(entry.mBaseEffect);
            // Only the effects that change an actor value (the first kind of effect) are abilities that add to it
            if (effect == nullptr || effect->mData.mArchetype != 0)
                continue;
            int value = effect->mData.mActorValue;
            if (!FalloutScript::ActorValues::isValid(value))
                value = entry.mData.mActorValue;
            if (!FalloutScript::ActorValues::isValid(value))
                continue;
            // The magnitude has no sign: a detrimental effect takes away what a beneficial one gives (as in an item)
            const double magnitude = static_cast<double>(entry.mData.mMagnitude);
            FalloutScript::AbilityEffect made;
            made.mActorValue = value;
            made.mMagnitude = (effect->mData.mEffectFlags & effectDetrimental) != 0 ? -magnitude : magnitude;
            made.mConditions = entry.mConditions;
            effects.push_back(std::move(made));
        }
        return true;
    }

    bool FalloutScripts::consumable(FalloutScript::FormId item, FalloutScript::Consumable& result)
    {
        const ESM4::Potion* potion = mStore.get<ESM4::Potion>().search(refId(item));
        if (potion == nullptr)
            return false;
        result = FalloutScript::Consumable();
        result.mAddictionChance = std::clamp<double>(potion->mItem.chanceAddition, 0.0, 1.0);
        result.mWithdrawal = potion->mItem.withdrawl;
        // The editor's second flag of the item says it is food
        result.mFood = (potion->mItem.flags & 0x2) != 0;
        for (const ESM4::EffectEntry& entry : potion->mEffects)
        {
            FalloutScript::ConsumableEffect made;
            if (effectOf(entry, made))
                result.mEffects.push_back(std::move(made));
        }
        return true;
    }

    bool FalloutScripts::effectOf(const ESM4::EffectEntry& entry, FalloutScript::ConsumableEffect& made)
    {
        made = FalloutScript::ConsumableEffect();
        made.mEffect = numberOf(entry.mBaseEffect);
        made.mMagnitude = entry.mData.mMagnitude;
        made.mDuration = entry.mData.mDuration;
        made.mConditions = entry.mConditions;
        const ESM4::MagicEffect* effect = mStore.get<ESM4::MagicEffect>().search(entry.mBaseEffect);
        if (effect != nullptr)
        {
            made.mArchetype = static_cast<int>(effect->mData.mArchetype);
            made.mActorValue = effect->mData.mActorValue;
            made.mDetrimental = (effect->mData.mEffectFlags & effectDetrimental) != 0;
            made.mHostile = (effect->mData.mEffectFlags & effectHostile) != 0;
            made.mRecover = (effect->mData.mEffectFlags & effectRecovers) != 0;
        }
        else if (entry.mBaseEffect.mIndex == engineRestoreHealth)
        {
            // The executable makes this effect itself: it restores health
            made.mActorValue = AV::health;
        }
        else
            return false;
        if (!FalloutScript::ActorValues::isValid(made.mActorValue))
            made.mActorValue = entry.mData.mActorValue;
        return true;
    }

    void FalloutScripts::castSpell(
        FalloutScript::FormId actor, FalloutScript::FormId, const std::vector<FalloutScript::SpellEffect>& kept)
    {
        // The conditions were asked once, by Magic, when it chose these effects; they are not asked again
        std::vector<FalloutScript::ConsumableEffect> effects;
        for (const FalloutScript::SpellEffect& effect : kept)
        {
            FalloutScript::ConsumableEffect made;
            made.mEffect = effect.mEffect;
            made.mArchetype = effect.mArchetype;
            made.mActorValue = effect.mActorValue;
            made.mMagnitude = effect.mMagnitude;
            made.mDuration = effect.mDuration;
            made.mDetrimental = effect.mDetrimental;
            made.mHostile = effect.mHostile;
            made.mRecover = effect.mRecover;
            effects.push_back(std::move(made));
        }
        mItemUse.cast(actor, effects);
    }

    bool FalloutScripts::spell(FalloutScript::FormId spell, FalloutScript::SpellInfo& info)
    {
        const ESM4::Spell* record = mStore.get<ESM4::Spell>().search(refId(spell));
        if (record == nullptr)
            return false;
        info = FalloutScript::SpellInfo();
        info.mType = static_cast<int>(record->mData.mType);
        for (const ESM4::EffectEntry& entry : record->mEffects)
        {
            FalloutScript::ConsumableEffect made;
            if (!effectOf(entry, made))
                continue;
            FalloutScript::SpellEffect effect;
            effect.mEffect = made.mEffect;
            effect.mArchetype = made.mArchetype;
            effect.mActorValue = made.mActorValue;
            effect.mMagnitude = made.mMagnitude;
            effect.mDuration = made.mDuration;
            effect.mDetrimental = made.mDetrimental;
            effect.mHostile = made.mHostile;
            effect.mRecover = made.mRecover;
            effect.mConditions = made.mConditions;
            info.mEffects.push_back(std::move(effect));
        }
        return true;
    }

    bool FalloutScripts::book(FalloutScript::FormId item, FalloutScript::BookInfo& result)
    {
        const ESM4::Book* book = mStore.get<ESM4::Book>().search(refId(item));
        if (book == nullptr)
            return false;
        // The skill a book teaches is a number from 0 on, counted from the first skill
        const int skill = book->mData.bookSkill;
        result.mSkill = skill >= 0 ? AV::firstSkill + skill : -1;
        if (result.mSkill > AV::lastSkill)
            result.mSkill = -1;
        return true;
    }

    std::vector<FalloutScript::PlayerProgress::LevelUp> FalloutScripts::takeLevelUps()
    {
        const std::lock_guard lock(mLevelUpMutex);
        std::vector<FalloutScript::PlayerProgress::LevelUp> result(mLevelUps.begin(), mLevelUps.end());
        mLevelUps.clear();
        return result;
    }

    void FalloutScripts::finishLevelUp()
    {
        const std::lock_guard lock(mLevelUpMutex);
        mLevelUpOpen = false;
    }

    void FalloutScripts::rewardExperience(int amount)
    {
        ensureLevellingRules();
        // The perks that change experience change what is gained, not what is lost, and not at the highest level
        if (amount > 0 && !mProgress.atHighestLevel())
            amount
                = static_cast<int>(std::lround(mPerks.apply(FalloutScript::EntryPoint::experience, mPlayer, amount)));
        mProgress.rewardExperience(amount);
    }

    void FalloutScripts::updateProgress(float frametime)
    {
        ensureLevellingRules();
        // The points of the levels that were reached are handed out one level at a time, for the intelligence the
        // player has when it gets there, and the next level waits until the screen of the one before is done: the
        // games show one screen for each level, with its own skill points and perk, never one for several
        bool screenOpen = false;
        {
            const std::lock_guard lock(mLevelUpMutex);
            screenOpen = mLevelUpOpen;
        }
        if (!screenOpen)
        {
            if (const auto up = mProgress.awardNextLevel(mActorValues.permanent(mPlayer, AV::intelligence)))
            {
                FalloutScript::PlayerProgress::LevelUp reached = *up;
                const double points
                    = mPerks.apply(FalloutScript::EntryPoint::gainedSkillPoints, mPlayer, reached.mSkillPoints);
                const int extra = static_cast<int>(std::lround(points)) - reached.mSkillPoints;
                if (extra != 0)
                {
                    mProgress.addSkillPoints(extra);
                    reached.mSkillPoints += extra;
                }
                log("Level " + std::to_string(reached.mLevel) + ": " + std::to_string(reached.mSkillPoints)
                    + " skill points" + (reached.mPerk ? " and a perk to choose" : ""));
                const std::lock_guard lock(mLevelUpMutex);
                mLevelUps.push_back(reached);
                mLevelUpOpen = true;
            }
        }
        // The perks of the characters that have them, and of those that had them (their effects are taken back)
        mPerkOwners.insert(mPlayer);
        for (const FalloutScript::FormId owner : mProgress.perkOwners())
            mPerkOwners.insert(owner);
        for (const FalloutScript::FormId owner : mPerkOwners)
            mPerks.refresh(owner);
        if (frametime > 0)
        {
            mItemUse.update(frametime);
            mPresentation.update(frametime);
            mMagic.update(frametime);
        }
    }

    void FalloutScripts::awardKillExperience(FalloutScript::FormId victim)
    {
        if (victim == 0 || victim == mPlayer)
            return;
        double damage = 0;
        {
            const std::lock_guard lock(mPlayerDamageMutex);
            const auto found = mPlayerDamage.find(victim);
            if (found != mPlayerDamage.end())
            {
                damage = found->second;
                mPlayerDamage.erase(found);
            }
        }
        ensureLevellingRules();
        const FalloutScript::LevellingRules rules = mProgress.rules();
        // Whoever strikes the last blow, a kill pays the player only when the player did more than a share of the
        // health of the victim in all (iXPDeathRewardHealthThreshold, 40 percent in both games): the share is told in
        // whole percent, a victim with no health counts as 100, and a player who did no damage at all gets nothing
        // (a character that a script kills pays nothing)
        {
            const double health = mActorValues.permanent(victim, AV::health);
            const int share = health > 0 ? static_cast<int>(std::min(damage / health * 100, 1e6)) : 100;
            if (damage <= 0 || share <= rules.mDeathHealthThreshold)
                return;
        }
        ESM::FormId base;
        if (!baseOf(victim, base) || base.isZeroOrUnset())
            return;
        bool creature = false;
        if (mStore.get<ESM4::Creature>().search(base) != nullptr)
            creature = true;
        else if (mStore.get<ESM4::Npc>().search(base) == nullptr)
            return;
        int level = 1;
        levelOf(victim, mProgress.level(), level);
        // Normal difficulty: the game has no setting of it yet
        constexpr int normalDifficulty = 2;
        const int experience = FalloutScript::Levelling::killExperience(rules, creature, level, normalDifficulty);
        if (experience > 0)
        {
            rewardExperience(experience);
            log("Kill of " + editorId(numberOf(base)) + " (level " + std::to_string(level) + "): "
                + std::to_string(experience) + " experience, " + std::to_string(mProgress.experience()) + " in all");
        }
        // What the Pip-Boy counts: people killed, creatures killed, and (New Vegas) things killed
        mProgress.modifyMiscStat(creature ? 3 : 2, 1);
        if (mGame == FalloutScript::Game::NewVegas)
            mProgress.modifyMiscStat(35, 1);
    }

    FalloutScript::ItemUse::Result FalloutScripts::useItem(FalloutScript::FormId actor, FalloutScript::FormId item)
    {
        ensureLevellingRules();
        const FalloutScript::ItemUse::Result result = mItemUse.use(actor, item);
        if (actor != mPlayer || result.mOutcome != FalloutScript::ItemUse::Outcome::Read || result.mSkill < 0)
            return result;
        // "Barter increased by 3"
        std::string name = "Skill";
        for (const FalloutScript::ChargenSkill& skill : mChargen.skills())
            if (skill.mNumber == result.mSkill)
                name = skill.mName;
        tellPlayer(name + " increased by " + std::to_string(static_cast<int>(std::lround(result.mGained))));
        return result;
    }
}
