#include "detection.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Detection
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        /// Defaults of the executable for the settings that the files of the games do not have
        double get(const Setting& setting, std::string_view name, double fallback)
        {
            return setting ? setting(name, fallback) : fallback;
        }
    }

    double stealth(const Input& input, const Setting& setting)
    {
        const auto value = [&](std::string_view name, double fallback) { return get(setting, name, fallback); };

        // What the watcher's perception makes of the distance, raised or lowered by what it is doing
        const double skillMin = value("fSneakPerceptionSkillMin", 10);
        double skill = (value("fSneakPerceptionSkillMax", 90) - skillMin) * input.mPerception / 10.0 + skillMin;
        double ambush = 0;
        if (input.mAmbush > 0)
            ambush = value("fSneakAmbushTargetMod", 2.5);
        else if (input.mAmbush < 0)
            ambush = value("fSneakAmbushNonTargetMod", -0.25);
        skill *= (1 + value("fSneakAlertMod", 0.2) * (input.mAlert ? 1 : 0))
            + value("fSneakSleepBonus", -0.2) * (input.mAsleep ? 1 : 0)
            + value("fSneakCombatMod", -0.4) * (input.mInCombat ? 1 : 0) + ambush;

        // How near the two are, as a share of how far a watcher can tell
        double maxDistance = value("fSneakMaxDistance", 1500);
        if (input.mExterior)
            maxDistance *= value("fSneakExteriorDistanceMult", 2.0);
        double near = maxDistance > 0 ? std::max(0.0, (maxDistance - input.mDistance) / maxDistance) : 0.0;
        near = std::pow(near, value("fSneakDistanceAttenuationExponent", 2.0));
        double heard = near;
        if (!input.mLineOfSight)
            heard *= value("fSneakSoundLosMult", 0.25);

        // The noise of the boots of a target that moves
        double boots = input.mMoving
            ? value("fSneakBootWeightBase", 14) + value("fSneakBootWeightMult", 1.0) * input.mBootWeight
            : 0.0;
        if (input.mRunning)
            boots *= value("fSneakRunningMult", 2.0);
        const double sound = std::max(0.0, heard * value("fSneakSoundsMult", 1.6) * boots);

        // How well the target is seen: the light on it, more when it moves
        const double light = (input.mLight + value("fDetectionSneakLightMod", 0.0)) * value("fSneakLightMult", 1.4);
        const double movement = (1 + value("fSneakLightMoveMult", 0.01) * (input.mMoving ? 1 : 0))
            + value("fSneakLightRunMult", 0.2) * (input.mRunning ? 1 : 0);
        double seen = std::max(0.0, near * movement * light);
        if (input.mCloaked)
            seen *= value("fSneakStealthBoyMult", 0.0);

        const double hidden = input.mSneaking ? input.mSneakSkill : 0.0;
        const double skillPart = value("fSneakSkillMult", 0.5) * (skill * near - hidden);
        return value("fSneakBaseValue", -25) + sound + seen + skillPart;
    }

    Sighting sighting(double stealth, const Setting& setting)
    {
        if (stealth >= get(setting, "fSneakSeenMin", 0))
            return Sighting::Seen;
        if (stealth >= get(setting, "fSneakNoticedMin", -20))
            return Sighting::Noticed;
        return Sighting::Unseen;
    }

    bool inViewCone(double facing, double dx, double dy, double coneDegrees)
    {
        if (dx == 0 && dy == 0)
            return true;
        const double bearing = std::atan2(dx, dy);
        const double apart = std::abs(std::remainder(bearing - facing, 2 * pi));
        return apart <= coneDegrees * pi / 360.0;
    }

    double checkInterval(const Setting& setting)
    {
        return std::max(0.05, get(setting, "fDetectionTimerSetting", 0.3));
    }
}
