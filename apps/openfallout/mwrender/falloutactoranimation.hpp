#ifndef OPENFALLOUT_MWRENDER_FALLOUTACTORANIMATION_H
#define OPENFALLOUT_MWRENDER_FALLOUTACTORANIMATION_H

#include <string>
#include <string_view>

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

        /// Adds the animation file to the actor and gives the name of its group, empty when it has nothing to play on
        /// this skeleton
        std::string addAnimation(const std::string& file, const std::string& skeleton);

        /// The gait that the speed at which the actor was moved calls for
        void updateGait(float duration);
        void setGait(FalloutGait gait);

        osg::ref_ptr<AdvanceCallback> mAdvanceCallback;

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
