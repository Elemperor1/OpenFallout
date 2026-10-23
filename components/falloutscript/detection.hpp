#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DETECTION_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_DETECTION_H

#include <functional>
#include <string_view>

namespace FalloutScript::Detection
{
    /// The value of a game setting by its name, the second argument when the files have none
    using Setting = std::function<double(std::string_view name, double fallback)>;

    /// What the stealth equation of the games is given about a watcher and the character it looks for
    struct Input
    {
        /// The Perception of the watcher
        double mPerception = 5;
        bool mAlert = false;
        bool mAsleep = false;
        bool mInCombat = false;
        /// 1 when the watcher is the target of an ambush, -1 when another character is, 0 when there is none
        int mAmbush = 0;
        /// Units between the two
        double mDistance = 0;
        bool mExterior = false;
        bool mLineOfSight = true;
        bool mMoving = false;
        bool mRunning = false;
        bool mSneaking = false;
        /// The weight class of the boots of the target
        double mBootWeight = 0;
        /// How brightly the target is lit, from 0 (dark) to 100
        double mLight = 0;
        /// The target is cloaked (a Stealth Boy)
        bool mCloaked = false;
        /// The Sneak skill of the target
        double mSneakSkill = 0;
    };

    /// The stealth value of the target for the watcher: the games see it at 0 or more (fSneakSeenMin) and only notice
    /// it from fSneakNoticedMin. It follows the equation of the executable (FalloutNV.exe 0x642ED0):
    ///   S = fSneakBaseValue + sound + visibility + fSneakSkillMult * (skill * distance - hidden)
    /// with the terms as the Mac read them. What the executable gives to the equation as "inputs" that were not traced
    /// (two multipliers of the visibility, the noise of an action, the two 16-bit values A and B of the skill term) is
    /// 1, 1, 0 and, for A*B, the Sneak skill of a target that sneaks. The size term is left out (the executable
    /// compares a size with 135 and the unit of it is not known). The line of sight only lowers the sound term; the
    /// view cone and the line of sight are asked after it, as the executable does, and decide whether a stealth value
    /// that is high enough is seen or only heard.
    double stealth(const Input& input, const Setting& setting);

    enum class Sighting
    {
        /// The watcher does not see or hear the target
        Unseen,
        /// It heard or noticed something (between fSneakNoticedMin and fSneakSeenMin)
        Noticed,
        /// It sees the target (at fSneakSeenMin or more)
        Seen,
    };

    Sighting sighting(double stealth, const Setting& setting);

    /// Whether a target lies in the cone of view of a watcher that faces `facing` radians (0 faces the y axis and the
    /// angle grows towards the x axis, as the games turn things), with a cone of `coneDegrees` degrees across
    /// (fDetectionViewCone), for the target at the offset (dx, dy) from the watcher
    bool inViewCone(double facing, double dx, double dy, double coneDegrees);

    /// The seconds between the checks of a watcher (fDetectionTimerSetting)
    double checkInterval(const Setting& setting);
}

#endif
