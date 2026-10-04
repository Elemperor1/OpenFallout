#include "engineevents.hpp"

#include <components/debug/debuglog.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/worldmodel.hpp"

#include "globalscripts.hpp"
#include "localscripts.hpp"
#include "object.hpp"

namespace OFLua
{

    class EngineEvents::Visitor
    {
    public:
        explicit Visitor(GlobalScripts& globalScripts)
            : mGlobalScripts(globalScripts)
        {
        }

        void operator()(const OnActive& event) const
        {
            OFWorld::Ptr ptr = getPtr(event.mObject);
            if (ptr.isEmpty())
                return;
            if (ptr.getCellRef().getRefId() == "player")
                mGlobalScripts.playerAdded(GObject(ptr));
            else
            {
                mGlobalScripts.objectActive(GObject(ptr));
                const OFWorld::Class& objClass = ptr.getClass();
                if (objClass.isActor())
                    mGlobalScripts.actorActive(GObject(ptr));
                if (objClass.isItem(ptr))
                    mGlobalScripts.itemActive(GObject(ptr));
            }
            if (auto* scripts = getLocalScripts(ptr))
                scripts->setActive(true);
        }

        void operator()(const OnInactive& event) const
        {
            if (auto* scripts = getLocalScripts(event.mObject))
                scripts->setActive(false);
        }

        void operator()(const OnTeleported& event) const
        {
            if (auto* scripts = getLocalScripts(event.mObject))
                scripts->onTeleported();
        }

        void operator()(const OnActivate& event) const
        {
            OFWorld::Ptr obj = getPtr(event.mObject);
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (actor.isEmpty() || obj.isEmpty())
                return;
            mGlobalScripts.onActivate(GObject(obj), GObject(actor));
            if (auto* scripts = getLocalScripts(obj))
                scripts->onActivated(LObject(actor));
        }

        void operator()(const OnUseItem& event) const
        {
            OFWorld::Ptr obj = getPtr(event.mObject);
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (actor.isEmpty() || obj.isEmpty())
                return;
            mGlobalScripts.onUseItem(GObject(obj), GObject(actor), event.mForce);
        }

        void operator()(const OnConsume& event) const
        {
            OFWorld::Ptr actor = getPtr(event.mActor);
            OFWorld::Ptr consumable = getPtr(event.mConsumable);
            if (actor.isEmpty() || consumable.isEmpty())
                return;
            if (auto* scripts = getLocalScripts(actor))
                scripts->onConsume(LObject(consumable));
        }

        void operator()(const OnDropped& event) const
        {
            OFWorld::Ptr obj = getPtr(event.mObject);
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (obj.isEmpty() || actor.isEmpty())
                return;
            mGlobalScripts.onDropped(GObject(obj), GObject(actor), event.mPosition, event.mRotation);
        }

        void operator()(const OnPlaced& event) const
        {
            OFWorld::Ptr obj = getPtr(event.mObject);
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (obj.isEmpty() || actor.isEmpty())
                return;
            mGlobalScripts.onPlaced(GObject(obj), GObject(actor), event.mPosition, event.mRotation);
        }

        void operator()(const OnNewExterior& event) const { mGlobalScripts.onNewExterior(GCell{ &event.mCell }); }

        void operator()(const OnAnimationTextKey& event) const
        {
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (actor.isEmpty())
                return;
            if (auto* scripts = getLocalScripts(actor))
                scripts->onAnimationTextKey(event.mGroupname, event.mKey);
        }

        void operator()(const OnAnimationEnded& event) const
        {
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (actor.isEmpty())
                return;
            if (auto* scripts = getLocalScripts(actor))
                scripts->onAnimationEnded(
                    event.mGroupname, event.mStartKey, event.mStopKey, event.mTime, event.mCompletion);
        }

        void operator()(const OnSkillUse& event) const
        {
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (actor.isEmpty())
                return;
            if (auto* scripts = getLocalScripts(actor))
                scripts->onSkillUse(event.mSkill, event.useType, event.scale);
        }

        void operator()(const OnSkillLevelUp& event) const
        {
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (actor.isEmpty())
                return;
            if (auto* scripts = getLocalScripts(actor))
                scripts->onSkillLevelUp(event.mSkill, event.mSource);
        }

        void operator()(const OnJailTimeServed& event) const
        {
            OFWorld::Ptr actor = getPtr(event.mActor);
            if (actor.isEmpty())
                return;
            if (auto* scripts = getLocalScripts(actor))
                scripts->onJailTimeServed(event.mDays);
        }

    private:
        OFWorld::Ptr getPtr(ESM::RefNum id) const
        {
            OFWorld::Ptr res = mWorldModel->getPtr(id);
            if (res.isEmpty() && Settings::lua().mLuaDebug)
                Log(Debug::Verbose) << "Can not find object" << id.toString() << " when calling engine hanglers";
            return res;
        }

        LocalScripts* getLocalScripts(const OFWorld::Ptr& ptr) const
        {
            if (ptr.isEmpty())
                return nullptr;
            else
                return ptr.getRefData().getLuaScripts();
        }

        LocalScripts* getLocalScripts(ESM::RefNum id) const { return getLocalScripts(getPtr(id)); }

        GlobalScripts& mGlobalScripts;
        OFWorld::WorldModel* mWorldModel = OFBase::Environment::get().getWorldModel();
    };

    void EngineEvents::callEngineHandlers()
    {
        Visitor vis(mGlobalScripts);
        for (const Event& event : mQueue)
            std::visit(vis, event);
        mQueue.clear();
    }

}
