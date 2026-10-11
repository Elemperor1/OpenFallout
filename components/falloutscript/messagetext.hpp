#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGETEXT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGETEXT_H

#include <string>
#include <string_view>
#include <vector>

namespace FalloutScript
{
    /// The text of a message with the values of the call in it. A message has format tokens of printf (for example
    /// `%.0f` or `%g`), which take the values one after the other; `%%` is a percent sign. A token that has no value
    /// left is given 0, as the games do. A token of a kind that no value fits (`%s`, or the `% C` in the running text
    /// of a few messages) stays as it is and takes no value.
    std::string formatMessage(std::string_view text, const std::vector<double>& values);
}

#endif
