#ifndef GAME_MWCLASS_CREATURE_H
#define GAME_MWCLASS_CREATURE_H

#include "../mwworld/registeredclass.hpp"

#include "actor.hpp"

namespace ESM
{
    struct GameSetting;
}

namespace OFClass
{
    class Creature : public OFWorld::RegisteredClass<Creature, Actor>
    {
        friend OFWorld::RegisteredClass<Creature, Actor>;

        Creature();

        void ensureCustomData(const OFWorld::Ptr& ptr) const;

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

        static int getSndGenTypeFromName(const OFWorld::Ptr& ptr, std::string_view name);

        // cached GMSTs
        struct GMST
        {
            const ESM::GameSetting* fMinWalkSpeedCreature;
            const ESM::GameSetting* fMaxWalkSpeedCreature;
            const ESM::GameSetting* fEncumberedMoveEffect;
            const ESM::GameSetting* fSneakSpeedMultiplier;
            const ESM::GameSetting* fAthleticsRunBonus;
            const ESM::GameSetting* fBaseRunMultiplier;
            const ESM::GameSetting* fMinFlySpeed;
            const ESM::GameSetting* fMaxFlySpeed;
            const ESM::GameSetting* fSwimRunBase;
            const ESM::GameSetting* fSwimRunAthleticsMult;
            const ESM::GameSetting* fKnockDownMult;
            const ESM::GameSetting* iKnockDownOddsMult;
            const ESM::GameSetting* iKnockDownOddsBase;
        };

        static const GMST& getGmst();

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        OFMechanics::CreatureStats& getCreatureStats(const OFWorld::Ptr& ptr) const override;
        ///< Return creature stats

        bool evaluateHit(const OFWorld::Ptr& ptr, OFWorld::Ptr& victim, osg::Vec3f& hitPosition) const override;

        void hit(const OFWorld::Ptr& ptr, float attackStrength, float attackWindUp, int type,
            const OFWorld::Ptr& victim, const osg::Vec3f& hitPosition, bool success) const override;

        void onHit(const OFWorld::Ptr& ptr, const std::map<std::string, float>& damages, ESM::RefId object,
            const OFWorld::Ptr& attacker, bool successful,
            const OFMechanics::DamageSourceType sourceType) const override;

        std::string_view getWerewolfRefusalSoundId() const override { return "WolfCreature"; }

        std::unique_ptr<OFWorld::Action> activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const override;
        ///< Generate action for activation

        OFWorld::ContainerStore& getContainerStore(const OFWorld::Ptr& ptr) const override;
        ///< Return container store

        OFWorld::InventoryStore& getInventoryStore(const OFWorld::Ptr& ptr) const override;
        ///< Return inventory store

        bool hasInventoryStore(const OFWorld::ConstPtr& ptr) const override;

        ESM::RefId getScript(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of the script attached to ptr

        float getCapacity(const OFWorld::Ptr& ptr) const override;
        ///< Return total weight that fits into the object. Throws an exception, if the object can't
        /// hold other objects.

        float getArmorRating(const OFWorld::Ptr& ptr, bool useLuaInterfaceIfAvailable) const override;
        ///< @return combined armor rating of this actor

        bool isEssential(const OFWorld::ConstPtr& ptr) const override;
        ///< Is \a ptr essential? (i.e. may losing \a ptr make the game unwinnable)

        int getServices(const OFWorld::ConstPtr& actor) const override;

        bool isPersistent(const OFWorld::ConstPtr& ptr) const override;

        ESM::RefId getSoundIdFromSndGen(const OFWorld::Ptr& ptr, std::string_view name) const override;

        OFMechanics::Movement& getMovementSettings(const OFWorld::Ptr& ptr) const override;
        ///< Return desired movement.

        float getMaxSpeed(const OFWorld::Ptr& ptr) const override;

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;

        void getModelsToPreload(
            const OFWorld::ConstPtr& ptr, std::vector<VFS::Path::NormalizedView>& models) const override;
        ///< Get a list of models to preload that this object may use (directly or indirectly). default implementation:
        ///< list getModel().

        bool isBipedal(const OFWorld::ConstPtr& ptr) const override;
        bool canFly(const OFWorld::ConstPtr& ptr) const override;
        bool canSwim(const OFWorld::ConstPtr& ptr) const override;
        bool canWalk(const OFWorld::ConstPtr& ptr) const override;

        float getSkill(const OFWorld::Ptr& ptr, ESM::RefId id) const override;

        void readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const override;
        ///< Read additional state from \a state into \a ptr.

        void writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const override;
        ///< Write additional state from \a ptr into \a state.

        int getBaseGold(const OFWorld::ConstPtr& ptr) const override;

        void respawn(const OFWorld::Ptr& ptr) const override;

        int getBaseFightRating(const OFWorld::ConstPtr& ptr) const override;

        void adjustScale(const OFWorld::ConstPtr& ptr, osg::Vec3f& scale, bool rendering) const override;
        /// @param rendering Indicates if the scale to adjust is for the rendering mesh, or for the collision mesh

        void setBaseAISetting(const ESM::RefId& id, OFMechanics::AiSetting setting, int value) const override;

        void modifyBaseInventory(const ESM::RefId& actorId, const ESM::RefId& itemId, int amount) const override;

        float getWalkSpeed(const OFWorld::Ptr& ptr) const override;

        float getRunSpeed(const OFWorld::Ptr& ptr) const override;

        float getSwimSpeed(const OFWorld::Ptr& ptr) const override;
    };
}

#endif
