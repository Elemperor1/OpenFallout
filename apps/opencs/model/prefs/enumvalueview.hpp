#ifndef OPENFALLOUT_APPS_OPENCS_MODEL_PREFS_ENUMVALUEVIEW_H
#define OPENFALLOUT_APPS_OPENCS_MODEL_PREFS_ENUMVALUEVIEW_H

#include <string_view>

namespace CSMPrefs
{
    struct EnumValueView
    {
        std::string_view mValue;
        std::string_view mTooltip;
    };
}

#endif
