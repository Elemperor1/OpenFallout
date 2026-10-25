#ifndef OPENFALLOUT_MWRENDER_FALLOUTACTORANIMATION_H
#define OPENFALLOUT_MWRENDER_FALLOUTACTORANIMATION_H

#include <map>
#include <string>
#include <string_view>

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

        /// The actor is dead: it stops in the pose it has and falls, a person on its back and a creature on its side,
        /// and lies where it fell (the games have it fall with a ragdoll of Havok, which this does not have). With
        /// `fallen` it lies there at once: it died before it was in sight.
        void die(bool fallen);
        bool isDown() const { return mFall != nullptr; }

        /// The actor takes a weapon in its hands: the model is put on its hand, the animation of the weapon's type
        /// takes it out and then holds it ready. The type is the animation type of the weapon record (0 to 10); a
        /// negative one is bare hands (creatures too), which only hold their hands ready. Any weapon it had is put away
        /// first.
        void drawWeapon(std::string_view model, int animationType);
        /// The weapon goes out of the hands
        void holsterWeapon();
        bool isWeaponDrawn() const { return mDrawn; }

        /// One attack with what it holds: a gun fires, a melee weapon or the hands swing in turn left and right
        void attack();
        /// A blow hits the actor and it reels, when the files have the animation for it
        void flinch();

        /// The actor sits, lies or leans in the furniture whose entry is of the kind (0: it does not), holding the pose
        /// that the files have for it. It is put in the place of the entry by whoever puts it in the furniture; the
        /// games play an animation to get in and to get out, which are not played here.
        void setPose(int entryKind);

        /// A script has the actor play an animation file once over what it does (the idle of a PlayIdle), after which
        /// it goes on as it was. False when the files have nothing to play on its skeleton, or it is down.
        bool playIdle(const std::string& file);

    protected:
        FalloutActorAnimation(
            const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);

        /// Plays the idle animation of the skeleton of the actor, when the game has one, and finds the animations with
        /// which it walks and runs. The animations of a female character are those of the folder for women, if there is
        /// one.
        void startAnimations(bool female);

        /// Adds a model to the actor and returns the node that it is, null when there is no model
        osg::ref_ptr<osg::Node> insertPart(std::string_view model);

    private:
        class AdvanceCallback;
        class FallCallback;

        /// Adds the animation file to the actor and gives the name of its group, empty when it has nothing to play on
        /// this skeleton
        std::string addAnimation(const std::string& file, const std::string& skeleton);

        /// Adds the animation file of the weapon move, if there is one, and plays it once over the body above the waist
        /// at the priority. Empty when the files have none.
        std::string playWeaponMove(FalloutWeaponMove move, int priority, bool loop);

        /// The gait that the speed at which the actor was moved calls for
        void updateGait(float duration);
        void setGait(FalloutGait gait);

        osg::ref_ptr<AdvanceCallback> mAdvanceCallback;
        /// The transform that the body falls with, when it has died
        osg::ref_ptr<osg::MatrixTransform> mFall;

        /// The weapon in the hands: the model on the hand, the animation type of the record and the move it makes
        osg::ref_ptr<osg::Node> mWeaponNode;
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
        std::map<std::string, std::string> mPoseGroups;
        /// The animations that scripts had it play, by file
        std::map<std::string, std::string> mIdleGroups;
        /// The group of the idle it stands in, which a script does not play over
        std::string mDefaultIdleGroup;

        std::string mWalkGroup;
        std::string mRunGroup;
        FalloutGaits mGaits;
        FalloutGait mGait = FalloutGait::Idle;
        /// The speed (units a second, across the ground) at which the actor is moved, smoothed over a few frames
        float mSpeed = 0.f;
        osg::Vec3f mLastPosition;
        bool mHasLastPosition = false;
    };
}

#endif
