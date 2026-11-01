#ifndef OPENFALLOUT_COMPONENTS_FALLOUTMENU_XML_H
#define OPENFALLOUT_COMPONENTS_FALLOUTMENU_XML_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace FalloutMenu::Xml
{
    /// An element of a menu file. The games read these files with a loader that forgives what a strict XML parser would
    /// not (comments that open with three dashes, an attribute whose value has no opening quote, tags that are never
    /// closed, a text with an `&` that has no `;`), so the tree is built by hand.
    struct Node
    {
        /// The tag, in lower case: the files write the same name in more than one way
        std::string mName;
        /// The attributes in the order they were written, with the name in lower case
        std::vector<std::pair<std::string, std::string>> mAttributes;
        /// The text of the element that is not inside a child, not trimmed
        std::string mText;
        std::vector<Node> mChildren;

        /// The value of an attribute, or nothing if it is not there
        const std::string* attribute(std::string_view name) const;
    };

    /// The elements at the top level of a file. A file that is a part of a menu (a prefab) has no single root.
    std::vector<Node> parse(std::string_view text);

    /// The text without the white space at both ends
    std::string_view trim(std::string_view text);

    /// The text in lower case (ASCII only)
    std::string lower(std::string_view text);
}

#endif
