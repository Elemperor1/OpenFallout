#ifndef GAME_RENDER_ESM4NPCANIMATION_H
#define GAME_RENDER_ESM4NPCANIMATION_H

#include "animation.hpp"

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

        /// Moves the time of the animation that plays on, as the frames of the scene do
        void advance(float duration);

    private:
        class AdvanceCallback;

        /// Plays the idle animation of the skeleton of a Fallout character, when the game has one
        void startIdle();

        void insertPart(std::string_view model);

        // Works for FO3/FONV/TES5
        void insertHeadParts(const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes);

        void updateParts();
        void updatePartsTES4(const ESM4::Npc& traits);
        void updatePartsFallout(const ESM4::Npc& traits);
        void updatePartsTES5(const ESM4::Npc& traits);

        osg::ref_ptr<AdvanceCallback> mAdvanceCallback;
    };
}

#endif // GAME_RENDER_ESM4NPCANIMATION_H
