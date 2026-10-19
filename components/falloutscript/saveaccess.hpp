#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVEACCESS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SAVEACCESS_H

namespace FalloutScript
{
    /// How the state of a class gets into a saved game and back: the code that writes and reads it (savestate files of
    /// their own, one specialisation for each class) is its friend, so that the classes keep nothing of the save in
    /// their headers.
    template <class T>
    struct SaveAccess;
}

#endif
