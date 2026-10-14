#include "enableparents.hpp"

#include <set>

namespace FalloutScript
{
    void EnableParents::add(FormId child, FormId parent, bool inversed)
    {
        if (child == 0 || parent == 0 || child == parent)
            return;
        mChildren[parent].push_back({ child, inversed });
    }

    std::vector<std::pair<FormId, bool>> EnableParents::followers(FormId parent, bool disabled) const
    {
        std::vector<std::pair<FormId, bool>> result;
        std::set<FormId> seen{ parent };
        // A queue, so that the children come before their own
        std::vector<std::pair<FormId, bool>> queue{ { parent, disabled } };
        for (std::size_t i = 0; i < queue.size(); ++i)
        {
            const auto [reference, parentDisabled] = queue[i];
            const auto found = mChildren.find(reference);
            if (found == mChildren.end())
                continue;
            for (const Child& child : found->second)
            {
                if (!seen.insert(child.mReference).second)
                    continue;
                const bool childDisabled = parentDisabled != child.mInversed;
                result.emplace_back(child.mReference, childDisabled);
                queue.emplace_back(child.mReference, childDisabled);
            }
        }
        return result;
    }
}
