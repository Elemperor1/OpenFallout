#include "activator.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadacti.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadsndg.hpp>
#include <components/esm3/loadsoun.hpp>
#include <components/misc/rng.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/action.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/nullaction.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwphysics/physicssystem.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwmechanics/npcstats.hpp"

#include "classmodel.hpp"

namespace OFClass
{
    Activator::Activator()
        : OFWorld::RegisteredClass<Activator>(ESM::Activator::sRecordId)
    {
    }

    void Activator::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
            ptr.getRefData().getBaseNode()->setNodeMask(OFRender::Mask_Static);
        }
    }

    void Activator::insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);
    }

    void Activator::insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, OFPhysics::CollisionType_World);
    }

    VFS::Path::NormalizedView Activator::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Activator>(ptr);
    }

    bool Activator::isActivator() const
    {
        return true;
    }

    bool Activator::useAnim() const
    {
        return true;
    }

    std::string_view Activator::getName(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Activator>* ref = ptr.get<ESM::Activator>();

        return ref->mBase->mName;
    }

    ESM::RefId Activator::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Activator>* ref = ptr.get<ESM::Activator>();

        return ref->mBase->mScript;
    }

    bool Activator::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        return !getName(ptr).empty();
    }

    OFGui::ToolTipInfo Activator::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Activator>* ref = ptr.get<ESM::Activator>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
        }

        return info;
    }

    std::unique_ptr<OFWorld::Action> Activator::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        std::unique_ptr<OFWorld::Action> werewolfAction = getWerewolfRefusalAction(actor);
        if (werewolfAction)
            return werewolfAction;

        return std::make_unique<OFWorld::NullAction>();
    }

    OFWorld::Ptr Activator::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Activator>* ref = ptr.get<ESM::Activator>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    ESM::RefId Activator::getSoundIdFromSndGen(const OFWorld::Ptr& ptr, std::string_view name) const
    {
        // Assume it's not empty, since we wouldn't have gotten the soundgen otherwise
        const VFS::Path::NormalizedView model = getModel(ptr);
        const OFWorld::ESMStore& store = *OFBase::Environment::get().getESMStore();
        ESM::RefId creatureId;

        for (const ESM::Creature& iter : store.get<ESM::Creature>())
        {
            if (!iter.mModel.empty() && model == iter.mModel.getNormalized())
            {
                creatureId = !iter.mOriginal.empty() ? iter.mOriginal : iter.mId;
                break;
            }
        }

        const int type = getSndGenTypeFromName(name);

        std::vector<const ESM::SoundGenerator*> fallbacksounds;
        auto& prng = OFBase::Environment::get().getWorld()->getPrng();
        if (!creatureId.empty())
        {
            std::vector<const ESM::SoundGenerator*> sounds;
            for (auto sound = store.get<ESM::SoundGenerator>().begin(); sound != store.get<ESM::SoundGenerator>().end();
                 ++sound)
            {
                if (type == sound->mType && !sound->mCreature.empty() && creatureId == sound->mCreature)
                    sounds.push_back(&*sound);
                if (type == sound->mType && sound->mCreature.empty())
                    fallbacksounds.push_back(&*sound);
            }

            if (!sounds.empty())
                return sounds[Misc::Rng::rollDice(sounds.size(), prng)]->mSound;
            if (!fallbacksounds.empty())
                return fallbacksounds[Misc::Rng::rollDice(fallbacksounds.size(), prng)]->mSound;
        }
        else
        {
            // The activator doesn't have a corresponding creature ID, but we can try to use the defaults
            for (auto sound = store.get<ESM::SoundGenerator>().begin(); sound != store.get<ESM::SoundGenerator>().end();
                 ++sound)
                if (type == sound->mType && sound->mCreature.empty())
                    fallbacksounds.push_back(&*sound);

            if (!fallbacksounds.empty())
                return fallbacksounds[Misc::Rng::rollDice(fallbacksounds.size(), prng)]->mSound;
        }

        return ESM::RefId();
    }

    int Activator::getSndGenTypeFromName(std::string_view name)
    {
        if (name == "left")
            return ESM::SoundGenerator::LeftFoot;
        if (name == "right")
            return ESM::SoundGenerator::RightFoot;
        if (name == "swimleft")
            return ESM::SoundGenerator::SwimLeft;
        if (name == "swimright")
            return ESM::SoundGenerator::SwimRight;
        if (name == "moan")
            return ESM::SoundGenerator::Moan;
        if (name == "roar")
            return ESM::SoundGenerator::Roar;
        if (name == "scream")
            return ESM::SoundGenerator::Scream;
        if (name == "land")
            return ESM::SoundGenerator::Land;

        throw std::runtime_error("Unexpected soundgen type: " + std::string(name));
    }
}
