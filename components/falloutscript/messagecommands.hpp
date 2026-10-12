#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGECOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGECOMMANDS_H

#include <mutex>
#include <vector>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// What the commands that tell the player something ask of the game
    class MessageWorld
    {
    public:
        virtual ~MessageWorld() = default;

        /// Shows the player the message (a MESG record) that a script names, with the values that the call gave for the
        /// tokens of its text. A message with buttons waits for the player to pick one. `owner` is who asked: the
        /// object the script is on, or the quest that holds it. Showing a message forgets the answer to the one before,
        /// and the answer to this one is for `owner` to read.
        virtual void showMessage(FormId message, const std::vector<double>& values, FormId owner) = 0;
        /// The button that the player picked in the last message with buttons, counted from 0 as the record lists
        /// them (the ones whose conditions failed are counted too; a message with none to pick has one that closes it,
        /// which is 0), or -1 when none was (the message is still open, no message with buttons was shown, or it was
        /// shown for another owner). It is told once: the read by its owner takes it, and the next one tells -1 until
        /// the player picks another.
        virtual int takeButtonPressed(FormId owner) = 0;
        /// Whether a menu is open: the one that the number names (the numbers of the games, 1001 is the message box)
        /// or, for 0, any of them.
        virtual bool menuOpen(int menu) = 0;
    };

    /// The answer to the last message with buttons and who may read it. The games keep one answer for all scripts:
    /// showing a message forgets the one before, the answer to a box is given to the one who showed it when the player
    /// closes it (also when another message was shown while it was open), and the answer is told once, to that one
    /// only (another script that asks gets -1 and leaves it where it is). It can be used from any thread.
    class MessageAnswer
    {
    public:
        /// A message was shown: the answer to the one before is gone
        void forget();
        /// The player picked a button of the message that `owner` showed, counted from 0 as the record lists them
        void picked(FormId owner, int button);
        /// The button for `owner`, or -1 when none was picked, it is for another, or `owner` is none; it is taken
        int take(FormId owner);
        void clear();

    private:
        std::mutex mMutex;
        int mButton = -1;
        FormId mOwner = 0;
    };

    /// Gives the commands that show messages to the interpreter: ShowMessage, ShowWarning (which shows nothing, as in
    /// the games), GetButtonPressed and MenuMode. The command table of a game that does not have a command is left as
    /// it is.
    void addMessageCommands(Interpreter& interpreter, MessageWorld& world);
}

#endif
