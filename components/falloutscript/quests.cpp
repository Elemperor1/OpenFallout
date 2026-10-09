#include "quests.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        const ESM4::QuestStage* findStage(const ESM4::Quest& quest, int index)
        {
            for (const ESM4::QuestStage& stage : quest.mStages)
                if (stage.mIndex == index)
                    return &stage;
            return nullptr;
        }
    }

    QuestManager::QuestManager(Interpreter& interpreter, const QuestRecords& records)
        : mInterpreter(interpreter)
        , mRecords(records)
    {
    }

    void QuestManager::reset()
    {
        mQuests.clear();
        mPrepared.clear();
        mNesting = 0;
        mRecords.forEachQuest([this](const ESM4::Quest& record) {
            const FormId id = record.mId.toUint32();
            QuestState& quest = mQuests[id];
            quest = QuestState();
            quest.mRecord = &record;
            quest.mRunning = (record.mData.flags & ESM4::Quest::Flag_StartGameEnabled) != 0;
            makeInstance(id, quest);
        });
    }

    const QuestState* QuestManager::state(FormId quest) const
    {
        return find(quest);
    }

    Instance* QuestManager::instance(FormId quest)
    {
        QuestState* state = find(quest);
        return state != nullptr ? state->mInstance.get() : nullptr;
    }

    bool QuestManager::setStage(FormId id, int index)
    {
        QuestState* quest = find(id);
        if (quest == nullptr)
            return false;
        const ESM4::QuestStage* stage = findStage(*quest->mRecord, index);
        if (stage == nullptr)
            return false;

        const bool done = stageDone(id, index);
        if (done && (quest->mRecord->mData.flags & ESM4::Quest::Flag_AllowRepeatStages) == 0)
            return false;
        if (mNesting >= maxNesting)
        {
            log("SetStage: the stage " + std::to_string(index) + " of the quest " + hex(id)
                + " is too deep inside other stages, it is not run");
            return false;
        }

        // Whatever ends the stage, the depth is given back
        struct Nesting
        {
            std::size_t& mDepth;
            explicit Nesting(std::size_t& depth)
                : mDepth(depth)
            {
                ++mDepth;
            }
            ~Nesting() { --mDepth; }
            Nesting(const Nesting&) = delete;
            Nesting& operator=(const Nesting&) = delete;
        } nesting(mNesting);
        quest->mStage = index;
        if (!done)
            quest->mStagesDone.push_back(index);
        if (!quest->mCompleted)
            quest->mRunning = true;

        // Every entry whose conditions are met has its script run, and the first of them tells the player. How the
        // games treat a stage with several entries is not known, but the conditions of such entries exclude each other
        // (one for each sex of the player and the like) in the cases looked at.
        bool told = false;
        for (const ESM4::QuestLogEntry& entry : stage->mLogEntries)
        {
            if (mConditions && !mConditions(entry.mTargetConditions))
                continue;
            if (!told && !entry.mText.empty())
            {
                told = true;
                quest->mJournal.push_back({ index, entry.mText });
                if (mJournalListener)
                    mJournalListener(id, quest->mJournal.back());
            }
            if ((entry.mFlags & ESM4::QuestLogEntry::Flag_CompleteQuest) != 0)
            {
                quest->mCompleted = true;
                quest->mRunning = false;
            }
            if ((entry.mFlags & ESM4::QuestLogEntry::Flag_FailQuest) != 0)
            {
                quest->mFailed = true;
                quest->mRunning = false;
            }
            runEntry(id, entry);
            if (!entry.mNextQuest.isZeroOrUnset())
                start(entry.mNextQuest.toUint32());
        }
        return true;
    }

    int QuestManager::stage(FormId quest) const
    {
        const QuestState* state = find(quest);
        return state != nullptr ? state->mStage : 0;
    }

    bool QuestManager::stageDone(FormId quest, int stage) const
    {
        const QuestState* state = find(quest);
        return state != nullptr
            && std::find(state->mStagesDone.begin(), state->mStagesDone.end(), stage) != state->mStagesDone.end();
    }

    bool QuestManager::running(FormId quest) const
    {
        const QuestState* state = find(quest);
        return state != nullptr && state->mRunning;
    }

    void QuestManager::start(FormId id)
    {
        QuestState* quest = find(id);
        if (quest != nullptr && !quest->mCompleted)
            quest->mRunning = true;
    }

    void QuestManager::stop(FormId id)
    {
        if (QuestState* quest = find(id))
            quest->mRunning = false;
    }

    bool QuestManager::completed(FormId quest) const
    {
        const QuestState* state = find(quest);
        return state != nullptr && state->mCompleted;
    }

    void QuestManager::complete(FormId id)
    {
        if (QuestState* quest = find(id))
        {
            quest->mCompleted = true;
            quest->mRunning = false;
        }
    }

    void QuestManager::setObjectiveDisplayed(FormId id, int objective, bool displayed)
    {
        if (QuestState* quest = find(id))
            quest->mObjectives[objective].mDisplayed = displayed;
    }

    void QuestManager::setObjectiveCompleted(FormId id, int objective, bool completed)
    {
        if (QuestState* quest = find(id))
            quest->mObjectives[objective].mCompleted = completed;
    }

    bool QuestManager::objectiveDisplayed(FormId id, int objective) const
    {
        const QuestState* quest = find(id);
        if (quest == nullptr)
            return false;
        const auto it = quest->mObjectives.find(objective);
        return it != quest->mObjectives.end() && it->second.mDisplayed;
    }

    bool QuestManager::objectiveCompleted(FormId id, int objective) const
    {
        const QuestState* quest = find(id);
        if (quest == nullptr)
            return false;
        const auto it = quest->mObjectives.find(objective);
        return it != quest->mObjectives.end() && it->second.mCompleted;
    }

    void QuestManager::update(float seconds)
    {
        for (auto& [id, quest] : mQuests)
        {
            if (!quest.mRunning || !quest.mInstance)
                continue;
            quest.mTimer += seconds;
            const float delay = quest.mRecord->mData.questDelay;
            if (delay > 0 && quest.mTimer < delay)
                continue;
            quest.mTimer = 0;
            mInterpreter.run(*quest.mInstance, BlockType::GameMode);
        }
    }

    QuestState* QuestManager::find(FormId quest)
    {
        const auto it = mQuests.find(quest);
        return it != mQuests.end() ? &it->second : nullptr;
    }

    const QuestState* QuestManager::find(FormId quest) const
    {
        const auto it = mQuests.find(quest);
        return it != mQuests.end() ? &it->second : nullptr;
    }

    std::shared_ptr<const Script> QuestManager::prepared(const ESM4::ScriptDefinition& definition)
    {
        auto& script = mPrepared[&definition];
        if (!script)
            script = std::make_shared<const Script>(Script::prepare(definition, mInterpreter.commands()));
        return script;
    }

    void QuestManager::makeInstance(FormId id, QuestState& quest)
    {
        if (quest.mRecord->mQuestScript.isZeroOrUnset())
            return;
        const ESM4::Script* record = mRecords.findScript(quest.mRecord->mQuestScript.toUint32());
        if (record == nullptr)
            return;
        std::shared_ptr<const Script> script = prepared(record->mScript);
        if (!script->usable())
        {
            log("The script of the quest " + hex(id) + " cannot be run");
            return;
        }
        quest.mInstance = std::make_shared<Instance>(std::move(script), id, true);
    }

    void QuestManager::runEntry(FormId quest, const ESM4::QuestLogEntry& entry)
    {
        if (entry.mScript.compiledScript.empty())
            return;
        std::shared_ptr<const Script> script = prepared(entry.mScript);
        if (!script->usable())
        {
            log("A script of the quest " + hex(quest) + " cannot be run");
            return;
        }
        Instance instance(std::move(script), quest, true);
        mInterpreter.runResult(instance);
    }

    void QuestManager::log(const std::string& message)
    {
        mInterpreter.host().log(message);
    }

    void addQuestCommands(Interpreter& interpreter, QuestManager& quests)
    {
        interpreter.setHandler("GetStage",
            [&quests](CallContext& call) { return static_cast<double>(quests.stage(formArgument(call, 0))); });
        interpreter.setHandler("GetStageDone", [&quests](CallContext& call) {
            return quests.stageDone(formArgument(call, 0), intArgument(call, 1)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetStage", [&quests](CallContext& call) {
            quests.setStage(formArgument(call, 0), intArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("GetQuestRunning",
            [&quests](CallContext& call) { return quests.running(formArgument(call, 0)) ? 1.0 : 0.0; });
        interpreter.setHandler("StartQuest", [&quests](CallContext& call) {
            quests.start(formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("StopQuest", [&quests](CallContext& call) {
            quests.stop(formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("GetQuestCompleted",
            [&quests](CallContext& call) { return quests.completed(formArgument(call, 0)) ? 1.0 : 0.0; });
        interpreter.setHandler("CompleteQuest", [&quests](CallContext& call) {
            quests.complete(formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("SetObjectiveDisplayed", [&quests](CallContext& call) {
            quests.setObjectiveDisplayed(formArgument(call, 0), intArgument(call, 1), argument(call, 2) != 0);
            return 0.0;
        });
        interpreter.setHandler("SetObjectiveCompleted", [&quests](CallContext& call) {
            quests.setObjectiveCompleted(formArgument(call, 0), intArgument(call, 1), argument(call, 2) != 0);
            return 0.0;
        });
        interpreter.setHandler("GetObjectiveDisplayed", [&quests](CallContext& call) {
            return quests.objectiveDisplayed(formArgument(call, 0), intArgument(call, 1)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetObjectiveCompleted", [&quests](CallContext& call) {
            return quests.objectiveCompleted(formArgument(call, 0), intArgument(call, 1)) ? 1.0 : 0.0;
        });
    }
}
