#include "localscripts.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwmechanics/aicombat.hpp"
#include "../mwmechanics/aiescort.hpp"
#include "../mwmechanics/aifollow.hpp"
#include "../mwmechanics/aipackage.hpp"
#include "../mwmechanics/aipursue.hpp"
#include "../mwmechanics/aisequence.hpp"
#include "../mwmechanics/aitravel.hpp"
#include "../mwmechanics/aiwander.hpp"
#include "../mwmechanics/attacktype.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"

namespace sol
{
    template <>
    struct is_automagical<OFBase::LuaManager::ActorControls> : std::false_type
    {
    };
    template <>
    struct is_automagical<OFLua::SelfObject> : std::false_type
    {
    };
}

namespace OFLua
{
    void SelfObject::cacheStat(LuaManager& manager, SelfObject::CachedStat key, sol::main_object value)
    {
        if (mStatsCache.empty())
        {
            manager.addAction(
                [obj = Object(*this)] {
                    LocalScripts* scripts = obj.ptr().getRefData().getLuaScripts();
                    if (scripts)
                        scripts->applyStatsCache();
                },
                "StatUpdateAction");
        }
        mStatsCache[std::move(key)] = std::move(value);
    }

    void LocalScripts::initializeSelfPackage(const Context& context)
    {
        auto lua = context.sol();
        using ActorControls = OFBase::LuaManager::ActorControls;
        sol::usertype<ActorControls> controls = lua.new_usertype<ActorControls>("ActorControls");

#define CONTROL(TYPE, FIELD)                                                                                           \
    sol::property([](const ActorControls& c) { return c.FIELD; },                                                      \
        [](ActorControls& c, const TYPE& v) {                                                                          \
            c.FIELD = v;                                                                                               \
            c.mChanged = true;                                                                                         \
        })
        controls["movement"] = CONTROL(float, mMovement);
        controls["sideMovement"] = CONTROL(float, mSideMovement);
        controls["pitchChange"] = CONTROL(float, mPitchChange);
        controls["yawChange"] = CONTROL(float, mYawChange);
        controls["run"] = CONTROL(bool, mRun);
        controls["sneak"] = CONTROL(bool, mSneak);
        controls["jump"] = CONTROL(bool, mJump);
        controls["use"] = CONTROL(OFMechanics::AttackType, mUse);
#undef CONTROL

        sol::usertype<SelfObject> selfAPI
            = lua.new_usertype<SelfObject>("SelfObject", sol::base_classes, sol::bases<LObject, Object>());
        selfAPI[sol::meta_function::to_string]
            = [](SelfObject& self) { return "openfallout.self[" + self.toString() + "]"; };
        selfAPI["object"] = sol::readonly_property([](SelfObject& self) -> LObject { return LObject(self); });
        selfAPI["controls"] = sol::readonly_property([](SelfObject& self) { return &self.mControls; });
        selfAPI["isActive"] = [](SelfObject& self) -> bool { return self.mIsActive; };
        selfAPI["enableAI"] = [](SelfObject& self, bool v) { self.mControls.mDisableAI = !v; };
        selfAPI["saveState"]
            = sol::readonly_property([](const SelfObject& self) { return self.ptr().getRefData().hasChanged(); });
        selfAPI["ATTACK_TYPE"]
            = LuaUtil::makeStrictReadOnly(LuaUtil::tableFromPairs<std::string_view, OFMechanics::AttackType>(lua,
                { { "NoAttack", OFMechanics::AttackType::NoAttack }, { "Any", OFMechanics::AttackType::Any },
                    { "Chop", OFMechanics::AttackType::Chop }, { "Slash", OFMechanics::AttackType::Slash },
                    { "Thrust", OFMechanics::AttackType::Thrust } }));

        using AiPackage = OFMechanics::AiPackage;
        sol::usertype<AiPackage> aiPackage = lua.new_usertype<AiPackage>("AiPackage");
        aiPackage["type"] = sol::readonly_property([](const AiPackage& p) -> std::string_view {
            switch (p.getTypeId())
            {
                case OFMechanics::AiPackageTypeId::Wander:
                    return "Wander";
                case OFMechanics::AiPackageTypeId::Travel:
                    return "Travel";
                case OFMechanics::AiPackageTypeId::Escort:
                    return "Escort";
                case OFMechanics::AiPackageTypeId::Follow:
                    return "Follow";
                case OFMechanics::AiPackageTypeId::Activate:
                    return "Activate";
                case OFMechanics::AiPackageTypeId::Combat:
                    return "Combat";
                case OFMechanics::AiPackageTypeId::Pursue:
                    return "Pursue";
                case OFMechanics::AiPackageTypeId::AvoidDoor:
                    return "AvoidDoor";
                case OFMechanics::AiPackageTypeId::Face:
                    return "Face";
                case OFMechanics::AiPackageTypeId::Breathe:
                    return "Breathe";
                case OFMechanics::AiPackageTypeId::Cast:
                    return "Cast";
                default:
                    return "Unknown";
            }
        });
        aiPackage["target"] = sol::readonly_property([](const AiPackage& p) -> sol::optional<LObject> {
            OFWorld::Ptr target = p.getTarget();
            if (target.isEmpty())
                return sol::nullopt;
            else
                return LObject(getId(target));
        });
        aiPackage["sideWithTarget"] = sol::readonly_property([](const AiPackage& p) { return p.sideWithTarget(); });
        aiPackage["destPosition"] = sol::readonly_property([](const AiPackage& p) { return p.getDestination(); });
        aiPackage["distance"] = sol::readonly_property([](const AiPackage& p) { return p.getDistance(); });
        aiPackage["duration"] = sol::readonly_property([](const AiPackage& p) { return p.getDuration(); });
        aiPackage["idle"]
            = sol::readonly_property([lua = lua.lua_state()](const AiPackage& p) -> sol::optional<sol::table> {
                  if (p.getTypeId() == OFMechanics::AiPackageTypeId::Wander)
                  {
                      sol::table idles(lua, sol::create);
                      const std::vector<unsigned char>& idle = static_cast<const OFMechanics::AiWander&>(p).getIdle();
                      if (!idle.empty())
                      {
                          for (size_t i = 0; i < idle.size(); ++i)
                          {
                              std::string_view groupName = OFMechanics::AiWander::getIdleGroupName(i);
                              idles[groupName] = idle[i];
                          }
                          return idles;
                      }
                  }
                  return sol::nullopt;
              });

        aiPackage["isRepeat"] = sol::readonly_property([](const AiPackage& p) { return p.getRepeat(); });

        selfAPI["_isFleeing"] = [](SelfObject& self) -> bool {
            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
            if (ai.isEmpty())
                return false;
            else
                return ai.isFleeing();
        };
        selfAPI["_getActiveAiPackage"] = [](SelfObject& self) -> sol::optional<std::shared_ptr<AiPackage>> {
            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
            if (ai.isEmpty())
                return sol::nullopt;
            else
                return *ai.begin();
        };
        selfAPI["_iterateAndFilterAiSequence"] = [](SelfObject& self, sol::function callback) {
            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();

            ai.erasePackagesIf([&](auto& entry) {
                auto result = LuaUtil::call(callback, entry);
                if (result.get_type() == sol::type::nil || result.return_count() == 0)
                    return false;

                bool keep = result.template get<bool>();
                return !keep;
            });
        };
        selfAPI["_startAiCombat"] = [](SelfObject& self, const LObject& target, bool cancelOther) {
            const OFWorld::Ptr& targetPtr = target.ptr();
            if (!targetPtr.getClass().isActor())
                throw std::runtime_error("Combat target must be an actor");

            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
            ai.stack(OFMechanics::AiCombat(targetPtr), ptr, cancelOther);
        };
        selfAPI["_startAiPursue"] = [](SelfObject& self, const LObject& target, bool cancelOther) {
            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
            ai.stack(OFMechanics::AiPursue(target.ptr()), ptr, cancelOther);
        };
        selfAPI["_startAiFollow"] = [](SelfObject& self, const LObject& target, sol::optional<LCell> cell,
                                        float duration, const osg::Vec3f& dest, bool repeat, bool cancelOther) {
            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
            std::string_view cellNameId;
            if (cell)
                cellNameId = cell->mStore->getCell()->getNameId();
            ai.stack(
                OFMechanics::AiFollow(getId(target.ptr()), cellNameId, duration, dest.x(), dest.y(), dest.z(), repeat),
                ptr, cancelOther);
        };
        selfAPI["_startAiEscort"] = [](SelfObject& self, const LObject& target, LCell cell, float duration,
                                        const osg::Vec3f& dest, bool repeat, bool cancelOther) {
            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
            int gameHoursDuration = static_cast<int>(std::ceil(duration / 3600.0));
            auto* esmCell = cell.mStore->getCell();
            std::string_view cellNameId;
            if (!esmCell->isExterior())
                cellNameId = esmCell->getNameId();
            ai.stack(OFMechanics::AiEscort(
                         getId(target.ptr()), cellNameId, gameHoursDuration, dest.x(), dest.y(), dest.z(), repeat),
                ptr, cancelOther);
        };
        selfAPI["_startAiWander"]
            = [](SelfObject& self, int distance, int duration, sol::table luaIdle, bool repeat, bool cancelOther) {
                  const OFWorld::Ptr& ptr = self.ptr();
                  OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
                  std::vector<unsigned char> idle;
                  // Lua index starts at 1
                  for (size_t i = 1; i <= luaIdle.size(); i++)
                      idle.emplace_back(luaIdle.get<unsigned char>(i));
                  ai.stack(OFMechanics::AiWander(distance, duration, 0, idle, repeat), ptr, cancelOther);
              };
        selfAPI["_startAiTravel"] = [](SelfObject& self, const osg::Vec3f& target, bool repeat, bool cancelOther) {
            const OFWorld::Ptr& ptr = self.ptr();
            OFMechanics::AiSequence& ai = ptr.getClass().getCreatureStats(ptr).getAiSequence();
            ai.stack(OFMechanics::AiTravel(target.x(), target.y(), target.z(), repeat), ptr, cancelOther);
        };
        selfAPI["_enableLuaAnimations"] = [](SelfObject& self, bool enable) {
            const OFWorld::Ptr& ptr = self.ptr();
            OFBase::Environment::get().getMechanicsManager()->enableLuaAnimations(ptr, enable);
        };
    }

    LocalScripts::LocalScripts(LuaUtil::LuaState* lua, const LObject& obj, LuaUtil::ScriptTracker* tracker)
        : LuaUtil::ScriptsContainer(lua, "L" + obj.id().toString(), tracker, false)
        , mData(obj)
    {
        lua->protectedCall(
            [&](LuaUtil::LuaView& view) { addPackage("openfallout.self", sol::make_object(view.sol(), &mData)); });
        registerEngineHandlers({ &mOnActiveHandlers, &mOnInactiveHandlers, &mOnConsumeHandlers, &mOnActivatedHandlers,
            &mOnTeleportedHandlers, &mOnAnimationTextKeyHandlers, &mOnPlayAnimationHandlers, &mOnAnimationEndedHandlers,
            &mOnSkillUse, &mOnSkillLevelUp, &mOnJailTimeServed });
    }

    void LocalScripts::setActive(bool active, bool callHandlers)
    {
        mData.mIsActive = active;
        if (callHandlers)
        {
            if (active)
                callEngineHandlers(mOnActiveHandlers);
            else
                callEngineHandlers(mOnInactiveHandlers);
        }
    }

    void LocalScripts::applyStatsCache()
    {
        const auto& ptr = mData.ptr();
        for (auto& [stat, value] : mData.mStatsCache)
            stat(ptr, value);
        mData.mStatsCache.clear();
    }
}
