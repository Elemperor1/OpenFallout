#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DIALOGUE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DIALOGUE_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <components/esm4/loaddial.hpp>
#include <components/esm4/loadinfo.hpp>

#include "conditions.hpp"
#include "interpreter.hpp"
#include "quests.hpp"

namespace FalloutScript
{
    /// The records that dialogue is made of
    class DialogueRecords
    {
    public:
        virtual ~DialogueRecords() = default;

        /// Calls the function for every topic, in any order
        virtual void forEachTopic(const std::function<void(const ESM4::Dialogue&)>& function) const = 0;
        /// Calls the function for every response, in the order the files define them
        virtual void forEachInfo(const std::function<void(const ESM4::DialogInfo&)>& function) const = 0;
    };

    /// Who is talking to the player: the reference of the character, and the record it is made from (the one a
    /// response is for when it names a speaker)
    struct DialogueSpeaker
    {
        FormId mReference = 0;
        FormId mBase = 0;
    };

    /// One thing the speaker says
    struct DialogueLine
    {
        std::string mText;
        FormId mInfo = 0;
        std::uint8_t mNumber = 0; // the number in the name of the voice file
        std::uint32_t mEmotion = 0; // 0 neutral, 1 anger, 2 disgust, 3 fear, 4 sad, 5 happy, 6 surprise, 7 pained
        std::int32_t mEmotionValue = 0;
        FormId mSound = 0; // a sound record that replaces the voice file
        FormId mSpeakerAnimation = 0;
        FormId mListenerAnimation = 0;
    };

    /// What the player can pick next: a topic, the response it will get, and the words that stand for it in the list
    struct DialogueOption
    {
        FormId mTopic = 0;
        FormId mInfo = 0;
        std::string mText;
    };

    /// What the player is shown of a conversation
    struct DialogueView
    {
        FormId mSpeaker = 0;
        FormId mTopic = 0; // the topic of the response that was said, 0 for none
        FormId mInfo = 0; // the response that was said
        std::vector<DialogueLine> mLines;
        /// A response is being said: it is over, and its end script has run, when finish() is called
        bool mSpeaking = false;
        /// The conversation is over
        bool mEnded = false;
        /// The topics to pick from, which are there once the response is finished
        std::vector<DialogueOption> mOptions;
    };

    /// The rules of dialogue as the games have them, without the window: which topics the player can pick from, which
    /// response a topic gives, what is said and what a response does. The responses of a topic are tried in the order
    /// of their chain of previous responses (PNAM), the first whose quest runs, whose speaker is the one talking,
    /// that was not said (for those that are said once or once a day) and whose conditions hold is the one said; a run
    /// of responses flagged Random gives one of those that can be said. A response that is said runs its begin script
    /// at once; its end script runs when the response is finished, which also adds the topics it names to those the
    /// player has, and either ends the conversation (a response flagged Goodbye) or offers the topics it lists as
    /// choices, or else the topics at the top of the list and those the player has been given.
    ///
    /// Not done: the speech challenge is passed unless a check is set (setChallengeCheck), the flags for characters
    /// of low or high intelligence are not looked at, and a response that is run immediately waits for the player to
    /// pick it like any other.
    class DialogueManager
    {
    public:
        /// The editor id of the topic that starts every conversation
        static constexpr const char* greetingTopic = "Greeting";

        DialogueManager(Interpreter& interpreter, ConditionEvaluator& conditions, QuestManager& quests,
            const DialogueRecords& records);

        /// Reads the records again and forgets what was said, what the player was told and who talked to the player
        void reset();

        /// The number of the day in the game, for the responses that are said once a day
        void setDay(std::function<int()> day) { mDay = std::move(day); }
        /// A number below the count given to it, for choosing among random responses
        void setRandom(std::function<std::size_t(std::size_t count)> random) { mRandom = std::move(random); }
        /// Whether a speech challenge is passed. Without one every challenge is.
        void setChallengeCheck(std::function<bool(const ESM4::DialogInfo&)> check) { mChallenge = std::move(check); }

        // The state that the games keep

        /// Gives the player the topic: it is offered from now on, also when it is not at the top of the list
        void addTopic(FormId topic);
        bool hasTopic(FormId topic) const { return mTopics.count(topic) != 0; }
        /// Whether the character with the reference has talked to the player
        bool talkedToPlayer(FormId reference) const { return mTalkedTo.count(reference) != 0; }
        bool wasSaid(FormId info) const { return mSaid.count(info) != 0; }

        // Conversations

        /// Starts a conversation with the speaker: the greeting is said (when a response of the topic can be) and the
        /// conversation goes on from there. A topic that is given takes the place of the greeting. Returns the view of
        /// it. Does nothing when a conversation is open.
        const DialogueView& start(const DialogueSpeaker& speaker, FormId topic = 0);
        /// The player picks a topic of the options. Does nothing when no conversation is open, a response is being
        /// said or the topic is not among the options.
        const DialogueView& choose(FormId topic);
        /// The response that was being said is over: its end script runs, it gives the player the topics it names,
        /// and then the conversation ends (a response flagged Goodbye), or the first of its follow-ups that can be
        /// said is the response being said now, or the options are worked out. Does nothing when no response is being
        /// said.
        const DialogueView& finish();
        /// Ends the conversation, also when a response is being said (its end script then does not run)
        void end();

        bool active() const { return mActive; }
        const DialogueView& view() const { return mView; }
        const DialogueSpeaker& speaker() const { return mSpeaker; }

        /// A response of the topic is said by the speaker outside of a conversation, and finished at once (the Say
        /// command). Returns the lines, which are empty when no response can be said.
        DialogueView say(const DialogueSpeaker& speaker, FormId topic);

        // Looking

        /// The responses of a topic in the order they are tried, empty for a topic that is not known
        std::vector<const ESM4::DialogInfo*> responses(FormId topic) const;
        /// Whether the response can be said now by the speaker
        bool canSay(const ESM4::DialogInfo& info, const DialogueSpeaker& speaker);
        /// The response a topic gives the speaker now, or null when it gives none
        const ESM4::DialogInfo* responseFor(FormId topic, const DialogueSpeaker& speaker);
        /// The topic with the editor id, or 0
        FormId findTopic(const std::string& editorId) const;
        const ESM4::Dialogue* topic(FormId topic) const;

    private:
        struct TopicEntry
        {
            const ESM4::Dialogue* mRecord = nullptr;
            std::vector<const ESM4::DialogInfo*> mInfos; // in the order they are tried
        };

        static std::vector<const ESM4::DialogInfo*> chainOrder(const std::vector<const ESM4::DialogInfo*>& inFileOrder);

        std::vector<DialogueOption> options(const DialogueSpeaker& speaker, const std::vector<ESM::FormId>* choices);
        bool addOption(std::vector<DialogueOption>& options, FormId topic, const DialogueSpeaker& speaker);
        /// The view of the response being said, after its start script ran. The topic is the one it was picked under,
        /// or 0 for the topic of the record.
        DialogueView speak(const ESM4::DialogInfo& info, const DialogueSpeaker& speaker, FormId topic = 0);
        /// The response has been said to its end: it is not said again if it is for once (or for a day)
        void markSaid(const ESM4::DialogInfo& info);
        void runScript(const ESM4::ScriptDefinition& definition, const DialogueSpeaker& speaker);
        int day() const { return mDay ? mDay() : 0; }

        Interpreter& mInterpreter;
        ConditionEvaluator& mConditions;
        QuestManager& mQuests;
        const DialogueRecords& mRecords;
        std::function<int()> mDay;
        std::function<std::size_t(std::size_t)> mRandom;
        std::function<bool(const ESM4::DialogInfo&)> mChallenge;

        std::unordered_map<FormId, TopicEntry> mEntries;
        std::unordered_map<FormId, const ESM4::DialogInfo*> mInfoIndex;
        FormId mGreeting = 0;
        /// The topics at the top of the list, the highest priority first
        std::vector<FormId> mTopLevel;
        /// The topics the player was given, in the order they were
        std::vector<FormId> mGiven;
        std::set<FormId> mTopics; // the same, to look up
        std::set<FormId> mSaid; // the responses that were said
        std::map<FormId, int> mSaidOnDay;
        std::set<FormId> mTalkedTo;
        std::set<FormId> mChain; // the responses said one after the other since the player picked the topic
        std::map<const ESM4::ScriptDefinition*, std::shared_ptr<const Script>> mPrepared;

        bool mActive = false;
        DialogueSpeaker mSpeaker;
        DialogueView mView;
        const ESM4::DialogInfo* mSpeaking = nullptr;
    };

    /// Gives the interpreter the commands of dialogue that need nothing of the world but the manager: AddTopic,
    /// GetTalkedToPC and GetTalkedToPCParam, IsTalking, GetDialogueEmotion and GetDialogueEmotionValue. Those that the
    /// command table does not have are left out.
    void addDialogueCommands(Interpreter& interpreter, DialogueManager& dialogue);
}

#endif
