#include "formlists.hpp"

#include <algorithm>

#include "arguments.hpp"

namespace FalloutScript
{
    FormLists::FormLists(FormListRecords& records)
        : mRecords(records)
    {
    }

    void FormLists::clear()
    {
        const std::lock_guard lock(mMutex);
        mLists.clear();
    }

    void FormLists::add(FormId list, FormId form)
    {
        if (list == 0 || form == 0)
            return;
        const std::lock_guard lock(mMutex);
        auto found = mLists.find(list);
        if (found == mLists.end())
        {
            std::vector<FormId> entries;
            if (!mRecords.formListEntries(list, entries))
                return;
            found = mLists.emplace(list, std::move(entries)).first;
        }
        found->second.push_back(form);
    }

    bool FormLists::contains(FormId list, FormId form)
    {
        if (list == 0 || form == 0)
            return false;
        const std::lock_guard lock(mMutex);
        const auto found = mLists.find(list);
        if (found != mLists.end())
            return std::find(found->second.begin(), found->second.end(), form) != found->second.end();
        std::vector<FormId> entries;
        return mRecords.formListEntries(list, entries)
            && std::find(entries.begin(), entries.end(), form) != entries.end();
    }

    bool FormLists::containsBaseOf(FormId list, FormId reference)
    {
        FormId base = 0;
        return reference != 0 && mRecords.baseFormOf(reference, base) && contains(list, base);
    }

    void addFormListCommands(Interpreter& interpreter, FormLists& lists)
    {
        interpreter.setHandler("AddFormToFormList", [&lists](CallContext& call) {
            lists.add(formArgument(call, 0), formArgument(call, 1));
            return 0.0;
        });
        interpreter.setHandler("IsInList", [&lists](CallContext& call) {
            return lists.containsBaseOf(formArgument(call, 0), call.mReference) ? 1.0 : 0.0;
        });
    }
}
