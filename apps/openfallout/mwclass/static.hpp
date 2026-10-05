#ifndef GAME_MWCLASS_STATIC_H
#define GAME_MWCLASS_STATIC_H

#include "../mwworld/cellstore.hpp"
#include "../mwworld/registeredclass.hpp"

#include <MyGUI_TextIterator.h>

namespace OFClass
{
    class Static : public OFWorld::RegisteredClass<Static>
    {
        friend OFWorld::RegisteredClass<Static>;

        Static();

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        void insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;
        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;
    };
}

#endif
