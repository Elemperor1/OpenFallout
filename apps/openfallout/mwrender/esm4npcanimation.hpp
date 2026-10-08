#ifndef GAME_RENDER_ESM4NPCANIMATION_H
#define GAME_RENDER_ESM4NPCANIMATION_H

#include <osg/Vec3f>

#include "animation.hpp"
#include "falloutanimation.hpp"

namespace ESM4
{
    struct Npc;
}

namespace OFRender
{
    class ESM4NpcAnimation : public Animation
    {
    public:
        ESM4NpcAnimation(
            const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);
        ~ESM4NpcAnimation() override;

        /// Moves the time of the animation that plays on, as the frames of the scene do, and switches between standing,
        /// walking and running with the speed at which the character is moved
        void advance(float duration);

    private:
        class AdvanceCallback;

        /// Plays the idle animation of the skeleton of a Fallout character, when the game has one, and finds the
        /// animations with which it walks and runs
        void startAnimations();

        /// Adds the animation file to the character and gives the name of its group, empty when it has nothing to play
        /// on this skeleton
        std::string addAnimation(const std::string& file, const std::string& skeleton);

        /// The gait that the speed at which the character was moved calls for
        void updateGait(float duration);
        void setGait(FalloutGait gait);

        void insertPart(std::string_view model);

        // Works for FO3/FONV/TES5
        void insertHeadParts(const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes);

        void updateParts();
        void updatePartsTES4(const ESM4::Npc& traits);
        void updatePartsFallout(const ESM4::Npc& traits);
        void updatePartsTES5(const ESM4::Npc& traits);

        osg::ref_ptr<AdvanceCallback> mAdvanceCallback;

        std::string mWalkGroup;
        std::string mRunGroup;
        FalloutGaits mGaits;
        FalloutGait mGait = FalloutGait::Idle;
        /// The speed (units a second, across the ground) at which the character is moved, smoothed over a few frames
        float mSpeed = 0.f;
        osg::Vec3f mLastPosition;
        bool mHasLastPosition = false;
    };
}

#endif // GAME_RENDER_ESM4NPCANIMATION_H
