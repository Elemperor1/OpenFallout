#ifndef GAME_RENDER_ESM4NPCANIMATION_H
#define GAME_RENDER_ESM4NPCANIMATION_H

#include <set>
#include <string_view>
#include <vector>

#include "falloutactoranimation.hpp"

namespace ESM4
{
    struct Npc;
}

namespace OFRender
{
    class ESM4NpcAnimation : public FalloutActorAnimation
    {
    public:
        ESM4NpcAnimation(
            const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);
        ~ESM4NpcAnimation() override;

    private:
        /// Gives a part of a Fallout character its face: the shape of the morph file beside the model, moved by the
        /// coefficients (FGGS and FGGA of the race and of the character together), and the texture of the face or of
        /// the body that the editor wrote for the character, if there is one
        void shapeFalloutPart(osg::Node& part, std::string_view model, const ESM4::Npc& traits,
            const std::vector<float>& symmetric, const std::vector<float>& asymmetric, bool isHead, bool isBody);

        // Works for FO3/FONV/TES5
        void insertHeadParts(const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes);

        void updateParts();
        void updatePartsTES4(const ESM4::Npc& traits);
        void updatePartsFallout(const ESM4::Npc& traits);
        void updatePartsTES5(const ESM4::Npc& traits);
    };
}

#endif // GAME_RENDER_ESM4NPCANIMATION_H
