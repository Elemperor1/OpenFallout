#ifndef OPENFALLOUT_MWWORLD_PROJECTILEMANAGER_H
#define OPENFALLOUT_MWWORLD_PROJECTILEMANAGER_H

#include <string>

#include <osg/PositionAttitudeTransform>
#include <osg/ref_ptr>

#include <components/esm3/effectlist.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwbase/soundmanager.hpp"

#include "ptr.hpp"

namespace OFPhysics
{
    class PhysicsSystem;
}

namespace Loading
{
    class Listener;
}

namespace osg
{
    class Group;
    class Quat;
}

namespace Resource
{
    class ResourceSystem;
}

namespace OFRender
{
    class EffectAnimationTime;
    class RenderingManager;
}

namespace OFWorld
{

    class ProjectileManager
    {
    public:
        ProjectileManager(osg::Group* parent, Resource::ResourceSystem* resourceSystem,
            OFRender::RenderingManager* rendering, OFPhysics::PhysicsSystem* physics);

        /// If caster is an actor, the actor's facing orientation is used. Otherwise fallbackDirection is used.
        void launchMagicBolt(const ESM::RefId& spellId, const OFWorld::Ptr& caster, const osg::Vec3f& fallbackDirection,
            ESM::RefNum item);

        void launchProjectile(const OFWorld::Ptr& actor, const OFWorld::ConstPtr& projectile, const osg::Vec3f& pos,
            const osg::Quat& orient, const OFWorld::Ptr& bow, float speed, float attackStrength, float attackWindUp);

        void updateCasters();

        void update(float dt);

        void processHits();

        /// Removes all current projectiles. Should be called when switching to a new worldspace.
        void clear();

        void write(ESM::ESMWriter& writer, Loading::Listener& progress) const;
        bool readRecord(ESM::ESMReader& reader, uint32_t type);
        size_t countSavedGameRecords() const;
        void saveLoaded(const ESM::ESMReader& reader);

    private:
        osg::ref_ptr<osg::Group> mParent;
        Resource::ResourceSystem* mResourceSystem;
        OFRender::RenderingManager* mRendering;
        OFPhysics::PhysicsSystem* mPhysics;
        float mCleanupTimer;

        struct State
        {
            osg::ref_ptr<osg::PositionAttitudeTransform> mNode;
            std::shared_ptr<OFRender::EffectAnimationTime> mEffectAnimationTime;

            ESM::RefNum mCaster;
            OFWorld::Ptr mCasterHandle;

            OFWorld::Ptr getCaster();

            // MW-ids of a magic projectile
            std::vector<ESM::RefId> mIdMagic;

            // MW-id of an arrow projectile
            ESM::RefId mIdArrow;

            int mProjectileId;
            bool mToDelete;
        };

        struct MagicBoltState : public State
        {
            ESM::RefId mSpellId;

            // Name of item to display as effect source in magic menu (in case we casted an enchantment)
            std::string mSourceName;

            ESM::EffectList mEffects;

            float mSpeed;
            // Refnum of the casting item
            ESM::RefNum mItem;

            std::vector<OFBase::Sound*> mSounds;
            std::set<ESM::RefId> mSoundIds;
        };

        struct ProjectileState : public State
        {
            // RefID of the bow or crossbow the actor was using when this projectile was fired (may be empty)
            ESM::RefId mBowId;

            osg::Vec3f mVelocity;
            float mAttackStrength;
            float mAttackWindUp;
        };

        std::vector<MagicBoltState> mMagicBolts;
        std::vector<ProjectileState> mProjectiles;

        void cleanupProjectile(ProjectileState& state);
        void cleanupMagicBolt(MagicBoltState& state);
        void periodicCleanup(float dt);

        void moveProjectiles(float dt);
        void moveMagicBolts(float dt);

        void createModel(State& state, VFS::Path::NormalizedView model, const osg::Vec3f& pos, const osg::Quat& orient,
            bool rotate, bool createLight, osg::Vec4 lightDiffuseColor, VFS::Path::NormalizedView texture = {});
        void update(State& state, float duration);

        void operator=(const ProjectileManager&);
        ProjectileManager(const ProjectileManager&);
    };

}

#endif
