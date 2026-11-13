#ifndef OPENFALLOUT_MWSOUND_FALLOUTFOOTSTEPS_H
#define OPENFALLOUT_MWSOUND_FALLOUTFOOTSTEPS_H

#include <optional>

#include <components/falloutscript/footsteps.hpp>

#include "../mwworld/ptr.hpp"

namespace OFSound
{
    /// One foot coming down, as the animation of a character says it (the text keys `Enum: Left`, `Right`, `BackLeft`
    /// and `BackRight`)
    struct FootstepRequest
    {
        OFWorld::Ptr mActor;
        FalloutScript::Footsteps::Foot mFoot = FalloutScript::Footsteps::Foot::Left;
        bool mSneaking = false;
        bool mRunning = false;
        /// The player seen through their own eyes: its steps have the fixed volume of the INI
        bool mPlayerInFirstPerson = false;
        /// What the armor adds: 0 nothing, 1 heavy armor, 2 power armor
        int mArmor = 0;
        /// The slot of the sound when the way the character moves does not decide it: landing and jumping
        std::optional<FalloutScript::Footsteps::Slot> mSlot;
    };

    /// What the armor that a character wears adds to its steps and jumps: 0 nothing, 1 heavy armor, 2 power armor (any
    /// piece of power armor makes it power armor)
    int armorSoundKind(const OFWorld::Ptr& actor);

    /// Plays the step as the executable does (FalloutNV.exe 0x837CC0): the sound of the surface the foot is on, in the
    /// slot of the way the character moves, with the sound of heavy or power armor over it. Nothing is played for a
    /// character farther from the listener than the INI's distance of steps.
    void playFootstep(const FootstepRequest& request);
}

#endif
