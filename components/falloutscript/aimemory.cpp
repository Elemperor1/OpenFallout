#include "aimemory.hpp"

namespace FalloutScript
{
    bool AiState::empty() const
    {
        return mDone.empty() && mPackage == 0 && !mChangeTold && !mFollows;
    }

    bool AiState::operator==(const AiState& other) const
    {
        return mDone == other.mDone && mPackage == other.mPackage && mChangeTold == other.mChangeTold
            && mFollows == other.mFollows && mFollowsPlayer == other.mFollowsPlayer
            && mFollowTarget == other.mFollowTarget && mFollowDistance == other.mFollowDistance;
    }

    void AiMemory::clear()
    {
        const std::lock_guard lock(mMutex);
        mStates.clear();
    }

    void AiMemory::set(FormId actor, AiState state)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        if (state.empty())
            mStates.erase(actor);
        else
            mStates[actor] = std::move(state);
    }

    std::optional<AiState> AiMemory::find(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mStates.find(actor);
        if (found == mStates.end())
            return std::nullopt;
        return found->second;
    }

    std::size_t AiMemory::size() const
    {
        const std::lock_guard lock(mMutex);
        return mStates.size();
    }

    std::vector<std::pair<FormId, AiState>> AiMemory::all() const
    {
        const std::lock_guard lock(mMutex);
        return std::vector<std::pair<FormId, AiState>>(mStates.begin(), mStates.end());
    }

    void writeAi(const AiMemory& memory, SaveWriter& out)
    {
        const std::vector<std::pair<FormId, AiState>> states = memory.all();
        out.count(states.size());
        for (const auto& [actor, state] : states)
        {
            out.form(actor);
            out.count(state.mDone.size());
            for (const FormId package : state.mDone)
                out.form(package);
            out.form(state.mPackage);
            out.boolean(state.mChangeTold);
            out.boolean(state.mFollows);
            out.boolean(state.mFollowsPlayer);
            out.form(state.mFollowTarget);
            out.f32(state.mFollowDistance);
        }
    }

    void readAi(AiMemory& memory, SaveReader& in, std::uint32_t)
    {
        const std::size_t count = in.count(8);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::optional<FormId> actor = in.form();
            AiState state;
            const std::size_t done = in.count(4);
            for (std::size_t j = 0; j < done; ++j)
                if (const std::optional<FormId> package = in.form())
                    state.mDone.push_back(*package);
            if (const std::optional<FormId> package = in.form())
                state.mPackage = *package;
            state.mChangeTold = in.boolean();
            state.mFollows = in.boolean();
            state.mFollowsPlayer = in.boolean();
            const std::optional<FormId> target = in.form();
            state.mFollowTarget = target.value_or(0);
            state.mFollowDistance = in.f32();
            // The one it followed is gone with its plugin: it follows no one
            if (state.mFollows && !state.mFollowsPlayer && !target)
                state.mFollows = false;
            if (actor)
                memory.set(*actor, std::move(state));
        }
    }
}
