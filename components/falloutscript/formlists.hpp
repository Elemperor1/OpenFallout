#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FORMLISTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FORMLISTS_H

#include <map>
#include <mutex>
#include <vector>

#include "script.hpp"
#include "saveaccess.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// What the records of the game say about form lists and about what a reference is
    class FormListRecords
    {
    public:
        virtual ~FormListRecords() = default;

        /// The forms in the list by its record. False when no record is the list.
        virtual bool formListEntries(FormId list, std::vector<FormId>& entries) = 0;
        /// The form that the reference stands for (the character, creature or object it is of). False when the host
        /// can not tell.
        virtual bool baseFormOf(FormId reference, FormId& base) = 0;
    };

    /// The form lists as scripts change them. A list is what its record says until a script adds to it, and then it is
    /// that and what was added. It can be used from any thread.
    class FormLists
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit FormLists(FormListRecords& records);

        /// Forgets everything, as for a new game
        void clear();

        /// Adds the form to the end of the list. Nothing happens for a list that no record has.
        void add(FormId list, FormId form);
        bool contains(FormId list, FormId form);
        /// Whether the form that the reference stands for is in the list
        bool containsBaseOf(FormId list, FormId reference);

    private:
        FormListRecords& mRecords;
        std::mutex mMutex;
        /// The lists that scripts changed, with all their forms
        std::map<FormId, std::vector<FormId>> mLists;
    };

    /// Gives the commands AddFormToFormList and IsInList to the interpreter. Those that the command table does not have
    /// are left out.
    void addFormListCommands(Interpreter& interpreter, FormLists& lists);
}

#endif
