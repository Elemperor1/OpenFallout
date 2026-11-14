#include "levelledlists.hpp"

#include <algorithm>

#include "arguments.hpp"

namespace FalloutScript
{
    void LevelledLists::clear()
    {
        const std::lock_guard lock(mMutex);
        mAdded.clear();
    }

    void LevelledLists::add(FormId list, const LevelledAddition& entry)
    {
        if (list == 0 || entry.mForm == 0)
            return;
        const std::lock_guard lock(mMutex);
        mAdded[list].push_back(entry);
    }

    std::vector<LevelledAddition> LevelledLists::additions(FormId list) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mAdded.find(list);
        return found == mAdded.end() ? std::vector<LevelledAddition>() : found->second;
    }

    std::size_t LevelledLists::listCount() const
    {
        const std::lock_guard lock(mMutex);
        return mAdded.size();
    }

    bool LevelledLists::merge(FormId list, const std::vector<ESM4::LVLO>& entries, std::vector<ESM4::LVLO>& merged) const
    {
        std::vector<LevelledAddition> added;
        {
            const std::lock_guard lock(mMutex);
            const auto found = mAdded.find(list);
            if (found == mAdded.end() || found->second.empty())
                return false;
            added = found->second;
        }
        merged = entries;
        for (const LevelledAddition& entry : added)
        {
            ESM4::LVLO add{};
            add.level = static_cast<std::int16_t>(entry.mLevel);
            add.item = entry.mForm;
            add.count = static_cast<std::int16_t>(entry.mCount);
            ESM4::insertLevelledEntry(merged, add);
        }
        return true;
    }

    void LevelledLists::write(SaveWriter& out) const
    {
        const std::lock_guard lock(mMutex);
        out.count(mAdded.size());
        for (const auto& [list, entries] : mAdded)
        {
            out.form(list);
            out.count(entries.size());
            for (const LevelledAddition& entry : entries)
            {
                out.form(entry.mForm);
                out.u16(entry.mLevel);
                out.u16(entry.mCount);
                out.f32(entry.mHealth);
            }
        }
    }

    void LevelledLists::read(SaveReader& in, std::uint32_t)
    {
        std::map<FormId, std::vector<LevelledAddition>> all;
        const std::size_t lists = in.count(8);
        for (std::size_t i = 0; i < lists; ++i)
        {
            const std::optional<FormId> list = in.form();
            std::vector<LevelledAddition> entries;
            const std::size_t size = in.count(12);
            for (std::size_t j = 0; j < size; ++j)
            {
                LevelledAddition entry;
                const std::optional<FormId> form = in.form();
                entry.mLevel = in.u16();
                entry.mCount = in.u16();
                entry.mHealth = in.f32();
                // an entry whose plugin is gone is dropped
                if (form)
                {
                    entry.mForm = *form;
                    entries.push_back(entry);
                }
            }
            if (list && !entries.empty())
                all[*list] = std::move(entries);
        }
        const std::lock_guard lock(mMutex);
        mAdded = std::move(all);
    }

    namespace
    {
        std::uint16_t sixteenBits(int value)
        {
            return static_cast<std::uint16_t>(value);
        }
    }

    void addLevelledListCommands(Interpreter& interpreter, LevelledLists& lists, LevelledListRecords& records)
    {
        using Kind = LevelledListRecords::Kind;

        // AddItemToLeveledList list, item, level, count, health: nothing is added for a count of 0 (0x5DE700)
        interpreter.setHandler("AddItemToLeveledList", [&lists, &records](CallContext& call) {
            const FormId list = formArgument(call, 0);
            const FormId item = formArgument(call, 1);
            LevelledAddition entry;
            entry.mForm = item;
            entry.mLevel = sixteenBits(intArgument(call, 2));
            entry.mCount = sixteenBits(intArgument(call, 3));
            // the health is given in percent and kept as that
            entry.mHealth = call.mArguments.size() > 4 ? static_cast<float>(argument(call, 4)) : 100.f;
            if (list != 0 && item != 0 && entry.mCount != 0 && records.kindOf(list) == Kind::Item
                && records.isEntryOf(Kind::Item, item))
                lists.add(list, entry);
            return 0.0;
        });
        // AddNPCToLeveledList list, character, level; AddCreatureToLeveledList list, creature, level: one of them
        // (0x5DE670)
        const auto addActor = [&lists, &records](CallContext& call, Kind kind) {
            const FormId list = formArgument(call, 0);
            const FormId actor = formArgument(call, 1);
            LevelledAddition entry;
            entry.mForm = actor;
            entry.mLevel = sixteenBits(intArgument(call, 2));
            entry.mCount = 1;
            if (list != 0 && actor != 0 && records.kindOf(list) == kind && records.isEntryOf(kind, actor))
                lists.add(list, entry);
            return 0.0;
        };
        interpreter.setHandler("AddNPCToLeveledList",
            [addActor](CallContext& call) { return addActor(call, Kind::Character); });
        interpreter.setHandler("AddCreatureToLeveledList",
            [addActor](CallContext& call) { return addActor(call, Kind::Creature); });
    }
}
