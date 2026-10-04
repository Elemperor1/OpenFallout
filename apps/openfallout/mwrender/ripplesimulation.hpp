#ifndef OPENFALLOUT_MWRENDER_RIPPLESIMULATION_H
#define OPENFALLOUT_MWRENDER_RIPPLESIMULATION_H

#include <osg/ref_ptr>

#include "../mwworld/ptr.hpp"

#include "ripples.hpp"

namespace osg
{
    class Group;
    class PositionAttitudeTransform;
}

namespace osgParticle
{
    class ParticleSystem;
}

namespace Resource
{
    class ResourceSystem;
}

namespace Fallback
{
    class Map;
}

namespace OFRender
{

    struct Emitter
    {
        OFWorld::ConstPtr mPtr;
        osg::Vec3f mLastEmitPosition;
        float mScale;
        float mForce;
        float mTimer;
    };

    class RippleSimulation
    {
    public:
        RippleSimulation(osg::Group* parent, Resource::ResourceSystem* resourceSystem);
        ~RippleSimulation();

        /// @param dt Time since the last frame
        void update(float dt);

        /// adds an emitter, position will be tracked automatically
        void addEmitter(const OFWorld::ConstPtr& ptr, float scale = 1.f, float force = 1.f);
        void removeEmitter(const OFWorld::ConstPtr& ptr);
        void updateEmitterPtr(const OFWorld::ConstPtr& old, const OFWorld::ConstPtr& ptr);
        void removeCell(const OFWorld::CellStore* store);

        void emitRipple(const osg::Vec3f& pos);

        /// Change the height of the water surface, thus moving all ripples with it
        void setWaterHeight(float height);

        /// Remove all active ripples
        void clear();

        void setRipples(Ripples* ripples) { mRipples = ripples; }

    private:
        osg::ref_ptr<osg::Group> mParent;

        osg::ref_ptr<osgParticle::ParticleSystem> mParticleSystem;
        osg::ref_ptr<osg::PositionAttitudeTransform> mParticleNode;

        std::vector<Emitter> mEmitters;

        Ripples* mRipples = nullptr;

        int mMaxNumberRipples;
    };

}

#endif
