#include "messagecommands.hpp"

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        // The one a message belongs to: the object that a command acts on, which is the object of the script unless the
        // call names another, or else the quest. Zero for a script that is neither.
        FormId ownerOf(const CallContext& call)
        {
            return call.mReference != 0 ? call.mReference : call.mInstance.owner();
        }
    }

    void MessageAnswer::shown(FormId owner)
    {
        const std::lock_guard lock(mMutex);
        mButton = -1;
        mOwner = owner;
    }

    void MessageAnswer::picked(int button)
    {
        const std::lock_guard lock(mMutex);
        mButton = button;
    }

    int MessageAnswer::take(FormId owner)
    {
        const std::lock_guard lock(mMutex);
        if (owner == 0 || owner != mOwner || mButton < 0)
            return -1;
        const int button = mButton;
        mButton = -1;
        mOwner = 0;
        return button;
    }

    void MessageAnswer::clear()
    {
        const std::lock_guard lock(mMutex);
        mButton = -1;
        mOwner = 0;
    }

    void addMessageCommands(Interpreter& interpreter, MessageWorld& world)
    {
        interpreter.setHandler("ShowMessage", [&world](CallContext& call) {
            const FormId message = formArgument(call, 0);
            const FormId owner = ownerOf(call);
            // A message is not shown for a script that could not read its answer
            if (message == 0 || owner == 0)
                return 0.0;
            std::vector<double> values;
            values.reserve(call.mValues.size());
            for (const Value& value : call.mValues)
                values.push_back(value.mNumber);
            world.showMessage(message, values, owner);
            return 1.0;
        });
        // The games format the text of a warning and show it nowhere: the retail executables have no visible effect
        interpreter.setHandler("ShowWarning", [](CallContext&) { return 1.0; });
        // A script treats 0 as the first button, and -1 as no button pressed
        interpreter.setHandler(
            "GetButtonPressed", [&world](CallContext& call) { return world.takeButtonPressed(ownerOf(call)); });
        interpreter.setHandler("MenuMode",
            [&world](CallContext& call) { return world.menuOpen(call.mArguments.empty() ? 0 : intArgument(call, 0)); });
    }
}
