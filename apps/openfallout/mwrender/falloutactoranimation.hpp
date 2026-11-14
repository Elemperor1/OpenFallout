#ifndef OPENFALLOUT_MWRENDER_FALLOUTACTORANIMATION_H
#define OPENFALLOUT_MWRENDER_FALLOUTACTORANIMATION_H

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <osg/MatrixTransform>
#include <osg/Vec3f>

#include "animation.hpp"
#include "falloutanimation.hpp"

namespace OFRender
{
    /// What a character or creature of Fallout 3 and New Vegas has in common: a skeleton with models on it (the
    /// derived classes add those) that plays the idle animation of the skeleton's folder and walks and runs with the
    /// animations of its locomotion folder as it is moved
    class FalloutActorAnimation : public Animation
    {
    public:
        ~FalloutActorAnimation() override;

        /// Moves the time of the animation that plays on, as the frames of the scene do, and switches between standing,
        /// walking and running with the speed at which the actor is moved
        void advance(float duration);

        /// Whether the actor has an animation to walk with, and the speed (units a second) at which that animation
        /// carries it, which an actor that is moved at that speed does not slide at
        bool canWalk() const { return mGaits.mHasWalk; }
        float getWalkVelocity() const { return mGaits.mWalkVelocity; }

        /// The same for running
        bool canRun() const { return mGaits.mHasRun; }
        float getRunVelocity() const { return mGaits.mRunVelocity; }

        /// Whose movement decides whether this actor stands, walks or runs, when it is not the actor itself: a body
        /// that is drawn for a character that has none of its own (the player) takes the speed of that character's
        /// place from one frame to the next
        void setMotionSource(const OFWorld::Ptr& source) { mMotionSource = source; }

        /// The actor is dead: it stops in the pose it has and falls, a person on its back and a creature on its side,
        /// and lies where it fell (the games have it fall with a ragdoll of Havok, which this does not have). With
        /// `fallen` it lies there at once: it died before it was in sight.
        void die(bool fallen);
        bool isDown() const { return mFall != nullptr; }

        /// The actor takes a weapon in its hands: the model is put on its hand, the animation of the weapon's type
        /// takes it out and then holds it ready. The type is the animation type of the weapon record (0 to 10); a
        /// negative one is bare hands (creatures too), which only hold their hands ready. Any weapon it had is put away
        /// first.
        /// `weapon` is the form of the weapon, 0 for bare hands: its record has the sounds of taking it out and putting it
        /// away and the one that goes on while it is held out
        void drawWeapon(std::string_view model, int animationType, std::uint32_t weapon = 0);
        /// The weapon goes out of the hands
        void holsterWeapon();
        bool isWeaponDrawn() const { return mDrawn; }

        /// One attack with what it holds: a gun fires, a melee weapon or the hands swing in turn left and right. A gun
        /// whose projectile has a muzzle flash (the record's flash model and the seconds it lasts) flashes at its muzzle.
        void attack();
        /// Whether the actor is in the air (thrown off the ground by a blow, or falling): it plays the animation of a jump
        /// while it is, and the landing when it is not. The characters of the world have no physics to ask, so what moves
        /// them says; the body of the player asks the world.
        void setAirborne(bool airborne) { mAirborneTold = airborne; }
        /// A blow of the weapon (its form) hits the actor: the impact of the weapon's impact data set for the kind of
        /// body the actor has (flesh, the blood of an insect, metal for a robot) is shown on the node of the part that
        /// was hit (empty: on the body) for the seconds the impact lasts
        /// `toward` is the way from the actor toward the one that struck, in the world: the impact model lies on the body
        /// facing that way (the surface normal of the hit, which the games take from the ray)
        void showImpact(std::uint32_t weapon, const std::string& node, const osg::Vec3f& toward);
        /// A blow hits the actor and it reels, when the files have the animation for it
        void flinch();

        /// The actor sits, lies or leans in the furniture whose entry is of the kind (0: it does not), holding the pose
        /// that the files have for it. It is put in the place of the entry by whoever puts it in the furniture; the
        /// games play an animation to get in and to get out, which are not played here.
        void setPose(int entryKind);

        /// A script has the actor play an animation file once over what it does (the idle of a PlayIdle), after which
        /// it goes on as it was. False when the files have nothing to play on its skeleton, or it is down.
        bool playIdle(const std::string& file);

        /// What the armor that the actor wears adds to the sound of its steps (the executable's two extra sounds): 0
        /// nothing, 1 heavy armor, 2 power armor
        void setArmorSoundKind(int kind) { mArmorSounds = kind; }

        /// Whether the arms that the player sees through their own eyes are in the scene. The sounds that the text keys
        /// of an animation name come from the arms then, and from the body of the player (which keeps going when it is
        /// not drawn, for its steps) when they are not, so that no sound is made twice.
        static void setFirstPersonArmsShown(bool shown);

    protected:
        FalloutActorAnimation(
            const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);

        /// Plays the idle animation of the skeleton of the actor, when the game has one, and finds the animations with
        /// which it walks and runs. The animations of a female character are those of the folder for women, if there is
        /// one.
        void startAnimations(bool female);

        /// The skeleton that is not the actor's own: the arms of the first person have one of their own, with the
        /// animations beside it. It is the path that the object root was made from. Empty for the skeleton of the
        /// actor's model.
        std::string mSkeletonOverride;
        /// The arms that the player sees through their own eyes: their skeleton hangs from the camera and not from the
        /// floor, so it is not lowered to it
        bool mFirstPerson = false;

        /// Adds a model to the actor and returns the node that it is, null when there is no model. A model that is not
        /// skinned names the bone it is put on in its file; one that does not (a hat) and is for the head
        /// (`forHead`), with its vertices around the origin of the head, is put on the head bone.
        osg::ref_ptr<osg::Node> insertPart(std::string_view model, bool forHead = false);

    private:
        class AdvanceCallback;
        class FallCallback;
        class SoundKeys;

        /// What an animation does at a text key that names a sound or a foot coming down (`Sound: <editor id>`, and
        /// `Enum: Left`, `Right`, `BackLeft` and `BackRight`)
        void playTextKeySound(std::string_view text);
        /// The sound of a step of the foot (the value of the key) on the ground, with the armor's own sound
        void playFootstep(int foot);
        /// The sound of the creature for an event (the value of its `Enum` key, the type of its sound lists)
        void playCreatureSound(int type);
        /// The foley and armor sounds of what the actor wears (New Vegas: the value of the key `Enum: FoleyWalk` or
        /// `ArmorRun`, the type of the sounds that the armor and its audio template list)
        void playWornSound(int type);
        std::unique_ptr<SoundKeys> mSoundKeys;
        int mArmorSounds = 0;
        /// The form of the weapon that is out, 0 for none
        std::uint32_t mWeaponForm = 0;

        /// A model that is shown for a moment on a node (a muzzle flash, an impact) and the seconds it has left
        struct Effect
        {
            osg::ref_ptr<osg::Node> mNode;
            osg::ref_ptr<osg::Group> mParent;
            float mLeft = 0.f;
            bool mFlash = false;
            /// The time that the model's controllers (its particles, its textures) run by: it starts at 0 when the effect
            /// is made, as those of a spell's effect do
            std::shared_ptr<EffectAnimationTime> mTime;
        };
        std::vector<Effect> mEffects;
        /// Shows the model on the node for the seconds. With `facing` (a way in the world) the model is turned so that its
        /// +Z (the normal of an impact's splat) points that way, and put `lift` units out along it.
        void addEffect(const std::string& model, osg::Group* parent, float seconds, bool flash,
            const osg::Vec3f* facing = nullptr, float lift = 0.f);
        void updateEffects(float duration);
        void removeEffects(bool flashesOnly);
        /// The flash of the weapon's muzzle, when its projectile has one
        void flashMuzzle();
        /// Which of the impact data set's materials the actor's body is (0 stone ... 6 flesh, 10 insect, 4 metal)
        std::size_t impactMaterial() const;
        /// The sound that the weapon makes as it is taken out (the key `Enum: Equip` of its animation)
        void playWeaponHandled(bool equip);

        /// Adds the animation file to the actor and gives the name of its group, empty when it has nothing to play on
        /// this skeleton
        std::string addAnimation(const std::string& file, const std::string& skeleton);

        /// Adds the animation file of the weapon move, if there is one, and plays it once over the body above the waist
        /// at the priority. Empty when the files have none.
        std::string playWeaponMove(FalloutWeaponMove move, int priority, bool loop);

        /// Starts the idle again when a part of the body has no animation (the T of the skeleton shows)
        void watchPose(float duration);

        /// Says in the log where the bones of the actor stand a second into its idle (the first few actors only)
        void logPose(float duration);

        /// Says in the log, a second and five seconds after a weapon was taken out, whether it is still on the hand and
        /// which way it points (the first few weapons only): round 14E found the pistol of the player in its hand
        /// pointing across the body at rest, and gone while it walked
        void logWeapon(float duration);
        /// The same, at once, when the gait of an actor with a weapon out changes
        void sayWeapon(const char* when);

        /// The gait that the speed at which the actor was moved calls for
        void updateGait(float duration);
        void setGait(FalloutGait gait);
        /// The animation for the way the actor moves now (the gait, the direction it goes in relative to where it
        /// faces, whether it sneaks, the weapon it holds): the old one stops and the new one plays over the idle
        void updateMovement();

        /// Tells the scripts what the actor does now (IsMoving, IsRunning, IsTurning, IsSwimming, GetAnimAction,
        /// IsIdlePlaying, GetIdleDoneOnce, GetWalkSpeed): once a frame, after the animations have run
        void publishActivity();
        /// The weapon move that plays and the action of the games' numbers it is (FalloutScript::AnimAction), for
        /// GetAnimAction; the group is empty when none plays
        void startAction(int action, const std::string& group);
        int mActionKind = -1;
        std::string mActionGroup;
        /// The group of the animation that a script had the actor play last, and whether it has played to its end
        /// since it began
        std::string mScriptIdleGroup;
        bool mScriptIdleWasPlaying = false;
        bool mScriptIdleDone = false;
        /// Whether the body is in the water (the player's body asks the world)
        bool mSwimming = false;
        /// The reference that the scripts were told of it by (0 when they were not): it is forgotten with the animation
        std::uint32_t mReported = 0;

        osg::ref_ptr<AdvanceCallback> mAdvanceCallback;
        /// The transform that the body falls with, when it has died
        osg::ref_ptr<osg::MatrixTransform> mFall;

        /// The weapon in the hands: the model on the hand, the animation type of the record and the move it makes
        osg::ref_ptr<osg::Node> mWeaponNode;
        /// The bone of the spine (IsFacingUp and IsLeftUp), looked for once the body is there
        osg::ref_ptr<osg::Node> mSpineNode;
        bool mSpineSearched = false;
        osg::ref_ptr<osg::Group> mWeaponParent;
        std::string mAimGroup;
        std::map<std::string, std::string> mWeaponGroups;
        std::string mSkeletonPath;
        int mWeaponType = -1;
        unsigned mAttacks = 0;
        bool mDrawn = false;

        /// The pose it holds in furniture: the kind of the entry and the group that plays
        int mPoseKind = 0;
        std::string mPoseGroup;
        bool mPoseLostReported = false;
        std::map<std::string, std::string> mPoseGroups;
        /// The animations that scripts had it play, by file
        std::map<std::string, std::string> mIdleGroups;
        /// The group of the idle it stands in, which a script does not play over
        std::string mDefaultIdleGroup;

        /// How long a part of the body has had no animation, and whether that has been said
        float mBareTime = 0.f;
        bool mBareReported = false;
        /// Whether the idle was looked at for a part of the body that it has nothing for
        bool mCoverageChecked = false;
        /// How long the idle has played, and whether the bones were said
        float mPoseTime = 0.f;
        bool mPoseLogged = false;
        /// Seconds since the weapon was taken out, and how many of its two looks were made
        float mWeaponTime = 0.f;
        int mWeaponLooks = 0;

        OFWorld::Ptr mMotionSource;
        std::string mWalkGroup;
        std::string mRunGroup;
        /// The files of the forward walk and run (those of mWalkGroup and mRunGroup), the folder of the skeleton, the
        /// sex of the actor, the groups made of the files of the other ways to move (by file), and the one that plays
        /// as the movement now with the file it was made from (empty when the actor stands in its idle)
        std::string mWalkFile;
        std::string mRunFile;
        std::string mMoveFolder;
        bool mFemale = false;
        std::map<std::string, std::string> mMoveGroups;
        std::string mMoveGroup;
        std::string mMoveFile;
        /// What the file was chosen for (gait, direction, sneaking, weapon prefix), so that it is not chosen again each
        /// frame
        struct MoveKey
        {
            FalloutGait mGait = FalloutGait::Idle;
            FalloutDirection mDirection = FalloutDirection::Forward;
            bool mSneak = false;
            std::string mPrefix;
            bool mValid = false;
            /// A leg of the character is crippled: it limps
            bool mHurt = false;
            /// 0 the gaits, 1 in the air, 2 landing, 3 swimming, 4 and 5 turning to the left and to the right where it
            /// stands
            int mSpecial = 0;
            bool operator==(const MoveKey&) const = default;
        };
        MoveKey mMoveKey;
        /// What the mechanics told of the air (`setAirborne`), how long the player's body has been off the ground, whether
        /// it was in the air the frame before, the seconds that its landing has left and the length of the last frame
        bool mAirborneTold = false;
        float mAirTime = 0.f;
        bool mWasAirborne = false;
        float mLandLeft = 0.f;
        float mFrameTime = 0.f;
        /// How fast the actor turns (radians a second, clockwise positive, smoothed), the way it was turning last and the
        /// seconds that the turn has left to show (a short turn plays its animation a moment, not for a flicker), and
        /// the yaw of the frame before
        float mTurnRate = 0.f;
        int mTurning = 0;
        float mTurnLeft = 0.f;
        float mLastYaw = 0.f;
        bool mHasLastYaw = false;
        /// How fast the actor goes ahead of where it faces and to its right (units a second, smoothed), and the
        /// direction that makes
        float mForwardSpeed = 0.f;
        float mSideSpeed = 0.f;
        FalloutDirection mDirection = FalloutDirection::Forward;
        /// Whether the actor sneaked the last time it was asked, and how many movement files were said in the log
        bool mSneaking = false;
        int mMovesSaid = 0;
        int mSpecialsSaid = 0;
        FalloutGaits mGaits;
        FalloutGait mGait = FalloutGait::Idle;
        /// The speed (units a second, across the ground) at which the actor is moved, smoothed over a few frames
        float mSpeed = 0.f;
        osg::Vec3f mLastPosition;
        bool mHasLastPosition = false;
    };
}

#endif
