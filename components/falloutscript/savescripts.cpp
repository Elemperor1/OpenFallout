#include "savescripts.hpp"

#include <algorithm>

#include "objects.hpp"
#include "quests.hpp"

namespace FalloutScript
{
    namespace
    {
        /// The variables of a script as a save holds them: the kind and the value of each from the first
        struct SavedVariables
        {
            std::vector<VariableKind> mKinds;
            std::vector<double> mValues;
        };

        void writeVariables(const Instance& instance, SaveWriter& out)
        {
            const std::vector<Variable>& variables = instance.script().variables();
            const std::size_t count = variables.empty() ? 0 : variables.size() - 1;
            out.count(count);
            for (std::size_t i = 1; i <= count; ++i)
            {
                const double value = instance.variable(static_cast<std::uint32_t>(i));
                out.u8(static_cast<std::uint8_t>(variables[i].mKind));
                // A variable that holds a form is told as one, so that a save can follow it to a changed load order
                if (variables[i].mKind == VariableKind::Reference)
                    out.form(static_cast<FormId>(value));
                else
                    out.f64(value);
            }
        }

        void writeSaved(const std::vector<VariableKind>& kinds, const std::vector<double>& values, SaveWriter& out)
        {
            out.count(values.size());
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                out.u8(static_cast<std::uint8_t>(kinds[i]));
                if (kinds[i] == VariableKind::Reference)
                    out.form(static_cast<FormId>(values[i]));
                else
                    out.f64(values[i]);
            }
        }

        SavedVariables readVariables(SaveReader& in)
        {
            SavedVariables result;
            // A kind and the four bytes of a form at least
            const std::size_t count = in.count(5);
            result.mKinds.reserve(count);
            result.mValues.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::uint8_t kind = in.u8();
                if (kind > static_cast<std::uint8_t>(VariableKind::Reference))
                    throw SaveError("A variable of a script has a kind that this version does not know");
                result.mKinds.push_back(static_cast<VariableKind>(kind));
                if (result.mKinds.back() == VariableKind::Reference)
                    result.mValues.push_back(in.form().value_or(0));
                else
                    result.mValues.push_back(in.f64());
            }
            return result;
        }

        /// Gives the instance the variables, unless its script has others (a script that changed since the save)
        void applyVariables(Instance& instance, const SavedVariables& saved)
        {
            const std::vector<Variable>& variables = instance.script().variables();
            if (saved.mValues.size() + 1 != variables.size())
                return;
            for (std::size_t i = 0; i < saved.mKinds.size(); ++i)
                if (variables[i + 1].mKind != saved.mKinds[i])
                    return;
            for (std::size_t i = 0; i < saved.mValues.size(); ++i)
                instance.setVariable(static_cast<std::uint32_t>(i + 1), saved.mValues[i]);
        }
    }

    template <>
    struct SaveAccess<QuestManager>
    {
        static void write(const QuestManager& quests, SaveWriter& out)
        {
            out.count(quests.mQuests.size());
            for (const auto& [id, quest] : quests.mQuests)
            {
                out.form(id);
                out.boolean(quest.mRunning);
                out.boolean(quest.mCompleted);
                out.boolean(quest.mFailed);
                out.i32(quest.mStage);
                out.count(quest.mStagesDone.size());
                for (const int stage : quest.mStagesDone)
                    out.i32(stage);
                out.count(quest.mObjectives.size());
                for (const auto& [index, objective] : quest.mObjectives)
                {
                    out.i32(index);
                    out.boolean(objective.mDisplayed);
                    out.boolean(objective.mCompleted);
                }
                out.count(quest.mJournal.size());
                for (const JournalEntry& entry : quest.mJournal)
                {
                    out.i32(entry.mStage);
                    out.string(entry.mText);
                }
                out.f32(quest.mTimer);
                out.boolean(quest.mDelay.has_value());
                out.f32(quest.mDelay.value_or(0));
                out.boolean(quest.mInstance != nullptr);
                if (quest.mInstance)
                    writeVariables(*quest.mInstance, out);
            }
        }

        static void read(QuestManager& quests, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(4);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> id = in.form();
                QuestState state;
                state.mRunning = in.boolean();
                state.mCompleted = in.boolean();
                state.mFailed = in.boolean();
                state.mStage = in.i32();
                const std::size_t done = in.count(4);
                for (std::size_t j = 0; j < done; ++j)
                    state.mStagesDone.push_back(in.i32());
                const std::size_t objectives = in.count(6);
                for (std::size_t j = 0; j < objectives; ++j)
                {
                    const int index = in.i32();
                    ObjectiveState& objective = state.mObjectives[index];
                    objective.mDisplayed = in.boolean();
                    objective.mCompleted = in.boolean();
                }
                const std::size_t entries = in.count(8);
                for (std::size_t j = 0; j < entries; ++j)
                {
                    JournalEntry& entry = state.mJournal.emplace_back();
                    entry.mStage = in.i32();
                    entry.mText = in.string();
                }
                state.mTimer = in.f32();
                const bool hasDelay = in.boolean();
                const float delay = in.f32();
                if (hasDelay)
                    state.mDelay = delay;
                SavedVariables variables;
                const bool hasInstance = in.boolean();
                if (hasInstance)
                    variables = readVariables(in);

                // A quest that the records do not have any more is left out
                const auto found = id ? quests.mQuests.find(*id) : quests.mQuests.end();
                if (found == quests.mQuests.end())
                    continue;
                QuestState& quest = found->second;
                quest.mRunning = state.mRunning;
                quest.mCompleted = state.mCompleted;
                quest.mFailed = state.mFailed;
                quest.mStage = state.mStage;
                quest.mStagesDone = std::move(state.mStagesDone);
                quest.mObjectives = std::move(state.mObjectives);
                quest.mJournal = std::move(state.mJournal);
                quest.mTimer = state.mTimer;
                quest.mDelay = state.mDelay;
                if (hasInstance && quest.mInstance)
                    applyVariables(*quest.mInstance, variables);
            }
        }
    };

    template <>
    struct SaveAccess<ObjectScripts>
    {
        static void write(const ObjectScripts& objects, SaveWriter& out)
        {
            // The scripts that have an instance, and those that a save gave variables to and no cell has asked for yet
            std::map<FormId, bool> references;
            for (const auto& [reference, object] : objects.mObjects)
                if (object.mInstance)
                    references[reference] = true;
            for (const auto& [reference, saved] : objects.mSaved)
                references.emplace(reference, false);
            out.count(references.size());
            for (const auto& [reference, live] : references)
            {
                out.form(reference);
                if (live)
                {
                    const ObjectScripts::Object& object = objects.mObjects.at(reference);
                    out.form(object.mScript);
                    writeVariables(*object.mInstance, out);
                }
                else
                {
                    const ObjectScripts::SavedInstance& saved = objects.mSaved.at(reference);
                    out.form(saved.mScript);
                    writeSaved(saved.mKinds, saved.mVariables, out);
                }
            }
        }

        static void read(ObjectScripts& objects, SaveReader& in, std::uint32_t)
        {
            const std::size_t count = in.count(12);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> reference = in.form();
                const std::optional<FormId> script = in.form();
                SavedVariables variables = readVariables(in);
                // An object or a script from a plugin that is gone has nothing to keep them for
                if (!reference || !script)
                    continue;
                if (const auto found = objects.mObjects.find(*reference);
                    found != objects.mObjects.end() && found->second.mInstance && found->second.mScript == *script)
                {
                    applyVariables(*found->second.mInstance, variables);
                    continue;
                }
                ObjectScripts::SavedInstance& saved = objects.mSaved[*reference];
                saved.mScript = *script;
                saved.mKinds = std::move(variables.mKinds);
                saved.mVariables = std::move(variables.mValues);
            }
        }
    };

    void writeQuests(const QuestManager& quests, SaveWriter& out)
    {
        SaveAccess<QuestManager>::write(quests, out);
    }

    void readQuests(QuestManager& quests, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<QuestManager>::read(quests, in, version);
    }

    void writeObjectScripts(const ObjectScripts& objects, SaveWriter& out)
    {
        SaveAccess<ObjectScripts>::write(objects, out);
    }

    void readObjectScripts(ObjectScripts& objects, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<ObjectScripts>::read(objects, in, version);
    }
}
