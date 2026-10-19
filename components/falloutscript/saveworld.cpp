#include "saveworld.hpp"

#include <algorithm>
#include <mutex>
#include <vector>

#include "animationcommands.hpp"
#include "dialogue.hpp"
#include "triggervolumes.hpp"

namespace FalloutScript
{
    template <>
    struct SaveAccess<DialogueManager>
    {
        static void write(DialogueManager& dialogue, SaveWriter& out)
        {
            out.count(dialogue.mGiven.size());
            for (const FormId topic : dialogue.mGiven)
                out.form(topic);
            out.count(dialogue.mSaid.size());
            for (const auto& [speaker, info] : dialogue.mSaid)
            {
                out.form(speaker);
                out.form(info);
            }
            out.count(dialogue.mSaidAt.size());
            for (const auto& [key, hour] : dialogue.mSaidAt)
            {
                out.form(key.first);
                out.form(key.second);
                out.f64(hour);
            }
            out.count(dialogue.mTalkedTo.size());
            for (const FormId reference : dialogue.mTalkedTo)
                out.form(reference);
        }

        static void read(DialogueManager& dialogue, SaveReader& in, std::uint32_t)
        {
            std::vector<FormId> given;
            const std::size_t topics = in.count(4);
            for (std::size_t i = 0; i < topics; ++i)
                if (const std::optional<FormId> topic = in.form())
                    given.push_back(*topic);
            std::set<std::pair<FormId, FormId>> said;
            const std::size_t saidCount = in.count(8);
            for (std::size_t i = 0; i < saidCount; ++i)
            {
                const std::optional<FormId> speaker = in.form();
                const std::optional<FormId> info = in.form();
                if (speaker && info)
                    said.emplace(*speaker, *info);
            }
            std::map<std::pair<FormId, FormId>, double> saidAt;
            const std::size_t atCount = in.count(16);
            for (std::size_t i = 0; i < atCount; ++i)
            {
                const std::optional<FormId> speaker = in.form();
                const std::optional<FormId> info = in.form();
                const double hour = in.f64();
                if (speaker && info)
                    saidAt[{ *speaker, *info }] = hour;
            }
            std::set<FormId> talkedTo;
            const std::size_t talked = in.count(4);
            for (std::size_t i = 0; i < talked; ++i)
                if (const std::optional<FormId> reference = in.form())
                    talkedTo.insert(*reference);

            dialogue.mGiven = std::move(given);
            dialogue.mTopics.clear();
            dialogue.mTopics.insert(dialogue.mGiven.begin(), dialogue.mGiven.end());
            dialogue.mSaid = std::move(said);
            dialogue.mSaidAt = std::move(saidAt);
            dialogue.mTalkedTo = std::move(talkedTo);
            dialogue.mChain.clear();
        }
    };

    template <>
    struct SaveAccess<TriggerVolumes>
    {
        static void write(
            const TriggerVolumes& triggers, const std::map<FormId, SavedTrigger>& waiting, SaveWriter& out)
        {
            // What a trigger holds now, and what a loaded game said of the triggers whose volumes are not in yet
            std::map<FormId, SavedTrigger> all = waiting;
            for (const auto& [trigger, state] : triggers.mTriggers)
            {
                if (state.mOccupants.empty() && state.mQueue.empty())
                    continue;
                SavedTrigger saved;
                saved.mOccupants = state.mOccupants;
                for (const TriggerVolumes::Event& event : state.mQueue)
                    saved.mQueue.emplace_back(event.mBlockType, event.mActor);
                all[trigger] = std::move(saved);
            }
            out.count(all.size());
            for (const auto& [trigger, saved] : all)
            {
                out.form(trigger);
                out.count(saved.mOccupants.size());
                for (const FormId actor : saved.mOccupants)
                    out.form(actor);
                out.count(saved.mQueue.size());
                for (const auto& [blockType, actor] : saved.mQueue)
                {
                    out.u16(blockType);
                    out.form(actor);
                }
            }
        }

        static std::map<FormId, SavedTrigger> read(SaveReader& in)
        {
            std::map<FormId, SavedTrigger> result;
            const std::size_t count = in.count(12);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> trigger = in.form();
                SavedTrigger saved;
                const std::size_t size = in.count(4);
                for (std::size_t j = 0; j < size; ++j)
                    if (const std::optional<FormId> actor = in.form())
                        saved.mOccupants.insert(*actor);
                const std::size_t queued = in.count(6);
                for (std::size_t j = 0; j < queued; ++j)
                {
                    const std::uint16_t blockType = in.u16();
                    const std::optional<FormId> actor = in.form();
                    if (actor && (blockType == BlockType::OnTriggerEnter || blockType == BlockType::OnTriggerLeave))
                        saved.mQueue.emplace_back(blockType, *actor);
                }
                if (trigger && (!saved.mOccupants.empty() || !saved.mQueue.empty()))
                    result[*trigger] = std::move(saved);
            }
            return result;
        }

        static void restore(TriggerVolumes& triggers, FormId trigger, const SavedTrigger& saved)
        {
            const auto found = triggers.mTriggers.find(trigger);
            if (found == triggers.mTriggers.end())
                return;
            found->second.mOccupants = saved.mOccupants;
            found->second.mQueue.clear();
            for (const auto& [blockType, actor] : saved.mQueue)
                found->second.mQueue.push_back(TriggerVolumes::Event{ blockType, actor });
        }
    };

    template <>
    struct SaveAccess<OpenStates>
    {
        static void write(OpenStates& states, SaveWriter& out)
        {
            const std::lock_guard lock(states.mMutex);
            std::vector<std::pair<FormId, bool>> all(states.mOpen.begin(), states.mOpen.end());
            std::sort(all.begin(), all.end());
            out.count(all.size());
            for (const auto& [reference, open] : all)
            {
                out.form(reference);
                out.boolean(open);
            }
        }

        static void read(OpenStates& states, SaveReader& in, std::uint32_t)
        {
            std::vector<std::pair<FormId, bool>> all;
            const std::size_t count = in.count(5);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> reference = in.form();
                const bool open = in.boolean();
                if (reference)
                    all.emplace_back(*reference, open);
            }
            const std::lock_guard lock(states.mMutex);
            states.mOpen.clear();
            for (const auto& [reference, open] : all)
                states.mOpen[reference] = open;
        }
    };

    void writeDialogue(DialogueManager& dialogue, SaveWriter& out)
    {
        SaveAccess<DialogueManager>::write(dialogue, out);
    }

    void readDialogue(DialogueManager& dialogue, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<DialogueManager>::read(dialogue, in, version);
    }

    void writeTriggers(TriggerVolumes& triggers, const std::map<FormId, SavedTrigger>& waiting, SaveWriter& out)
    {
        SaveAccess<TriggerVolumes>::write(triggers, waiting, out);
    }

    std::map<FormId, SavedTrigger> readTriggers(SaveReader& in, std::uint32_t)
    {
        return SaveAccess<TriggerVolumes>::read(in);
    }

    void restoreTrigger(TriggerVolumes& triggers, FormId trigger, const SavedTrigger& saved)
    {
        SaveAccess<TriggerVolumes>::restore(triggers, trigger, saved);
    }

    void writeOpenStates(OpenStates& states, SaveWriter& out)
    {
        SaveAccess<OpenStates>::write(states, out);
    }

    void readOpenStates(OpenStates& states, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<OpenStates>::read(states, in, version);
    }
}
