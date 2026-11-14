#include "falloutscripts.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/vatsvalue.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwphysics/raycasting.hpp"
#include "../mwrender/camera.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        namespace VV = FalloutScript::VatsValue;

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        /// What an argument that is a form or a number is as the 32 bits the values compare (a part of -1 is 0xFFFFFFFF)
        std::uint32_t wordArgument(const FalloutScript::CallContext& call, std::size_t index)
        {
            const double value = FalloutScript::argument(call, index);
            if (std::isnan(value))
                return 0;
            if (value < 0)
                return static_cast<std::uint32_t>(static_cast<std::int32_t>(std::max(value, -2147483648.0)));
            return value < 4294967296.0 ? static_cast<std::uint32_t>(value) : 0u;
        }

        /// The forward direction of a heading (the angle about the vertical axis, in radians; 0 looks along +y)
        osg::Vec3f headingDirection(double heading)
        {
            return osg::Vec3f(static_cast<float>(std::sin(heading)), static_cast<float>(std::cos(heading)), 0.f);
        }

        /// How far the world lets a ray go from a point, up to `length`; `length` when it meets nothing
        float rayDistance(const osg::Vec3f& from, const osg::Vec3f& direction, float length)
        {
            return OFBase::Environment::get().getWorld()->getDistToNearestRayHit(from, direction, length);
        }

        /// The direction the camera looks in (its orientation turns +y, as the renderer's view matrix does)
        osg::Vec3f cameraDirection(const OFRender::Camera& camera)
        {
            const osg::Vec3d forward = camera.getOrient() * osg::Vec3d(0.0, 1.0, 0.0);
            return osg::Vec3f(static_cast<float>(forward.x()), static_cast<float>(forward.y()),
                static_cast<float>(forward.z()));
        }

        /// The farthest the camera ray of the reticle commands goes (the executable's limit, 50000 units)
        constexpr float maxReticleRange = 50000.f;
    }

    void FalloutScripts::setVatsValue(int mode, const VV::Action& action)
    {
        const std::lock_guard lock(mVatsValueMutex);
        mVatsValueMode = mode;
        mVatsValueAction = action;
    }

    void FalloutScripts::clearVatsValue()
    {
        const std::lock_guard lock(mVatsValueMutex);
        mVatsValueMode = VV::None;
        mVatsValueAction = VV::Action();
    }

    VV::Action FalloutScripts::vatsActionFor(
        const OFWorld::Ptr& target, FalloutScript::FormId weapon, int group, double distance)
    {
        VV::Action action;
        // The kind of an attack the player queued on a target (FalloutNV.exe 0x9CA2C0 writes it)
        action.mKind = 0x12;
        action.mHasTarget = !target.isEmpty();
        action.mPart = group;
        action.mWeapon = weapon;
        action.mDistance = distance;
        if (!target.isEmpty())
        {
            const FalloutScript::FormId reference = scriptReferenceOf(target);
            action.mTargetReference = reference;
            FalloutScript::FormId base = 0;
            if (reference != 0 && baseFormOf(reference, base))
                action.mTargetBase = base;
        }
        if (weapon != 0)
            if (const ESM4::Weapon* record = mStore.get<ESM4::Weapon>().search(refId(weapon));
                record != nullptr && record->mHasGameData)
                action.mWeaponKind = record->mGame.mAnimationType;
        action.mAmmo = weaponStats(mPlayer).mAmmo;
        return action;
    }

    void FalloutScripts::setUpVatsCommands()
    {
        // GetVATSValue (FalloutNV.exe 0x594E40): a question about the attack that V.A.T.S. is playing, or the one that the
        // player is choosing; 0 whenever there is none
        mInterpreter.setHandler("GetVATSValue", [this](FalloutScript::CallContext& call) {
            const int index = FalloutScript::intArgument(call, 0);
            const std::uint32_t form = wordArgument(call, 1);
            int mode = 0;
            VV::Action action;
            {
                const std::lock_guard lock(mVatsValueMutex);
                mode = mVatsValueMode;
                action = mVatsValueAction;
            }
            return VV::value(mode, &action, index, form, [this](std::uint32_t list, std::uint32_t item) {
                return mFormLists.contains(list, item);
            });
        });

        // SetVATSTarget (0x5DAAE0): the reference is made a target of V.A.T.S. (1) or not (0), whatever its record says
        mInterpreter.setHandler("SetVATSTarget", [this](FalloutScript::CallContext& call) {
            if (call.mReference == 0)
                return 0.0;
            const bool wanted = FalloutScript::intArgument(call, 0) != 0;
            // The record of a character or creature takes part in V.A.T.S. unless the reference is turned round; the port
            // has no target for anything else, which has the mark set for a 1 (the record says no)
            const bool record = isActor(call.mReference);
            const std::lock_guard lock(mVatsValueMutex);
            mVatsMarks.set(call.mReference, VV::targetMark(wanted, record));
            return 0.0;
        });

        // GetVatsTargetHeight (0x5A25F0): in the playback of an attack, how high above the player the part that is aimed at
        // is. The port has the height of the middle of the target's box, which it takes for the part's node.
        mInterpreter.setHandler("GetVatsTargetHeight", [this](FalloutScript::CallContext&) {
            int mode = 0;
            VV::Action action;
            {
                const std::lock_guard lock(mVatsValueMutex);
                mode = mVatsValueMode;
                action = mVatsValueAction;
            }
            if (mode != VV::Playback || !action.mHasTarget)
                return 0.0;
            const OFWorld::Ptr target = findReference(action.mTargetReference);
            const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
            if (target.isEmpty() || !target.isInCell() || player.isEmpty())
                return 0.0;
            const osg::Vec3f half = OFBase::Environment::get().getWorld()->getHalfExtents(target);
            const double node = target.getRefData().getPosition().pos[2] + half.z();
            return VV::targetHeight(
                mode, VV::isAttackKind(action.mKind), true, node, player.getRefData().getPosition().pos[2]);
        });

        // The areas around an actor (0x8BD830, 0x8BDBD0): a ray from the actor's position raised by
        // fVATSSmartCameraCheckHeight, along its heading turned a quarter, a half or none, 10000 units long; the free
        // area is how far it goes before the world stops it
        const struct
        {
            const char* mFree;
            const char* mVisible;
            VV::Side mSide;
        } sides[] = {
            { "GetVATSFrontAreaFree", "GetVATSFrontTargetVisible", VV::Side::Front },
            { "GetVATSBackAreaFree", "GetVATSBackTargetVisible", VV::Side::Back },
            { "GetVATSLeftAreaFree", "GetVATSLeftTargetVisible", VV::Side::Left },
            { "GetVATSRightAreaFree", "GetVATSRightTargetVisible", VV::Side::Right },
        };
        for (const auto& entry : sides)
        {
            const VV::Side side = entry.mSide;
            mInterpreter.setHandler(entry.mFree, [this, side](FalloutScript::CallContext& call) {
                const OFWorld::Ptr actor = call.mReference != 0 ? findReference(call.mReference) : OFWorld::Ptr();
                if (actor.isEmpty() || !actor.isInCell())
                    return 0.0;
                const osg::Vec3f from = actor.getRefData().getPosition().asVec3()
                    + osg::Vec3f(0.f, 0.f, static_cast<float>(gameSetting("fVATSSmartCameraCheckHeight", 64.0)));
                const double heading = actor.getRefData().getPosition().rot[2] + VV::sideAngle(side);
                const float length = static_cast<float>(VV::areaRayLength);
                const float reached = rayDistance(from, headingDirection(heading), length);
                // (the world answers the length for a ray that meets nothing)
                return VV::freeArea(reached >= length ? -1.0 : reached / length);
            });
            mInterpreter.setHandler(entry.mVisible, [this, side](FalloutScript::CallContext& call) {
                const OFWorld::Ptr actor = call.mReference != 0 ? findReference(call.mReference) : OFWorld::Ptr();
                const OFWorld::Ptr target = findReference(FalloutScript::formArgument(call, 0));
                if (actor.isEmpty() || !actor.isInCell() || target.isEmpty() || !target.isInCell())
                    return 0.0;
                const double height = gameSetting("fVATSSmartCameraCheckHeight", 64.0);
                const double stepDistance = gameSetting("fVATSSmartCameraCheckStepDistance", 64.0);
                const double stepCount = gameSetting("fVATSSmartCameraCheckStepCount", 5.0);
                const osg::Vec3f start = actor.getRefData().getPosition().asVec3()
                    + osg::Vec3f(0.f, 0.f, static_cast<float>(height));
                const osg::Vec3f along = headingDirection(actor.getRefData().getPosition().rot[2] + VV::sideAngle(side));
                const osg::Vec3f goal = target.getRefData().getPosition().asVec3()
                    + osg::Vec3f(0.f, 0.f, static_cast<float>(height));
                return VV::visibleDistance(stepDistance, stepCount, [&](int step) {
                    const osg::Vec3f from = start + along * static_cast<float>((step + 1) * stepDistance);
                    const osg::Vec3f toGoal = goal - from;
                    const float apart = toGoal.length();
                    return apart < 1.f || rayDistance(from, toGoal, apart) >= apart - 1.f;
                });
            });
        }

        // GetThreatRatio (a reference command with an actor): how dangerous the other is, by level, health and the armor
        // that stops part of a blow, compared with the actor itself. The formula is the executable's (the threat of one
        // actor); which of the two goes on top is not read.
        mInterpreter.setHandler("GetThreatRatio", [this](FalloutScript::CallContext& call) {
            namespace AV = FalloutScript::ActorValue;
            const auto threatOf = [this](FalloutScript::FormId actor) {
                int level = 0;
                levelOf(actor, mProgress.level(), level);
                return VV::threat(level, mActorValues.current(actor, AV::health), mEquipment.damageThreshold(actor));
            };
            const FalloutScript::FormId other = FalloutScript::formArgument(call, 0);
            if (call.mReference == 0 || other == 0)
                return 0.0;
            return VV::threatRatio(threatOf(call.mReference), threatOf(other));
        });

        // ObjectUnderReticle [range]: the reference of the object the camera looks at, 0 when it sees none within the range
        mInterpreter.setHandler("ObjectUnderReticle", [this](FalloutScript::CallContext& call) {
            const double asked = FalloutScript::argument(call, 0);
            const float range = asked > 0 ? static_cast<float>(std::min<double>(asked, maxReticleRange)) : maxReticleRange;
            OFBase::World* world = OFBase::Environment::get().getWorld();
            OFRender::Camera* camera = world->getCamera();
            if (camera == nullptr)
                return 0.0;
            const osg::Vec3f direction = cameraDirection(*camera);
            const osg::Vec3f from(camera->getPosition().x(), camera->getPosition().y(), camera->getPosition().z());
            OFPhysics::RayCastingResult result;
            const OFWorld::Ptr player = world->getPlayerPtr();
            std::vector<OFWorld::Ptr> ignore;
            if (!player.isEmpty())
                ignore.push_back(player);
            world->castRenderingRay(result, from, from + direction * range, false, false, false, ignore);
            if (!result.mHit || result.mHitObject.isEmpty())
                return 0.0;
            return static_cast<double>(referenceOf(result.mHitObject));
        });

        // PlaceAtReticle form [count]: makes the object where the camera looks. The port puts it in front of the player, at
        // the distance along the ground at which the ray meets the world (the second and third numbers are not read).
        mInterpreter.setHandler("PlaceAtReticle", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId base = FalloutScript::formArgument(call, 0);
            const int count = std::max(call.mArguments.size() > 1 ? FalloutScript::intArgument(call, 1) : 1, 1);
            OFBase::World* world = OFBase::Environment::get().getWorld();
            OFRender::Camera* camera = world->getCamera();
            if (base == 0 || camera == nullptr || mPlayer == 0)
                return 0.0;
            const osg::Vec3f direction = cameraDirection(*camera);
            const osg::Vec3f from(camera->getPosition().x(), camera->getPosition().y(), camera->getPosition().z());
            const OFWorld::Ptr player = world->getPlayerPtr();
            std::vector<OFWorld::Ptr> ignore;
            if (!player.isEmpty())
                ignore.push_back(player);
            OFPhysics::RayCastingResult result;
            world->castRenderingRay(result, from, from + direction * maxReticleRange, false, false, false, ignore);
            const osg::Vec3f at = result.mHit ? result.mHitPos : from + direction * 300.f;
            const osg::Vec3f feet = player.isEmpty() ? from : player.getRefData().getPosition().asVec3();
            const double along = std::hypot(at.x() - feet.x(), at.y() - feet.y());
            return static_cast<double>(placeAtMe(mPlayer, base, count, along, 0));
        });

        // IsPlayerGrabbedRef ref / GetPlayerGrabbedRef: the port has no grabbing, so nothing is held (the executable
        // answers from the player's grab state)
        mInterpreter.setHandler("IsPlayerGrabbedRef", [](FalloutScript::CallContext&) { return 0.0; });
        mInterpreter.setHandler("GetPlayerGrabbedRef", [](FalloutScript::CallContext&) { return 0.0; });

        // ForceTakeCover actor distance: a stand-in. The AI of the port has no cover to take, so the order is told once
        // and the actor goes on as it was.
        mInterpreter.setHandler("ForceTakeCover", [this](FalloutScript::CallContext& call) {
            static std::atomic<int> sTold{ 0 };
            if (sTold.fetch_add(1) < 3)
                log("ForceTakeCover on " + FalloutScript::hex(call.mReference) + ": the AI has no cover, so it does nothing");
            return 0.0;
        });

        // EssentialDeathReload: the game offers to go back to the last save (what the death of an essential character does
        // in the executable's story scripts)
        mInterpreter.setHandler("EssentialDeathReload", [this](FalloutScript::CallContext&) {
            runOnMainThread([] {
                if (OFBase::StateManager* states = OFBase::Environment::get().getStateManager())
                    states->askLoadRecent();
            });
            return 0.0;
        });
    }
}
