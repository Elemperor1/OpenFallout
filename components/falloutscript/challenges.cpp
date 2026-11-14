#include "challenges.hpp"

#include <algorithm>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        /// The messages that tell the progress come at each multiple of this when the record has no interval of its own
        constexpr int defaultInterval = 100;

        /// The words of a notice: the name, the progress and the threshold, and the description under them (the format
        /// the executable gives its message function, 0x1048A9C)
        std::string noticeText(const ChallengeRecord& record, int progress)
        {
            return record.mName + "   " + std::to_string(progress) + "\\" + std::to_string(record.mThreshold) + "\n"
                + record.mDescription;
        }
    }

    Challenges::Challenges(ChallengeRecords& records)
        : mRecords(records)
    {
    }

    void Challenges::setScriptRunner(std::function<void(FormId)> runner)
    {
        const std::lock_guard lock(mMutex);
        mScriptRunner = std::move(runner);
    }

    void Challenges::setNotice(std::function<void(const ChallengeNotice&)> notice)
    {
        const std::lock_guard lock(mMutex);
        mNotice = std::move(notice);
    }

    void Challenges::setCompletedCounter(std::function<void(int, int)> counter)
    {
        const std::lock_guard lock(mMutex);
        mCompletedCounter = std::move(counter);
    }

    void Challenges::setShowUpdates(bool show)
    {
        const std::lock_guard lock(mMutex);
        mShowUpdates = show;
    }

    void Challenges::reset()
    {
        std::map<FormId, Entry> entries;
        mRecords.forEachChallenge([&entries](const ChallengeRecord& record) {
            Entry& entry = entries[record.mId];
            entry.mRecord = record;
            entry.mState = ChallengeState();
        });
        const std::lock_guard lock(mMutex);
        mEntries = std::move(entries);
        rebuild();
    }

    bool Challenges::counts(const Entry& entry) const
    {
        // A challenge that is over does not count (a recurring one that can go on is never over: its last completion is the
        // one after its recurring was taken away); one that starts disabled counts once unlocked
        if (entry.mState.mCompleted)
            return false;
        if ((entry.mRecord.mFlags & ChallengeFlag::startDisabled) != 0 && !entry.mState.mUnlocked)
            return false;
        return true;
    }

    void Challenges::rebuild()
    {
        // The lists are made again from all the challenges (0x5F5880: those that are not completed, and those that do
        // not start disabled or were unlocked)
        for (auto& list : mLists)
            list.clear();
        for (const auto& [id, entry] : mEntries)
        {
            if (entry.mState.mCompleted)
                continue;
            if ((entry.mRecord.mFlags & ChallengeFlag::startDisabled) != 0 && !entry.mState.mUnlocked)
                continue;
            const int type = entry.mRecord.mType;
            if (type >= 0 && type < ChallengeType::count)
                mLists[type].push_back(id);
        }
    }

    bool Challenges::matches(const Entry& entry, const ChallengeEvent& event)
    {
        const ChallengeRecord& record = entry.mRecord;
        if (record.mSnam != 0)
        {
            // The form of the record is the one the event is about (or a list that has it): the types that count any
            // kill and the counters ask nothing of it
            if (event.mFormA == 0)
                return false;
            switch (record.mType)
            {
                case ChallengeType::killFromList:
                case ChallengeType::useItemFromList:
                case ChallengeType::acquireItemFromList:
                    if (!mRecords.inList(record.mSnam, event.mFormA))
                        return false;
                    break;
                case ChallengeType::killAny:
                case ChallengeType::miscStat:
                case ChallengeType::scripted:
                    break;
                default:
                    if (event.mFormA != record.mSnam)
                        return false;
                    break;
            }
        }
        // The second form (the weapon, or the place): a record that names one wants an event that has one, and for the
        // kills, the hits, the places found and the damage it is a list the form has to be in (0x5F5AB9)
        if (record.mXnam != 0)
        {
            if (event.mFormB == 0)
                return false;
            if ((record.mType <= ChallengeType::discoverMarker || record.mType == ChallengeType::damage)
                && !mRecords.inList(record.mXnam, event.mFormB))
                return false;
        }
        // The first value: a counter, or the part of the body that was hit
        if (record.mFirst != 0)
        {
            if (event.mFirst == 0 || event.mFirst != record.mFirst)
                return false;
        }
        else if (record.mType == ChallengeType::miscStat && event.mFirst != record.mFirst)
            return false;
        // The second value: a skill, or the kind of creature; a hit counts the ordinary blow and the one that cripples
        // (0 and 1) when the record names none
        if (record.mSecond != 0)
        {
            if (event.mSecond == 0 || event.mSecond != record.mSecond)
                return false;
        }
        else if (record.mType == ChallengeType::hit && event.mSecond != 0 && event.mSecond != 1)
            return false;
        // The third: the kind of weapon
        if (record.mThird != 0)
        {
            if (event.mThird == 0 || event.mThird != record.mThird)
                return false;
        }
        return true;
    }

    void Challenges::event(const ChallengeEvent& event)
    {
        if (event.mType < 0 || event.mType >= ChallengeType::count)
            return;
        std::vector<FormId> ids;
        {
            const std::lock_guard lock(mMutex);
            ids = mLists[event.mType];
        }
        bool done = false;
        for (const FormId id : ids)
        {
            {
                // What an earlier challenge's script did to the others (an unlock, a completion) is seen by the next
                const std::lock_guard lock(mMutex);
                const auto found = mEntries.find(id);
                if (found == mEntries.end() || !counts(found->second))
                    continue;
                if (!matches(found->second, event))
                    continue;
            }
            done = count(id, event.mAmount) || done;
        }
        if (done)
        {
            const std::lock_guard lock(mMutex);
            rebuild();
        }
    }

    void Challenges::incrementScripted(FormId challenge)
    {
        std::vector<FormId> ids;
        {
            const std::lock_guard lock(mMutex);
            ids = mLists[ChallengeType::scripted];
        }
        bool done = false;
        for (const FormId id : ids)
        {
            if (id != challenge)
                continue;
            {
                const std::lock_guard lock(mMutex);
                const auto found = mEntries.find(id);
                if (found == mEntries.end() || !counts(found->second))
                    continue;
            }
            done = count(id, 1) || done;
        }
        if (done)
        {
            const std::lock_guard lock(mMutex);
            rebuild();
        }
    }

    bool Challenges::count(FormId id, int amount)
    {
        ChallengeRecord record;
        int progress = 0;
        bool reached = false;
        bool last = false;
        std::function<void(FormId)> runner;
        std::function<void(const ChallengeNotice&)> notice;
        std::function<void(int, int)> counter;
        {
            const std::lock_guard lock(mMutex);
            Entry& entry = mEntries.at(id);
            // Whether it counts is asked again here, with the count: another thread, or the script of a completion that
            // came meanwhile, may have completed it since the caller looked
            if (!counts(entry))
                return false;
            entry.mState.mProgress += amount;
            record = entry.mRecord;
            progress = entry.mState.mProgress;
            runner = mScriptRunner;
            notice = mNotice;
            counter = mCompletedCounter;
            // The player is told only when the setting lets it, and the record has a name and a description (0x5F5CA4)
            if (!mShowUpdates)
                notice = nullptr;
            // The completion is taken here, before the callbacks run (they run with the lock free and may count again): a
            // recurring challenge starts over, with what went beyond the threshold, and any other is over
            if (progress >= record.mThreshold)
            {
                reached = true;
                if ((record.mFlags & ChallengeFlag::recurring) != 0 && !entry.mState.mRecurringRemoved)
                {
                    entry.mState.mProgress -= record.mThreshold;
                    entry.mState.mRepeated = true;
                }
                else
                {
                    entry.mState.mCompleted = true;
                    last = true;
                }
            }
        }
        const bool named = !record.mName.empty() && !record.mDescription.empty();
        if (!reached)
        {
            // The progress is told when it is a multiple of the interval (0 counts), and never before the first count
            const int step = record.mInterval != 0 ? record.mInterval : defaultInterval;
            if (progress >= 0 && progress % step == 0 && named && notice)
                notice({ id, noticeText(record, progress), record.mIcon, false });
            return false;
        }

        // Completed: its script runs, the counter of completions goes up (not for the challenge that counts that
        // counter: it would count itself), and the player is told
        if (record.mScript != 0 && runner)
            runner(record.mScript);
        if (!(record.mType == ChallengeType::miscStat && record.mFirst == challengesCompletedStat) && counter)
            counter(challengesCompletedStat, 1);
        if (named && notice)
            notice({ id, noticeText(record, record.mThreshold), record.mIcon, true });
        return last;
    }

    void Challenges::unlock(FormId challenge)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(challenge);
        if (found == mEntries.end())
            return;
        found->second.mState.mUnlocked = true;
        // It goes on the list of its type if it is not there (even one that is completed: it does not count while it is)
        const int type = found->second.mRecord.mType;
        if (type >= 0 && type < ChallengeType::count
            && std::find(mLists[type].begin(), mLists[type].end(), challenge) == mLists[type].end())
            mLists[type].push_back(challenge);
    }

    void Challenges::removeRecurring(FormId challenge, bool removed)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(challenge);
        if (found != mEntries.end())
            found->second.mState.mRecurringRemoved = removed;
    }

    bool Challenges::known(FormId challenge) const
    {
        const std::lock_guard lock(mMutex);
        return mEntries.count(challenge) != 0;
    }

    bool Challenges::unlocked(FormId challenge) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(challenge);
        return found != mEntries.end() && found->second.mState.mUnlocked;
    }

    int Challenges::completedBit(FormId challenge) const
    {
        return completed(challenge) ? 2 : 0;
    }

    bool Challenges::completed(FormId challenge) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(challenge);
        return found != mEntries.end() && found->second.mState.mCompleted;
    }

    bool Challenges::repeated(FormId challenge) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(challenge);
        return found != mEntries.end() && found->second.mState.mRepeated;
    }

    int Challenges::progress(FormId challenge) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(challenge);
        return found != mEntries.end() ? found->second.mState.mProgress : 0;
    }

    std::vector<Challenges::View> Challenges::list() const
    {
        const std::lock_guard lock(mMutex);
        std::vector<View> result;
        for (const auto& [id, entry] : mEntries)
        {
            // Those the player cannot count yet are not listed (one that starts disabled and was not unlocked)
            const bool locked
                = (entry.mRecord.mFlags & ChallengeFlag::startDisabled) != 0 && !entry.mState.mUnlocked;
            if (!locked)
                result.push_back({ entry.mRecord, entry.mState });
        }
        return result;
    }

    std::vector<FormId> Challenges::active(int type) const
    {
        const std::lock_guard lock(mMutex);
        return type >= 0 && type < ChallengeType::count ? mLists[type] : std::vector<FormId>();
    }

    template <>
    struct SaveAccess<Challenges>
    {
        static void write(Challenges& challenges, SaveWriter& out)
        {
            const std::lock_guard lock(challenges.mMutex);
            // Only what has changed from the start is kept; the rest is what the records say
            std::vector<const Challenges::Entry*> kept;
            for (const auto& [id, entry] : challenges.mEntries)
            {
                const ChallengeState& state = entry.mState;
                if (state.mProgress != 0 || state.mUnlocked || state.mCompleted || state.mRepeated
                    || state.mRecurringRemoved)
                    kept.push_back(&entry);
            }
            out.count(kept.size());
            for (const Challenges::Entry* entry : kept)
            {
                const ChallengeState& state = entry->mState;
                out.form(entry->mRecord.mId);
                out.i32(state.mProgress);
                out.u8(static_cast<std::uint8_t>((state.mUnlocked ? 1 : 0) | (state.mCompleted ? 2 : 0)
                    | (state.mRepeated ? 4 : 0) | (state.mRecurringRemoved ? 8 : 0)));
            }
        }

        static void read(Challenges& challenges, SaveReader& in, std::uint32_t)
        {
            struct Saved
            {
                FormId mId;
                ChallengeState mState;
            };
            std::vector<Saved> saved;
            const std::size_t count = in.count(9);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> id = in.form();
                Saved entry;
                entry.mState.mProgress = in.i32();
                const std::uint8_t bits = in.u8();
                entry.mState.mUnlocked = (bits & 1) != 0;
                entry.mState.mCompleted = (bits & 2) != 0;
                entry.mState.mRepeated = (bits & 4) != 0;
                entry.mState.mRecurringRemoved = (bits & 8) != 0;
                // What comes from a plugin that is gone is left out
                if (id)
                {
                    entry.mId = *id;
                    saved.push_back(entry);
                }
            }
            const std::lock_guard lock(challenges.mMutex);
            for (auto& [id, entry] : challenges.mEntries)
                entry.mState = ChallengeState();
            for (const Saved& entry : saved)
            {
                const auto found = challenges.mEntries.find(entry.mId);
                if (found != challenges.mEntries.end())
                    found->second.mState = entry.mState;
            }
            challenges.rebuild();
        }
    };

    void writeChallenges(Challenges& challenges, SaveWriter& out)
    {
        SaveAccess<Challenges>::write(challenges, out);
    }

    void readChallenges(Challenges& challenges, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Challenges>::read(challenges, in, version);
    }

    void addChallengeCommands(Interpreter& interpreter, Challenges& challenges)
    {
        interpreter.setHandler("IncrementScriptedChallenge", [&challenges](CallContext& call) {
            challenges.incrementScripted(formArgument(call, 0));
            return 1.0;
        });
        interpreter.setHandler("UnlockChallenge", [&challenges](CallContext& call) {
            challenges.unlock(formArgument(call, 0));
            return 1.0;
        });
        interpreter.setHandler("RemoveRecurringFromChallenge", [&challenges](CallContext& call) {
            challenges.removeRecurring(formArgument(call, 0));
            return 1.0;
        });
        interpreter.setHandler("ChallengeLocked", [&challenges](CallContext& call) {
            return challenges.unlocked(formArgument(call, 0)) ? 1.0 : 0.0;
        });
        // The command tells 1 for a challenge that is completed or has been completed once and started over (0x5DEEF0)
        interpreter.setHandler("GetChallengeCompleted", [&challenges](CallContext& call) {
            const FormId challenge = formArgument(call, 0);
            return challenges.completed(challenge) || challenges.repeated(challenge) ? 1.0 : 0.0;
        });
    }

    void addChallengeConditionFunctions(ConditionEvaluator& evaluator, Challenges& challenges)
    {
        // The numbers of the condition functions are the codes of the commands less 0x1000
        evaluator.setFunction(0x266, [&challenges](ConditionCall& call) {
            return static_cast<double>(challenges.completedBit(call.mFirst));
        });
        evaluator.setFunction(0x25F, [&challenges](ConditionCall& call) {
            return challenges.unlocked(call.mFirst) ? 1.0 : 0.0;
        });
    }
}
