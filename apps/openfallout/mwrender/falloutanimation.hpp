#ifndef OPENFALLOUT_MWRENDER_FALLOUTANIMATION_H
#define OPENFALLOUT_MWRENDER_FALLOUTANIMATION_H

#include <string>
#include <string_view>
#include <vector>

namespace OFRender
{
    /// The folder with the animation files (.kf) of a character of Fallout 3 or New Vegas: the folder of its skeleton,
    /// with a slash at the end, lower case. The path is one under the virtual file system (`meshes/...`). A path with
    /// no folder gives an empty one.
    std::string falloutAnimationFolder(std::string_view skeleton);

    /// The animation a character plays when it stands, chosen among the paths of the files in the folder of its
    /// skeleton (lower case, as the virtual file system lists them, files of folders below that one included): the file
    /// `mtidle.kf`, else `idle.kf`, else `h2hidle.kf`, else the first of the files in the folder itself (in the order
    /// of their names) that has `idle` in its name. Empty when there is none.
    std::string chooseFalloutIdle(std::string_view folder, const std::vector<std::string>& files);

    /// The animation files with which a character walks and runs forward
    struct FalloutLocomotion
    {
        std::string mWalk;
        std::string mRun;
    };

    /// The files `mtforward.kf` (walking) and `mtfastforward.kf` (running) among the paths of the files in the folder
    /// of the skeleton of a character (lower case, as the virtual file system lists them, files of folders below that
    /// one included). They are in the folder `locomotion` below that of the skeleton or in one below it: a folder named
    /// for the sex of the character comes first, then `locomotion` itself, then any other, except those named for
    /// children and for the hurt, which hold the movements of those. Some creatures name the movements otherwise:
    /// when the usual file is not there, the walk is `mtfoward` (a misspelling in the files of the deathclaw),
    /// `forwardwalk`, `forward` or `h2hforward`, the run `fastforward` or `h2hfastforward`, and when none of those is
    /// there, the first file whose name has "forward" in it (and "fast" in it for the run, and not "fast", "run" or
    /// "sprint" for the walk). Empty for a file that is not there.
    FalloutLocomotion chooseFalloutLocomotion(
        std::string_view folder, const std::vector<std::string>& files, bool female);

    enum class FalloutGait
    {
        Idle,
        Walk,
        Run,
    };

    /// What the animations a character has to move with look like
    struct FalloutGaits
    {
        bool mHasWalk = false;
        bool mHasRun = false;
        /// The units a second that the root of the animation travels, 0 when the file does not say
        float mWalkVelocity = 0.f;
        float mRunVelocity = 0.f;
    };

    /// The gait of a character that moves at a speed (units a second), from the gait it is in: it starts to move above
    /// a few units a second and stops below fewer, and runs above the speed half way between those of the animations
    /// (a little more to start, a little less to stop), or above a fixed speed when an animation does not say. A
    /// character that has only one of the two animations moves with that one.
    FalloutGait chooseFalloutGait(float speed, FalloutGait current, const FalloutGaits& gaits);

    /// How fast to play an animation whose root travels at `velocity` units a second so that the feet keep up with a
    /// character that moves at `speed`: the ratio of the two, kept within what looks right, 1 when the animation has no
    /// velocity
    float falloutAnimationSpeed(float speed, float velocity);

    /// The letters in front of the names of the animation files of a weapon, by the animation type of its record
    /// (0 to 10: bare hands, one-handed and two-handed melee, one-handed pistol, two-handed rifle, automatic, handle,
    /// launcher, grenade throw and the two mines, as FalloutNV.exe lists them at 0x11977A8). A negative type is a
    /// character with no weapon, which uses the hands. Empty for a type that is none of those.
    std::string_view falloutWeaponPrefix(int animationType);

    /// What a character does with the weapon in its hands
    enum class FalloutWeaponMove
    {
        /// One attack: the files differ with the type, and a melee weapon takes them in turn (`counter` counts them)
        Attack,
        /// Taking the weapon out
        Equip,
        /// Putting it away
        Unequip,
        /// Holding it ready, in a loop
        Aim,
        /// Reeling from a blow
        Recoil,
    };

    /// The animation file of a character for a move with a weapon, among the paths of the files in the folder of its
    /// skeleton (lower case, as the virtual file system lists them): the file named by the prefix of the weapon type
    /// and the move (`1hpattack3.kf`, `h2hattackleft_a.kf`, `2hrequip.kf`). Guns attack with `attack3` and, when the
    /// files have no such, the next of `attack4` to `attack8`; a melee weapon and bare hands take, in turn, the files
    /// of the left and the right blow (`attackleft_a`, `attackright_a`, then the second variants and those without a
    /// variant); a thrown weapon takes `attackthrow` and its numbered forms. Empty when the files have none.
    std::string chooseFalloutWeaponAnimation(std::string_view folder, int animationType, FalloutWeaponMove move,
        const std::vector<std::string>& files, unsigned counter = 0);
}

#endif
