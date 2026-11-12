#include "falloutscripts.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>

#include <components/debug/debuglog.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/detection.hpp>
#include <components/sceneutil/util.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/movement.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/refdata.hpp"

namespace OFScript
{
    namespace
    {
        /// How bright the sky is for a character outside, from 0 to 100, by the hour of the day. The weather is not
        /// looked at: the games read the light that falls on the character.
        double daylight(float hour)
        {
            constexpr double night = 20.0;
            if (hour >= 7.f && hour <= 18.f)
                return 100.0;
            if (hour < 5.f || hour > 20.f)
                return night;
            // The hours of dawn and dusk go from the dark to the light and back
            const double share = hour < 7.f ? (hour - 5.f) / 2.0 : (20.f - hour) / 2.0;
            return night + (100.0 - night) * share;
        }

        constexpr float eyeHeight = 110.f;

        /// Whether nothing of the world is between the eyes of the two
        bool lineOfSight(const osg::Vec3f& from, const osg::Vec3f& to)
        {
            const osg::Vec3f eyes(from.x(), from.y(), from.z() + eyeHeight);
            const osg::Vec3f goal(to.x(), to.y(), to.z() + eyeHeight);
            const float apart = (goal - eyes).length();
            return apart < 1.f
                || OFBase::Environment::get().getWorld()->getDistToNearestRayHit(eyes, goal - eyes, apart)
                >= apart - 1.f;
        }
    }

    double FalloutScripts::lightOn(const OFWorld::Ptr& target)
    {
        const OFWorld::CellStore* cell = target.isInCell() ? target.getCell() : nullptr;
        if (cell == nullptr)
            return 0.0;
        if (cell->isExterior())
            return daylight(OFBase::Environment::get().getWorld()->getTimeStamp().getHour());
        // Inside the light of the lamps is not counted: the ambient light of the cell stands for it
        const osg::Vec4f ambient = SceneUtil::colourFromRGB(cell->getCell()->getMood().mAmbiantColor);
        return 100.0 * std::clamp(0.2126 * ambient.r() + 0.7152 * ambient.g() + 0.0722 * ambient.b(), 0.0, 1.0);
    }

    FalloutScript::Detection::Sighting FalloutScripts::sightOf(const OFWorld::Ptr& watcher, const OFWorld::Ptr& target)
    {
        namespace AV = FalloutScript::ActorValue;
        using FalloutScript::Detection::Sighting;
        const FalloutScript::FormId self = scriptReferenceOf(watcher);
        const FalloutScript::FormId other = scriptReferenceOf(target);
        if (self == 0 || other == 0 || !watcher.isInCell() || !target.isInCell())
            return Sighting::Unseen;

        const osg::Vec3f from = watcher.getRefData().getPosition().asVec3();
        const osg::Vec3f to = target.getRefData().getPosition().asVec3();
        FalloutScript::Detection::Input input;
        input.mPerception = mActorValues.current(self, AV::perception);
        input.mInCombat = mCombat.inCombat(self);
        input.mDistance = (to - from).length();
        input.mExterior = watcher.getCell()->isExterior();
        const bool visible = lineOfSight(from, to);
        input.mLineOfSight = visible;
        input.mCloaked
            = mActorValues.current(other, AV::chameleon) > 0 || mActorValues.current(other, AV::invisibility) > 0;
        input.mSneakSkill = mActorValues.current(other, AV::sneak);
        input.mLight = lightOn(target);
        if (other == mPlayer)
        {
            OFBase::MechanicsManager* mechanics = OFBase::Environment::get().getMechanicsManager();
            const OFMechanics::Movement& movement = target.getClass().getMovementSettings(target);
            input.mMoving = movement.mPosition[0] != 0.f || movement.mPosition[1] != 0.f;
            input.mRunning = input.mMoving && mechanics->isRunning(target);
            input.mSneaking = mechanics->isSneaking(target);
        }

        const FalloutScript::Detection::Setting setting
            = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
        const double stealth = FalloutScript::Detection::stealth(input, setting);
        const Sighting sighting = FalloutScript::Detection::sighting(stealth, setting);
        // What is high enough to be seen is only heard when the target is behind the watcher or behind a wall
        const double facing = watcher.getRefData().getPosition().rot[2];
        const double cone = gameSetting("fDetectionViewCone", 190);
        const bool inCone
            = FalloutScript::Detection::inViewCone(facing, to.x() - from.x(), to.y() - from.y(), cone);
        const Sighting result = sighting == Sighting::Seen && (!visible || !inCone) ? Sighting::Noticed : sighting;
        // What a watcher made of the player, said now and then for the ones that are near (round 14A: a sneaking player
        // at 3 units from an armed character that never noticed it, and nothing said why)
        if (other == mPlayer && input.mDistance < 2500)
        {
            static std::map<FalloutScript::FormId, std::chrono::steady_clock::time_point> told;
            const auto now = std::chrono::steady_clock::now();
            auto& last = told[self];
            if (last == std::chrono::steady_clock::time_point() || now - last > std::chrono::seconds(3))
            {
                last = now;
                static const char* const names[] = { "does not see it", "notices it", "sees it" };
                Log(Debug::Info) << FalloutScript::hex(self) << " and the player, " << std::lround(input.mDistance)
                                 << " units apart: stealth " << stealth << ", " << names[static_cast<int>(result)]
                                 << " (perception " << input.mPerception << ", light " << input.mLight << ", sneaks "
                                 << input.mSneaking << ", moves " << input.mMoving << ", runs " << input.mRunning
                                 << ", line of sight " << visible << ", in its view " << inCone << ")";
            }
        }
        return result;
    }

    FalloutScript::Detection::Sighting FalloutScripts::sightOfPlayer(const OFWorld::Ptr& watcher)
    {
        return sightOf(watcher, OFBase::Environment::get().getWorld()->getPlayerPtr());
    }

    double FalloutScripts::detectionInterval()
    {
        return FalloutScript::Detection::checkInterval(
            [this](std::string_view name, double fallback) { return gameSetting(name, fallback); });
    }
}
