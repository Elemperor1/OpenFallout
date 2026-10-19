#include "savecharacters.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "actors.hpp"
#include "actorvalues.hpp"
#include "combatstate.hpp"
#include "equipment.hpp"
#include "formlists.hpp"
#include "itemuse.hpp"
#include "perks.hpp"
#include "playerprogress.hpp"
#include "referencestate.hpp"

namespace FalloutScript
{
    namespace
    {
        void writeForms(SaveWriter& out, const std::set<FormId>& forms)
        {
            out.count(forms.size());
            for (const FormId form : forms)
                out.form(form);
        }

        /// The forms of the list that are still there (a plugin that is gone takes its forms with it)
        std::set<FormId> readForms(SaveReader& in)
        {
            std::set<FormId> result;
            const std::size_t count = in.count(4);
            for (std::size_t i = 0; i < count; ++i)
                if (const std::optional<FormId> form = in.form())
                    result.insert(*form);
            return result;
        }

        void writeCounts(SaveWriter& out, const std::map<FormId, int>& counts)
        {
            out.count(counts.size());
            for (const auto& [form, count] : counts)
            {
                out.form(form);
                out.i32(count);
            }
        }

        std::map<FormId, int> readCounts(SaveReader& in)
        {
            std::map<FormId, int> result;
            const std::size_t count = in.count(8);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> form = in.form();
                const int value = in.i32();
                if (form)
                    result[*form] = value;
            }
            return result;
        }

        void writePairs(SaveWriter& out, const std::map<FormId, FormId>& pairs)
        {
            out.count(pairs.size());
            for (const auto& [first, second] : pairs)
            {
                out.form(first);
                out.form(second);
            }
        }

        std::map<FormId, FormId> readPairs(SaveReader& in)
        {
            std::map<FormId, FormId> result;
            const std::size_t count = in.count(8);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> first = in.form();
                const std::optional<FormId> second = in.form();
                if (first && second)
                    result[*first] = *second;
            }
            return result;
        }
    }

    template <>
    struct SaveAccess<ActorState>
    {
        static void write(ActorState& state, SaveWriter& out)
        {
            const std::lock_guard lock(state.mMutex);
            out.count(state.mHolders.size());
            for (const auto& [id, holder] : state.mHolders)
            {
                out.form(id);
                out.boolean(holder.mItemsKnown);
                out.boolean(holder.mFactionsKnown);
                writeCounts(out, holder.mItems);
                out.count(holder.mItemEdits.size());
                for (const auto& [item, edit] : holder.mItemEdits)
                {
                    out.form(item);
                    out.i64(edit.mChange);
                    out.i64(edit.mLowest);
                    out.i64(edit.mHighest);
                }
                writeCounts(out, holder.mFactions);
                writeCounts(out, holder.mFactionEdits);
            }
            writeForms(out, state.mDead);
            writeForms(out, state.mStartKnown);
            writePairs(out, state.mKillers);
            writeCounts(out, state.mKillingLimbs);
            out.count(state.mDeaths.size());
            for (const ActorState::Death& death : state.mDeaths)
            {
                out.form(death.mActor);
                out.form(death.mKiller);
            }
            out.count(state.mChanges.size());
            for (const ActorState::ItemChange& change : state.mChanges)
            {
                out.form(change.mHolder);
                out.form(change.mItem);
                out.i32(change.mCount);
            }
        }

        static void read(ActorState& state, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, ActorState::Holder> holders;
            const std::size_t count = in.count(22);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> id = in.form();
                ActorState::Holder holder;
                holder.mItemsKnown = in.boolean();
                holder.mFactionsKnown = in.boolean();
                holder.mItems = readCounts(in);
                const std::size_t edits = in.count(28);
                for (std::size_t j = 0; j < edits; ++j)
                {
                    const std::optional<FormId> item = in.form();
                    ActorState::ItemEdit edit;
                    edit.mChange = in.i64();
                    edit.mLowest = in.i64();
                    edit.mHighest = in.i64();
                    if (item)
                        holder.mItemEdits[*item] = edit;
                }
                holder.mFactions = readCounts(in);
                holder.mFactionEdits = readCounts(in);
                if (id)
                    holders[*id] = std::move(holder);
            }
            std::set<FormId> dead = readForms(in);
            std::set<FormId> startKnown = readForms(in);
            std::map<FormId, FormId> killers = readPairs(in);
            std::map<FormId, int> limbs = readCounts(in);
            std::vector<ActorState::Death> deaths;
            const std::size_t deathCount = in.count(8);
            for (std::size_t i = 0; i < deathCount; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const std::optional<FormId> killer = in.form();
                if (actor)
                    deaths.push_back({ *actor, killer.value_or(0) });
            }
            std::vector<ActorState::ItemChange> changes;
            const std::size_t changeCount = in.count(12);
            for (std::size_t i = 0; i < changeCount; ++i)
            {
                const std::optional<FormId> holder = in.form();
                const std::optional<FormId> item = in.form();
                const int amount = in.i32();
                if (holder && item)
                    changes.push_back({ *holder, *item, amount });
            }

            const std::lock_guard lock(state.mMutex);
            state.mHolders = std::move(holders);
            state.mDead = std::move(dead);
            state.mStartKnown = std::move(startKnown);
            state.mKillers = std::move(killers);
            state.mKillingLimbs = std::move(limbs);
            state.mDeaths = std::move(deaths);
            state.mChanges = std::move(changes);
        }
    };

    template <>
    struct SaveAccess<Equipment>
    {
        static void write(Equipment& equipment, SaveWriter& out)
        {
            const std::lock_guard lock(equipment.mMutex);
            out.count(equipment.mWorn.size());
            for (const auto& [actor, worn] : equipment.mWorn)
            {
                out.form(actor);
                out.form(worn.mWeapon);
                out.form(worn.mAmmo);
                out.count(worn.mArmor.size());
                for (const FormId piece : worn.mArmor)
                    out.form(piece);
                out.boolean(worn.mDrawn);
                out.boolean(worn.mSeeded);
            }
            out.count(equipment.mChanges.size());
            for (const Equipment::WornChange& change : equipment.mChanges)
            {
                out.form(change.mActor);
                out.form(change.mItem);
                out.boolean(change.mWorn);
            }
        }

        static void read(Equipment& equipment, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, Equipment::Worn> all;
            const std::size_t count = in.count(18);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                Equipment::Worn worn;
                // A weapon or ammunition from a plugin that is gone is not held any more
                worn.mWeapon = in.form().value_or(0);
                worn.mAmmo = in.form().value_or(0);
                const std::size_t pieces = in.count(4);
                for (std::size_t j = 0; j < pieces; ++j)
                    if (const std::optional<FormId> piece = in.form())
                        worn.mArmor.push_back(*piece);
                worn.mDrawn = in.boolean();
                worn.mSeeded = in.boolean();
                if (actor)
                    all[*actor] = std::move(worn);
            }
            std::vector<Equipment::WornChange> changes;
            const std::size_t changeCount = in.count(9);
            for (std::size_t i = 0; i < changeCount; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const std::optional<FormId> item = in.form();
                const bool worn = in.boolean();
                if (actor && item)
                    changes.push_back({ *actor, *item, worn });
            }

            const std::lock_guard lock(equipment.mMutex);
            equipment.mWorn = std::move(all);
            equipment.mChanges = std::move(changes);
        }
    };

    template <>
    struct SaveAccess<ActorValues>
    {
        static void write(ActorValues& values, SaveWriter& out)
        {
            const std::lock_guard lock(values.mMutex);
            out.count(values.mEntries.size());
            for (const auto& [key, entry] : values.mEntries)
            {
                out.form(key.first);
                out.i32(key.second);
                out.boolean(entry.mBase.has_value());
                out.f64(entry.mBase.value_or(0));
                out.f64(entry.mModifier);
                out.f64(entry.mDamage);
            }
        }

        static void read(ActorValues& values, SaveReader& in, std::uint32_t)
        {
            std::map<std::pair<FormId, int>, ActorValues::Entry> entries;
            const std::size_t count = in.count(29);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const int value = in.i32();
                ActorValues::Entry entry;
                const bool hasBase = in.boolean();
                const double base = in.f64();
                if (hasBase)
                    entry.mBase = base;
                entry.mModifier = in.f64();
                entry.mDamage = in.f64();
                if (actor)
                    entries[{ *actor, value }] = entry;
            }
            const std::lock_guard lock(values.mMutex);
            values.mEntries = std::move(entries);
        }
    };

    template <>
    struct SaveAccess<PlayerProgress>
    {
        static void write(PlayerProgress& progress, SaveWriter& out)
        {
            const std::lock_guard lock(progress.mMutex);
            out.i32(progress.mExperience);
            out.i32(progress.mKarma);
            out.i32(progress.mAwardedLevel);
            out.i32(progress.mSkillPoints);
            out.i32(progress.mPerkPoints);
            out.count(progress.mPerkLevels.size());
            for (const int level : progress.mPerkLevels)
                out.i32(level);
            out.count(progress.mMiscStats.size());
            for (const auto& [stat, value] : progress.mMiscStats)
            {
                out.i32(stat);
                out.i32(value);
            }
            out.count(progress.mPerks.size());
            for (const auto& [key, rank] : progress.mPerks)
            {
                out.form(key.first);
                out.form(key.second);
                out.i32(rank);
                const auto order = progress.mPerkOrder.find(key);
                out.u64(order != progress.mPerkOrder.end() ? order->second : 0);
            }
            out.u64(progress.mPerkCount);
            writeForms(out, progress.mNotes);
        }

        static void read(PlayerProgress& progress, SaveReader& in, std::uint32_t)
        {
            const int experience = in.i32();
            const int karma = in.i32();
            const int awarded = in.i32();
            const int skillPoints = in.i32();
            const int perkPoints = in.i32();
            std::vector<int> perkLevels;
            const std::size_t levels = in.count(4);
            for (std::size_t i = 0; i < levels; ++i)
                perkLevels.push_back(in.i32());
            std::map<int, int> stats;
            const std::size_t statCount = in.count(8);
            for (std::size_t i = 0; i < statCount; ++i)
            {
                const int stat = in.i32();
                stats[stat] = in.i32();
            }
            std::map<std::pair<FormId, FormId>, int> perks;
            std::map<std::pair<FormId, FormId>, std::uint64_t> order;
            const std::size_t perkCount = in.count(20);
            for (std::size_t i = 0; i < perkCount; ++i)
            {
                const std::optional<FormId> owner = in.form();
                const std::optional<FormId> perk = in.form();
                const int rank = in.i32();
                const std::uint64_t place = in.u64();
                if (owner && perk)
                {
                    perks[{ *owner, *perk }] = rank;
                    order[{ *owner, *perk }] = place;
                }
            }
            const std::uint64_t counted = in.u64();
            std::set<FormId> notes = readForms(in);

            const std::lock_guard lock(progress.mMutex);
            progress.mExperience = experience;
            progress.mKarma = karma;
            progress.mAwardedLevel = awarded;
            progress.mSkillPoints = skillPoints;
            progress.mPerkPoints = perkPoints;
            progress.mPerkLevels = std::move(perkLevels);
            progress.mMiscStats = std::move(stats);
            progress.mPerks = std::move(perks);
            progress.mPerkOrder = std::move(order);
            progress.mPerkCount = counted;
            progress.mNotes = std::move(notes);
        }
    };

    template <>
    struct SaveAccess<PerkEngine>
    {
        static void write(PerkEngine& perks, SaveWriter& out)
        {
            const std::lock_guard lock(perks.mMutex);
            out.count(perks.mApplied.size());
            for (const auto& [key, applied] : perks.mApplied)
            {
                out.form(key.first);
                out.form(key.second);
                out.i32(applied.mRank);
                out.count(applied.mEffects.size());
                for (std::size_t i = 0; i < applied.mEffects.size(); ++i)
                {
                    out.i32(applied.mEffects[i].mActorValue);
                    out.f64(applied.mEffects[i].mMagnitude);
                    out.boolean(i < applied.mOn.size() && applied.mOn[i]);
                }
            }
        }

        /// The effects the abilities of the perk at the rank give, as refresh() makes them
        static std::vector<AbilityEffect> derive(PerkEngine& perks, FormId perk, int rank)
        {
            std::vector<AbilityEffect> effects;
            PerkInfo info;
            if (!perks.info(perk, info))
                return effects;
            const int rankIndex = std::min(rank, info.mRanks) - 1;
            for (const PerkEntry& entry : info.mEntries)
            {
                if (entry.mRank != rankIndex || entry.mType != PerkEntry::Ability)
                    continue;
                std::vector<AbilityEffect> found;
                if (perks.mRecords.abilityEffects(entry.mAbility, found))
                    effects.insert(effects.end(), found.begin(), found.end());
            }
            return effects;
        }

        static void read(PerkEngine& perks, ActorValues& values, SaveReader& in, std::uint32_t)
        {
            struct Saved
            {
                int mActorValue = 0;
                double mMagnitude = 0;
                bool mOn = false;
            };
            struct Entry
            {
                FormId mOwner = 0;
                FormId mPerk = 0;
                int mRank = 0;
                std::vector<Saved> mEffects;
                bool mHasOwner = false;
                bool mHasPerk = false;
            };
            std::vector<Entry> entries;
            const std::size_t count = in.count(16);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> owner = in.form();
                const std::optional<FormId> perk = in.form();
                Entry entry;
                entry.mOwner = owner.value_or(0);
                entry.mPerk = perk.value_or(0);
                entry.mHasOwner = owner.has_value();
                entry.mHasPerk = perk.has_value();
                entry.mRank = in.i32();
                const std::size_t effects = in.count(13);
                for (std::size_t j = 0; j < effects; ++j)
                {
                    Saved saved;
                    saved.mActorValue = in.i32();
                    saved.mMagnitude = in.f64();
                    saved.mOn = in.boolean();
                    entry.mEffects.push_back(saved);
                }
                entries.push_back(std::move(entry));
            }

            for (const Entry& entry : entries)
            {
                // Nothing to keep for a character that is gone
                if (!entry.mHasOwner)
                    continue;
                PerkEngine::Applied applied;
                applied.mRank = entry.mRank;
                if (entry.mHasPerk)
                    applied.mEffects = derive(perks, entry.mPerk, entry.mRank);
                bool same = applied.mEffects.size() == entry.mEffects.size();
                for (std::size_t i = 0; same && i < entry.mEffects.size(); ++i)
                    same = applied.mEffects[i].mActorValue == entry.mEffects[i].mActorValue
                        && std::abs(applied.mEffects[i].mMagnitude - entry.mEffects[i].mMagnitude) < 1e-9;
                if (same)
                {
                    for (const Saved& saved : entry.mEffects)
                        applied.mOn.push_back(saved.mOn);
                }
                else
                {
                    // The perk is no longer what it was when the game was saved: what its effects had added is taken
                    // back, and the next refresh adds what the perk gives now
                    for (const Saved& saved : entry.mEffects)
                        if (saved.mOn)
                            values.modify(entry.mOwner, saved.mActorValue, -saved.mMagnitude);
                    applied.mOn.assign(applied.mEffects.size(), false);
                    if (!entry.mHasPerk)
                        continue;
                }
                const std::lock_guard lock(perks.mMutex);
                perks.mApplied[{ entry.mOwner, entry.mPerk }] = std::move(applied);
            }
        }
    };

    template <>
    struct SaveAccess<ItemUse>
    {
        static void write(ItemUse& use, SaveWriter& out)
        {
            const std::lock_guard lock(use.mMutex);
            out.count(use.mActive.size());
            for (const ItemUse::Active& active : use.mActive)
            {
                out.form(active.mActor);
                out.i32(active.mActorValue);
                out.f64(active.mMagnitude);
                out.f64(active.mRemaining);
                out.boolean(active.mTemporary);
            }
            out.count(use.mAddictions.size());
            for (const auto& [actor, item] : use.mAddictions)
            {
                out.form(actor);
                out.form(item);
            }
        }

        static void read(ItemUse& use, SaveReader& in, std::uint32_t)
        {
            std::vector<ItemUse::Active> active;
            const std::size_t count = in.count(25);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                ItemUse::Active effect;
                effect.mActorValue = in.i32();
                effect.mMagnitude = in.f64();
                effect.mRemaining = in.f64();
                effect.mTemporary = in.boolean();
                if (actor)
                {
                    effect.mActor = *actor;
                    active.push_back(effect);
                }
            }
            std::vector<std::pair<FormId, FormId>> addictions;
            const std::size_t addicted = in.count(8);
            for (std::size_t i = 0; i < addicted; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const std::optional<FormId> item = in.form();
                if (actor && item)
                    addictions.emplace_back(*actor, *item);
            }
            const std::lock_guard lock(use.mMutex);
            use.mActive = std::move(active);
            use.mAddictions = std::move(addictions);
        }
    };

    template <>
    struct SaveAccess<CombatState>
    {
        static void writeHit(SaveWriter& out, const CombatState::Hit& hit)
        {
            out.form(hit.mAttacker);
            out.form(hit.mWeapon);
            out.f64(hit.mDamage);
            out.boolean(hit.mCritical);
            out.i32(hit.mLimb);
        }

        /// The hit as it is read; none when the attacker's plugin is gone (a weapon that is gone is none)
        static CombatState::Hit readHit(SaveReader& in, bool& attackerKnown)
        {
            CombatState::Hit hit;
            const std::optional<FormId> attacker = in.form();
            attackerKnown = attacker.has_value();
            hit.mAttacker = attacker.value_or(0);
            hit.mWeapon = in.form().value_or(0);
            hit.mDamage = in.f64();
            hit.mCritical = in.boolean();
            hit.mLimb = in.i32();
            return hit;
        }

        static constexpr std::size_t hitBytes = 21;

        static void write(CombatState& combat, SaveWriter& out)
        {
            const std::lock_guard lock(combat.mMutex);
            writePairs(out, combat.mTargets);
            writeForms(out, combat.mAlert);
            writeForms(out, combat.mUnconscious);
            out.count(combat.mWakeTimers.size());
            for (const auto& [actor, timer] : combat.mWakeTimers)
            {
                out.form(actor);
                out.f32(timer);
            }
            out.count(combat.mWoken.size());
            for (const FormId actor : combat.mWoken)
                out.form(actor);
            writeForms(out, combat.mRestrained);
            out.count(combat.mEssential.size());
            for (const auto& [actor, essential] : combat.mEssential)
            {
                out.form(actor);
                out.boolean(essential);
            }
            out.count(combat.mRelations.size());
            for (const auto& [key, relation] : combat.mRelations)
            {
                out.form(key.first);
                out.form(key.second);
                out.u8(static_cast<std::uint8_t>(relation));
            }
            writeForms(out, combat.mAttacked);
            writeForms(out, combat.mIgnoreFriendlyHits);
            out.count(combat.mHitTimes.size());
            for (const auto& [victim, attackers] : combat.mHitTimes)
            {
                out.form(victim);
                out.count(attackers.size());
                for (const auto& [attacker, times] : attackers)
                {
                    out.form(attacker);
                    out.count(times.size());
                    for (const double time : times)
                        out.f64(time);
                }
            }
            out.f64(combat.mClock);
            out.count(combat.mLastHits.size());
            for (const auto& [victim, hit] : combat.mLastHits)
            {
                out.form(victim);
                writeHit(out, hit);
            }
            out.count(combat.mHitEvents.size());
            for (const CombatState::HitEvent& event : combat.mHitEvents)
            {
                out.form(event.mVictim);
                writeHit(out, event.mHit);
            }
            out.count(combat.mChanges.size());
            for (const CombatState::Change& change : combat.mChanges)
            {
                out.form(change.mActor);
                out.form(change.mTarget);
                out.form(change.mWas);
            }
        }

        static void read(CombatState& combat, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, FormId> targets = readPairs(in);
            std::set<FormId> alert = readForms(in);
            std::set<FormId> unconscious = readForms(in);
            std::map<FormId, float> timers;
            const std::size_t timerCount = in.count(8);
            for (std::size_t i = 0; i < timerCount; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const float timer = in.f32();
                if (actor)
                    timers[*actor] = timer;
            }
            std::vector<FormId> woken;
            const std::size_t wokenCount = in.count(4);
            for (std::size_t i = 0; i < wokenCount; ++i)
                if (const std::optional<FormId> actor = in.form())
                    woken.push_back(*actor);
            std::set<FormId> restrained = readForms(in);
            std::map<FormId, bool> essential;
            const std::size_t essentialCount = in.count(5);
            for (std::size_t i = 0; i < essentialCount; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const bool value = in.boolean();
                if (actor)
                    essential[*actor] = value;
            }
            std::map<std::pair<FormId, FormId>, CombatState::Relation> relations;
            const std::size_t relationCount = in.count(9);
            for (std::size_t i = 0; i < relationCount; ++i)
            {
                const std::optional<FormId> first = in.form();
                const std::optional<FormId> second = in.form();
                const std::uint8_t relation = in.u8();
                if (first && second && relation <= static_cast<std::uint8_t>(CombatState::Relation::Friend))
                    relations[{ *first, *second }] = static_cast<CombatState::Relation>(relation);
            }
            std::set<FormId> attacked = readForms(in);
            std::set<FormId> ignoring = readForms(in);
            std::map<FormId, std::map<FormId, std::vector<double>>> hitTimes;
            const std::size_t victimCount = in.count(8);
            for (std::size_t i = 0; i < victimCount; ++i)
            {
                const std::optional<FormId> victim = in.form();
                const std::size_t attackerCount = in.count(8);
                std::map<FormId, std::vector<double>> attackers;
                for (std::size_t j = 0; j < attackerCount; ++j)
                {
                    const std::optional<FormId> attacker = in.form();
                    std::vector<double> times;
                    const std::size_t timeCount = in.count(8);
                    for (std::size_t k = 0; k < timeCount; ++k)
                        times.push_back(in.f64());
                    if (attacker)
                        attackers[*attacker] = std::move(times);
                }
                if (victim)
                    hitTimes[*victim] = std::move(attackers);
            }
            const double clock = in.f64();
            std::map<FormId, CombatState::Hit> lastHits;
            const std::size_t lastCount = in.count(4 + hitBytes);
            for (std::size_t i = 0; i < lastCount; ++i)
            {
                const std::optional<FormId> victim = in.form();
                bool known = false;
                const CombatState::Hit hit = readHit(in, known);
                if (victim && known)
                    lastHits[*victim] = hit;
            }
            std::vector<CombatState::HitEvent> events;
            const std::size_t eventCount = in.count(4 + hitBytes);
            for (std::size_t i = 0; i < eventCount; ++i)
            {
                const std::optional<FormId> victim = in.form();
                bool known = false;
                const CombatState::Hit hit = readHit(in, known);
                if (victim && known)
                    events.push_back({ *victim, hit });
            }
            std::vector<CombatState::Change> changes;
            const std::size_t changeCount = in.count(12);
            for (std::size_t i = 0; i < changeCount; ++i)
            {
                const std::optional<FormId> actor = in.form();
                const std::optional<FormId> target = in.form();
                const std::optional<FormId> was = in.form();
                if (actor)
                    changes.push_back({ *actor, target.value_or(0), was.value_or(0) });
            }

            const std::lock_guard lock(combat.mMutex);
            combat.mTargets = std::move(targets);
            combat.mAlert = std::move(alert);
            combat.mUnconscious = std::move(unconscious);
            combat.mWakeTimers = std::move(timers);
            combat.mWoken = std::move(woken);
            combat.mRestrained = std::move(restrained);
            combat.mEssential = std::move(essential);
            combat.mRelations = std::move(relations);
            combat.mAttacked = std::move(attacked);
            combat.mIgnoreFriendlyHits = std::move(ignoring);
            combat.mHitTimes = std::move(hitTimes);
            combat.mClock = clock;
            combat.mLastHits = std::move(lastHits);
            combat.mHitEvents = std::move(events);
            combat.mChanges = std::move(changes);
        }
    };

    template <>
    struct SaveAccess<ReferenceState>
    {
        static void write(ReferenceState& state, SaveWriter& out)
        {
            const std::lock_guard lock(state.mMutex);
            out.count(state.mLocks.size());
            for (const auto& [reference, edit] : state.mLocks)
            {
                out.form(reference);
                out.boolean(edit.mLocked.has_value());
                out.boolean(edit.mLocked.value_or(false));
                out.boolean(edit.mLevel.has_value());
                out.i32(edit.mLevel.value_or(0));
            }
            writeForms(out, state.mDestroyed);
        }

        static void read(ReferenceState& state, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, ReferenceState::LockEdit> locks;
            const std::size_t count = in.count(11);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> reference = in.form();
                ReferenceState::LockEdit edit;
                const bool hasLocked = in.boolean();
                const bool locked = in.boolean();
                const bool hasLevel = in.boolean();
                const int level = in.i32();
                if (hasLocked)
                    edit.mLocked = locked;
                if (hasLevel)
                    edit.mLevel = level;
                if (reference)
                    locks[*reference] = edit;
            }
            std::set<FormId> destroyed = readForms(in);
            const std::lock_guard lock(state.mMutex);
            state.mLocks = std::move(locks);
            state.mDestroyed = std::move(destroyed);
        }
    };

    template <>
    struct SaveAccess<FormLists>
    {
        static void write(FormLists& lists, SaveWriter& out)
        {
            const std::lock_guard lock(lists.mMutex);
            out.count(lists.mLists.size());
            for (const auto& [id, forms] : lists.mLists)
            {
                out.form(id);
                out.count(forms.size());
                for (const FormId form : forms)
                    out.form(form);
            }
        }

        static void read(FormLists& lists, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, std::vector<FormId>> all;
            const std::size_t count = in.count(8);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> id = in.form();
                std::vector<FormId> forms;
                const std::size_t size = in.count(4);
                for (std::size_t j = 0; j < size; ++j)
                    if (const std::optional<FormId> form = in.form())
                        forms.push_back(*form);
                if (id)
                    all[*id] = std::move(forms);
            }
            const std::lock_guard lock(lists.mMutex);
            lists.mLists = std::move(all);
        }
    };

    void writeActors(ActorState& state, SaveWriter& out)
    {
        SaveAccess<ActorState>::write(state, out);
    }

    void readActors(ActorState& state, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<ActorState>::read(state, in, version);
    }

    void writeEquipment(Equipment& equipment, SaveWriter& out)
    {
        SaveAccess<Equipment>::write(equipment, out);
    }

    void readEquipment(Equipment& equipment, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Equipment>::read(equipment, in, version);
    }

    void writeActorValues(ActorValues& values, SaveWriter& out)
    {
        SaveAccess<ActorValues>::write(values, out);
    }

    void readActorValues(ActorValues& values, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<ActorValues>::read(values, in, version);
    }

    void writeProgress(PlayerProgress& progress, SaveWriter& out)
    {
        SaveAccess<PlayerProgress>::write(progress, out);
    }

    void readProgress(PlayerProgress& progress, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<PlayerProgress>::read(progress, in, version);
    }

    void writePerks(PerkEngine& perks, ActorValues&, SaveWriter& out)
    {
        SaveAccess<PerkEngine>::write(perks, out);
    }

    void readPerks(PerkEngine& perks, ActorValues& values, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<PerkEngine>::read(perks, values, in, version);
    }

    void writeItemUse(ItemUse& use, SaveWriter& out)
    {
        SaveAccess<ItemUse>::write(use, out);
    }

    void readItemUse(ItemUse& use, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<ItemUse>::read(use, in, version);
    }

    void writeCombat(CombatState& combat, SaveWriter& out)
    {
        SaveAccess<CombatState>::write(combat, out);
    }

    void readCombat(CombatState& combat, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<CombatState>::read(combat, in, version);
    }

    void writeReferenceState(ReferenceState& state, SaveWriter& out)
    {
        SaveAccess<ReferenceState>::write(state, out);
    }

    void readReferenceState(ReferenceState& state, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<ReferenceState>::read(state, in, version);
    }

    void writeFormLists(FormLists& lists, SaveWriter& out)
    {
        SaveAccess<FormLists>::write(lists, out);
    }

    void readFormLists(FormLists& lists, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<FormLists>::read(lists, in, version);
    }
}
