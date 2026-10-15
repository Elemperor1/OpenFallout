#include "actors.hpp"

#include <algorithm>
#include <limits>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        constexpr std::int64_t mostItems = std::numeric_limits<int>::max();
        /// Further than any count can be moved, so that a change that grows without end stays a number
        constexpr std::int64_t farthest = std::int64_t(1) << 40;
    }

    std::int64_t ActorState::ItemEdit::apply(std::int64_t start) const
    {
        return std::clamp(start + mChange, mLowest, mHighest);
    }

    void ActorState::ItemEdit::add(std::int64_t count)
    {
        mChange = std::min(mChange + count, farthest);
        mLowest = std::clamp<std::int64_t>(mLowest + count, 0, mostItems);
        mHighest = std::clamp<std::int64_t>(mHighest + count, 0, mostItems);
    }

    void ActorState::ItemEdit::remove(std::int64_t count)
    {
        mChange = std::max(mChange - count, -farthest);
        mLowest = std::clamp<std::int64_t>(mLowest - count, 0, mostItems);
        mHighest = std::clamp<std::int64_t>(mHighest - count, 0, mostItems);
    }

    ActorState::ActorState(ActorRecords& records)
        : mRecords(records)
    {
    }

    void ActorState::clear()
    {
        const std::lock_guard lock(mMutex);
        mHolders.clear();
        mDead.clear();
        mKillers.clear();
        mKillingLimbs.clear();
        mDeaths.clear();
        mChanges.clear();
    }

    void ActorState::knowItems(FormId id, Holder& entry)
    {
        if (entry.mItemsKnown)
            return;
        std::vector<std::pair<FormId, int>> items;
        if (!mRecords.initialItems(id, items))
            return;
        entry.mItemsKnown = true;
        for (const auto& [item, count] : items)
        {
            if (item != 0 && count > 0)
            {
                int& held = entry.mItems[item];
                held = static_cast<int>(std::min<std::int64_t>(std::int64_t(held) + count, mostItems));
            }
        }
        // What scripts did before the records could be asked is done to what the records say
        for (const auto& [item, edit] : entry.mItemEdits)
        {
            const auto held = entry.mItems.find(item);
            const std::int64_t result = edit.apply(held != entry.mItems.end() ? held->second : 0);
            if (result > 0)
                entry.mItems[item] = static_cast<int>(result);
            else if (held != entry.mItems.end())
                entry.mItems.erase(held);
        }
        entry.mItemEdits.clear();
    }

    void ActorState::knowFactions(FormId id, Holder& entry)
    {
        if (entry.mFactionsKnown)
            return;
        std::vector<std::pair<FormId, int>> factions;
        if (!mRecords.initialFactions(id, factions))
            return;
        entry.mFactionsKnown = true;
        for (const auto& [faction, rank] : factions)
            if (faction != 0 && rank >= 0)
                entry.mFactions.emplace(faction, rank);
        // What scripts did before the records could be asked is the later word, a removal as much as a rank
        for (const auto& [faction, rank] : entry.mFactionEdits)
        {
            if (rank < 0)
                entry.mFactions.erase(faction);
            else
                entry.mFactions[faction] = rank;
        }
        entry.mFactionEdits.clear();
    }

    int ActorState::itemCount(FormId id, FormId item)
    {
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(id);
        knowItems(id, entry);
        if (!entry.mItemsKnown)
        {
            const auto edit = entry.mItemEdits.find(item);
            return edit == entry.mItemEdits.end() ? 0 : static_cast<int>(edit->second.apply(0));
        }
        const auto found = entry.mItems.find(item);
        return found == entry.mItems.end() ? 0 : found->second;
    }

    void ActorState::addItem(FormId id, FormId item, int count)
    {
        const std::lock_guard lock(mMutex);
        if (item == 0 || count <= 0)
            return;
        Holder& entry = holder(id);
        knowItems(id, entry);
        if (!entry.mItemsKnown)
        {
            entry.mItemEdits[item].add(count);
            noteChange(id, item, count);
            return;
        }
        int& held = entry.mItems[item];
        const int before = held;
        held = static_cast<int>(std::min<std::int64_t>(std::int64_t(held) + count, mostItems));
        noteChange(id, item, held - before);
    }

    int ActorState::removeItem(FormId id, FormId item, int count)
    {
        const std::lock_guard lock(mMutex);
        if (item == 0 || count <= 0)
            return 0;
        Holder& entry = holder(id);
        knowItems(id, entry);
        if (!entry.mItemsKnown)
        {
            // What it had is not known yet; it loses the count that it would have had from nothing
            ItemEdit& edit = entry.mItemEdits[item];
            const std::int64_t before = edit.apply(0);
            edit.remove(count);
            const int removed = static_cast<int>(before - edit.apply(0));
            noteChange(id, item, -removed);
            return removed;
        }
        const auto found = entry.mItems.find(item);
        if (found == entry.mItems.end())
            return 0;
        const int removed = std::min(count, found->second);
        found->second -= removed;
        if (found->second <= 0)
            entry.mItems.erase(found);
        noteChange(id, item, -removed);
        return removed;
    }

    bool ActorState::items(FormId id, std::vector<std::pair<FormId, int>>& result)
    {
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(id);
        knowItems(id, entry);
        if (!entry.mItemsKnown)
            return false;
        result.assign(entry.mItems.begin(), entry.mItems.end());
        return true;
    }

    void ActorState::noteChange(FormId holder, FormId item, int count)
    {
        constexpr std::size_t mostChanges = 4096;
        if (count != 0 && mChanges.size() < mostChanges)
            mChanges.push_back({ holder, item, count });
    }

    std::vector<ActorState::ItemChange> ActorState::takeChanges()
    {
        const std::lock_guard lock(mMutex);
        std::vector<ItemChange> changes;
        changes.swap(mChanges);
        return changes;
    }

    bool ActorState::isDead(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return mDead.count(actor) != 0;
    }

    bool ActorState::setDead(FormId actor, bool dead, FormId killer, int limb)
    {
        if (actor == 0)
            return false;
        const std::lock_guard transition(mTransitionMutex);
        if (dead)
        {
            {
                const std::lock_guard lock(mMutex);
                if (!mDead.insert(actor).second)
                    return false;
                constexpr std::size_t mostDeaths = 1024;
                if (mDeaths.size() < mostDeaths)
                    mDeaths.push_back({ actor, killer });
                if (killer != 0)
                    mKillers[actor] = killer;
                // A death that no blow caused has no part of the body
                mKillingLimbs.erase(actor);
                if (limb >= 0)
                    mKillingLimbs[actor] = limb;
            }
            // The host reacts to the death as it happens, with the lock free
            if (mDeathListener)
                mDeathListener(actor);
            return true;
        }
        const std::lock_guard lock(mMutex);
        mKillers.erase(actor);
        mKillingLimbs.erase(actor);
        return mDead.erase(actor) != 0;
    }

    void ActorState::setDeathListener(std::function<void(FormId)> listener)
    {
        mDeathListener = std::move(listener);
    }

    void ActorState::setFallHandler(std::function<bool(FormId)> handler)
    {
        mFallHandler = std::move(handler);
    }

    bool ActorState::falls(FormId actor)
    {
        return actor != 0 && mFallHandler && mFallHandler(actor);
    }

    int ActorState::killingLimb(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mKillingLimbs.find(actor);
        return mDead.count(actor) != 0 && found != mKillingLimbs.end() ? found->second : -1;
    }

    FormId ActorState::killerOf(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mKillers.find(actor);
        return found == mKillers.end() ? 0 : found->second;
    }

    std::vector<ActorState::Death> ActorState::takeDeaths()
    {
        const std::lock_guard lock(mMutex);
        std::vector<Death> deaths;
        deaths.swap(mDeaths);
        return deaths;
    }

    bool ActorState::factions(FormId actor, std::vector<std::pair<FormId, int>>& result)
    {
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(actor);
        knowFactions(actor, entry);
        if (!entry.mFactionsKnown)
            return false;
        result.assign(entry.mFactions.begin(), entry.mFactions.end());
        return true;
    }

    bool ActorState::baseOf(FormId reference, FormId& base)
    {
        return mRecords.baseFormOf(reference, base);
    }

    int ActorState::deadCount(FormId base) const
    {
        const std::lock_guard lock(mMutex);
        int count = 0;
        for (const FormId actor : mDead)
        {
            FormId actorBase = 0;
            if (mRecords.baseFormOf(actor, actorBase) && actorBase == base)
                ++count;
        }
        return count;
    }

    int ActorState::factionRank(FormId actor, FormId faction)
    {
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(actor);
        knowFactions(actor, entry);
        const std::map<FormId, int>& ranks = entry.mFactionsKnown ? entry.mFactions : entry.mFactionEdits;
        const auto found = ranks.find(faction);
        return found == ranks.end() ? -1 : std::max(found->second, -1);
    }

    void ActorState::setFactionRank(FormId actor, FormId faction, int rank)
    {
        if (faction == 0)
            return;
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(actor);
        knowFactions(actor, entry);
        // A rank below 0 is the way to say that the actor is not in the faction
        if (entry.mFactionsKnown)
        {
            if (rank < 0)
                entry.mFactions.erase(faction);
            else
                entry.mFactions[faction] = rank;
        }
        else
            entry.mFactionEdits[faction] = std::max(rank, -1);
    }

    void ActorState::removeFromFaction(FormId actor, FormId faction)
    {
        setFactionRank(actor, faction, -1);
    }

    void addActorCommands(Interpreter& interpreter, ActorState& state)
    {
        interpreter.setHandler("AddItem", [&state](CallContext& call) {
            const FormId item = formArgument(call, 0);
            if (call.mReference != 0 && item != 0)
                state.addItem(call.mReference, item, intArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("RemoveItem", [&state](CallContext& call) {
            const FormId item = formArgument(call, 0);
            if (call.mReference != 0 && item != 0)
                state.removeItem(call.mReference, item, intArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("GetItemCount", [&state](CallContext& call) {
            const FormId item = formArgument(call, 0);
            return call.mReference != 0 && item != 0 ? static_cast<double>(state.itemCount(call.mReference, item))
                                                     : 0.0;
        });
        interpreter.setHandler("RemoveAllItems", [&state](CallContext& call) {
            // The items go to the reference that the script names, or are gone
            std::vector<std::pair<FormId, int>> held;
            if (call.mReference == 0 || !state.items(call.mReference, held))
                return 0.0;
            const FormId target = formArgument(call, 0);
            for (const auto& [item, count] : held)
            {
                const int removed = state.removeItem(call.mReference, item, count);
                if (target != 0 && target != call.mReference)
                    state.addItem(target, item, removed);
            }
            return 0.0;
        });
        interpreter.setHandler("DuplicateAllItems", [&state](CallContext& call) {
            std::vector<std::pair<FormId, int>> held;
            const FormId target = formArgument(call, 0);
            if (call.mReference == 0 || target == 0 || target == call.mReference || !state.items(call.mReference, held))
                return 0.0;
            for (const auto& [item, count] : held)
                state.addItem(target, item, count);
            return 0.0;
        });
        interpreter.setHandler("AddItemHealthPercent", [&state](CallContext& call) {
            // Nothing wears out yet: the health of the item is the same as for AddItem
            const FormId item = formArgument(call, 0);
            if (call.mReference != 0 && item != 0)
                state.addItem(call.mReference, item, intArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("GetDead",
            [&state](CallContext& call) { return call.mReference != 0 && state.isDead(call.mReference) ? 1.0 : 0.0; });
        interpreter.setHandler("GetDeadCount", [&state](CallContext& call) {
            const FormId base = formArgument(call, 0);
            return base != 0 ? static_cast<double>(state.deadCount(base)) : 0.0;
        });
        interpreter.setHandler("KillActor", [&state](CallContext& call) {
            // The first argument, when there is one, is the killer
            state.setDead(call.mReference, true, formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("AddToFaction", [&state](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            if (call.mReference != 0 && faction != 0)
                state.setFactionRank(call.mReference, faction, std::max(intArgument(call, 1), 0));
            return 0.0;
        });
        interpreter.setHandler("RemoveFromFaction", [&state](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            if (call.mReference != 0 && faction != 0)
                state.removeFromFaction(call.mReference, faction);
            return 0.0;
        });
        interpreter.setHandler("SetFactionRank", [&state](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            if (call.mReference != 0 && faction != 0)
                state.setFactionRank(call.mReference, faction, intArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("GetInFaction", [&state](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            return call.mReference != 0 && faction != 0 && state.factionRank(call.mReference, faction) >= 0 ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetFactionRank", [&state](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            return call.mReference != 0 && faction != 0
                ? static_cast<double>(state.factionRank(call.mReference, faction))
                : -1.0;
        });
    }
}
