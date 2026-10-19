#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ANIMATIONCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ANIMATIONCOMMANDS_H

#include <mutex>
#include <string_view>
#include <unordered_map>

#include "interpreter.hpp"
#include "saveaccess.hpp"
#include "vanillacommands.hpp"

namespace FalloutScript
{
    /// The animations that an object holds in its model, as scripts ask for them (PlayGroup, IsAnimPlaying and the
    /// open state of doors and switches)
    class AnimationWorld
    {
    public:
        virtual ~AnimationWorld() = default;

        /// Whether the model of the reference has the group (a name of the table of the executable, in any case; a
        /// group named `<name>_<variant>` counts for `name`)
        virtual bool hasGroup(FormId reference, std::string_view group) = 0;
        /// Plays the group on the model of the reference, once from its start. A group that plays already goes on
        /// where it is, unless `restart` (the flag of PlayGroup). False when the model has no such group.
        virtual bool playGroup(FormId reference, std::string_view group, bool restart) = 0;
        /// Whether a group plays on the model of the reference: the named one, or any when the name is empty
        virtual bool isGroupPlaying(FormId reference, std::string_view group) = 0;
    };

    /// What GetOpenState gives
    namespace OpenState
    {
        constexpr int open = 1;
        constexpr int opening = 2;
        constexpr int closed = 3;
        constexpr int closing = 4;
    }

    /// Whether the doors, the switches and the other objects with an Open and a Close animation are open. An object
    /// rests open or closed (closed until something opens it), and while its Open or Close animation plays it is on
    /// the way. An object whose model has not both of those never is: it only rests.
    class OpenStates
    {
        template <class T>
        friend struct SaveAccess;

    public:
        /// The state of the object, one of OpenState
        int stateOf(FormId reference, AnimationWorld& world) const;

        /// SetOpenState: opens the object, or closes it, unless it is so or on the way (open or opening to open,
        /// closed or closing to close). The object rests in the new state at once, and the animation takes its
        /// time. Returns whether anything was done.
        bool request(FormId reference, bool open, AnimationWorld& world);

        /// Opens a closed object and closes an open one, as the activation of a door does. Returns whether it is open.
        bool toggle(FormId reference, AnimationWorld& world);

        /// The object rests open or closed without an animation (how a loaded object starts)
        void rest(FormId reference, bool open);
        bool restsOpen(FormId reference) const;
        /// Whether the state of the object was set (by rest or a request), so that the state it is loaded in, which a
        /// record may give, does not take the place of it
        bool knows(FormId reference) const;
        /// Forgets all states (a game is loaded or begun)
        void clear();

    private:
        // A script that Lua starts sets the state on the thread of Lua while the thread of the frame reads it
        mutable std::mutex mMutex;
        std::unordered_map<FormId, bool> mOpen;
    };

    /// Gives the commands of animations to the interpreter: PlayGroup, IsAnimPlaying, GetOpenState and SetOpenState.
    /// The groups scripts name by number are the ones of the table of the game.
    void addAnimationCommands(Interpreter& interpreter, OpenStates& states, AnimationWorld& world, Game game);
}

#endif
