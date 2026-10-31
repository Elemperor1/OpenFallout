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
        /// The turn (an osg::Quat) that the node with the extra data "Prn" has in the file of a Fallout model: the
        /// optimizer folds it into the meshes, so it is kept here for whoever puts the model on its bone
        static const std::string sAttachRotation;
    };
}

#endif
