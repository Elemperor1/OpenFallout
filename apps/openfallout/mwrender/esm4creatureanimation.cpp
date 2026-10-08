#include "esm4creatureanimation.hpp"

#include <components/debug/debuglog.hpp>

#include "../mwclass/esm4creature.hpp"
#include "../mwworld/class.hpp"

namespace OFRender
{
    ESM4CreatureAnimation::ESM4CreatureAnimation(
        const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem)
        : FalloutActorAnimation(ptr, std::move(parentNode), resourceSystem)
    {
        setObjectRoot(mPtr.getClass().getCorrectedModel(mPtr), true, true, false);
        if (mObjectRoot == nullptr)
            return;

        // The skeleton alone has nothing to see; the body hangs on it
        const ESM4::CreatureModel& model = OFClass::ESM4Creature::getCreatureModel(mPtr);
        for (const std::string& part : model.mParts)
            if (insertPart(part) == nullptr)
                Log(Debug::Warning) << "A body model of the creature " << mPtr.getCellRef().getRefId()
                                    << " is not found: " << part;
        startAnimations(false);
    }

    ESM4CreatureAnimation::~ESM4CreatureAnimation() = default;
}
