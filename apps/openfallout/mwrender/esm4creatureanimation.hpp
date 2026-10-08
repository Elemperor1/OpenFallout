#ifndef GAME_RENDER_ESM4CREATUREANIMATION_H
#define GAME_RENDER_ESM4CREATUREANIMATION_H

#include "falloutactoranimation.hpp"

namespace OFRender
{
    /// A creature of Fallout 3 or New Vegas: its skeleton with the models that its record (or its template) lists on
    /// it, idle and moving with the animation files in the folder of the skeleton
    class ESM4CreatureAnimation : public FalloutActorAnimation
    {
    public:
        ESM4CreatureAnimation(
            const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem);
        ~ESM4CreatureAnimation() override;
    };
}

#endif // GAME_RENDER_ESM4CREATUREANIMATION_H
