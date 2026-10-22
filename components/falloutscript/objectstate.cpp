#include "objectstate.hpp"

#include <algorithm>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        void writeSet(SaveWriter& out, const std::set<FormId>& forms)
        {
            out.count(forms.size());
            for (const FormId form : forms)
                out.form(form);
        }

        std::set<FormId> readSet(SaveReader& in)
        {
            std::set<FormId> forms;
            const std::size_t count = in.count(4);
            for (std::size_t i = 0; i < count; ++i)
                // What a plugin that is gone marked is left out
                if (const std::optional<FormId> form = in.form())
                    forms.insert(*form);
            return forms;
        }

        void writeFlags(SaveWriter& out, const std::map<FormId, bool>& flags)
        {
            out.count(flags.size());
            for (const auto& [form, flag] : flags)
            {
                out.form(form);
                out.boolean(flag);
            }
        }

        std::map<FormId, bool> readFlags(SaveReader& in)
        {
            std::map<FormId, bool> flags;
            const std::size_t count = in.count(5);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> form = in.form();
                const bool flag = in.boolean();
                if (form)
                    flags[*form] = flag;
            }
            return flags;
        }

        void writeOwners(SaveWriter& out, const std::map<FormId, FormId>& owners)
        {
            out.count(owners.size());
            for (const auto& [form, owner] : owners)
            {
                out.form(form);
                out.form(owner);
            }
        }

        std::map<FormId, FormId> readOwners(SaveReader& in)
        {
            std::map<FormId, FormId> owners;
            const std::size_t count = in.count(8);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> form = in.form();
                const std::optional<FormId> owner = in.form();
                if (form && owner)
                    owners[*form] = *owner;
            }
            return owners;
        }
    }

    ObjectState::ObjectState(ObjectStateRecords& records)
        : mRecords(records)
    {
    }

    void ObjectState::clear()
    {
        const std::lock_guard lock(mMutex);
        mOwners.clear();
        mCellOwners.clear();
        mCellPublic.clear();
        mIgnoreCrime.clear();
        mGhosts.clear();
        mQuestObjects.clear();
        mTeammates.clear();
        mEnemyFactions.clear();
        mCombatStyles.clear();
    }

    FormId ObjectState::owner(FormId reference)
    {
        {
            const std::lock_guard lock(mMutex);
            const auto found = mOwners.find(reference);
            if (found != mOwners.end())
                return found->second;
        }
        return mRecords.recordOwner(reference);
    }

    void ObjectState::setOwner(FormId reference, FormId owner)
    {
        const FormId form = owner != 0 ? owner : mRecords.playerBase();
        const std::lock_guard lock(mMutex);
        mOwners[reference] = form;
    }

    bool ObjectState::isOwner(FormId reference, FormId candidate)
    {
        const FormId has = owner(reference);
        return has != 0 && has == candidate;
    }

    FormId ObjectState::cellOwner(FormId cell)
    {
        {
            const std::lock_guard lock(mMutex);
            const auto found = mCellOwners.find(cell);
            if (found != mCellOwners.end())
                return found->second;
        }
        return mRecords.recordCellOwner(cell);
    }

    void ObjectState::setCellOwner(FormId cell, FormId owner)
    {
        const FormId form = owner != 0 ? owner : mRecords.playerBase();
        const std::lock_guard lock(mMutex);
        mCellOwners[cell] = form;
    }

    bool ObjectState::cellPublic(FormId cell)
    {
        {
            const std::lock_guard lock(mMutex);
            const auto found = mCellPublic.find(cell);
            if (found != mCellPublic.end())
                return found->second;
        }
        return mRecords.recordCellPublic(cell);
    }

    void ObjectState::setCellPublic(FormId cell, bool isPublic)
    {
        const std::lock_guard lock(mMutex);
        mCellPublic[cell] = isPublic;
    }

    bool ObjectState::ignoresCrime(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        return mIgnoreCrime.count(actor) != 0;
    }

    void ObjectState::setIgnoresCrime(FormId actor, bool ignores)
    {
        const std::lock_guard lock(mMutex);
        if (ignores)
            mIgnoreCrime.insert(actor);
        else
            mIgnoreCrime.erase(actor);
    }

    bool ObjectState::ghost(FormId actor)
    {
        {
            const std::lock_guard lock(mMutex);
            const auto found = mGhosts.find(actor);
            if (found != mGhosts.end())
                return found->second;
        }
        return mRecords.recordGhost(actor);
    }

    void ObjectState::setGhost(FormId actor, bool isGhost)
    {
        // A character that the record makes a ghost stays one until a script says it is not
        const std::lock_guard lock(mMutex);
        mGhosts[actor] = isGhost;
    }

    bool ObjectState::questObject(FormId base)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mQuestObjects.find(base);
        return found != mQuestObjects.end() && found->second;
    }

    void ObjectState::setQuestObject(FormId base, bool marked)
    {
        const std::lock_guard lock(mMutex);
        mQuestObjects[base] = marked;
    }

    bool ObjectState::teammate(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        return mTeammates.count(actor) != 0;
    }

    void ObjectState::setTeammate(FormId actor, bool isTeammate)
    {
        const std::lock_guard lock(mMutex);
        if (isTeammate)
            mTeammates.insert(actor);
        else
            mTeammates.erase(actor);
    }

    std::vector<FormId> ObjectState::teammates()
    {
        const std::lock_guard lock(mMutex);
        return std::vector<FormId>(mTeammates.begin(), mTeammates.end());
    }

    bool ObjectState::enemyOfFaction(FormId faction)
    {
        const std::lock_guard lock(mMutex);
        return mEnemyFactions.count(faction) != 0;
    }

    void ObjectState::setEnemyOfFaction(FormId faction, bool enemy)
    {
        const std::lock_guard lock(mMutex);
        if (enemy)
            mEnemyFactions.insert(faction);
        else
            mEnemyFactions.erase(faction);
    }

    FormId ObjectState::combatStyle(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mCombatStyles.find(actor);
        return found != mCombatStyles.end() ? found->second : 0;
    }

    void ObjectState::setCombatStyle(FormId actor, FormId style)
    {
        const std::lock_guard lock(mMutex);
        if (style != 0)
            mCombatStyles[actor] = style;
        else
            mCombatStyles.erase(actor);
    }

    template <>
    struct SaveAccess<ObjectState>
    {
        static void write(ObjectState& state, SaveWriter& out)
        {
            const std::lock_guard lock(state.mMutex);
            writeOwners(out, state.mOwners);
            writeOwners(out, state.mCellOwners);
            writeFlags(out, state.mCellPublic);
            writeSet(out, state.mIgnoreCrime);
            writeFlags(out, state.mGhosts);
            writeFlags(out, state.mQuestObjects);
            writeSet(out, state.mTeammates);
            writeSet(out, state.mEnemyFactions);
            writeOwners(out, state.mCombatStyles);
        }

        static void read(ObjectState& state, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, FormId> owners = readOwners(in);
            std::map<FormId, FormId> cellOwners = readOwners(in);
            std::map<FormId, bool> cellPublic = readFlags(in);
            std::set<FormId> ignoreCrime = readSet(in);
            std::map<FormId, bool> ghosts = readFlags(in);
            std::map<FormId, bool> questObjects = readFlags(in);
            std::set<FormId> teammates = readSet(in);
            std::set<FormId> enemyFactions = readSet(in);
            std::map<FormId, FormId> combatStyles = readOwners(in);
            const std::lock_guard lock(state.mMutex);
            state.mOwners = std::move(owners);
            state.mCellOwners = std::move(cellOwners);
            state.mCellPublic = std::move(cellPublic);
            state.mIgnoreCrime = std::move(ignoreCrime);
            state.mGhosts = std::move(ghosts);
            state.mQuestObjects = std::move(questObjects);
            state.mTeammates = std::move(teammates);
            state.mEnemyFactions = std::move(enemyFactions);
            state.mCombatStyles = std::move(combatStyles);
        }
    };

    void writeObjectState(ObjectState& state, SaveWriter& out)
    {
        SaveAccess<ObjectState>::write(state, out);
    }

    void readObjectState(ObjectState& state, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<ObjectState>::read(state, in, version);
    }

    void addObjectStateCommands(Interpreter& interpreter, ObjectState& state, ObjectStateRecords& records)
    {
        // The owner a script names, or the player when it names none
        const auto ownerArgument = [&records](const CallContext& call, std::size_t index) {
            return call.mArguments.size() > index ? records.ownerForm(formArgument(call, index)) : records.playerBase();
        };
        interpreter.setHandler("SetOwnership", [&state, ownerArgument](CallContext& call) {
            // The command cannot take an owner away: without one the player becomes the owner
            if (call.mReference != 0)
                state.setOwner(call.mReference, ownerArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("IsOwner", [&state, ownerArgument](CallContext& call) {
            return call.mReference != 0 && state.isOwner(call.mReference, ownerArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetCellOwnership", [&state, ownerArgument](CallContext& call) {
            if (const FormId cell = formArgument(call, 0); cell != 0)
                state.setCellOwner(cell, ownerArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("SetCellPublicFlag", [&state](CallContext& call) {
            if (const FormId cell = formArgument(call, 0); cell != 0)
                state.setCellPublic(cell, argument(call, 1) > 0);
            return 0.0;
        });
        // The marks of a character are those of characters only
        interpreter.setHandler("IgnoreCrime", [&state, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                state.setIgnoresCrime(call.mReference, argument(call, 0) > 0);
            return 0.0;
        });
        interpreter.setHandler("GetIgnoreCrime", [&state](CallContext& call) {
            return call.mReference != 0 && state.ignoresCrime(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetGhost", [&state, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                state.setGhost(call.mReference, argument(call, 0) > 0);
            return 0.0;
        });
        interpreter.setHandler("GetIsGhost",
            [&state](CallContext& call) { return call.mReference != 0 && state.ghost(call.mReference) ? 1.0 : 0.0; });
        // The mark is on the form of the thing and not on a reference of it
        interpreter.setHandler("SetQuestObject", [&state](CallContext& call) {
            if (const FormId base = formArgument(call, 0); base != 0)
                state.setQuestObject(base, argument(call, 1) > 0);
            return 0.0;
        });
        // The player can not be their own teammate
        interpreter.setHandler("SetPlayerTeammate", [&state, &records](CallContext& call) {
            if (call.mReference != 0 && call.mReference != records.player() && records.isActor(call.mReference))
                state.setTeammate(call.mReference, argument(call, 0) > 0);
            return 0.0;
        });
        interpreter.setHandler("GetPlayerTeammate", [&state](CallContext& call) {
            return call.mReference != 0 && state.teammate(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetPCEnemyofFaction", [&state](CallContext& call) {
            if (const FormId faction = formArgument(call, 0); faction != 0)
                state.setEnemyOfFaction(faction, argument(call, 1) > 0);
            return 0.0;
        });
        interpreter.setHandler("GetPCEnemyofFaction", [&state](CallContext& call) {
            const FormId faction = formArgument(call, 0);
            return faction != 0 && state.enemyOfFaction(faction) ? 1.0 : 0.0;
        });
        // Without a style the character goes back to the one of its record
        interpreter.setHandler("SetCombatStyle", [&state, &records](CallContext& call) {
            if (call.mReference != 0 && records.isActor(call.mReference))
                state.setCombatStyle(call.mReference, formArgument(call, 0));
            return 0.0;
        });
        // The answers of the executables when a character has no cause of death, a thing no stage of destruction and a
        // dead character nobody fed on
        interpreter.setHandler("GetCauseofDeath", [](CallContext&) { return -1.0; });
        interpreter.setHandler("GetDestructionStage", [](CallContext&) { return -1.0; });
        interpreter.setHandler("HasBeenEaten", [](CallContext&) { return 0.0; });
    }
}
