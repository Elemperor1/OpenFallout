#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_QUESTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_QUESTS_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadscpt.hpp>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// The records the quests are made from
    class QuestRecords
    {
    public:
        virtual ~QuestRecords() = default;

        virtual const ESM4::Script* findScript(FormId id) const = 0;
        /// Calls the function for every quest, in any order
        virtual void forEachQuest(const std::function<void(const ESM4::Quest&)>& function) const = 0;
    };

    /// What the player has been told about one objective of a quest
    struct ObjectiveState
    {
        bool mDisplayed = false;
        bool mCompleted = false;
    };

    /// A line of the journal of a quest: the text of the log entry of the stage that was reached
    struct JournalEntry
    {
        int mStage = 0;
        std::string mText;
    };

    /// Everything that changes about a quest while the game runs
    struct QuestState
    {
        const ESM4::Quest* mRecord = nullptr;
        bool mRunning = false;
        bool mCompleted = false;
        bool mFailed = false;
        /// The stage that was set last, which is what GetStage tells
        int mStage = 0;
        std::vector<int> mStagesDone; // in the order they were reached
        std::map<int, ObjectiveState> mObjectives;
        std::vector<JournalEntry> mJournal;
        /// Seconds since the script of the quest last ran
        float mTimer = 0;
        /// The script of the quest and its variables, for a quest that has one
        std::shared_ptr<Instance> mInstance;
    };

    /// The quests of the loaded plugins and how far the player is in each: which stage was reached, what the objectives
    /// show, and the script of the quest that runs while it does. SetStage runs the scripts of the log entries of the
    /// stage at once, nested inside the script that set it.
    class QuestManager
    {
    public:
        /// Tells whether the conditions of a log entry are met. Without one, every condition is met.
        using ConditionCheck = std::function<bool(const std::vector<ESM4::TargetCondition>&)>;
        using JournalListener = std::function<void(FormId quest, const JournalEntry& entry)>;

        /// The longest nesting of SetStage inside SetStage that is run. The games run at least 250.
        static constexpr std::size_t maxNesting = 250;

        QuestManager(Interpreter& interpreter, const QuestRecords& records);

        void setConditionCheck(ConditionCheck check) { mConditions = std::move(check); }
        void setJournalListener(JournalListener listener) { mJournalListener = std::move(listener); }

        /// Reads the quests again: every quest starts from the beginning, and those that are enabled when the game
        /// starts are running.
        void reset();

        bool known(FormId quest) const { return mQuests.count(quest) != 0; }
        const QuestState* state(FormId quest) const;

        /// The script of the quest, which holds its variables (null when the quest has no script or is not known)
        Instance* instance(FormId quest);

        // The commands of scripts, which do nothing for a quest that is not known (and tell 0)

        /// Sets the stage: marks it done, runs the scripts of its log entries and starts the quest when it is not
        /// running. Returns false when the quest has no such stage, or has it done already and does not allow it twice.
        bool setStage(FormId quest, int stage);
        int stage(FormId quest) const;
        bool stageDone(FormId quest, int stage) const;

        bool running(FormId quest) const;
        void start(FormId quest);
        void stop(FormId quest);
        bool completed(FormId quest) const;
        void complete(FormId quest);

        void setObjectiveDisplayed(FormId quest, int objective, bool displayed);
        void setObjectiveCompleted(FormId quest, int objective, bool completed);
        bool objectiveDisplayed(FormId quest, int objective) const;
        bool objectiveCompleted(FormId quest, int objective) const;

        /// Lets time pass for the scripts of the quests that are running: each runs its GameMode block once every
        /// delay of the quest (every time when the quest has none).
        void update(float seconds);

    private:
        QuestState* find(FormId quest);
        const QuestState* find(FormId quest) const;

        std::shared_ptr<const Script> prepared(const ESM4::ScriptDefinition& definition);
        void makeInstance(FormId id, QuestState& quest);
        void runEntry(FormId quest, const ESM4::QuestLogEntry& entry);
        void log(const std::string& message);

        Interpreter& mInterpreter;
        const QuestRecords& mRecords;
        ConditionCheck mConditions;
        JournalListener mJournalListener;
        std::map<FormId, QuestState> mQuests;
        /// The scripts that were made ready, by the definition they were made from
        std::map<const ESM4::ScriptDefinition*, std::shared_ptr<const Script>> mPrepared;
        std::size_t mNesting = 0;
    };

    /// Gives the commands of quests to the interpreter: GetStage, GetStageDone, SetStage, GetQuestRunning,
    /// StartQuest, StopQuest, GetQuestCompleted, CompleteQuest and the commands of objectives. Those that the command
    /// table does not have are left out.
    void addQuestCommands(Interpreter& interpreter, QuestManager& quests);
}

#endif
