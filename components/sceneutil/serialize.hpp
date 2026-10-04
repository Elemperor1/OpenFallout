#ifndef OPENFALLOUT_COMPONENTS_SCENEUTIL_SERIALIZE_H
#define OPENFALLOUT_COMPONENTS_SCENEUTIL_SERIALIZE_H

namespace SceneUtil
{

    /// Register osg node serializers for certain SceneUtil classes if not already done so
    void registerSerializers(bool skipGeometry = true);

}

#endif
