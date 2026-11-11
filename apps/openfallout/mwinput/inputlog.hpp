#ifndef OPENFALLOUT_MWINPUT_INPUTLOG_H
#define OPENFALLOUT_MWINPUT_INPUTLOG_H

#include <string>

namespace OFInput
{
    // What a hand did, as lines of the log that start with `Input:`. A report of a hand-played hour could not say whether
    // the player walked by the stick or by the keys, or which button was pressed on a screen that did nothing with it,
    // and the lines are what it needs. Each kind is written for its first lines only, so a long game does not fill the
    // log: the last line of a kind says that the rest is not logged.
    enum class InputLogKind
    {
        Key, // a key of the keyboard
        Button, // a button of the pad (a trigger is told as the button it stands for)
        Sticks, // the sticks and triggers while they are away from the middle (at most twice a second)
    };

    void logInput(InputLogKind kind, const std::string& text);
}

#endif
