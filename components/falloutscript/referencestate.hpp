#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REFERENCESTATE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REFERENCESTATE_H

#include <map>
#include <mutex>
#include <optional>
#include <set>

#include "saveaccess.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// What the records of the game say about a reference before a script changes anything
    class ReferenceRecords
    {
    public:
        virtual ~ReferenceRecords() = default;

        /// The lock the reference has by its record: whether it is locked, and its lock level. False when the host can
        /// not tell (a reference that no record has).
        virtual bool initialLock(FormId reference, bool& locked, int& level) = 0;
    };

    /// What scripts do to a reference that nothing else in the game keeps yet: whether it is locked and how hard, and
    /// whether it is destroyed. It can be used from any thread. The lock starts as the record of the reference says and
    /// is kept as what scripts made it; nothing here makes a locked door stay shut, which the game does not have the
    /// activation of doors for yet.
    class ReferenceState
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit ReferenceState(ReferenceRecords& records);

        /// Forgets everything, as for a new game
        void clear();

        bool isLocked(FormId reference);
        /// The lock level; 0 when the reference has none
        int lockLevel(FormId reference);
        /// Locks the reference; a level of 0 or more is its new lock level, else the lock keeps the level it had
        void lock(FormId reference, int level);
        void unlock(FormId reference);

        bool isDestroyed(FormId reference) const;
        void setDestroyed(FormId reference, bool destroyed);

    private:
        /// What scripts made of the lock, as far as they said
        struct LockEdit
        {
            std::optional<bool> mLocked;
            std::optional<int> mLevel;
        };

        /// The lock of the reference, from the edits and for what they leave out the record
        void lockOf(FormId reference, bool& locked, int& level);

        ReferenceRecords& mRecords;
        mutable std::mutex mMutex;
        std::map<FormId, LockEdit> mLocks;
        std::set<FormId> mDestroyed;
    };

    /// Gives the commands GetLocked, GetLockLevel, Lock, UnLock, GetDestroyed and SetDestroyed to the interpreter.
    /// Those that the command table does not have are left out.
    void addReferenceStateCommands(Interpreter& interpreter, ReferenceState& state);
}

#endif
