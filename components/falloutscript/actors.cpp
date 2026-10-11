#include "actors.hpp"

#include <algorithm>
#include <limits>

#include "arguments.hpp"

namespace FalloutScript
{
    ActorState::ActorState(ActorRecords& records)
        : mRecords(records)
    {
    }

    void ActorState::clear()
    {
        const std::lock_guard lock(mMutex);
        mHolders.clear();
        mDead.clear();
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
                held = static_cast<int>(
                    std::min<std::int64_t>(std::int64_t(held) + count, std::numeric_limits<int>::max()));
            }
        }
    }

    void ActorState::knowFactions(FormId id, Holder& entry)
    {
        if (entry.mFactionsKnown)
            return;
        std::vector<std::pair<FormId, int>> factions;
        if (!mRecords.initialFactions(id, factions))
            return;
        entry.mFactionsKnown = true;
        // What a script did before the records could be asked stays: it is the later word
        for (const auto& [faction, rank] : factions)
            if (faction != 0 && rank >= 0)
                entry.mFactions.emplace(faction, rank);
    }

    int ActorState::itemCount(FormId id, FormId item)
    {
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(id);
        knowItems(id, entry);
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
        int& held = entry.mItems[item];
        held = static_cast<int>(std::min<std::int64_t>(std::int64_t(held) + count, std::numeric_limits<int>::max()));
    }

    int ActorState::removeItem(FormId id, FormId item, int count)
    {
        const std::lock_guard lock(mMutex);
        if (item == 0 || count <= 0)
            return 0;
        Holder& entry = holder(id);
        knowItems(id, entry);
        const auto found = entry.mItems.find(item);
        if (found == entry.mItems.end())
            return 0;
        const int removed = std::min(count, found->second);
        found->second -= removed;
        if (found->second <= 0)
            entry.mItems.erase(found);
        return removed;
    }

    bool ActorState::isDead(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return mDead.count(actor) != 0;
    }

    void ActorState::setDead(FormId actor, bool dead)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        if (dead)
            mDead.insert(actor);
        else
            mDead.erase(actor);
    }

    int ActorState::factionRank(FormId actor, FormId faction)
    {
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(actor);
        knowFactions(actor, entry);
        const auto found = entry.mFactions.find(faction);
        return found == entry.mFactions.end() ? -1 : found->second;
    }

    void ActorState::setFactionRank(FormId actor, FormId faction, int rank)
    {
        if (faction == 0)
            return;
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(actor);
        knowFactions(actor, entry);
        // A rank below 0 is the way to say that the actor is not in the faction
        if (rank < 0)
            entry.mFactions.erase(faction);
        else
            entry.mFactions[faction] = rank;
    }

    void ActorState::removeFromFaction(FormId actor, FormId faction)
    {
        const std::lock_guard lock(mMutex);
        Holder& entry = holder(actor);
        knowFactions(actor, entry);
        entry.mFactions.erase(faction);
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
        interpreter.setHandler("GetDead",
            [&state](CallContext& call) { return call.mReference != 0 && state.isDead(call.mReference) ? 1.0 : 0.0; });
        interpreter.setHandler("KillActor", [&state](CallContext& call) {
            state.setDead(call.mReference, true);
            return 0.0;
        });
        interpreter.setHandler("ResurrectActor", [&state](CallContext& call) {
            state.setDead(call.mReference, false);
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
