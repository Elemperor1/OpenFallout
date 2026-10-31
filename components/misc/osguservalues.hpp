#ifndef OPENFALLOUT_COMPONENTS_MISC_OSGUSERVALUES_H
#define OPENFALLOUT_COMPONENTS_MISC_OSGUSERVALUES_H

#include <string>

namespace Misc
{
    struct OsgUserValues
    {
        static const std::string sFileHash;
        static const std::string sXSoftEffect;
        /// The name of the node of a skeleton that a model asks to be attached to (the extra data "Prn" of Fallout
        /// files)
        static const std::string sAttachTo;
    };
}

#endif
