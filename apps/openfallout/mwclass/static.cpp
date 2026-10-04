#include "static.hpp"

#include <components/esm3/loadstat.hpp>
#include <components/esm4/loadstat.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

#include "classmodel.hpp"

namespace OFClass
{
    Static::Static()
        : OFWorld::RegisteredClass<Static>(ESM::Static::sRecordId)
    {
    }

    void Static::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
            ptr.getRefData().getBaseNode()->setNodeMask(OFRender::Mask_Static);
        }
    }

    void Static::insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);
    }

    void Static::insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, OFPhysics::CollisionType_World);
    }

    VFS::Path::NormalizedView Static::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Static>(ptr);
    }

    std::string_view Static::getName(const OFWorld::ConstPtr& ptr) const
    {
        return {};
    }

    bool Static::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        return false;
    }

    OFWorld::Ptr Static::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Static>* ref = ptr.get<ESM::Static>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }
}
