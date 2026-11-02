#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SETTINGTEXT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SETTINGTEXT_H

#include <string_view>

namespace FalloutScript::SettingText
{
    /// The words of the screens' labels, for the text settings that the plugins do not have a record of. The games keep
    /// those words in their executables (the plugins only hold the settings that were changed from them), and the menu
    /// files, which name them as `&-sName;`, would be blank without them. These are the port's own words for them, as
    /// short as the labels are; a plugin that has a record of the setting (a game in another language) takes the place
    /// of them. The names are not case sensitive. An empty text when the name is not one of them.
    std::string_view standIn(std::string_view name);
}

#endif
