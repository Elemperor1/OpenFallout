#include "animationcommands.hpp"

#include "animgroups.hpp"
#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        constexpr std::string_view openGroup = "Open";
        constexpr std::string_view closeGroup = "Close";
    }

    int OpenStates::stateOf(FormId reference, AnimationWorld& world) const
    {
        // An object that does not animate being opened and closed only has the state it rests in
        if (world.hasGroup(reference, openGroup) && world.hasGroup(reference, closeGroup))
        {
            if (world.isGroupPlaying(reference, openGroup))
                return OpenState::opening;
            if (world.isGroupPlaying(reference, closeGroup))
                return OpenState::closing;
        }
        return restsOpen(reference) ? OpenState::open : OpenState::closed;
    }

    bool OpenStates::request(FormId reference, bool open, AnimationWorld& world)
    {
        const int state = stateOf(reference, world);
        if (open ? (state == OpenState::open || state == OpenState::opening)
                 : (state == OpenState::closed || state == OpenState::closing))
            return false;
        rest(reference, open);
        world.playGroup(reference, open ? openGroup : closeGroup, true);
        return true;
    }

    bool OpenStates::toggle(FormId reference, AnimationWorld& world)
    {
        const int state = stateOf(reference, world);
        const bool open = state == OpenState::closed || state == OpenState::closing;
        request(reference, open, world);
        return open;
    }

    void OpenStates::rest(FormId reference, bool open)
    {
        const std::lock_guard lock(mMutex);
        mOpen[reference] = open;
    }

    bool OpenStates::restsOpen(FormId reference) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mOpen.find(reference);
        return found != mOpen.end() && found->second;
    }

    bool OpenStates::knows(FormId reference) const
    {
        const std::lock_guard lock(mMutex);
        return mOpen.find(reference) != mOpen.end();
    }

    void OpenStates::clear()
    {
        const std::lock_guard lock(mMutex);
        mOpen.clear();
    }

    void addAnimationCommands(Interpreter& interpreter, OpenStates& states, AnimationWorld& world, Game game)
    {
        interpreter.setHandler("PlayGroup", [&world, game](CallContext& call) {
            // The group is a number of the table of the executable. The flag that follows it starts the group over when
            // it plays already (and the executable eases it in over a tenth of a second for each point of it, which
            // this does not)
            const std::string_view group = animationGroupName(game, intArgument(call, 0));
            if (call.mReference != 0 && !group.empty())
                world.playGroup(call.mReference, group, intArgument(call, 1) != 0);
            return 0.0;
        });
        interpreter.setHandler("IsAnimPlaying", [&world, game](CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            // With no group, any animation counts
            std::string_view group;
            if (!call.mArguments.empty())
            {
                group = animationGroupName(game, intArgument(call, 0));
                if (group.empty())
                    return 0.0;
            }
            return world.isGroupPlaying(call.mReference, group) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetOpenState", [&states, &world](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(states.stateOf(call.mReference, world)) : 0.0;
        });
        interpreter.setHandler("SetOpenState", [&states, &world](CallContext& call) {
            if (call.mReference != 0)
                states.request(call.mReference, intArgument(call, 0) != 0, world);
            return 0.0;
        });
    }
}
