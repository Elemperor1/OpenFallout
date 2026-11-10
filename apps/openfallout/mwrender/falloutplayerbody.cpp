#include "falloutplayerbody.hpp"

#include <exception>
#include <utility>

#include <osg/Group>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadnpc.hpp>

#include "../mwbase/environment.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/manualref.hpp"

#include "esm4npcanimation.hpp"

namespace OFRender
{
    FalloutPlayerBody::FalloutPlayerBody(osg::ref_ptr<osg::Group> parent, Resource::ResourceSystem* resourceSystem)
        : mParent(std::move(parent))
        , mResourceSystem(resourceSystem)
    {
    }

    FalloutPlayerBody::~FalloutPlayerBody()
    {
        clear();
    }

    void FalloutPlayerBody::clear()
    {
        if (mAnimation != nullptr)
            mAnimation->removeFromScene();
        mAnimation = nullptr;
        if (mRoot != nullptr && mParent != nullptr)
            mParent->removeChild(mRoot);
        mRoot = nullptr;
        mRef.reset();
        mRecord.reset();
        mPose = 0;
    }

    bool FalloutPlayerBody::set(const ESM::RefId& templateId, const ESM4::Npc& record,
        const std::vector<const ESM4::Armor*>& armor, const OFWorld::Ptr& player, std::uint64_t revision)
    {
        clear();
        mRevision = revision;
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        if (store == nullptr || mParent == nullptr)
            return false;
        try
        {
            mRecord = std::make_unique<ESM4::Npc>(record);
            mRef = std::make_unique<OFWorld::ManualRef>(*store, templateId);
            const OFWorld::Ptr& ptr = mRef->getPtr();
            OFClass::ESM4Npc::setLook(ptr, *mRecord);
            OFClass::ESM4Npc::setWornArmor(ptr, armor);
            mRoot = new osg::Group;
            mRoot->setName("Player Body");
            mRoot->setNodeMask(mVisible ? ~0u : 0u);
            mParent->addChild(mRoot);
            mAnimation = new ESM4NpcAnimation(ptr, mRoot, mResourceSystem);
            // The character of the data stands still; the body walks as the player does
            mAnimation->setMotionSource(player);
            if (mPose != 0)
                mAnimation->setPose(mPose);
        }
        catch (const std::exception& e)
        {
            Log(Debug::Error) << "Player body: " << e.what();
            clear();
            return false;
        }
        Log(Debug::Info) << "Player body: made for the third person camera (" << armor.size() << " piece(s) of armor)";
        return true;
    }

    void FalloutPlayerBody::setVisible(bool visible)
    {
        mVisible = visible;
        if (mRoot != nullptr)
            mRoot->setNodeMask(visible ? ~0u : 0u);
    }

    void FalloutPlayerBody::setPose(int entryKind)
    {
        // Asked for every frame, as it is for the characters of the world: the animation keeps what it has, and asks
        // again for a pose that it could not start yet
        mPose = entryKind;
        if (mAnimation != nullptr)
            mAnimation->setPose(entryKind);
    }
}
