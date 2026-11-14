#include "spawncommands.hpp"

#include <algorithm>
#include <vector>

#include "actors.hpp"
#include "arguments.hpp"
#include "formlists.hpp"
#include "objectcommands.hpp"
#include "respawn.hpp"
#include "spawnrules.hpp"

namespace FalloutScript
{
    void ItemValues::clear()
    {
        const std::lock_guard lock(mMutex);
        mValues.clear();
    }

    void ItemValues::set(FormId item, int value)
    {
        if (item == 0)
            return;
        const std::lock_guard lock(mMutex);
        mValues[item] = value;
    }

    bool ItemValues::find(FormId item, int& value) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mValues.find(item);
        if (found == mValues.end())
            return false;
        value = found->second;
        return true;
    }

    std::size_t ItemValues::size() const
    {
        const std::lock_guard lock(mMutex);
        return mValues.size();
    }

    void ItemValues::write(SaveWriter& out) const
    {
        const std::lock_guard lock(mMutex);
        out.count(mValues.size());
        for (const auto& [item, value] : mValues)
        {
            out.form(item);
            out.i32(value);
        }
    }

    void ItemValues::read(SaveReader& in, std::uint32_t)
    {
        std::map<FormId, int> all;
        for (std::size_t i = 0, n = in.count(8); i < n; ++i)
        {
            const std::optional<FormId> item = in.form();
            const int value = in.i32();
            if (item)
                all[*item] = value;
        }
        const std::lock_guard lock(mMutex);
        mValues = std::move(all);
    }

    void NameOverrides::clear()
    {
        const std::lock_guard lock(mMutex);
        mMessages.clear();
    }

    void NameOverrides::set(FormId target, FormId message)
    {
        if (target == 0)
            return;
        const std::lock_guard lock(mMutex);
        mMessages[target] = message;
    }

    bool NameOverrides::find(FormId target, FormId& message) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mMessages.find(target);
        if (found == mMessages.end())
            return false;
        message = found->second;
        return true;
    }

    std::size_t NameOverrides::size() const
    {
        const std::lock_guard lock(mMutex);
        return mMessages.size();
    }

    void NameOverrides::write(SaveWriter& out) const
    {
        const std::lock_guard lock(mMutex);
        out.count(mMessages.size());
        for (const auto& [target, message] : mMessages)
        {
            out.form(target);
            out.form(message);
        }
    }

    void NameOverrides::read(SaveReader& in, std::uint32_t)
    {
        std::map<FormId, FormId> all;
        for (std::size_t i = 0, n = in.count(8); i < n; ++i)
        {
            const std::optional<FormId> target = in.form();
            const std::optional<FormId> message = in.form();
            // a name whose message is gone is dropped
            if (target && message)
                all[*target] = *message;
        }
        const std::lock_guard lock(mMutex);
        mMessages = std::move(all);
    }

    namespace
    {
        /// Takes the copies of the stacks of an item out of a holder and puts them into another (if there is one),
        /// keeping their condition and mods. Tells how many were moved, and (when asked) the stacks that were.
        int moveItem(ActorState& actors, FormId from, FormId to, FormId item, int count,
            std::vector<ActorState::ConditionStack>* stacks = nullptr)
        {
            int moved = 0;
            for (const ActorState::ConditionStack& stack : actors.conditions(from, item))
            {
                if (count <= 0)
                    break;
                const int wanted = std::min(stack.mCount, count);
                const int taken = actors.removeItem(from, item, wanted, stack.mCondition, stack.mMods);
                if (taken > 0 && to != 0)
                    actors.addItem(to, item, taken, stack.mCondition, stack.mMods);
                if (taken > 0 && stacks != nullptr)
                    stacks->push_back({ stack.mCondition, taken, stack.mMods });
                moved += taken;
                count -= taken;
            }
            return moved;
        }

        /// Puts the stacks that were taken out of a holder into the world, each in its own condition with its own mods; a
        /// stack that the world does not take is given back to the holder
        void dropStacks(ActorState& actors, SpawnWorld& world, FormId holder, FormId item,
            const std::vector<ActorState::ConditionStack>& stacks)
        {
            for (const ActorState::ConditionStack& stack : stacks)
                if (world.dropItem(holder, item, stack.mCount, stack.mCondition, stack.mMods) == 0)
                    actors.addItem(holder, item, stack.mCount, stack.mCondition, stack.mMods);
        }
    }

    void addSpawnCommands(Interpreter& interpreter, const SpawnParts& parts)
    {
        ActorState& actors = parts.mActors;
        FormLists& formLists = parts.mFormLists;
        ObjectWorld& objects = parts.mObjects;
        SpawnWorld& world = parts.mWorld;
        RespawnState& respawn = parts.mRespawn;
        ItemValues& itemValues = parts.mItemValues;
        NameOverrides& names = parts.mNames;

        // PlaceLeveledActorAtMe base, levelMod, zone: a character or creature at the place of the reference, whose
        // levelled lists are rolled for the level of the zone and the modifier (0x5D9810). The modifier is 4, which
        // changes nothing, when the call leaves it out.
        interpreter.setHandler("PlaceLeveledActorAtMe", [&world](CallContext& call) {
            const FormId base = formArgument(call, 0);
            if (call.mReference == 0 || base == 0)
                return 0.0;
            const int levelMod = call.mArguments.size() > 1 ? intArgument(call, 1) : noLevelMod;
            return static_cast<double>(world.placeLeveledActor(call.mReference, base, levelMod, formArgument(call, 2)));
        });

        // PlaceAtMeHealthPercent base, health, count, distance, direction: as PlaceAtMe, with the health that the
        // objects have (0x5D90A0). Nothing in the world has a condition yet, so the health is only checked.
        interpreter.setHandler("PlaceAtMeHealthPercent", [&objects](CallContext& call) {
            const FormId base = formArgument(call, 0);
            if (call.mReference == 0 || base == 0)
                return 0.0;
            const int count = call.mArguments.size() > 2 ? intArgument(call, 2) : 1;
            if (count <= 0)
                return 0.0;
            const double distance = argument(call, 3);
            const int direction = intArgument(call, 4);
            return static_cast<double>(objects.placeAtMe(call.mReference, base, count, distance, direction));
        });

        // The calling reference exists, and is not the reference that is named (0x5A4210)
        interpreter.setHandler("Exists", [&world](CallContext& call) {
            const FormId named = formArgument(call, 0);
            return call.mReference != 0 && call.mReference != named && world.exists(call.mReference) ? 1.0 : 0.0;
        });

        // The commands of the item whose script runs: one copy of it leaves the character or container that holds it,
        // to the reference that is named, to the world (DropMe), or out of the game (RemoveMe) (0x5B53D0, 0x5B5860)
        interpreter.setHandler("RemoveMe", [&actors, &world](CallContext& call) {
            const FormId item = call.mReference;
            const FormId holder = item != 0 ? world.holderOfItem(item) : 0;
            if (holder == 0)
                return 0.0;
            moveItem(actors, holder, formArgument(call, 0), item, 1);
            return 0.0;
        });
        interpreter.setHandler("DropMe", [&actors, &world](CallContext& call) {
            const FormId item = call.mReference;
            const FormId holder = item != 0 ? world.holderOfItem(item) : 0;
            if (holder == 0)
                return 0.0;
            std::vector<ActorState::ConditionStack> stacks;
            if (moveItem(actors, holder, 0, item, 1, &stacks) > 0)
                dropStacks(actors, world, holder, item, stacks);
            return 0.0;
        });

        // Drop item, count: the reference puts that many of the item into the world (0x5B58D0)
        interpreter.setHandler("Drop", [&actors, &world](CallContext& call) {
            const FormId item = formArgument(call, 0);
            const int count = intArgument(call, 1);
            if (call.mReference == 0 || item == 0 || count <= 0)
                return 0.0;
            std::vector<ActorState::ConditionStack> stacks;
            if (moveItem(actors, call.mReference, 0, item, count, &stacks) > 0)
                dropStacks(actors, world, call.mReference, item, stacks);
            return 0.0;
        });

        // RemoveAllTypedItems destination, retainOwnership, suppressMessage, formType, exceptions: every item of the
        // reference that is of that kind of form (any kind when the type is below 0 or left out) and not in the form
        // list of the exceptions goes to the destination, or out of the game when there is none (0x5B55A0, 0x4CE380).
        // Ownership and the message of the items added to the player are not kept here.
        interpreter.setHandler("RemoveAllTypedItems", [&actors, &formLists, &world](CallContext& call) {
            const FormId holder = call.mReference;
            if (holder == 0)
                return 0.0;
            const FormId destination = formArgument(call, 0);
            const int type = call.mArguments.size() > 3 ? intArgument(call, 3) : -1;
            const FormId exceptions = formArgument(call, 4);
            std::vector<std::pair<FormId, int>> items;
            if (!actors.items(holder, items))
                return 0.0;
            double moved = 0;
            for (const auto& [item, count] : items)
            {
                if (type >= 0 && world.formTypeOf(item) != type)
                    continue;
                if (exceptions != 0 && formLists.contains(exceptions, item))
                    continue;
                moved += moveItem(actors, holder, destination == holder ? 0 : destination, item, count);
            }
            return 0.0 * moved;
        });

        // The inventory of the reference is made new from its record: what it held is forgotten, and the levelled lists
        // in it are rolled again (0x5DAC60 asks the reference to rebuild)
        interpreter.setHandler("ResetInventory", [&actors, &respawn](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            respawn.resetInventory(call.mReference);
            actors.resetItems(call.mReference);
            return 0.0;
        });

        // SetZoneRespawns zone, value: a value of 0 sets the flag that the zone never resets, any other clears it
        // (0x5DAB90)
        interpreter.setHandler("SetZoneRespawns", [&respawn](CallContext& call) {
            const FormId zone = formArgument(call, 0);
            if (zone != 0)
                respawn.setZoneNeverResets(zone, intArgument(call, 1) == 0);
            return 0.0;
        });

        // ResetInterior cell: the cell resets when it is loaded next, however soon (0x5D24A0 sets its time of leaving to
        // -1)
        interpreter.setHandler("ResetInterior", [&respawn](CallContext& call) {
            const FormId cell = formArgument(call, 0);
            if (cell != 0)
                respawn.forceReset(cell);
            return 0.0;
        });

        interpreter.setHandler("PurgeCellBuffers", [&world](CallContext&) {
            world.purgeCellBuffers();
            return 0.0;
        });

        // GetInCellParam cell, reference: GetInCell for another reference than the calling one (0x59EF60)
        interpreter.setHandler("GetInCellParam", [&objects](CallContext& call) {
            const FormId cell = formArgument(call, 0);
            const FormId named = formArgument(call, 1);
            const FormId reference = named != 0 ? named : call.mReference;
            return reference != 0 && cell != 0 && objects.inCell(reference, cell) ? 1.0 : 0.0;
        });

        interpreter.setHandler("GetParentRef", [&world](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(world.parentOf(call.mReference)) : 0.0;
        });

        // The number of items that the reference holds (0x5E0100)
        interpreter.setHandler("GetContainerInventoryCount", [&actors](CallContext& call) {
            std::vector<std::pair<FormId, int>> items;
            if (call.mReference == 0 || !actors.items(call.mReference, items))
                return 0.0;
            double total = 0;
            for (const auto& [item, count] : items)
                total += count;
            return total;
        });

        // SetItemValue value: the item the reference stands for is worth that from now on
        interpreter.setHandler("SetItemValue", [&itemValues, &objects](CallContext& call) {
            FormId item = call.mReference;
            FormId base = 0;
            if (item != 0 && objects.baseFormOf(item, base) && base != 0)
                item = base;
            itemValues.set(item, intArgument(call, 0));
            return 0.0;
        });

        // The name of a character or creature (SetActorFullName message) or of a cell (SetCellFullName cell, message)
        // is the text of a message of the files (0x5D1920, 0x5D1890)
        interpreter.setHandler("SetActorFullName", [&names, &objects](CallContext& call) {
            const FormId message = formArgument(call, 0);
            if (call.mReference == 0 || message == 0)
                return 0.0;
            // The executable changes the name of the base record (0x5D1920: the full name of the form that is given is
            // copied into the base of the actor), so every reference of that record shows it
            FormId base = 0;
            names.set(objects.baseFormOf(call.mReference, base) && base != 0 ? base : call.mReference, message);
            return 0.0;
        });
        interpreter.setHandler("SetCellFullName", [&names](CallContext& call) {
            const FormId cell = formArgument(call, 0);
            const FormId message = formArgument(call, 1);
            if (cell != 0 && message != 0)
                names.set(cell, message);
            return 0.0;
        });
    }
}
