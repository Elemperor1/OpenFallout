#include "esm4base.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwphysics/physicssystem.hpp"

#include "../mwscript/falloutscripts.hpp"

#include "../mwworld/actiontalk.hpp"
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

    std::uint32_t ESM4Impl::actorSeed(const OFWorld::ConstPtr& ptr, ESM::FormId baseId)
    {
        return actorSeed(ptr.getCellRef().getRefNum(), baseId);
    }

    std::uint32_t ESM4Impl::actorSeed(ESM::FormId reference, ESM::FormId baseId)
    {
        return (reference.mIndex * 2654435761u + static_cast<std::uint32_t>(reference.mContentFile))
            ^ (baseId.mIndex * 40503u);
    }

    std::unique_ptr<OFWorld::Action> ESM4Impl::talkAction(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor)
    {
        if (actor != OFMechanics::getPlayer())
            return nullptr;
        const OFScript::FalloutScripts& scripts = *OFBase::Environment::get().getFalloutScripts();
        if (!scripts.hasDialogue() || scripts.isDead(ptr))
            return nullptr;
        return std::make_unique<OFWorld::ActionTalk>(ptr);
    }

    OFGui::ToolTipInfo ESM4Impl::getToolTipInfo(std::string_view name, int count)
    {
        OFGui::ToolTipInfo info;
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        return info;
    }
}
