#include "esm4base.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/ptr.hpp"

namespace OFClass
{
    void ESM4Impl::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface)
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
            ptr.getRefData().getBaseNode()->setNodeMask(OFRender::Mask_Static);
        }
    }

    void ESM4Impl::insertObjectPhysics(
        const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, OFPhysics::PhysicsSystem& physics)
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, OFPhysics::CollisionType_World);
    }

    OFGui::ToolTipInfo ESM4Impl::getToolTipInfo(std::string_view name, int count)
    {
        OFGui::ToolTipInfo info;
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        return info;
    }
}
