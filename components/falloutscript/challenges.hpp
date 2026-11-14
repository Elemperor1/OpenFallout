#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CHALLENGES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CHALLENGES_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "conditions.hpp"
#include "interpreter.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    /// What a challenge counts (the type of the record, in the order of the executable's fourteen lists)
    namespace ChallengeType
    {
        /// Kills of a character that is in a list (SNAM), maybe with weapons of a list (XNAM)
        constexpr int killFromList = 0;
        constexpr int killSpecific = 1;
        /// Any kill; the second value of the record is the kind of creature plus one (0 for any)
        constexpr int killAny = 2;
        constexpr int hit = 3;
        constexpr int discoverMarker = 4;
        constexpr int useItem = 5;
        constexpr int acquireItem = 6;
        constexpr int useSkill = 7;
        constexpr int damage = 8;
        constexpr int useItemFromList = 9;
        constexpr int acquireItemFromList = 10;
        /// A counter of the Pip-Boy; the first value of the record is the counter
        constexpr int miscStat = 11;
        constexpr int craft = 12;
        /// Counted by a script (IncrementScriptedChallenge)
        constexpr int scripted = 13;
        constexpr int count = 14;
    }

    /// The flags of the record
    namespace ChallengeFlag
    {
        /// The challenge counts only once a script has unlocked it
        constexpr int startDisabled = 1;
        /// Completing it starts it over, until a script takes the recurring away
        constexpr int recurring = 2;
        constexpr int showZeroProgress = 4;
    }

    /// The counter of the Pip-Boy that counts the challenges that were completed (the executable raises it at the end of
    /// each completion, FalloutNV.exe 0x5F5C8F, unless the challenge counts that very counter)
    constexpr int challengesCompletedStat = 27;

    /// What the record of a challenge says
    struct ChallengeRecord
    {
        FormId mId = 0;
        std::string mName;
        std::string mDescription;
        std::string mIcon;
        int mType = 0;
        int mThreshold = 0;
        int mFlags = 0;
        /// A message tells the progress each time it is a multiple of this (0: of 100)
        int mInterval = 0;
        /// The three values the type decides: a counter, a skill or a kind of creature, a kind of weapon
        int mFirst = 0;
        int mSecond = 0;
        int mThird = 0;
        /// The forms the type decides (a list, or one form) and the list of the weapons or places
        FormId mSnam = 0;
        FormId mXnam = 0;
        /// The script that runs when the challenge is completed (its ScriptEffectStart block, on the player)
        FormId mScript = 0;
    };

    /// What happened in the game that a challenge may count. The meaning of the forms and numbers is the type's (the
    /// executable's function at 0x5F5950 takes the same seven arguments).
    struct ChallengeEvent
    {
        int mType = 0;
        int mAmount = 1;
        FormId mFormA = 0;
        FormId mFormB = 0;
        int mFirst = 0;
        int mSecond = 0;
        int mThird = 0;
    };

    class ChallengeRecords
    {
    public:
        virtual ~ChallengeRecords() = default;

        virtual void forEachChallenge(const std::function<void(const ChallengeRecord&)>& function) = 0;
        /// Whether the form is in the list (false for a form that is no list)
        virtual bool inList(FormId list, FormId form) = 0;
    };

    /// How far the player is in a challenge
    struct ChallengeState
    {
        int mProgress = 0;
        /// A script unlocked it
        bool mUnlocked = false;
        bool mCompleted = false;
        /// It has been completed once and runs again (a recurring challenge)
        bool mRepeated = false;
        /// A script took the recurring away: the next completion is the last
        bool mRecurringRemoved = false;
    };

    /// What the player is told: the progress of a challenge, or that it is completed
    struct ChallengeNotice
    {
        FormId mChallenge = 0;
        std::string mText;
        std::string mIcon;
        bool mCompleted = false;
    };

    /// The challenges of New Vegas: what the player does is counted by the challenges that count it, and a challenge
    /// that reaches its threshold runs its script, counts as completed and tells the player (FalloutNV.exe
    /// 0x5F5950, the function that every counter calls, and 0x5F6110 for the scripted ones). A challenge is on a list
    /// of its type while it can be counted: one that is completed (a recurring one is completed only by the completion after
    /// its recurring was taken away), or that starts disabled and has not been unlocked, is on none. It can be used from
    /// any thread: whether a challenge counts is asked again when the count is made, and a completion is taken before its
    /// script, counter and notice run (with the lock free), so that a count of theirs or of another thread comes too late
    /// to complete it again.
    class Challenges
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit Challenges(ChallengeRecords& records);

        /// Runs the block that starts the script of a completed challenge, for the player
        void setScriptRunner(std::function<void(FormId script)> runner);
        /// Tells the player
        void setNotice(std::function<void(const ChallengeNotice&)> notice);
        /// Raises the counter of the Pip-Boy that counts the challenges completed (one more)
        void setCompletedCounter(std::function<void(int stat, int amount)> counter);
        /// The setting of the player's game that lets the notices come (bShowChallengeUpdates, on unless the player turned it
        /// off: 0x11CB4D4). Without it nothing is told, the counting goes on.
        void setShowUpdates(bool show);

        /// Reads the records again and starts every challenge from the beginning, as for a new game
        void reset();

        /// Counts what happened for the challenges of its type
        void event(const ChallengeEvent& event);
        /// IncrementScriptedChallenge: one more for the scripted challenge, whatever the record says it counts
        void incrementScripted(FormId challenge);
        /// UnlockChallenge
        void unlock(FormId challenge);
        /// RemoveRecurringFromChallenge
        void removeRecurring(FormId challenge, bool removed = true);

        bool known(FormId challenge) const;
        /// The bit UnlockChallenge sets, which ChallengeLocked answers (0x5A60D0, the flag word of the challenge at +0x70)
        bool unlocked(FormId challenge) const;
        /// What the condition function GetChallengeCompleted answers: 2 (the bit the executable keeps, 0x5A6150) for a
        /// completed challenge, else 0
        int completedBit(FormId challenge) const;
        bool completed(FormId challenge) const;
        /// A recurring challenge that has been completed once and started over (the bit at 0x5DEF70)
        bool repeated(FormId challenge) const;
        int progress(FormId challenge) const;

        /// A challenge as the Pip-Boy lists it
        struct View
        {
            ChallengeRecord mRecord;
            ChallengeState mState;
        };
        /// The challenges the player can see: the ones that count now or are done, in the order of their form ids
        std::vector<View> list() const;
        /// The challenges that count now, of a type (for a test)
        std::vector<FormId> active(int type) const;

    private:
        struct Entry
        {
            ChallengeRecord mRecord;
            ChallengeState mState;
        };

        bool counts(const Entry& entry) const;
        bool matches(const Entry& entry, const ChallengeEvent& event);
        /// Counts the amount for the challenge, tells the player and completes it when it is there; true when it was
        /// completed for good
        bool count(FormId id, int amount);
        void rebuild();

        ChallengeRecords& mRecords;
        mutable std::mutex mMutex;
        std::function<void(FormId)> mScriptRunner;
        std::function<void(const ChallengeNotice&)> mNotice;
        std::function<void(int, int)> mCompletedCounter;
        bool mShowUpdates = true;
        std::map<FormId, Entry> mEntries;
        /// The challenges that count, for each type (the executable's lists, 0x11CB3E0)
        std::vector<FormId> mLists[ChallengeType::count];
    };

    constexpr std::uint32_t challengesSectionVersion = 1;
    void writeChallenges(Challenges& challenges, SaveWriter& out);
    void readChallenges(Challenges& challenges, SaveReader& in, std::uint32_t version);

    /// Gives the commands IncrementScriptedChallenge, UnlockChallenge, ChallengeLocked, GetChallengeCompleted and
    /// RemoveRecurringFromChallenge to the interpreter. Those that the command table does not have are left out.
    void addChallengeCommands(Interpreter& interpreter, Challenges& challenges);
    /// The conditions GetChallengeCompleted and ChallengeLocked answer with the executable's own numbers (the condition
    /// function of GetChallengeCompleted tells 2 for a completed challenge, where the script command tells 1)
    void addChallengeConditionFunctions(ConditionEvaluator& evaluator, Challenges& challenges);
}

#endif
