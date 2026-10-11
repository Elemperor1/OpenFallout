#include "referencestate.hpp"

#include <algorithm>

#include "arguments.hpp"

namespace FalloutScript
{
    ReferenceState::ReferenceState(ReferenceRecords& records)
        : mRecords(records)
    {
    }

    void ReferenceState::clear()
    {
        const std::lock_guard lock(mMutex);
        mLocks.clear();
        mDestroyed.clear();
    }

    void ReferenceState::lockOf(FormId reference, bool& locked, int& level)
    {
        locked = false;
        level = 0;
        const auto found = mLocks.find(reference);
        const LockEdit edit = found != mLocks.end() ? found->second : LockEdit();
        if (!edit.mLocked || !edit.mLevel)
        {
            bool recordLocked = false;
            int recordLevel = 0;
            if (mRecords.initialLock(reference, recordLocked, recordLevel))
            {
                locked = recordLocked;
                level = recordLevel;
            }
        }
        if (edit.mLocked)
            locked = *edit.mLocked;
        if (edit.mLevel)
            level = *edit.mLevel;
    }

    bool ReferenceState::isLocked(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        bool locked;
        int level;
        lockOf(reference, locked, level);
        return locked;
    }

    int ReferenceState::lockLevel(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        bool locked;
        int level;
        lockOf(reference, locked, level);
        return level;
    }

    void ReferenceState::lock(FormId reference, int level)
    {
        const std::lock_guard lock(mMutex);
        LockEdit& edit = mLocks[reference];
        edit.mLocked = true;
        if (level >= 0)
            edit.mLevel = level;
    }

    void ReferenceState::unlock(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        mLocks[reference].mLocked = false;
    }

    bool ReferenceState::isDestroyed(FormId reference) const
    {
        const std::lock_guard lock(mMutex);
        return mDestroyed.count(reference) != 0;
    }

    void ReferenceState::setDestroyed(FormId reference, bool destroyed)
    {
        const std::lock_guard lock(mMutex);
        if (destroyed)
            mDestroyed.insert(reference);
        else
            mDestroyed.erase(reference);
    }

    void addReferenceStateCommands(Interpreter& interpreter, ReferenceState& state)
    {
        interpreter.setHandler("GetLocked", [&state](CallContext& call) {
            return call.mReference != 0 && state.isLocked(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetLockLevel", [&state](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(state.lockLevel(call.mReference)) : 0.0;
        });
        // The level is the first argument and optional; the second says whether the lock is hidden from the player,
        // which nothing here shows
        interpreter.setHandler("Lock", [&state](CallContext& call) {
            if (call.mReference != 0)
                state.lock(call.mReference, call.mArguments.empty() ? -1 : intArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("UnLock", [&state](CallContext& call) {
            if (call.mReference != 0)
                state.unlock(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("GetDestroyed", [&state](CallContext& call) {
            return call.mReference != 0 && state.isDestroyed(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("SetDestroyed", [&state](CallContext& call) {
            if (call.mReference != 0)
                state.setDestroyed(call.mReference, intArgument(call, 0) != 0);
            return 0.0;
        });
    }
}
