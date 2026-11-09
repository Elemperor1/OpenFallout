#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGETEXT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGETEXT_H

#include <functional>
#include <optional>
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

    /// What a token of the form `&name;` (the name of a setting or a value of the player: `&sUActnVats;`,
    /// `&PCName;`) stands for, or nothing when it is not known. The form `&-name;` (the bare text of a key, where
    /// `&name;` has the words of the games before it) is given with its dash: `-sUActnVats`.
    using TokenResolver = std::function<std::optional<std::string>(std::string_view name)>;

    /// As above, and a token `&name;` or `&-name;` is replaced by what the resolver gives for the name; one it gives
    /// nothing for stays as it is
    std::string formatMessage(std::string_view text, const std::vector<double>& values, const TokenResolver& resolve);

    /// The setting that names a button of the mouse (the number of SDL: 1 left, 2 middle, 3 right, 4 and up the
    /// extra buttons), empty for a button the games have no name for
    std::string_view mouseButtonSetting(unsigned button);

    /// What a token of a control gives (FalloutNV.exe 0x7039B0): the text of the key put after the words that the
    /// games set before it (`prefix`), the words for a control with no key (`none`) when it has none; the bare form
    /// (`&-name;`) is only the text of the key, empty when there is none
    std::string controlText(std::string_view prefix, std::string_view key, std::string_view none, bool bare);
}

#endif
