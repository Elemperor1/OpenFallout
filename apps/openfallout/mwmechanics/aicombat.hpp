#ifndef GAME_MWMECHANICS_AICOMBAT_H
#define GAME_MWMECHANICS_AICOMBAT_H

#include "aitemporarybase.hpp"
#include "aitimer.hpp"
#include "movement.hpp"
#include "typedaipackage.hpp"

#include "../mwworld/cellstore.hpp" // for Doors

#include <components/esm3/loadpgrd.hpp>

namespace ESM
{
    namespace AiSequence
    {
        struct AiCombat;
    }
}

namespace OFMechanics
{
    class Action;

    /// \brief This class holds the variables AiCombat needs which are deleted if the package becomes inactive.
    struct AiCombatStorage : AiTemporaryBase
    {
        float mAttackCooldown;
        AiReactionTimer mReaction;
        float mTimerCombatMove;
        bool mReadyToAttack;
        bool mShouldApproach{ true };
        bool mAttack;
        float mAttackRange;
        bool mCombatMove;
        bool mRotateMove;
        const OFWorld::CellStore* mCell;
        std::unique_ptr<Action> mCurrentAction;
        float mActionCooldown;
        float mStrength;
        bool mForceNoShortcut;
        ESM::Position mShortcutFailPos;
        OFMechanics::Movement mMovement;

        enum FleeState
        {
            FleeState_None,
            FleeState_Idle,
            FleeState_RunBlindly,
            FleeState_RunToDestination
        };
        FleeState mFleeState;
        bool mLOS;
        float mUpdateLOSTimer;
        float mFleeBlindRunTimer;
        ESM::Pathgrid::Point mFleeDest;

        bool mUseCustomDestination;
        osg::Vec3f mCustomDestination;

        AiCombatStorage();

        void startCombatMove(bool isDistantCombat, float distToTarget, float rangeAttack, const OFWorld::Ptr& actor,
            const OFWorld::Ptr& target);
        void updateCombatMove(float duration);
        void stopCombatMove();
        void updateAttack(const OFWorld::Ptr& actor, CharacterController& characterController,
            const ESM::Weapon* weapon, bool distantCombat, float duration);
        void stopAttack();

        void startFleeing();
        void stopFleeing();
        bool isFleeing() const;
    };

    /// \brief Causes the actor to fight another actor
    class AiCombat final : public TypedAiPackage<AiCombat>
    {
    public:
        /// Constructor
        /** \param actor Actor to fight **/
        explicit AiCombat(const OFWorld::Ptr& actor);

        explicit AiCombat(const ESM::AiSequence::AiCombat* combat);

        void init();

        bool execute(const OFWorld::Ptr& actor, CharacterController& characterController, AiState& state,
            float duration) override;

        static constexpr AiPackageTypeId getTypeId() { return AiPackageTypeId::Combat; }

        static constexpr Options makeDefaultOptions()
        {
            AiPackage::Options options;
            options.mPriority = 1;
            options.mCanCancel = false;
            options.mShouldCancelPreviousAi = false;
            return options;
        }

        /// Return the combat target if it is an actor, otherwise an empty Ptr.
        OFWorld::Ptr getTarget() const override;

        void writeState(ESM::AiSequence::AiSequence& sequence) const override;

    private:
        /// Returns true if combat should end
        bool attack(const OFWorld::Ptr& actor, const OFWorld::Ptr& target, AiCombatStorage& storage,
            CharacterController& characterController);

        void updateLOS(const OFWorld::Ptr& actor, const OFWorld::Ptr& target, float duration, AiCombatStorage& storage);

        void updateFleeing(const OFWorld::Ptr& actor, const OFWorld::Ptr& target, float duration,
            OFWorld::MovementDirectionFlags supportedMovementDirections, AiCombatStorage& storage);

        /// Transfer desired movement (from AiCombatStorage) to Actor
        void updateActorsMovement(const OFWorld::Ptr& actor, float duration, AiCombatStorage& storage);
        void rotateActorOnAxis(const OFWorld::Ptr& actor, int axis, OFMechanics::Movement& actorMovementSettings,
            AiCombatStorage& storage);
    };

}

#endif
