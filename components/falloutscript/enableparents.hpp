#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ENABLEPARENTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ENABLEPARENTS_H

#include <map>
#include <utility>
#include <vector>

#include "script.hpp"

namespace FalloutScript
{
    /// The references whose state follows that of another one, their enable parent (the XESP of the record). A child
    /// is enabled when its parent is, or, when the record says so (the flag that inverses it), disabled when the
    /// parent is enabled. The games change the state of the children whenever one of the parents is enabled or
    /// disabled, and so do the children of the children.
    class EnableParents
    {
    public:
        /// The reference `child` follows `parent`
        void add(FormId child, FormId parent, bool inversed);

        bool empty() const { return mChildren.empty(); }

        /// The references to put in a state when `parent` is disabled (true) or enabled (false), by the state each is
        /// to have (true: disabled): the children first, then theirs. A reference that is a child of itself, even by
        /// way of others, is listed once.
        std::vector<std::pair<FormId, bool>> followers(FormId parent, bool disabled) const;

    private:
        struct Child
        {
            FormId mReference = 0;
            bool mInversed = false;
        };
        std::map<FormId, std::vector<Child>> mChildren;
    };
}

#endif
