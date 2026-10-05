#ifndef GAME_MWCLASS_BODYPART_H
#define GAME_MWCLASS_BODYPART_H

#include "../mwworld/registeredclass.hpp"

namespace OFClass
{

    class BodyPart : public OFWorld::RegisteredClass<BodyPart>
    {
        friend OFWorld::RegisteredClass<BodyPart>;

        BodyPart();

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;
    };

}

#endif
